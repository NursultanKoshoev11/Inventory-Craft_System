// Copyright Epic Games, Inc. All Rights Reserved.

#include "InventoryItemInstance.h"

#include "InventorySystem/Inventory/InventoryItemDefinition.h"
#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#endif // UE_WITH_IRIS

#include UE_INLINE_GENERATED_CPP_BY_NAME(InventoryItemInstance)

class FLifetimeProperty;

UInventoryItemInstance::UInventoryItemInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, StatTags);
	DOREPLIFETIME(ThisClass, ItemDef);
}

void UInventoryItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.AddStack(Tag, StackCount);
}

void UInventoryItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.RemoveStack(Tag, StackCount);
}

int32 UInventoryItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

int UInventoryItemInstance::GetItemStack()
{
	return ItemStack;
}

void UInventoryItemInstance::SetItemStack(int32 InStack)
{
	ItemStack = InStack;
	OnStackChange.Broadcast();
}

bool UInventoryItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}

void UInventoryItemInstance::SetItemDef(TSubclassOf<UInventoryItemDefinition> InDef)
{
	ItemDef = InDef;
}

const UInventoryItemFragment* UInventoryItemInstance::FindFragmentByClass(TSubclassOf<UInventoryItemFragment> FragmentClass) const
{
	if ((ItemDef != nullptr) && (FragmentClass != nullptr))
	{
		return GetDefault<UInventoryItemDefinition>(ItemDef)->FindFragmentByClass(FragmentClass);
	}

	return nullptr;
}


