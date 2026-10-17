// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeepGoingUpPrankBridge.generated.h"

/**
 * Watches BP_WebSocketServer queues after PrankUI matching (incl. 测试 button).
 * Per-tick rising-edge detection so Delay=0 (≈1-frame) entries are not missed.
 */
UCLASS()
class AKeepGoingUpPrankBridge : public AActor
{
	GENERATED_BODY()

public:
	AKeepGoingUpPrankBridge();

	/** Can be called from Blueprint (e.g. Test button) to force a gameplay prank. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void NotifyPrankCallName(FName CallName, float DelaySeconds = 0.f);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void ResolveWebSocketServer();
	void PollAllQueues();
	void PollQueueProperty(FName PropertyName, TSet<FString>& InOutSeenKeys);
	void SchedulePrankRepeats(const FString& CallName, int32 Count, float IntervalSeconds);
	void ExecutePrankNow(const FString& CallName);
	bool FindConfiguredPrank(const FString& CallName, int32& OutCount, float& OutDelay) const;
	bool FindConfiguredPrankOnObject(const UObject* Object, const FString& CallName, int32& OutCount, float& OutDelay) const;
	UObject* LoadPrankSaveGame() const;
	static bool ReadPrankNumbers(const UScriptStruct* Struct, const void* StructPtr, int32& OutCount, float& OutDelay);
	static bool ReadCallNameFromStruct(const UScriptStruct* Struct, const void* StructPtr, FString& OutCallName);
	static bool StructContainsCallName(const UScriptStruct* Struct, const void* StructPtr, const FString& CallName);

	UPROPERTY(EditAnywhere, Category = "Prank")
	TSoftClassPtr<AActor> WebSocketServerClass;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedWebSocketServer;

	TSet<FString> SeenNormalKeys;
	TSet<FString> SeenBoxKeys;
	TSet<FString> SeenSpecialKeys;

	/** World time when this call name can spawn the next repeat. Clicks append after it. */
	TMap<FString, double> NextPrankTime;

	FTimerHandle FastPollTimer;
};
