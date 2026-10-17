// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EndMenuWidget.generated.h"

class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEndCountdownFinished);

/**
 * Goal / end overlay. Reparent UI_End to this class; keep tx_desc and tx_endTime.
 */
UCLASS(abstract)
class UEndMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "End Menu")
	FOnEndCountdownFinished OnCountdownFinished;

	/** Begin a whole-second countdown; each tick pulses tx_endTime scale 1.2 → 1.0. */
	UFUNCTION(BlueprintCallable, Category = "End Menu")
	void StartCountdown(int32 Seconds);

	UFUNCTION(BlueprintCallable, Category = "End Menu")
	void StopCountdown();

protected:
	virtual void NativeDestruct() override;

	void ApplyCountdownText();
	void BeginPulse();
	void StopPulse();

	UFUNCTION()
	void HandleCountdownTick();

	UFUNCTION()
	void HandlePulseTick();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> tx_desc;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> tx_endTime;

	UPROPERTY(EditAnywhere, Category = "End Menu", meta = (ClampMin = "0.05"))
	float PulseDuration = 0.28f;

	UPROPERTY(EditAnywhere, Category = "End Menu", meta = (ClampMin = "1.0"))
	float PulsePeakScale = 1.2f;

	int32 RemainingSeconds = 0;
	float PulseElapsed = 0.f;
	FTimerHandle CountdownTimerHandle;
	FTimerHandle PulseTimerHandle;
};
