// Copyright Epic Games, Inc. All Rights Reserved.

#include "InventoryManagerComponent.h"

#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "InventoryItemDefinition.h"
#include "InventoryItemInstance.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "CraftManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryManagerComponent)

class FLifetimeProperty;
struct FReplicationFlags;

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Lyra_Inventory_Message_StackChanged, "Inventory.Message.StackChanged");

//////////////////////////////////////////////////////////////////////
// FLyraInventoryEntry

FString FInventoryEntry::GetDebugString() const
{
	TSubclassOf<UInventoryItemDefinition> ItemDef;
	if (Instance != nullptr)
	{
		ItemDef = Instance->GetItemDef();
	}

	return FString::Printf(TEXT("%s (%d x %s)"), *GetNameSafe(Instance), StackCount, *GetNameSafe(ItemDef));
}

//////////////////////////////////////////////////////////////////////
// FLyraInventoryList

void FInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FInventoryEntry& Stack = Entries[Index];
		Stack.LastObservedCount = 0;
	}
}

void FInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FInventoryEntry& Stack = Entries[Index];
		Stack.LastObservedCount = Stack.StackCount;
	}
}

void FInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FInventoryEntry& Stack = Entries[Index];
		check(Stack.LastObservedCount != INDEX_NONE);
		Stack.LastObservedCount = Stack.StackCount;
	}
}


void FInventoryList::ChangeStackCount(UInventoryItemInstance* Instance, int32 NewCount)
{
	for (FInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			int32 OldCount = Entry.StackCount;
			Entry.Instance->SetItemStack(NewCount);
			Entry.StackCount = NewCount;
			MarkItemDirty(Entry);
			return;
		}
	}
}



UInventoryItemInstance* FInventoryList::AddEntry(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount)
{
	UInventoryItemInstance* Result = nullptr;

	check(ItemDef != nullptr);
 	check(OwnerComponent);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor->HasAuthority());

	FInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewObject<UInventoryItemInstance>(OwnerComponent->GetOwner());  //@TODO: Using the actor instead of component as the outer due to UE-127172
	NewEntry.Instance->SetItemDef(ItemDef);
	for (UInventoryItemFragment* Fragment : GetDefault<UInventoryItemDefinition>(ItemDef)->Fragments)
	{
		if (Fragment != nullptr)
		{
			Fragment->OnInstanceCreated(NewEntry.Instance);
		}
	}
	NewEntry.StackCount = StackCount;
	NewEntry.Instance->SetItemStack(StackCount);
	Result = NewEntry.Instance;
	

	//const ULyraInventoryItemDefinition* ItemCDO = GetDefault<ULyraInventoryItemDefinition>(ItemDef);
	MarkItemDirty(NewEntry);
	return Result;
}

void FInventoryList::AddEntry(UInventoryItemInstance* Instance)
{
	unimplemented();
}

void FInventoryList::RemoveEntry(UInventoryItemInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			EntryIt.RemoveCurrent();
			MarkArrayDirty();
		}
	}
}

TArray<UInventoryItemInstance*> FInventoryList::GetAllItems() const
{
	TArray<UInventoryItemInstance*> Results;
	Results.Reserve(Entries.Num());
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr) //@TODO: Would prefer to not deal with this here and hide it further?
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

//////////////////////////////////////////////////////////////////////
// ULyraInventoryManagerComponent

UInventoryManagerComponent::UInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
}

void UInventoryManagerComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

bool UInventoryManagerComponent::CanAddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount)
{
	//@TODO: Add support for stack limit / uniqueness checks / etc...
	return true;
}

UInventoryItemInstance* UInventoryManagerComponent::AddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount)
{
	UInventoryItemInstance* Result = nullptr;
	if (ItemDef != nullptr)
	{
		Result = FindFirstItemStackByDefinition(ItemDef);
		if(IsValid(Result))
		{
			InventoryList.ChangeStackCount(Result, GetItemCountByInstance(Result) + StackCount);
			return Result;
		}

		Result = InventoryList.AddEntry(ItemDef, StackCount);
		
		if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result)
		{
			AddReplicatedSubObject(Result);
		}
		OnInventoryChange.Broadcast();
	}
	return Result;
}

