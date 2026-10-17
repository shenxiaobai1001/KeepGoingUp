// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DistanceWidget.generated.h"

class UTextBlock;

/**
 * Overhead distance readout. Reparent BP_Distance to this class; keep the TextBlock named tx_distance.
 */
UCLASS(abstract)
class UDistanceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Distance")
	void SetDistanceDisplay(int32 Distance, FLinearColor Color);

	/** Running on the road: distance visible, HP hidden. */
	UFUNCTION(BlueprintCallable, Category = "Distance")
	void ShowRoadDistance(int32 Distance, FLinearColor Color);

	/** Held at BP_Start: HP visible, distance hidden. */
	UFUNCTION(BlueprintCallable, Category = "Distance")
	void ShowPitHp(int32 Hp);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> tx_distance;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> tx_Hp;
};
