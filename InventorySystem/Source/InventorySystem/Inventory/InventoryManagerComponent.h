// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "InventoryManagerComponent.generated.h"

class UCraftManagerComponent;
class UInventoryItemDefinition;
class UInventoryItemInstance;
class UInventoryManagerComponent;
class UObject;
struct FFrame;
struct FInventoryList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/** A message when an item is added to the inventory */
USTRUCT(BlueprintType)
struct FInventoryChangeMessage
{
	GENERATED_BODY()

	//@TODO: Tag based names+owning actors for inventories instead of directly exposing the component?
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 NewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Delta = 0;
};

/** A single entry in an inventory */
USTRUCT(BlueprintType)
struct FInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FInventoryEntry()
	{}

	FString GetDebugString() const;
	
	UPROPERTY()
	int32 StackCount = 0;
	
private:
	friend FInventoryList;
	friend UInventoryManagerComponent;
	friend UCraftManagerComponent;

	UPROPERTY()
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;



	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

/** List of inventory items */
USTRUCT(BlueprintType)
struct FInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FInventoryList()
		: OwnerComponent(nullptr)
	{
	}

	FInventoryList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<UInventoryItemInstance*> GetAllItems() const;

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryEntry, FInventoryList>(Entries, DeltaParms, *this);
	}

	UInventoryItemInstance* AddEntry(TSubclassOf<UInventoryItemDefinition> ItemClass, int32 StackCount);
	void AddEntry(UInventoryItemInstance* Instance);

	void RemoveEntry(UInventoryItemInstance* Instance);

private:
	void ChangeStackCount(UInventoryItemInstance* Instance, int32 NewCount);

private:
	friend UInventoryManagerComponent;

private:
	// Replicated list of items
	UPROPERTY()
	TArray<FInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FInventoryList> : public TStructOpsTypeTraitsBase2<FInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};







DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOninventoryChange);


/**
 * Manages an inventory
 */
UCLASS(BlueprintType, Blueprintable,meta=(BlueprintSpawnableComponent))
class UInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	bool CanAddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	UInventoryItemInstance* AddItemDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void AddItemInstance(UInventoryItemInstance* ItemInstance);

	UPROPERTY(BlueprintAssignable)
	FOninventoryChange OnInventoryChange;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void RemoveItemInstance(UInventoryItemInstance* ItemInstance);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	void RemoveItemByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 StackCount = 1);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Inventory)
	int32 GetItemCountByInstance(UInventoryItemInstance* ItemInst);

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure=false)
	TArray<UInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, Category=Inventory, BlueprintPure)
	UInventoryItemInstance* FindFirstItemStackByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const;
	
	UFUNCTION(BlueprintCallable, Category=Inventory)
	void MoveItemToInventory(UInventoryManagerComponent* OtherInventory, UInventoryItemInstance* ItemInstance);

	int32 GetTotalItemCountByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef) const;
	bool ConsumeItemsByDefinition(TSubclassOf<UInventoryItemDefinition> ItemDef, int32 NumToConsume);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;
	//~End of UObject interface

private:
	UPROPERTY(Replicated)
	FInventoryList InventoryList;
};
