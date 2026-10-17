// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GamingHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * In-game HUD. Reparent UI_Gaming to this class; keep tx_passTime / passSlider / tx_pass.
 */
UCLASS(abstract)
class UGamingHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Gaming HUD")
	void SetPassCount(int32 PassCount);

	UFUNCTION(BlueprintCallable, Category = "Gaming HUD")
	void SetProgress(float Progress01);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> tx_passDesc;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> tx_passTime;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> passSlider;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> tx_pass;
};
