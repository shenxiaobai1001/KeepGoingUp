// Copyright Epic Games, Inc. All Rights Reserved.

#include "KeepGoingUpGameMode.h"
#include "KeepGoingUpCharacter.h"
#include "KeepGoingUp.h"
#include "Map/MapCreater.h"
#include "Prank/KeepGoingUpPrankBridge.h"
#include "UI/StartMenuWidget.h"
#include "UI/EndMenuWidget.h"
#include "UI/GamingHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"

namespace
{
	class FStartMenuTabRelay : public IInputProcessor
	{
	public:
		TWeakObjectPtr<AKeepGoingUpGameMode> GameMode;

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() != EKeys::Tab || InKeyEvent.IsRepeat())
			{
				return false;
			}

			AKeepGoingUpGameMode* Mode = GameMode.Get();
			if (!Mode || Mode->GetPlayState() != EKeepGoingUpPlayState::Menu)
			{
				return false;
			}

			APlayerController* PC = UGameplayStatics::GetPlayerController(Mode, 0);
			if (!PC)
			{
				return false;
			}

			// UI-only menu input blocks the level actor. Hand Tab to BP_PrankUI's existing key event.
			const FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(
				EKeys::Tab,
				IE_Pressed,
				1.f,
				-1,
				InKeyEvent.GetInputDeviceId());
			PC->InputKey(Args);
			return true;
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("StartMenuTabRelay");
		}
	};

	TSharedPtr<FStartMenuTabRelay> GStartMenuTabRelay;
}

AKeepGoingUpGameMode::AKeepGoingUpGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FClassFinder<UUserWidget> StartFinder(TEXT("/Game/Game/Code/UI/UI_Start"));
	if (StartFinder.Succeeded())
	{
		StartMenuClass = StartFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> EndFinder(TEXT("/Game/Game/Code/UI/UI_End"));
	if (EndFinder.Succeeded())
	{
		EndMenuClass = EndFinder.Class;
	}

	static ConstructorHelpers::FClassFinder<UUserWidget> GamingFinder(TEXT("/Game/Game/Code/UI/UI_Gaming"));
	if (GamingFinder.Succeeded())
	{
		GamingHUDClass = GamingFinder.Class;
	}
}

void AKeepGoingUpGameMode::BeginPlay()
{
	Super::BeginPlay();

	CachedMapCreater = FindMapCreater();
	CachePlayerStartTransform();

	if (CachedMapCreater)
	{
		CachedMapCreater->SetCarSpawningEnabled(false);
		CachedMapCreater->RecycleAllCars();
		CachedMapCreater->ResetMapEnd();
		CachedMapCreater->ResetScenery();
	}

	EnterMenuState(false);

	if (FSlateApplication::IsInitialized())
	{
		if (!GStartMenuTabRelay.IsValid())
		{
			GStartMenuTabRelay = MakeShared<FStartMenuTabRelay>();
			FSlateApplication::Get().RegisterInputPreProcessor(GStartMenuTabRelay, 0);
		}
		GStartMenuTabRelay->GameMode = this;
	}

	// Auto-spawn prank bridge so PrankUI-queued 传送起点/终点 execute without extra BP wiring.
	{
		TArray<AActor*> Existing;
		UGameplayStatics::GetAllActorsOfClass(this, AKeepGoingUpPrankBridge::StaticClass(), Existing);
		if (Existing.Num() == 0)
		{
			FActorSpawnParameters Params;
			Params.Name = TEXT("KeepGoingUpPrankBridge");
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AKeepGoingUpPrankBridge>(AKeepGoingUpPrankBridge::StaticClass(), FTransform::Identity, Params);
		}
	}
}

void AKeepGoingUpGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GStartMenuTabRelay.IsValid() && GStartMenuTabRelay->GameMode.Get() == this)
	{
		GStartMenuTabRelay->GameMode = nullptr;
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(GStartMenuTabRelay);
		}
		GStartMenuTabRelay.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void AKeepGoingUpGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PlayState == EKeepGoingUpPlayState::Menu)
	{
		MaintainStartMenuPrankUI();
	}
	else if (PlayState == EKeepGoingUpPlayState::Playing || PlayState == EKeepGoingUpPlayState::Finished)
	{
		UpdateGamingHUD();
	}
}

void AKeepGoingUpGameMode::EnterMenuState(bool bResetPlayer)
{
	PlayState = EKeepGoingUpPlayState::Menu;

	HideEndMenu();
	HideGamingHUD();

	if (CachedMapCreater)
	{
		CachedMapCreater->SetCarSpawningEnabled(false);
		CachedMapCreater->RecycleAllCars();
		CachedMapCreater->ResetMapEnd();
		CachedMapCreater->ResetScenery();
	}

	HideFailWidget();

	if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
	{
		Character->ClearStartPit();
		Character->EndCarPush();
		Character->ResetEndSequence();
		Character->SetRunGameplayEnabled(false, false);
		if (bResetPlayer)
		{
			ResetPlayerToStart();
		}
	}

	ApplyMenuInputMode();
	ShowStartMenu();
}

void AKeepGoingUpGameMode::EnterPlayingState()
{
	PlayState = EKeepGoingUpPlayState::Playing;

	HideStartMenu();
	HideEndMenu();
	HideFailWidget();
	ShowGamingHUD();
	UpdateGamingHUD();

	if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
	{
		Character->ClearStartPit();
		Character->EndCarPush();
		Character->ResetEndSequence();
		Character->SetRunGameplayEnabled(true, true);
	}

	if (CachedMapCreater)
	{
		CachedMapCreater->RecycleAllCars();
		CachedMapCreater->ResetMapEnd();
		// Seed scenery once when the run starts (menu no longer pre-spawns).
		CachedMapCreater->ResetScenery();
		CachedMapCreater->SetCarSpawningEnabled(true);
	}

	ApplyGameplayInputMode();
}

void AKeepGoingUpGameMode::NotifyPlayerEndSequenceComplete()
{
	// Pass count waits until UI_End countdown finishes (HandleEndCountdownFinished).
	// Reaching the end / starting the countdown must not count as a clear.
	EnterFinishedState();
}

void AKeepGoingUpGameMode::EnterFinishedState()
{
	if (PlayState == EKeepGoingUpPlayState::Finished)
	{
		return;
	}

	PlayState = EKeepGoingUpPlayState::Finished;

	if (CachedMapCreater)
	{
		CachedMapCreater->SetCarSpawningEnabled(false);
		CachedMapCreater->RecycleAllCars();
	}

	if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
	{
		Character->EndCarPush();
		// Keep victory pose / camera; no free look during countdown
		Character->SetRunGameplayEnabled(false, false);
	}

	ApplyFinishedInputMode();
	ShowEndMenu();
}

void AKeepGoingUpGameMode::NotifyStartPitFailed()
{
	if (GetWorldTimerManager().IsTimerActive(StartPitFailTimer))
	{
		return;
	}

	ShowFailWidget();
	GetWorldTimerManager().SetTimer(StartPitFailTimer, this, &AKeepGoingUpGameMode::HandleStartPitFailDelay, 1.f, false);
}

void AKeepGoingUpGameMode::HandleStartPitFailDelay()
{
	HideFailWidget();
	EnterMenuState(true);
}

void AKeepGoingUpGameMode::ShowFailWidget()
{
	if (FailWidget)
	{
		FailWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	UClass* FailClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Game/Code/UI/UI_Fail.UI_Fail_C"));
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!FailClass || !PC)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: UI_Fail missing."));
		return;
	}

	FailWidget = CreateWidget<UUserWidget>(PC, FailClass);
	if (FailWidget)
	{
		FailWidget->AddToViewport(200);
	}
}

