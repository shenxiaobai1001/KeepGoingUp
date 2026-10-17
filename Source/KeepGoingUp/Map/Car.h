// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "Car.generated.h"

class AMapCreater;
class AKeepGoingUpCharacter;
class UStaticMeshComponent;
class USceneComponent;
class UPrimitiveComponent;

/**
 * Slope obstacle that slides toward -local X on MapCreater.
 * On hit: snaps the player to PlayerPos and pushes them back, then returns to the pool.
 */
UCLASS(Blueprintable)
class ACar : public AActor
{
	GENERATED_BODY()

public:
	ACar();

	/** Activate from pool on a fixed lane. Local space is MapCreater space. */
	void ActivateCar(AMapCreater* InOwner, float StartLocalX, int32 InLaneIndex, float LocalZ, float InMoveSpeed);

	/** Return to pool and release any carried player. */
	void DeactivateCar();

	/** Drop this player if the car is currently carrying them. The car keeps sliding. */
	void ReleaseCapturedPlayer(AKeepGoingUpCharacter* Character);

	/** True when the character is inside this car's bounds (used while prank-pushing, which teleports). */
	bool IsCharacterInside(const AKeepGoingUpCharacter* Character) const;

	/** Break HotDogCar into flying parts, play shatter SFX, then recycle this car. */
	void ShatterFromPush(const FVector& PushDirection);

	bool IsActive() const { return bActive; }
	bool IsCarryingPlayer() const { return bCarryingPlayer; }
	int32 GetLaneIndex() const { return LaneIndex; }
	float GetRoadLocalX() const { return CurrentLocalX; }
	float GetRoadLocalY() const { return CurrentLocalY; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void CacheComponents();
	void ConfigureCollision();

	UFUNCTION()
	void OnCubeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void TryCapturePlayer(AKeepGoingUpCharacter* Character);
	void ReleasePlayer();
	void MoveAlongSlope(float DeltaSeconds);
	void PlayCarSound(class USoundBase* Sound) const;
	AActor* FindHotDogCar() const;
	void TickShatter(float DeltaSeconds);
	void RestoreFlyingParts();

	UFUNCTION()
	void FinishShatter();

	struct FFlyingPart
	{
		USceneComponent* Component = nullptr;
		USceneComponent* Parent = nullptr;
		FTransform RelativeTransform = FTransform::Identity;
		FVector Velocity = FVector::ZeroVector;
	};

	TArray<FFlyingPart> FlyingParts;
	FTimerHandle ShatterTimer;
	bool bShattering = false;

	UPROPERTY(EditAnywhere, Category = "Car|Audio")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, Category = "Car|Audio")
	TObjectPtr<USoundBase> ShatterSound;

	UPROPERTY(EditAnywhere, Category = "Car")
	FName CubeComponentName = TEXT("Cube");

	UPROPERTY(EditAnywhere, Category = "Car")
	FName PlayerPosComponentName = TEXT("PlayerPos");

	/** Runtime caches (not UPROPERTY — names must not clash with BP components Cube / PlayerPos). */
	UStaticMeshComponent* CachedCubeMesh = nullptr;
	USceneComponent* CachedPlayerPos = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AMapCreater> MapOwner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AKeepGoingUpCharacter> CapturedPlayer = nullptr;

	float MoveSpeed = 2000.f;
	float TravelDistance = 10000.f;
	float PushDistance = 5000.f;

	float StartLocalX = 0.f;
	float CurrentLocalX = 0.f;
	float CurrentLocalY = 0.f;
	float CurrentLocalZ = 0.f;
	float DistanceTraveled = 0.f;
	float PushDistanceRemaining = 0.f;
	int32 LaneIndex = 0;

	bool bActive = false;
	bool bCarryingPlayer = false;
	bool bComponentsCached = false;
	bool bShattered = false;
};
