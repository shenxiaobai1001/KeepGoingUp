// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KeepGoingUpGameMode.generated.h"

class UUserWidget;
class UStartMenuWidget;
class UEndMenuWidget;
class UGamingHUDWidget;
class UKeepGoingUpSettingsWidget;
class AMapCreater;
class AKeepGoingUpCharacter;

UENUM(BlueprintType)
enum class EKeepGoingUpPlayState : uint8
{
	Menu,
	Playing,
	Finished
};

/**
 * Owns start / play / finish loop and the UI_Start / UI_End widgets.
 * Prefer reparenting UI_Start → StartMenuWidget and UI_End → EndMenuWidget;
 * name-based binding is used as a fallback if those parents are not set yet.
 */
UCLASS(abstract)
class AKeepGoingUpGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKeepGoingUpGameMode();

	UFUNCTION(BlueprintPure, Category = "Game Flow")
	EKeepGoingUpPlayState GetPlayState() const { return PlayState; }

	UFUNCTION(BlueprintPure, Category = "Game Flow")
	bool IsGameplayActive() const { return PlayState == EKeepGoingUpPlayState::Playing; }

	/** Called by the character after EndPos walk + front camera turn finishes. */
	void NotifyPlayerEndSequenceComplete();

	/**
	 * Prank: close UI_End countdown and return to Playing without going to the start menu.
	 * Used when "传送起点" fires during the end countdown window.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void AbortEndCountdownForPrank();

	/** Menu / Finished → Playing so prank teleports (and PrankUI 测试) always work. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void EnsurePlayingForPrank();

	/** HP at BP_Start hit 0: show UI_Fail, then UI_Start after one second. */
	void NotifyStartPitFailed();

	/** Cached spawn transform used by prank teleport-to-start. */
	UFUNCTION(BlueprintPure, Category = "Prank")
	bool GetPlayerStartTransform(FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetTileSoundVolume() const { return TileSoundVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	float GetPrankSoundVolume() const { return PrankSoundVolume; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	int32 GetStartPitMaxHp() const { return StartPitMaxHp; }

	UFUNCTION(BlueprintPure, Category = "Settings")
	int32 GetMapLength() const;

	void SetTileSoundVolume(float Volume);
	void SetPrankSoundVolume(float Volume);
	void ApplyMapLength(int32 LayerCount);
	void ApplyStartPitMaxHp(int32 Hp);
	void CloseSettings();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void EnterMenuState(bool bResetPlayer);
	void EnterPlayingState();
	void EnterFinishedState();

	void ShowStartMenu();
	void HideStartMenu();
	void ShowEndMenu();
	void HideEndMenu();
	void ShowGamingHUD();
	void HideGamingHUD();
	void UpdateGamingHUD();

	void BindStartMenuButtons(UUserWidget* Widget);
	void UnbindStartMenuButtons();
	void BindSettingsButton(UUserWidget* Widget);
	void UnbindSettingsButton();

	void ApplyMenuInputMode();
	void ApplyGameplayInputMode();
	void ApplyFinishedInputMode();
	void MaintainStartMenuPrankUI();
	bool IsPrankConfigOpen() const;
	void RaisePrankConfigWidget() const;

	AMapCreater* FindMapCreater() const;
	AKeepGoingUpCharacter* FindPlayerCharacter() const;
	void ResetPlayerToStart();
	void CachePlayerStartTransform();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleExitClicked();

	UFUNCTION()
	void HandleEndCountdownFinished();

	void StartFallbackEndCountdown();
	void StopFallbackEndCountdown();

	UFUNCTION()
	void HandleFallbackCountdownTick();

	UFUNCTION()
	void HandleFallbackPulseTick();

	void ShowFailWidget();
	void HideFailWidget();

	UFUNCTION()
	void HandleStartPitFailDelay();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Flow|UI")
	TSubclassOf<UUserWidget> StartMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Flow|UI")
	TSubclassOf<UUserWidget> EndMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Flow|UI")
	TSubclassOf<UUserWidget> GamingHUDClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Flow", meta = (ClampMin = "1"))
	int32 EndCountdownSeconds = 10;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> StartMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UStartMenuWidget> TypedStartMenu;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> EndMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UEndMenuWidget> TypedEndMenu;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> GamingHUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGamingHUDWidget> TypedGamingHUD;

	UPROPERTY(Transient)
	TObjectPtr<AMapCreater> CachedMapCreater;

	EKeepGoingUpPlayState PlayState = EKeepGoingUpPlayState::Menu;

	bool bHasPlayerStartTransform = false;
	FTransform PlayerStartTransform = FTransform::Identity;

	/** Times the player has completed the end sequence this session. */
	int32 PassCount = 0;

	float TileSoundVolume = 1.f;
	float PrankSoundVolume = 1.f;
	int32 StartPitMaxHp = 10000;

	UPROPERTY(Transient)
	TObjectPtr<UKeepGoingUpSettingsWidget> SettingsWidget;

	int32 FallbackRemainingSeconds = 0;
	float FallbackPulseElapsed = 0.f;
	FTimerHandle FallbackCountdownTimer;
	FTimerHandle FallbackPulseTimer;
	FTimerHandle StartPitFailTimer;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> FailWidget;
};
