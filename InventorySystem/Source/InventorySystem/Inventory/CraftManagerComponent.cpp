// Fill out your copyright notice in the Description page of Project Settings.


#include "CraftManagerComponent.h"
#include "InventoryManagerComponent.h"
#include "InventoryItemInstance.h"
#include "InventoryItemDefinition.h"



// Sets default values for this component's properties
UCraftManagerComponent::UCraftManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void UCraftManagerComponent::SetInventoryManagerComponent(
	TSoftObjectPtr<UInventoryManagerComponent> InInventoryManagerComponent)
{
	InventoryManagerComponent = InInventoryManagerComponent;
}

bool UCraftManagerComponent::AddItemToCraft(UInventoryItemInstance* InventoryItemInstance, int ID)
{
	if(!InventoryManagerComponent or !InventoryItemInstance) return false;
	if(!(ID>=0 && ID < CraftItemInstances.Num())) return false;

	CraftItemInstances[ID] = InventoryItemInstance;
	InventoryManagerComponent->RemoveItemInstance(InventoryItemInstance);
	UpdateCraft();
	OnCraftChange.Broadcast();
	
	return true;
}

bool UCraftManagerComponent::MoveItemFromCraftToInventory(UInventoryItemInstance* InventoryItemInstance)
{
	if(!InventoryManagerComponent or !InventoryItemInstance) return false;

	auto ItemIndex = CraftItemInstances.Find(InventoryItemInstance);
	if(ItemIndex == -1) return false;

	InventoryManagerComponent->AddItemInstance(InventoryItemInstance);
	CraftItemInstances[ItemIndex] = nullptr;
	UpdateCraft();
	OnCraftChange.Broadcast();
	return true;
}

bool UCraftManagerComponent::Craft()
{
	if(InventoryManagerComponent and ResultInventoryItem.ItemDefinitionClass)
	{
		int32 RemoveCount = MultiplierCount>0?MultiplierCount:1;
		for(int i = 0; i<CraftItemInstances.Num(); i++)
		{
			if(CraftItemInstances[i])
			{				
				CraftItemInstances[i]->SetItemStack(CraftItemInstances[i]->GetItemStack()-(StackCounts[i]*RemoveCount));
				if(CraftItemInstances[i]->GetItemStack()<=0)
				{
					if(IsValid(CraftItemInstances[i]))
					{
						CraftItemInstances[i]->ConditionalBeginDestroy();
						CraftItemInstances[i] = nullptr;
					}
				}
			}
				
		}
		InventoryManagerComponent->AddItemDefinition(ResultInventoryItem.ItemDefinitionClass,(ResultInventoryItem.StackCount*RemoveCount));
		ResultInventoryItem.ItemDefinitionClass = nullptr;

		UpdateCraft();
		
		return true;
	}
	return false;
}


// Called when the game starts
void UCraftManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	for (int i = 1; i<=NumberOfCells;i++)
	{
		UInventoryItemInstance* CraftItemInstance = nullptr;
		CraftItemInstances.Add(CraftItemInstance);
	}

	StackCounts.SetNum(NumberOfCells);
	
	if(CraftDataTable)
		CraftDataTable->RowStruct = FCraftListStruct::StaticStruct();
	SetupDatas();
	
}

void UCraftManagerComponent::SetupDatas()
{
	if(!IsValid(CraftDataTable)) return;
	
	auto Names = CraftDataTable->GetRowNames();
	for(const FName &Name : Names)
	{
		FCraftListStruct *RowData = CraftDataTable->FindRow<FCraftListStruct>(Name,"GetRow");
		if(RowData)
		{
			CraftListData.Add(RowData);
		}
	}
}

void UCraftManagerComponent::UpdateCraft()
{
	if(CraftItemInstances.IsEmpty() or CraftListData.IsEmpty())
	{
		return;
	}

	FCraftStruct *ResultData = nullptr;
	
	for(auto Data : CraftListData)
	{		
		TMap<TSubclassOf<UInventoryItemDefinition>,int32> FirstMap;
		TMap<TSubclassOf<UInventoryItemDefinition>,int32> SecondMap;
		for(const auto& Elem : Data->ItensForCraft)
		{
			FirstMap.FindOrAdd(Elem.ItemDefinitionClass) = Elem.StackCount;
		}
		
		for(const auto& Instance : CraftItemInstances)
		{
			if(Instance)
			{
				SecondMap.FindOrAdd(Instance->GetItemDef());	
			}
		}

		bool IsNotFind = false;
		int32 ID = 0;
		for(const auto& Elem : SecondMap)
		{
			int32* Item = FirstMap.Find(Elem.Key);
			if( Item == nullptr or (FirstMap.Num()!=SecondMap.Num()))
			{
				IsNotFind = true;
				break;
			}
			StackCounts[ID] = *Item;
			ID++;
		}
		if(!IsNotFind)
		{
			for(int i = 0; i < CraftItemInstances.Num(); i++)
			{
				if(CraftItemInstances[i])
				{
					if(!(CraftItemInstances[i]->GetItemStack()>= StackCounts[i]))
					{
						IsNotFind = true;
						break;
					}
					int32 TmpCount = CraftItemInstances[i]->GetItemStack()/(StackCounts[i]);
					TmpCount = TmpCount<0?0:TmpCount;
					if(MultiplierCount == 0 or TmpCount < MultiplierCount)
						MultiplierCount = TmpCount;
				}
			}
			if(!IsNotFind)
				ResultData = &Data->ResultItem;
		}
	}
	if(ResultData)
		ResultInventoryItem = *ResultData;
}