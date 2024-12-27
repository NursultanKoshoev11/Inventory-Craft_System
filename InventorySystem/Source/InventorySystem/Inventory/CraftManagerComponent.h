// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "CraftManagerComponent.generated.h"


class UInventoryItemDefinition;
class UInventoryItemInstance;
class UInventoryManagerComponent;
class UDataTable;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCraftChange);

USTRUCT(BlueprintType)
struct FCraftStruct
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly,Category = "Crafting")
	TSubclassOf<UInventoryItemDefinition> ItemDefinitionClass;
	UPROPERTY(EditDefaultsOnly,Category = "Crafting")
	int32 StackCount = 1;
	
};

USTRUCT(BlueprintType)
struct FCraftListStruct : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category = "Crafting")
	TArray<FCraftStruct> ItensForCraft;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category = "Crafting")
	FCraftStruct ResultItem;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INVENTORYSYSTEM_API UCraftManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCraftManagerComponent();

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	void SetInventoryManagerComponent(TSoftObjectPtr<UInventoryManagerComponent> InInventoryManagerComponent);

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	bool AddItemToCraft(UInventoryItemInstance* InventoryItemInstance,int ID);

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	bool MoveItemFromCraftToInventory(UInventoryItemInstance* InventoryItemInstance);

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	bool Craft();

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	TSubclassOf<UInventoryItemDefinition> GetResultInventoryItem() const
	{
		return ResultInventoryItem.ItemDefinitionClass;
	}

	UFUNCTION(BlueprintCallable,Category = "Crafting")
	TArray<UInventoryItemInstance*> GetCrafItemInstances() const {return CraftItemInstances;};
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	void SetupDatas();
	void UpdateCraft();

private:
	TSoftObjectPtr<UInventoryManagerComponent> InventoryManagerComponent;

	TArray<UInventoryItemInstance*> CraftItemInstances;
	TArray<int32> StackCounts;

	FCraftStruct ResultInventoryItem;
	int32 MultiplierCount = 0;

	TArray<FCraftListStruct*> CraftListData;

public:
	UPROPERTY(EditDefaultsOnly,Category="Setup",meta = (ClampMin = "1",ClampMax = "5"))
	int16 NumberOfCells = 1;

	UPROPERTY(BlueprintAssignable)
	FOnCraftChange OnCraftChange;

	UPROPERTY(EditDefaultsOnly,Category = "Crafting")
	UDataTable* CraftDataTable;
};