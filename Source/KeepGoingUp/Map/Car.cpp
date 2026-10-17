// Copyright Epic Games, Inc. All Rights Reserved.

#include "Map/Car.h"
#include "Map/MapCreater.h"
#include "KeepGoingUpCharacter.h"
#include "KeepGoingUp.h"
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ACar::ACar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<USoundBase> ImpactFinder(TEXT("/Game/Game/Audio/撞击.撞击"));
	if (ImpactFinder.Succeeded())
	{
		ImpactSound = ImpactFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> ShatterFinder(TEXT("/Game/Game/Audio/撞碎.撞碎"));
	if (ShatterFinder.Succeeded())
	{
		ShatterSound = ShatterFinder.Object;
	}
}

void ACar::BeginPlay()
{
	Super::BeginPlay();
	CacheComponents();
	ConfigureCollision();
}

void ACar::CacheComponents()
{
	if (bComponentsCached)
	{
		return;
	}

	TArray<UActorComponent*> Comps;
	GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp)
		{
			continue;
		}

		const FString Name = Comp->GetName();
		if (!CachedCubeMesh)
		{
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp))
			{
				if (Name.StartsWith(CubeComponentName.ToString()) || Name.Equals(TEXT("Cube")))
				{
					CachedCubeMesh = Mesh;
				}
			}
		}

		if (!CachedPlayerPos)
		{
			if (USceneComponent* Scene = Cast<USceneComponent>(Comp))
			{
				if (Name.StartsWith(PlayerPosComponentName.ToString()) || Name.Equals(TEXT("PlayerPos")))
				{
					// Prefer the dedicated marker, not the root / cube
					if (!Cast<UStaticMeshComponent>(Scene))
					{
						CachedPlayerPos = Scene;
					}
				}
			}
		}
	}

	// Fallback: any scene component literally named PlayerPos
	if (!CachedPlayerPos)
	{
		TArray<USceneComponent*> Scenes;
		GetComponents<USceneComponent>(Scenes);
		for (USceneComponent* Scene : Scenes)
		{
			if (Scene && Scene->GetName().Contains(TEXT("PlayerPos")))
			{
				CachedPlayerPos = Scene;
				break;
			}
		}
	}

	bComponentsCached = true;

	UE_LOG(LogKeepGoingUp, Log, TEXT("Car[%s] Cube=%s PlayerPos=%s"),
		*GetName(), *GetNameSafe(CachedCubeMesh), *GetNameSafe(CachedPlayerPos));
}

void ACar::ConfigureCollision()
{
	if (!CachedCubeMesh)
	{
		CacheComponents();
	}
	if (!CachedCubeMesh)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("Car[%s]: Cube mesh not found."), *GetName());
		return;
	}

	CachedCubeMesh->SetGenerateOverlapEvents(true);
	CachedCubeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CachedCubeMesh->SetCollisionObjectType(ECC_WorldDynamic);
	CachedCubeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CachedCubeMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CachedCubeMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if (!CachedCubeMesh->OnComponentBeginOverlap.IsAlreadyBound(this, &ACar::OnCubeBeginOverlap))
	{
		CachedCubeMesh->OnComponentBeginOverlap.AddDynamic(this, &ACar::OnCubeBeginOverlap);
	}
}

void ACar::ActivateCar(AMapCreater* InOwner, float InStartLocalX, int32 InLaneIndex, float LocalZ, float InMoveSpeed)
{
	CacheComponents();
	ConfigureCollision();

	MapOwner = InOwner;
	MoveSpeed = InMoveSpeed;
	StartLocalX = InStartLocalX;
	CurrentLocalX = InStartLocalX;
	LaneIndex = InLaneIndex;
	CurrentLocalY = MapOwner ? MapOwner->GetLaneLocalY(InLaneIndex) : 0.f;
	CurrentLocalZ = LocalZ;
	DistanceTraveled = 0.f;
	PushDistanceRemaining = 0.f;
	bCarryingPlayer = false;
	bShattered = false;
	CapturedPlayer = nullptr;
	GetWorldTimerManager().ClearTimer(ShatterTimer);
	RestoreFlyingParts();

	if (MapOwner)
	{
		const float Len = MapOwner->GetLayerLength();
		TravelDistance = Len * static_cast<float>(MapOwner->GetCarTravelTiles());
		PushDistance = Len * static_cast<float>(MapOwner->GetCarPushTiles());
	}

	AttachToActor(MapOwner, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(CurrentLocalX, CurrentLocalY, CurrentLocalZ));
	// Stay flush with the slope (MapCreater already tilted)
	SetActorRelativeRotation(FRotator::ZeroRotator);

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bActive = true;
	SetActorTickEnabled(true);
}

void ACar::DeactivateCar()
{
	GetWorldTimerManager().ClearTimer(ShatterTimer);
	RestoreFlyingParts();
	ReleasePlayer();

	bActive = false;
	bCarryingPlayer = false;
	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetActorRelativeLocation(FVector(-100000.f, 0.f, 0.f));

	if (MapOwner)
	{
		MapOwner->ReleaseCar(this);
	}
}

void ACar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bActive)
	{
		return;
	}
	if (bShattering)
	{
		TickShatter(DeltaSeconds);
		return;
	}
	MoveAlongSlope(DeltaSeconds);
}

