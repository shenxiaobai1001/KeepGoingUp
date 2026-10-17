// Copyright Epic Games, Inc. All Rights Reserved.

#include "Map/MapEnd.h"
#include "KeepGoingUpCharacter.h"
#include "KeepGoingUp.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

AMapEnd::AMapEnd()
{
	PrimaryActorTick.bCanEverTick = false;
	// Root comes from BP_End (SharedRoot). Do not create a conflicting native root.
}

void AMapEnd::BeginPlay()
{
	Super::BeginPlay();
	CacheComponents();
}

void AMapEnd::ResetEndTrigger()
{
	bTriggered = false;
}

void AMapEnd::CacheComponents()
{
	if (bComponentsCached && CachedEndPos && CachedEndCollider)
	{
		return;
	}

	CachedEndPos = nullptr;
	CachedEndCollider = nullptr;

	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (!Comp)
		{
			continue;
		}
		const FString Name = Comp->GetName();
		if (Comp->GetFName() == EndPosComponentName || Name.StartsWith(EndPosComponentName.ToString()))
		{
			CachedEndPos = Comp;
		}
	}

	TArray<UPrimitiveComponent*> PrimComps;
	GetComponents<UPrimitiveComponent>(PrimComps);
	for (UPrimitiveComponent* Comp : PrimComps)
	{
		if (!Comp)
		{
			continue;
		}
		const FString Name = Comp->GetName();
		if (Comp->GetFName() == EndColliderComponentName
			|| Name.StartsWith(EndColliderComponentName.ToString())
			|| Name.Contains(TEXT("Endcollider"), ESearchCase::IgnoreCase)
			|| Name.Contains(TEXT("EndCollider"), ESearchCase::IgnoreCase))
		{
			CachedEndCollider = Comp;
		}
	}

	if (CachedEndCollider)
	{
		CachedEndCollider->SetGenerateOverlapEvents(true);
		CachedEndCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CachedEndCollider->SetCollisionResponseToAllChannels(ECR_Ignore);
		CachedEndCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		CachedEndCollider->OnComponentBeginOverlap.RemoveAll(this);
		CachedEndCollider->OnComponentBeginOverlap.AddDynamic(this, &AMapEnd::OnEndColliderBeginOverlap);
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("MapEnd[%s]: Endcollider component not found."), *GetName());
	}

	if (!CachedEndPos)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("MapEnd[%s]: EndPos component not found."), *GetName());
	}

	bComponentsCached = true;
}

FVector AMapEnd::GetEndPosWorldLocation() const
{
	if (CachedEndPos)
	{
		return CachedEndPos->GetComponentLocation();
	}
	return GetActorLocation();
}

void AMapEnd::OnEndColliderBeginOverlap(
	UPrimitiveComponent* /*OverlappedComponent*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/)
{
	if (bTriggered)
	{
		return;
	}

	AKeepGoingUpCharacter* Character = Cast<AKeepGoingUpCharacter>(OtherActor);
	if (!Character)
	{
		return;
	}

	bTriggered = true;
	Character->BeginEndSequence(this);
}
