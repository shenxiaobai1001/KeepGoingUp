// Copyright Epic Games, Inc. All Rights Reserved.

#include "Prank/KeepGoingUpPrankLibrary.h"
#include "KeepGoingUpCharacter.h"
#include "KeepGoingUp.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

bool UKeepGoingUpPrankLibrary::IsGameplayPrankCallName(FName CallName)
{
	const FString S = CallName.ToString();
	return S.Equals(TEXT("传送起点"))
		|| S.Equals(TEXT("传送终点"))
		|| S.Equals(TEXT("彩虹猫"))
		|| S.Equals(TEXT("忍者神龟"))
		|| S.Equals(TEXT("火箭"))
		|| S.Equals(TEXT("导弹"))
		|| S.Equals(TEXT("猪突猛进"))
		|| S.Equals(TEXT("落叶斩"))
		|| S.Equals(TEXT("孙悟空"))
		|| S.Equals(TEXT("贝吉塔"))
		|| S.Equals(TEXT("鸣人"))
		|| S.Equals(TEXT("佐助"))
		|| S.Equals(TEXT("毁灭游戏"));
}

bool UKeepGoingUpPrankLibrary::ExecutePrankCallName(UObject* WorldContextObject, FName CallName)
{
	if (!WorldContextObject || !IsGameplayPrankCallName(CallName))
	{
		return false;
	}

	const FString S = CallName.ToString();
	if (S.Equals(TEXT("毁灭游戏")))
	{
		UKismetSystemLibrary::QuitGame(
			WorldContextObject,
			UGameplayStatics::GetPlayerController(WorldContextObject, 0),
			EQuitPreference::Quit,
			false);
		return true;
	}

	AKeepGoingUpCharacter* Character = Cast<AKeepGoingUpCharacter>(
		UGameplayStatics::GetPlayerCharacter(WorldContextObject, 0));
	if (!Character)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("ExecutePrankCallName(%s): no player character."), *CallName.ToString());
		return false;
	}

	if (S.Equals(TEXT("传送起点")))
	{
		return Character->TeleportToRoadStart();
	}
	if (S.Equals(TEXT("传送终点")))
	{
		return Character->TeleportToRoadEnd();
	}
	if (S.Equals(TEXT("彩虹猫")))
	{
		Character->StartRainbowCatPrank();
		return true;
	}
	if (S.Equals(TEXT("忍者神龟")))
	{
		Character->StartNinjaTurtlePrank();
		return true;
	}
	if (S.Equals(TEXT("火箭")))
	{
		Character->StartRocketPrank();
		return true;
	}
	if (S.Equals(TEXT("导弹")))
	{
		Character->StartMissilePrank();
		return true;
	}
	if (S.Equals(TEXT("猪突猛进")))
	{
		Character->StartZhuTuMengJinPrank();
		return true;
	}
	if (S.Equals(TEXT("落叶斩")))
	{
		Character->StartLuoYeZhanPrank();
		return true;
	}
	if (S.Equals(TEXT("孙悟空")))
	{
		Character->StartSaiYaRenPrank();
		return true;
	}
	if (S.Equals(TEXT("贝吉塔")))
	{
		Character->StartBeiJiTaPrank();
		return true;
	}
	if (S.Equals(TEXT("鸣人")))
	{
		Character->StartNarutoPrank();
		return true;
	}
	if (S.Equals(TEXT("佐助")))
	{
		Character->StartSasukePrank();
		return true;
	}
	return false;
}