void AKeepGoingUpGameMode::HideFailWidget()
{
	GetWorldTimerManager().ClearTimer(StartPitFailTimer);
	if (FailWidget)
	{
		FailWidget->RemoveFromParent();
		FailWidget = nullptr;
	}
}

void AKeepGoingUpGameMode::ShowStartMenu()
{
	if (StartMenuWidget)
	{
		StartMenuWidget->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	if (!StartMenuClass)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: StartMenuClass is not set (UI_Start)."));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	StartMenuWidget = CreateWidget<UUserWidget>(PC, StartMenuClass);
	if (!StartMenuWidget)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: failed to create UI_Start."));
		return;
	}

	TypedStartMenu = Cast<UStartMenuWidget>(StartMenuWidget);
	if (TypedStartMenu)
	{
		TypedStartMenu->OnStartClicked.AddDynamic(this, &AKeepGoingUpGameMode::HandleStartClicked);
		TypedStartMenu->OnExitClicked.AddDynamic(this, &AKeepGoingUpGameMode::HandleExitClicked);
	}
	else
	{
		BindStartMenuButtons(StartMenuWidget);
	}

	StartMenuWidget->AddToViewport(100);
}

void AKeepGoingUpGameMode::HideStartMenu()
{
	UnbindStartMenuButtons();

	if (TypedStartMenu)
	{
		TypedStartMenu->OnStartClicked.RemoveDynamic(this, &AKeepGoingUpGameMode::HandleStartClicked);
		TypedStartMenu->OnExitClicked.RemoveDynamic(this, &AKeepGoingUpGameMode::HandleExitClicked);
		TypedStartMenu = nullptr;
	}

	if (StartMenuWidget)
	{
		StartMenuWidget->RemoveFromParent();
		StartMenuWidget = nullptr;
	}
}

void AKeepGoingUpGameMode::ShowGamingHUD()
{
	if (GamingHUDWidget)
	{
		GamingHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		UpdateGamingHUD();
		return;
	}

	if (!GamingHUDClass)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: GamingHUDClass is not set (UI_Gaming)."));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	GamingHUDWidget = CreateWidget<UUserWidget>(PC, GamingHUDClass);
	if (!GamingHUDWidget)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: failed to create UI_Gaming."));
		return;
	}

	TypedGamingHUD = Cast<UGamingHUDWidget>(GamingHUDWidget);
	GamingHUDWidget->AddToViewport(50);
	GamingHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	UpdateGamingHUD();
}

void AKeepGoingUpGameMode::HideGamingHUD()
{
	if (GamingHUDWidget)
	{
		GamingHUDWidget->RemoveFromParent();
		GamingHUDWidget = nullptr;
	}
	TypedGamingHUD = nullptr;
}

void AKeepGoingUpGameMode::UpdateGamingHUD()
{
	if (!GamingHUDWidget)
	{
		return;
	}

	float Progress = 0.f;
	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (CachedMapCreater)
	{
		const float GoalX = FMath::Max(CachedMapCreater->GetGoalLocalX(), KINDA_SMALL_NUMBER);
		float PlayerLocalX = 0.f;
		if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
		{
			PlayerLocalX = CachedMapCreater->WorldToRoadLocal(Character->GetActorLocation()).X;
		}
		Progress = FMath::Clamp(PlayerLocalX / GoalX, 0.f, 1.f);
	}

	if (TypedGamingHUD)
	{
		TypedGamingHUD->SetPassCount(PassCount);
		TypedGamingHUD->SetProgress(Progress);
		return;
	}

	// Name-based fallback if UI_Gaming is not parented to GamingHUDWidget.
	if (UTextBlock* PassTime = Cast<UTextBlock>(GamingHUDWidget->GetWidgetFromName(TEXT("tx_passTime"))))
	{
		PassTime->SetText(FText::AsNumber(PassCount));
	}
	if (UProgressBar* Slider = Cast<UProgressBar>(GamingHUDWidget->GetWidgetFromName(TEXT("passSlider"))))
	{
		Slider->SetPercent(Progress);
	}
	if (UTextBlock* PassPct = Cast<UTextBlock>(GamingHUDWidget->GetWidgetFromName(TEXT("tx_pass"))))
	{
		PassPct->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Progress * 100.f))));
	}
}

