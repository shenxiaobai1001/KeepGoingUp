// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StartMenuWidget.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMenuStartClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartMenuExitClicked);

/**
 * Start menu. Reparent UI_Start to this class; keep button names Btn_Start / Btn_Setting / Btn_Exit.
 */
UCLASS(abstract)
class UStartMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Start Menu")
	FOnStartMenuStartClicked OnStartClicked;

	UPROPERTY(BlueprintAssignable, Category = "Start Menu")
	FOnStartMenuExitClicked OnExitClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleExitClicked();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Setting;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Btn_Exit;
};
