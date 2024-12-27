// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/Texture2D.h"
#include "InventorySystem/Inventory/InventoryItemDefinition.h"
#include "InventoryFragment_DisplayInfo.generated.h"

/**
 * 
 */
UCLASS()
class UInventoryFragment_DisplayInfo : public UInventoryItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite,EditAnywhere,Category = "Inventory")
	FText DisplayName;

	UPROPERTY(	BlueprintReadWrite, EditAnywhere,Category = "Inventory")
	FText Description;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category = "Inventory")
	UTexture2D* Icon;

	UPROPERTY(BlueprintReadWrite, EditAnywhere,Category = "Inventory")
	FGameplayTag Category;
};
