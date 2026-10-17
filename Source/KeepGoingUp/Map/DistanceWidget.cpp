// Copyright Epic Games, Inc. All Rights Reserved.

#include "Map/DistanceWidget.h"
#include "Components/TextBlock.h"
#include "Styling/SlateColor.h"

void UDistanceWidget::SetDistanceDisplay(int32 Distance, FLinearColor Color)
{
	ShowRoadDistance(Distance, Color);
}

void UDistanceWidget::ShowRoadDistance(int32 Distance, FLinearColor Color)
{
	if (tx_Hp)
	{
		tx_Hp->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!tx_distance)
	{
		return;
	}

	tx_distance->SetVisibility(ESlateVisibility::HitTestInvisible);
	tx_distance->SetText(FText::AsNumber(Distance));
	tx_distance->SetColorAndOpacity(FSlateColor(Color));
}

void UDistanceWidget::ShowPitHp(int32 Hp)
{
	if (tx_distance)
	{
		tx_distance->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (!tx_Hp)
	{
		return;
	}

	tx_Hp->SetVisibility(ESlateVisibility::HitTestInvisible);
	tx_Hp->SetText(FText::AsNumber(Hp));
}
