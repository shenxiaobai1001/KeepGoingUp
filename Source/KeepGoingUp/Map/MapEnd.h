// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapEnd.generated.h"

class USceneComponent;
class UPrimitiveComponent;
class AKeepGoingUpCharacter;

/**
 * Final tile. Reparent BP_End to this class.
 * Expects BP components: EndPos (Scene) + Endcollider (Box Collision trigger).
 */
UCLASS(Blueprintable)
class AMapEnd : public AActor
{
	GENERATED_BODY()

public:
	AMapEnd();

	UFUNCTION(BlueprintPure, Category = "Map End")
	FVector GetEndPosWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "Map End")
	bool HasTriggered() const { return bTriggered; }

	void ResetEndTrigger();

	/** Mark finish as consumed (e.g. prank teleport started the end sequence directly). */
	void NotifyEndSequenceStarted() { bTriggered = true; }

protected:
	virtual void BeginPlay() override;

	void CacheComponents();

	UFUNCTION()
	void OnEndColliderBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(EditAnywhere, Category = "Map End")
	FName EndPosComponentName = TEXT("EndPos");

	UPROPERTY(EditAnywhere, Category = "Map End")
	FName EndColliderComponentName = TEXT("Endcollider");

	/** Runtime caches — names must not clash with BP component names. */
	USceneComponent* CachedEndPos = nullptr;
	UPrimitiveComponent* CachedEndCollider = nullptr;

	bool bTriggered = false;
	bool bComponentsCached = false;
};
