// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/StartMenuWidget.h"
#include "Components/Button.h"

void UStartMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Start)
	{
		Btn_Start->OnClicked.AddDynamic(this, &UStartMenuWidget::HandleStartClicked);
	}
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.AddDynamic(this, &UStartMenuWidget::HandleExitClicked);
	}
	if (Btn_Setting)
	{
		Btn_Setting->OnClicked.AddDynamic(this, &UStartMenuWidget::HandleSettingClicked);
	}
}

void UStartMenuWidget::NativeDestruct()
{
	if (Btn_Start)
	{
		Btn_Start->OnClicked.RemoveDynamic(this, &UStartMenuWidget::HandleStartClicked);
	}
	if (Btn_Exit)
	{
		Btn_Exit->OnClicked.RemoveDynamic(this, &UStartMenuWidget::HandleExitClicked);
	}
	if (Btn_Setting)
	{
		Btn_Setting->OnClicked.RemoveDynamic(this, &UStartMenuWidget::HandleSettingClicked);
	}

	Super::NativeDestruct();
}

void UStartMenuWidget::HandleSettingClicked()
{
	OnSettingClicked.Broadcast();
}

void UStartMenuWidget::HandleStartClicked()
{
	OnStartClicked.Broadcast();
}

void UStartMenuWidget::HandleExitClicked()
{
	OnExitClicked.Broadcast();
}