void AKeepGoingUpGameMode::BindStartMenuButtons(UUserWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	if (UButton* StartBtn = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Btn_Start"))))
	{
		StartBtn->OnClicked.AddDynamic(this, &AKeepGoingUpGameMode::HandleStartClicked);
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("UI_Start: Btn_Start not found."));
	}

	if (UButton* ExitBtn = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Btn_Exit"))))
	{
		ExitBtn->OnClicked.AddDynamic(this, &AKeepGoingUpGameMode::HandleExitClicked);
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("UI_Start: Btn_Exit not found."));
	}
	// Btn_Setting intentionally unbound
}

void AKeepGoingUpGameMode::UnbindStartMenuButtons()
{
	if (!StartMenuWidget || TypedStartMenu)
	{
		return;
	}

	if (UButton* StartBtn = Cast<UButton>(StartMenuWidget->GetWidgetFromName(TEXT("Btn_Start"))))
	{
		StartBtn->OnClicked.RemoveDynamic(this, &AKeepGoingUpGameMode::HandleStartClicked);
	}
	if (UButton* ExitBtn = Cast<UButton>(StartMenuWidget->GetWidgetFromName(TEXT("Btn_Exit"))))
	{
		ExitBtn->OnClicked.RemoveDynamic(this, &AKeepGoingUpGameMode::HandleExitClicked);
	}
}

void AKeepGoingUpGameMode::ShowEndMenu()
{
	HideEndMenu();

	if (!EndMenuClass)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: EndMenuClass is not set (UI_End)."));
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	EndMenuWidget = CreateWidget<UUserWidget>(PC, EndMenuClass);
	if (!EndMenuWidget)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("KeepGoingUpGameMode: failed to create UI_End."));
		return;
	}

	TypedEndMenu = Cast<UEndMenuWidget>(EndMenuWidget);
	EndMenuWidget->AddToViewport(110);
	EndMenuWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (TypedEndMenu)
	{
		TypedEndMenu->OnCountdownFinished.AddDynamic(this, &AKeepGoingUpGameMode::HandleEndCountdownFinished);
		TypedEndMenu->StartCountdown(EndCountdownSeconds);
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Warning,
			TEXT("UI_End is not parented to EndMenuWidget; using GameMode countdown fallback. Reparent for cleaner pulse."));
		StartFallbackEndCountdown();
	}
}

void AKeepGoingUpGameMode::HideEndMenu()
{
	if (TypedEndMenu)
	{
		TypedEndMenu->OnCountdownFinished.RemoveDynamic(this, &AKeepGoingUpGameMode::HandleEndCountdownFinished);
		TypedEndMenu->StopCountdown();
		TypedEndMenu = nullptr;
	}

	StopFallbackEndCountdown();

	if (EndMenuWidget)
	{
		EndMenuWidget->RemoveFromParent();
		EndMenuWidget = nullptr;
	}
}

void AKeepGoingUpGameMode::StartFallbackEndCountdown()
{
	StopFallbackEndCountdown();
	FallbackRemainingSeconds = FMath::Max(0, EndCountdownSeconds);

	if (EndMenuWidget)
	{
		if (UTextBlock* TimeText = Cast<UTextBlock>(EndMenuWidget->GetWidgetFromName(TEXT("tx_endTime"))))
		{
			TimeText->SetText(FText::FromString(FString::Printf(TEXT("%d秒"), FallbackRemainingSeconds)));
			TimeText->SetRenderScale(FVector2D(1.2f, 1.2f));
		}
	}

	FallbackPulseElapsed = 0.f;
	GetWorldTimerManager().SetTimer(FallbackPulseTimer, this, &AKeepGoingUpGameMode::HandleFallbackPulseTick, 0.016f, true);

	if (FallbackRemainingSeconds <= 0)
	{
		HandleEndCountdownFinished();
		return;
	}

	GetWorldTimerManager().SetTimer(FallbackCountdownTimer, this, &AKeepGoingUpGameMode::HandleFallbackCountdownTick, 1.f, true);
}