void ACar::MoveAlongSlope(float DeltaSeconds)
{
	const float Step = MoveSpeed * DeltaSeconds;
	CurrentLocalX -= Step; // slide down toward the player (-local X)
	DistanceTraveled += Step;

	const float TrackStartX = MapOwner ? MapOwner->GetTrackStartLocalX() : 0.f;

	// 1) Reached track start → jail a carried player (keep the hit animation) and recycle.
	if (CurrentLocalX <= TrackStartX)
	{
		if (bCarryingPlayer && CapturedPlayer)
		{
			AKeepGoingUpCharacter* Character = CapturedPlayer;
			CapturedPlayer = nullptr;
			bCarryingPlayer = false;
			Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Character->EnterStartPit();
		}
		DeactivateCar();
		return;
	}

	SetActorRelativeLocation(FVector(CurrentLocalX, CurrentLocalY, CurrentLocalZ));

	// 2) Hit player → push exactly PushDistance then recycle (ignores remaining travel budget)
	if (bCarryingPlayer)
	{
		PushDistanceRemaining -= Step;
		if (PushDistanceRemaining <= 0.f)
		{
			DeactivateCar();
		}
		return;
	}

	// 3) No hit → recycle after sliding TravelDistance (default 100 tiles)
	if (DistanceTraveled >= TravelDistance)
	{
		DeactivateCar();
	}
}

void ACar::OnCubeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bActive || bCarryingPlayer)
	{
		return;
	}

	if (AKeepGoingUpCharacter* Character = Cast<AKeepGoingUpCharacter>(OtherActor))
	{
		TryCapturePlayer(Character);
	}
}

void ACar::ReleaseCapturedPlayer(AKeepGoingUpCharacter* Character)
{
	if (Character && CapturedPlayer == Character)
	{
		ReleasePlayer();
	}
}

void ACar::TryCapturePlayer(AKeepGoingUpCharacter* Character)
{
	if (!Character || Character->IsCarPushLocked())
	{
		return;
	}

	if (Character->IsPushPrankLockingMove())
	{
		ShatterFromPush(Character->GetActorForwardVector());
		return;
	}

	PlayCarSound(ImpactSound);

	CacheComponents();
	if (!CachedPlayerPos)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("Car[%s]: PlayerPos missing, cannot capture player."), *GetName());
		return;
	}

	CapturedPlayer = Character;
	bCarryingPlayer = true;
	PushDistanceRemaining = PushDistance;

	Character->BeginCarPush(this);

	Character->AttachToComponent(
		CachedPlayerPos,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void ACar::ReleasePlayer()
{
	if (!CapturedPlayer)
	{
		return;
	}

	AKeepGoingUpCharacter* Character = CapturedPlayer;
	CapturedPlayer = nullptr;
	bCarryingPlayer = false;

	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	Character->EndCarPush();
}

bool ACar::IsCharacterInside(const AKeepGoingUpCharacter* Character) const
{
	if (!Character || !bActive || bShattered)
	{
		return false;
	}

	const FBox Bounds = GetComponentsBoundingBox(true).ExpandBy(FVector(40.f, 40.f, 80.f));
	return Bounds.IsInsideOrOn(Character->GetActorLocation());
}

AActor* ACar::FindHotDogCar() const
{
	TArray<UChildActorComponent*> ChildComps;
	GetComponents<UChildActorComponent>(ChildComps);
	for (UChildActorComponent* ChildComp : ChildComps)
	{
		if (!ChildComp)
		{
			continue;
		}
		if (ChildComp->GetName().Contains(TEXT("HotDogCar")))
		{
			return ChildComp->GetChildActor();
		}
	}
	return nullptr;
}

void ACar::PlayCarSound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void ACar::ShatterFromPush(const FVector& PushDirection)
{
	if (!bActive || bShattered || bShattering)
	{
		return;
	}
	bShattered = true;
	bShattering = true;

	PlayCarSound(ShatterSound);
	SetActorEnableCollision(false);

	// Fly along the direction the player is running, plus up.
	const FVector Forward = PushDirection.GetSafeNormal().IsNearlyZero()
		? FVector::XAxisVector
		: PushDirection.GetSafeNormal();
	const float SpeedScale = 1.5f;
	AActor* HotDog = FindHotDogCar();
	if (HotDog)
	{
		TArray<UMeshComponent*> Meshes;
		HotDog->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (!Mesh || !Mesh->GetAttachParent())
			{
				continue;
			}

			FFlyingPart Part;
			Part.Component = Mesh;
			Part.Parent = Mesh->GetAttachParent();
			Part.RelativeTransform = Mesh->GetRelativeTransform();
			Part.Velocity = Forward * FMath::FRandRange(900.f, 1500.f) * SpeedScale
				+ FVector(0.f, FMath::FRandRange(-180.f, 180.f), 0.f) * SpeedScale
				+ FVector::UpVector * FMath::FRandRange(1000.f, 1600.f) * SpeedScale;
			Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			FlyingParts.Add(Part);
		}
	}

	GetWorldTimerManager().SetTimer(ShatterTimer, this, &ACar::FinishShatter, 2.8f, false);
}

void ACar::TickShatter(float DeltaSeconds)
{
	for (FFlyingPart& Part : FlyingParts)
	{
		if (!Part.Component)
		{
			continue;
		}
		Part.Velocity.Z -= 1800.f * DeltaSeconds;
		Part.Component->AddWorldOffset(Part.Velocity * DeltaSeconds, false);
		Part.Component->AddWorldRotation(FRotator(Part.Velocity.Z * DeltaSeconds * 0.04f, 50.f * DeltaSeconds, 0.f));
	}
}

void ACar::RestoreFlyingParts()
{
	for (FFlyingPart& Part : FlyingParts)
	{
		if (!Part.Component || !Part.Parent)
		{
			continue;
		}
		Part.Component->AttachToComponent(Part.Parent, FAttachmentTransformRules::KeepRelativeTransform);
		Part.Component->SetRelativeTransform(Part.RelativeTransform);
	}
	FlyingParts.Reset();
	bShattering = false;
}

void ACar::FinishShatter()
{
	RestoreFlyingParts();
	DeactivateCar();
}