void UInventoryManagerComponent::AddItemInstance(UInventoryItemInstance* ItemInstance)
{
	InventoryList.AddEntry(ItemInstance);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && ItemInstance)
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

void UInventoryManagerComponent::RemoveItemInstance(UInventoryItemInstance* ItemInstance)
{
	if(IsValid(ItemInstance))
	{
		ItemInstance->OnItemRemove.Broadcast();
	}
	InventoryList.RemoveEntry(ItemInstance);

	if (ItemInstance && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}
}

void UInventoryManagerComponent::RemoveItemByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef,
	int32 StackCount)
{
	UInventoryItemInstance* Item = FindFirstItemStackByDefinition(ItemDef);
	if (Item)
	{
		InventoryList.ChangeStackCount(Item, GetItemCountByInstance(Item) - StackCount);
	}
	if(Item->ItemStack <= 0)
	{
		RemoveItemInstance(Item);
	}
}

int32 UInventoryManagerComponent::GetItemCountByInstance(UInventoryItemInstance* ItemInst) 
{
	for (const FInventoryEntry& Entry : InventoryList.Entries)
	{
		UInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance == ItemInst)
			{
				return Entry.StackCount;
			}
		}
	}

	return 0;
}

TArray<UInventoryItemInstance*> UInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

UInventoryItemInstance* UInventoryManagerComponent::FindFirstItemStackByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const
{
	for (const FInventoryEntry& Entry : InventoryList.Entries)
	{
		UInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				return Instance;
			}
		}
	}

	return nullptr;
}

void UInventoryManagerComponent::MoveItemToInventory(UInventoryManagerComponent* OtherInventory,
	UInventoryItemInstance* ItemInstance)
{
	if (OtherInventory == nullptr || ItemInstance == nullptr)
	{
		return;
	}

	OtherInventory->AddItemDefinition(ItemInstance->GetItemDef(), ItemInstance->GetItemStack());
	RemoveItemInstance(ItemInstance);
}

int32 UInventoryManagerComponent::GetTotalItemCountByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const
{
	int32 TotalCount = 0;
	for (const FInventoryEntry& Entry : InventoryList.Entries)
	{
		UInventoryItemInstance* Instance = Entry.Instance;

		if (IsValid(Instance))
		{
			if (Instance->GetItemDef() == ItemDef)
			{
				++TotalCount;
			}
		}
	}

	return TotalCount;
}

bool UInventoryManagerComponent::ConsumeItemsByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 NumToConsume)
{
	AActor* OwningActor = GetOwner();
	if (!OwningActor || !OwningActor->HasAuthority())
	{
		return false;
	}

	//@TODO: N squared right now as there's no acceleration structure
	int32 TotalConsumed = 0;
	while (TotalConsumed < NumToConsume)
	{
		if (UInventoryItemInstance* Instance = UInventoryManagerComponent::FindFirstItemStackByDefinition(ItemDef))
		{
			InventoryList.RemoveEntry(Instance);
			++TotalConsumed;
		}
		else
		{
			return false;
		}
	}

	return TotalConsumed == NumToConsume;
}

void UInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	// Register existing ULyraInventoryItemInstance
	if (IsUsingRegisteredSubObjectList())
	{
		for (const FInventoryEntry& Entry : InventoryList.Entries)
		{
			UInventoryItemInstance* Instance = Entry.Instance;

			if (IsValid(Instance))
			{
				AddReplicatedSubObject(Instance);
			}
		}
	}
}

bool UInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		UInventoryItemInstance* Instance = Entry.Instance;

		if (Instance && IsValid(Instance))
		{
			WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

//////////////////////////////////////////////////////////////////////
//

// UCLASS(Abstract)
// class ULyraInventoryFilter : public UObject
// {
// public:
// 	virtual bool PassesFilter(ULyraInventoryItemInstance* Instance) const { return true; }
// };

// UCLASS()
// class ULyraInventoryFilter_HasTag : public ULyraInventoryFilter
// {
// public:
// 	virtual bool PassesFilter(ULyraInventoryItemInstance* Instance) const { return true; }
// };


