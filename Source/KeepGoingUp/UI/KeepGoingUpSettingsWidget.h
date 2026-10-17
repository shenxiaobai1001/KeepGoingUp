// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KeepGoingUpSettingsWidget.generated.h"

class USlider;
class UEditableTextBox;
class UTextBlock;

/** Start-menu settings: tile sound, prank sound, map length, and start-pit HP. */
UCLASS()
class UKeepGoingUpSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	void BuildLayout();
	void RefreshValues();

	UFUNCTION()
	void HandleTileVolumeChanged(float Value);

	UFUNCTION()
	void HandlePrankVolumeChanged(float Value);

	UFUNCTION()
	void HandleMapLengthConfirmed();

	UFUNCTION()
	void HandlePitHpConfirmed();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	TObjectPtr<USlider> TileVolumeSlider;

	UPROPERTY()
	TObjectPtr<USlider> PrankVolumeSlider;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> MapLengthBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> PitHpBox;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	bool bLayoutBuilt = false;
};
