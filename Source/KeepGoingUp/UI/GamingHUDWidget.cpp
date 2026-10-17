// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GamingHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UGamingHUDWidget::SetPassCount(int32 PassCount)
{
	if (tx_passTime)
	{
		tx_passTime->SetText(FText::AsNumber(FMath::Max(0, PassCount)));
	}
}

void UGamingHUDWidget::SetProgress(float Progress01)
{
	const float Clamped = FMath::Clamp(Progress01, 0.f, 1.f);
	if (passSlider)
	{
		passSlider->SetPercent(Clamped);
	}
	if (tx_pass)
	{
		const int32 Pct = FMath::RoundToInt(Clamped * 100.f);
		tx_pass->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Pct)));
	}
}