void AKeepGoingUpGameMode::StopFallbackEndCountdown()
{
	GetWorldTimerManager().ClearTimer(FallbackCountdownTimer);
	GetWorldTimerManager().ClearTimer(FallbackPulseTimer);
}

void AKeepGoingUpGameMode::HandleFallbackCountdownTick()
{
	FallbackRemainingSeconds = FMath::Max(0, FallbackRemainingSeconds - 1);

	if (EndMenuWidget)
	{
		if (UTextBlock* TimeText = Cast<UTextBlock>(EndMenuWidget->GetWidgetFromName(TEXT("tx_endTime"))))
		{
			TimeText->SetText(FText::FromString(FString::Printf(TEXT("%d秒"), FallbackRemainingSeconds)));
			TimeText->SetRenderScale(FVector2D(1.2f, 1.2f));
		}
	}

	FallbackPulseElapsed = 0.f;
	if (!GetWorldTimerManager().IsTimerActive(FallbackPulseTimer))
	{
		GetWorldTimerManager().SetTimer(FallbackPulseTimer, this, &AKeepGoingUpGameMode::HandleFallbackPulseTick, 0.016f, true);
	}

	if (FallbackRemainingSeconds <= 0)
	{
		StopFallbackEndCountdown();
		HandleEndCountdownFinished();
	}
}

void AKeepGoingUpGameMode::HandleFallbackPulseTick()
{
	if (!EndMenuWidget)
	{
		StopFallbackEndCountdown();
		return;
	}

	UTextBlock* TimeText = Cast<UTextBlock>(EndMenuWidget->GetWidgetFromName(TEXT("tx_endTime")));
	if (!TimeText)
	{
		return;
	}

	FallbackPulseElapsed += 0.016f;
	const float Duration = 0.28f;
	const float Alpha = FMath::Clamp(FallbackPulseElapsed / Duration, 0.f, 1.f);
	const float Scale = FMath::Lerp(1.2f, 1.f, Alpha);
	TimeText->SetRenderScale(FVector2D(Scale, Scale));

	if (Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(FallbackPulseTimer);
	}
}

void AKeepGoingUpGameMode::ApplyMenuInputMode()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
}

bool AKeepGoingUpGameMode::IsPrankConfigOpen() const
{
	UClass* PrankActorClass = LoadClass<AActor>(nullptr, TEXT("/Game/Game/Barrage/BP_PrankUI.BP_PrankUI_C"));
	if (!PrankActorClass)
	{
		return false;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, PrankActorClass, Found);
	for (AActor* Actor : Found)
	{
		if (!Actor)
		{
			continue;
		}
		if (const FBoolProperty* Prop = FindFProperty<FBoolProperty>(Actor->GetClass(), TEXT("IsShow")))
		{
			return Prop->GetPropertyValue_InContainer(Actor);
		}
	}
	return false;
}

void AKeepGoingUpGameMode::RaisePrankConfigWidget() const
{
	UClass* WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Game/Barrage/UI/WBP_Prank.WBP_Prank_C"));
	if (!WidgetClass)
	{
		return;
	}

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, WidgetClass, false);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget && Widget->IsInViewport())
		{
			// UI_Start is viewport Z 100. Keep the config panel above it.
			Widget->AddToViewport(300);
		}
	}
}

