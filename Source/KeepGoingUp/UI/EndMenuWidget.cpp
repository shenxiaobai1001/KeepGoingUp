// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EndMenuWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UEndMenuWidget::NativeDestruct()
{
	StopCountdown();
	Super::NativeDestruct();
}

void UEndMenuWidget::StartCountdown(int32 Seconds)
{
	StopCountdown();

	RemainingSeconds = FMath::Max(0, Seconds);
	ApplyCountdownText();
	BeginPulse();

	if (RemainingSeconds <= 0)
	{
		OnCountdownFinished.Broadcast();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CountdownTimerHandle,
			this,
			&UEndMenuWidget::HandleCountdownTick,
			1.f,
			true);
	}
}

void UEndMenuWidget::StopCountdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CountdownTimerHandle);
	}
	StopPulse();
}

void UEndMenuWidget::HandleCountdownTick()
{
	RemainingSeconds = FMath::Max(0, RemainingSeconds - 1);
	ApplyCountdownText();

	if (RemainingSeconds <= 0)
	{
		StopCountdown();
		OnCountdownFinished.Broadcast();
		return;
	}

	BeginPulse();
}

void UEndMenuWidget::ApplyCountdownText()
{
	if (!tx_endTime)
	{
		return;
	}

	tx_endTime->SetText(FText::FromString(FString::Printf(TEXT("%d秒"), RemainingSeconds)));
}

void UEndMenuWidget::BeginPulse()
{
	if (!tx_endTime)
	{
		return;
	}

	PulseElapsed = 0.f;
	tx_endTime->SetRenderScale(FVector2D(PulsePeakScale, PulsePeakScale));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PulseTimerHandle,
			this,
			&UEndMenuWidget::HandlePulseTick,
			0.016f,
			true);
	}
}

void UEndMenuWidget::StopPulse()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PulseTimerHandle);
	}
	if (tx_endTime)
	{
		tx_endTime->SetRenderScale(FVector2D(1.f, 1.f));
	}
}

void UEndMenuWidget::HandlePulseTick()
{
	if (!tx_endTime || PulseDuration <= KINDA_SMALL_NUMBER)
	{
		StopPulse();
		return;
	}

	PulseElapsed += 0.016f;
	const float Alpha = FMath::Clamp(PulseElapsed / PulseDuration, 0.f, 1.f);
	const float Scale = FMath::Lerp(PulsePeakScale, 1.f, Alpha);
	tx_endTime->SetRenderScale(FVector2D(Scale, Scale));

	if (Alpha >= 1.f)
	{
		StopPulse();
	}
}
