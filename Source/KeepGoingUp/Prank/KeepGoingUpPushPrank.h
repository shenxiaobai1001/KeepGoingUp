// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KeepGoingUpPushPrank.generated.h"

class AKeepGoingUpCharacter;
class AMapCreater;
class UAudioComponent;
class USoundBase;

UENUM()
enum class EKeepGoingUpPushPrankKind : uint8
{
	/** BP_Cat: chase from behind, then push the player forward along the road. */
	RainbowCat,
	/** BP_TMNT: fly in from ahead, then push the player backward along the road. */
	NinjaTurtle,
	/** BP_Rocket: chase from behind, then push the player forward for 5 seconds. */
	Rocket,
	/** BP_DaoDan: fly in from ahead, then push the player backward for 5 seconds. */
	Missile,
	/** BP_ZhuTuMengJin: chase from behind, then push the player forward for 8 seconds. */
	ZhuTuMengJin,
	/** BPLuoYeZhan: fly in from ahead, then push the player backward for 8 seconds. */
	LuoYeZhan,
	/** BP_Saiyaren: hold five meters behind, then push forward for 10 seconds. */
	SaiYaRen,
	/** BP_BeiJiTa: hold five meters ahead, then push backward for 10 seconds. */
	BeiJiTa,
	/** BP_liudao: chase from behind, then push the player forward for 12 seconds. */
	Naruto,
	/** BP_ZuoZhu: fly in from ahead, then push the player backward for 12 seconds. */
	Sasuke
};

UENUM()
enum class EKeepGoingUpPushPrankPhase : uint8
{
	Approach,
	Pushing,
	Finished
};

/**
 * One stacked prank instance. Multiple cats / turtles can exist at once.
 * Push time only counts during the Pushing phase.
 */
UCLASS()
class AKeepGoingUpPushPrank : public AActor
{
	GENERATED_BODY()

public:
	AKeepGoingUpPushPrank();

	void InitPrank(EKeepGoingUpPushPrankKind InKind, AKeepGoingUpCharacter* InTarget);

	void Advance(float DeltaSeconds, AMapCreater* MapCreater);

	bool IsPushingForward() const;
	bool IsPushingBackward() const;
	EKeepGoingUpPushPrankKind GetKind() const { return Kind; }
	bool IsFinished() const { return Phase == EKeepGoingUpPushPrankPhase::Finished; }

	/** True while SaiYaRen / BeiJiTa is still playing its intro animation. */
	bool WantsIntroCamera() const;
	/** True while SaiYaRen / BeiJiTa is in the pushing phase. */
	bool UsesExtendedPushCamera() const;
	float GetIntroCameraYaw() const;
	int32 GetSerial() const { return Serial; }
	AActor* GetVisual() const { return Visual; }

	void Recycle();

private:
	void SpawnVisual(AMapCreater* MapCreater);
	void PlayTurtleIntro();
	void AttachVisualAtLocalOffset(float AlongPlayerX);
	void SnapVisualToPlayer();
	void SetBeamActive(bool bActive) const;
	bool IsIntroAnimFinished() const;
	void DisableVisualCollision(AActor* Actor) const;
	void StopVisualAnimLoop(AActor* Actor) const;
	void ApplyVisualAnimation(AActor* Actor) const;
	void StartPrankSound();
	void PlayPrankSound(USoundBase* Sound, bool bLoop);
	void StopPrankSound();

	UFUNCTION()
	void HandlePrankSoundFinished();

	UPROPERTY()
	TObjectPtr<AKeepGoingUpCharacter> Target;

	UPROPERTY()
	TObjectPtr<AActor> Visual;

	EKeepGoingUpPushPrankKind Kind = EKeepGoingUpPushPrankKind::RainbowCat;
	EKeepGoingUpPushPrankPhase Phase = EKeepGoingUpPushPrankPhase::Approach;

	float ApproachElapsed = 0.f;
	float PushTimeLeft = 3.f;
	int32 Serial = 0;

	bool bVisualSpawned = false;
	bool bAttachedToPlayer = false;
	bool bBeamOpened = false;
	bool bLoopingSound = false;

	UPROPERTY()
	TObjectPtr<UAudioComponent> PrankSound;

	UPROPERTY()
	TObjectPtr<USoundBase> LoopSound;
};