void AKeepGoingUpGameMode::MaintainStartMenuPrankUI()
{
	if (IsPrankConfigOpen())
	{
		RaisePrankConfigWidget();
		return;
	}

	// Closing BP_PrankUI switches to game-only and hides the cursor. Put the start menu back.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (!PC->bShowMouseCursor)
		{
			ApplyMenuInputMode();
		}
	}
}

void AKeepGoingUpGameMode::ApplyGameplayInputMode()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = false;
	}
}

void AKeepGoingUpGameMode::ApplyFinishedInputMode()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = false;
	}
}

AMapCreater* AKeepGoingUpGameMode::FindMapCreater() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AMapCreater::StaticClass(), Found);
	return Found.Num() > 0 ? Cast<AMapCreater>(Found[0]) : nullptr;
}

AKeepGoingUpCharacter* AKeepGoingUpGameMode::FindPlayerCharacter() const
{
	return Cast<AKeepGoingUpCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void AKeepGoingUpGameMode::CachePlayerStartTransform()
{
	if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
	{
		PlayerStartTransform = Character->GetActorTransform();
		bHasPlayerStartTransform = true;
	}
}

void AKeepGoingUpGameMode::ResetPlayerToStart()
{
	AKeepGoingUpCharacter* Character = FindPlayerCharacter();
	if (!Character)
	{
		return;
	}

	if (!bHasPlayerStartTransform)
	{
		CachePlayerStartTransform();
	}

	Character->TeleportTo(PlayerStartTransform.GetLocation(), PlayerStartTransform.GetRotation().Rotator(), false, true);
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(PlayerStartTransform.GetRotation().Rotator());
	}
}

void AKeepGoingUpGameMode::HandleStartClicked()
{
	if (PlayState != EKeepGoingUpPlayState::Menu)
	{
		return;
	}

	EnterPlayingState();
}

void AKeepGoingUpGameMode::HandleExitClicked()
{
	UKismetSystemLibrary::QuitGame(
		this,
		UGameplayStatics::GetPlayerController(this, 0),
		EQuitPreference::Quit,
		false);
}

void AKeepGoingUpGameMode::HandleEndCountdownFinished()
{
	++PassCount;
	UpdateGamingHUD();
	UE_LOG(LogKeepGoingUp, Log, TEXT("End countdown finished → PassCount=%d"), PassCount);
	EnterMenuState(true);
}

void AKeepGoingUpGameMode::AbortEndCountdownForPrank()
{
	HideEndMenu();

	if (PlayState == EKeepGoingUpPlayState::Menu)
	{
		return;
	}

	PlayState = EKeepGoingUpPlayState::Playing;

	if (AKeepGoingUpCharacter* Character = FindPlayerCharacter())
	{
		Character->EndCarPush();
		Character->ResetEndSequence();
		Character->SetRunGameplayEnabled(true, true);
	}

	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (CachedMapCreater)
	{
		CachedMapCreater->ResetMapEnd();
		CachedMapCreater->RecycleAllCars();
		CachedMapCreater->SetCarSpawningEnabled(true);
	}

	ShowGamingHUD();
	ApplyGameplayInputMode();

	UE_LOG(LogKeepGoingUp, Log, TEXT("Prank: aborted end countdown → Playing"));
}

void AKeepGoingUpGameMode::EnsurePlayingForPrank()
{
	if (PlayState == EKeepGoingUpPlayState::Playing)
	{
		return;
	}

	if (PlayState == EKeepGoingUpPlayState::Finished)
	{
		AbortEndCountdownForPrank();
		return;
	}

	// Menu → start a run so 测试 / gifts can teleport to end immediately.
	EnterPlayingState();
	UE_LOG(LogKeepGoingUp, Log, TEXT("Prank: EnsurePlayingForPrank (was Menu)"));
}

bool AKeepGoingUpGameMode::GetPlayerStartTransform(FTransform& OutTransform) const
{
	if (!bHasPlayerStartTransform)
	{
		return false;
	}
	OutTransform = PlayerStartTransform;
	return true;
}
