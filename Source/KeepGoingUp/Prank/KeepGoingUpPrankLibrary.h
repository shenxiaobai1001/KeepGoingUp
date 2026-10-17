// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KeepGoingUpPrankLibrary.generated.h"

/**
 * Dispatches PrankUI CallName → gameplay effects.
 * BP_WebSocketServer / queues can also call this after config match;
 * AKeepGoingUpPrankBridge polls the WS queues so Normal/Box/Special configs work without extra BP wiring.
 */
UCLASS()
class UKeepGoingUpPrankLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns true if CallName was a known gameplay prank and was executed. */
	UFUNCTION(BlueprintCallable, Category = "Prank", meta = (WorldContext = "WorldContextObject"))
	static bool ExecutePrankCallName(UObject* WorldContextObject, FName CallName);

	UFUNCTION(BlueprintPure, Category = "Prank")
	static bool IsGameplayPrankCallName(FName CallName);

	UFUNCTION(BlueprintPure, Category = "Prank")
	static FName GetTeleportStartCallName() { return FName(TEXT("传送起点")); }

	UFUNCTION(BlueprintPure, Category = "Prank")
	static FName GetTeleportEndCallName() { return FName(TEXT("传送终点")); }
};
