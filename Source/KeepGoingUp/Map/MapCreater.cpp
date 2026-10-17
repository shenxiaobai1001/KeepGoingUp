// Copyright Epic Games, Inc. All Rights Reserved.

#include "Map/MapCreater.h"
#include "Map/MapLayer.h"
#include "Map/MapEnd.h"
#include "Map/Car.h"
#include "KeepGoingUp.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AMapCreater::AMapCreater()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FClassFinder<AMapEnd> EndFinder(TEXT("/Game/Game/Code/Scene/BP_End"));
	if (EndFinder.Succeeded())
	{
		EndClass = EndFinder.Class;
	}

	LayerEnterSounds = {
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano1.piano1"))),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano2.piano2"))),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano3.piano3"))),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano4.piano4"))),
	};

	SceneryClasses = {
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly1.BP_fly1_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly2.BP_fly2_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly3.BP_fly3_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly4.BP_fly4_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly5.BP_fly5_C"))),
		TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly6.BP_fly6_C"))),
	};
}

void AMapCreater::BeginPlay()
{
	Super::BeginPlay();

	if (!LayerClass)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("MapCreater: LayerClass is not set. Assign BP_Layer on the actor."));
	}
	else
	{
		EnsurePoolCapacity();
		SyncLayersToPlayer();
	}

	if (!CarClass)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: CarClass is not set. Assign BP_Car to enable obstacles."));
	}
	else
	{
		// Keep interval modern if an older BP CDO still has 0.5 / 1.0
		if (CarSpawnInterval < 0.75f - KINDA_SMALL_NUMBER || CarSpawnInterval > 0.75f + KINDA_SMALL_NUMBER)
		{
			if (FMath::IsNearlyEqual(CarSpawnInterval, 0.5f) || FMath::IsNearlyEqual(CarSpawnInterval, 1.0f))
			{
				CarSpawnInterval = 0.75f;
			}
		}
		EnsureCarPoolCapacity();
		CarSpawnTimer = CarSpawnInterval;
		bCarSpawningEnabled = false; // GameMode enables after UI_Start
	}

	// LayerLength = UU per tile (keep ~100). TotalLayers = tile count to the goal.
	// If BP accidentally set LayerLength to 10000 (meant TotalLayers), snap it back.
	if (LayerLength >= 1000.f)
	{
		UE_LOG(LogKeepGoingUp, Warning,
			TEXT("MapCreater: LayerLength was %.0f (looks like TotalLayers was edited by mistake). Restoring LayerLength=100. Change TotalLayers for map length."),
			LayerLength);
		LayerLength = 100.f;
	}

	if (TotalLayers < 2)
	{
		TotalLayers = 2;
	}

	if (FMath::IsNearlyEqual(LayerEnterVolume, 1.f))
	{
		LayerEnterVolume = 0.67f;
	}

	if (!EndClass)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: EndClass not set. Assign BP_End (parent MapEnd) for the finish tile."));
	}

	if (LayerEnterSounds.Num() == 0)
	{
		LayerEnterSounds = {
			TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano1.piano1"))),
			TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano2.piano2"))),
			TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano3.piano3"))),
			TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Game/Audio/piano4.piano4"))),
		};
	}

	EnsureSFXPoolCapacity();
	LastSFXLayerIndex = CurrentPlayerLayerIndex;

	if (SceneryClasses.Num() == 0)
	{
		SceneryClasses = {
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly1.BP_fly1_C"))),
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly2.BP_fly2_C"))),
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly3.BP_fly3_C"))),
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly4.BP_fly4_C"))),
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly5.BP_fly5_C"))),
			TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Game/Code/Scene/BP_fly6.BP_fly6_C"))),
		};
	}

	// Do not spawn scenery until gameplay starts (avoids double-spawn on Start click).
	RecycleAllScenery();
	NextSceneryTile = FMath::Max(1, FMath::Min(SceneryIntervalMin, SceneryIntervalMax));
}

void AMapCreater::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncLayersToPlayer();
	TickCarSpawning(DeltaSeconds);
}

FVector AMapCreater::WorldToRoadLocal(const FVector& WorldLocation) const
{
	return GetActorTransform().InverseTransformPosition(WorldLocation);
}

FVector AMapCreater::RoadLocalToWorld(const FVector& LocalLocation) const
{
	return GetActorTransform().TransformPosition(LocalLocation);
}

int32 AMapCreater::LocalXToLayerIndex(float LocalX) const
{
	if (LayerLength <= KINDA_SMALL_NUMBER)
	{
		return 1;
	}

	const int32 ZeroBased = FMath::FloorToInt(LocalX / LayerLength);
	return FMath::Clamp(ZeroBased + 1, 1, TotalLayers);
}

FLinearColor AMapCreater::GetPlayerLayerColor() const
{
	return AMapLayer::GetColorForLayerIndex(CurrentPlayerLayerIndex);
}

APawn* AMapCreater::FindPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

void AMapCreater::EnsurePoolCapacity()
{
	const int32 DesiredPool = FMath::Min(TotalLayers, PoolRadius * 2 + 1);
	const int32 Have = ActiveLayers.Num() + FreeLayers.Num();
	const int32 Need = DesiredPool - Have;
	if (Need <= 0 || !LayerClass)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < Need; ++i)
	{
		AMapLayer* Layer = GetWorld()->SpawnActor<AMapLayer>(LayerClass, GetActorLocation(), GetActorRotation(), Params);
		if (!Layer)
		{
			continue;
		}

		Layer->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Layer->SetActorHiddenInGame(true);
		Layer->SetActorEnableCollision(false);
		FreeLayers.Add(Layer);
	}
}

AMapLayer* AMapCreater::AcquireLayer()
{
	EnsurePoolCapacity();

	AMapLayer* Layer = nullptr;
	if (FreeLayers.Num() > 0)
	{
		Layer = FreeLayers.Pop(EAllowShrinking::No);
	}
	else if (ActiveLayers.Num() > 0)
	{
		int32 FarthestIdx = 0;
		int32 FarthestDist = -1;
		for (int32 i = 0; i < ActiveLayers.Num(); ++i)
		{
			AMapLayer* Candidate = ActiveLayers[i];
			if (!Candidate)
			{
				continue;
			}
			const int32 Dist = FMath::Abs(Candidate->GetLayerIndex() - CurrentPlayerLayerIndex);
			if (Dist > FarthestDist)
			{
				FarthestDist = Dist;
				FarthestIdx = i;
			}
		}
		Layer = ActiveLayers[FarthestIdx];
		ActiveLayers.RemoveAt(FarthestIdx, 1, EAllowShrinking::No);
	}

	if (Layer)
	{
		Layer->SetActorHiddenInGame(false);
		Layer->SetActorEnableCollision(true);
	}
	return Layer;
}

void AMapCreater::PlaceLayer(AMapLayer* Layer, int32 LayerIndex)
{
	if (!Layer)
	{
		return;
	}

	const float LocalX = (LayerIndex - 1) * LayerLength;
	Layer->SetActorRelativeLocation(FVector(LocalX, 0.f, 0.f));
	Layer->SetActorRelativeRotation(FRotator::ZeroRotator);
	Layer->SetupLayer(LayerIndex, LayerLength);
}

void AMapCreater::SyncLayersToPlayer()
{
	if (!LayerClass)
	{
		return;
	}

	APawn* Player = FindPlayerPawn();
	float PlayerLocalX = 0.f;
	if (Player)
	{
		PlayerLocalX = WorldToRoadLocal(Player->GetActorLocation()).X;
	}

	CurrentPlayerLayerIndex = LocalXToLayerIndex(PlayerLocalX);
	TickLayerEnterSFX();

	// BP_Layer only for 1 .. TotalLayers-1; TotalLayers is reserved for BP_End.
	const int32 MaxLayerTile = FMath::Max(TotalLayers - 1, 1);
	const int32 DesiredMin = FMath::Max(1, CurrentPlayerLayerIndex - PoolRadius);
	const int32 DesiredMax = FMath::Min(MaxLayerTile, CurrentPlayerLayerIndex + PoolRadius);

	for (int32 i = ActiveLayers.Num() - 1; i >= 0; --i)
	{
		AMapLayer* Layer = ActiveLayers[i];
		if (!Layer)
		{
			ActiveLayers.RemoveAt(i);
			continue;
		}

		const int32 Idx = Layer->GetLayerIndex();
		// Never keep a pooled layer sitting on the end tile index
		if (Idx < DesiredMin || Idx > DesiredMax || Idx >= TotalLayers)
		{
			Layer->SetActorHiddenInGame(true);
			Layer->SetActorEnableCollision(false);
			FreeLayers.Add(Layer);
			ActiveLayers.RemoveAt(i);
		}
	}

	TSet<int32> Present;
	Present.Reserve(ActiveLayers.Num());
	for (AMapLayer* Layer : ActiveLayers)
	{
		if (Layer)
		{
			Present.Add(Layer->GetLayerIndex());
		}
	}

	for (int32 Index = DesiredMin; Index <= DesiredMax; ++Index)
	{
		if (Present.Contains(Index))
		{
			continue;
		}

		AMapLayer* Layer = AcquireLayer();
		if (!Layer)
		{
			UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: failed to acquire layer for index %d"), Index);
			break;
		}

		PlaceLayer(Layer, Index);
		ActiveLayers.Add(Layer);
	}

	ActiveMinIndex = DesiredMin;
	ActiveMaxIndex = DesiredMax;

	for (AMapLayer* Layer : ActiveLayers)
	{
		if (Layer)
		{
			Layer->UpdateRoadByPlayerLocalX(PlayerLocalX, EnterOffset);
		}
	}

	SyncSceneryToPlayer(DesiredMin, DesiredMax);

	// Spawn / keep BP_End when the end tile is within the pool window
	if (CurrentPlayerLayerIndex + PoolRadius >= TotalLayers)
	{
		EnsureMapEndPlaced();
	}
}

AMapEnd* AMapCreater::GetOrCreateMapEnd()
{
	EnsureMapEndPlaced();
	return MapEndActor;
}

void AMapCreater::EnsureMapEndPlaced()
{
	if (!EndClass)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: EndClass is not set. Assign BP_End (parent MapEnd)."));
		return;
	}

	if (!MapEndActor)
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		MapEndActor = GetWorld()->SpawnActor<AMapEnd>(EndClass, GetActorLocation(), GetActorRotation(), Params);
		if (MapEndActor)
		{
			MapEndActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}

	if (!MapEndActor)
	{
		return;
	}

	const float LocalX = (TotalLayers - 1) * LayerLength;
	MapEndActor->SetActorRelativeLocation(FVector(LocalX, 0.f, 0.f));
	MapEndActor->SetActorRelativeRotation(FRotator::ZeroRotator);
	MapEndActor->SetActorHiddenInGame(false);
	MapEndActor->SetActorEnableCollision(true);
}

void AMapCreater::ResetMapEnd()
{
	if (MapEndActor)
	{
		MapEndActor->ResetEndTrigger();
		MapEndActor->SetActorHiddenInGame(true);
		MapEndActor->SetActorEnableCollision(false);
	}
}

void AMapCreater::ResetScenery()
{
	RecycleAllScenery();
	const int32 IntervalMin = FMath::Max(1, FMath::Min(SceneryIntervalMin, SceneryIntervalMax));
	const int32 IntervalMax = FMath::Max(IntervalMin, FMath::Max(SceneryIntervalMin, SceneryIntervalMax));
	NextSceneryTile = FMath::RandRange(IntervalMin, IntervalMax);
}

void AMapCreater::SyncSceneryToPlayer(int32 DesiredMin, int32 DesiredMax)
{
	if (SceneryClasses.Num() == 0)
	{
		return;
	}

	// Menu / pre-start: keep the roadside clear so Start doesn't regenerate a second pass.
	if (!bCarSpawningEnabled)
	{
		if (ActiveScenery.Num() > 0)
		{
			RecycleAllScenery();
		}
		return;
	}

	const int32 MaxSceneryTile = FMath::Max(TotalLayers - 1, 1);

	// Spawn clusters as the pool window advances
	while (NextSceneryTile <= DesiredMax && NextSceneryTile <= MaxSceneryTile)
	{
		SpawnSceneryClusterAtTile(NextSceneryTile);
		const int32 IntervalMin = FMath::Max(1, FMath::Min(SceneryIntervalMin, SceneryIntervalMax));
		const int32 IntervalMax = FMath::Max(IntervalMin, FMath::Max(SceneryIntervalMin, SceneryIntervalMax));
		NextSceneryTile += FMath::RandRange(IntervalMin, IntervalMax);
	}

	// Recycle scenery that left the pooled window
	for (int32 i = ActiveScenery.Num() - 1; i >= 0; --i)
	{
		const int32 Tile = ActiveSceneryTiles.IsValidIndex(i) ? ActiveSceneryTiles[i] : 0;
		if (Tile < DesiredMin || Tile > DesiredMax)
		{
			ReleaseScenery(ActiveScenery[i]);
		}
	}
}

void AMapCreater::SpawnSceneryClusterAtTile(int32 TileIndex)
{
	const int32 CountMin = FMath::Max(1, FMath::Min(SceneryCountMin, SceneryCountMax));
	const int32 CountMax = FMath::Max(CountMin, FMath::Max(SceneryCountMin, SceneryCountMax));
	const int32 SpawnCount = FMath::RandRange(CountMin, CountMax);

	const float RoadHalf = RoadWidth * 0.5f;
	const float SideY = RoadHalf + ScenerySidePadding;
	const float BaseLocalX = (TileIndex - 1) * LayerLength + LayerLength * 0.5f;

	// Prefer opposite sides when spawning two
	TArray<int32> Sides;
	if (SpawnCount >= 2)
	{
		Sides = { -1, 1 };
		if (FMath::RandBool())
		{
			Sides.Swap(0, 1);
		}
	}
	else
	{
		Sides = { FMath::RandBool() ? -1 : 1 };
	}

	for (int32 i = 0; i < SpawnCount; ++i)
	{
		TSubclassOf<AActor> Class = PickRandomSceneryClass();
		if (!Class)
		{
			continue;
		}

		AActor* Prop = AcquireScenery(Class);
		if (!Prop)
		{
			continue;
		}

		const int32 Side = Sides.IsValidIndex(i) ? Sides[i] : (FMath::RandBool() ? -1 : 1);
		const float JitterX = FMath::FRandRange(-LayerLength * 0.35f, LayerLength * 0.35f);
		const float JitterY = FMath::FRandRange(-80.f, 80.f);
		const float LocalY = static_cast<float>(Side) * SideY + JitterY;
		const float Yaw = FMath::FRandRange(0.f, 360.f);

		Prop->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Prop->SetActorRelativeLocation(FVector(BaseLocalX + JitterX, LocalY, SceneryZ));
		Prop->SetActorRelativeRotation(FRotator(0.f, Yaw, 0.f));
		Prop->SetActorHiddenInGame(false);
		Prop->SetActorEnableCollision(false);
		Prop->SetActorTickEnabled(false);

		ActiveScenery.Add(Prop);
		ActiveSceneryTiles.Add(TileIndex);
	}
}

void AMapCreater::EnsureSceneryPoolCapacity()
{
	// Pool grows on demand in AcquireScenery; nothing to pre-warm beyond soft classes.
}

AActor* AMapCreater::AcquireScenery(TSubclassOf<AActor> Class)
{
	if (!Class)
	{
		return nullptr;
	}

	for (int32 i = FreeScenery.Num() - 1; i >= 0; --i)
	{
		AActor* Candidate = FreeScenery[i];
		if (Candidate && Candidate->GetClass() == Class)
		{
			FreeScenery.RemoveAt(i, 1, EAllowShrinking::No);
			return Candidate;
		}
	}

	if (ActiveScenery.Num() + FreeScenery.Num() >= SceneryPoolSize)
	{
		// Steal farthest free slot of any class if pool is full — recycle oldest active
		if (ActiveScenery.Num() > 0)
		{
			AActor* Stolen = ActiveScenery[0];
			ReleaseScenery(Stolen);
			if (Stolen && Stolen->GetClass() == Class)
			{
				FreeScenery.Remove(Stolen);
				return Stolen;
			}
		}
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Spawned = GetWorld()->SpawnActor<AActor>(Class, GetActorLocation(), GetActorRotation(), Params);
	if (Spawned)
	{
		Spawned->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Spawned->SetActorHiddenInGame(true);
		Spawned->SetActorEnableCollision(false);
		Spawned->SetActorTickEnabled(false);
	}
	return Spawned;
}

void AMapCreater::ReleaseScenery(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	const int32 Idx = ActiveScenery.IndexOfByKey(Actor);
	if (Idx != INDEX_NONE)
	{
		ActiveScenery.RemoveAt(Idx, 1, EAllowShrinking::No);
		if (ActiveSceneryTiles.IsValidIndex(Idx))
		{
			ActiveSceneryTiles.RemoveAt(Idx, 1, EAllowShrinking::No);
		}
	}

	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	Actor->SetActorRelativeLocation(FVector(-100000.f, 0.f, 0.f));

	if (!FreeScenery.Contains(Actor))
	{
		FreeScenery.Add(Actor);
	}
}

void AMapCreater::RecycleAllScenery()
{
	TArray<TObjectPtr<AActor>> Snapshot = ActiveScenery;
	for (AActor* Actor : Snapshot)
	{
		ReleaseScenery(Actor);
	}
	ActiveScenery.Reset();
	ActiveSceneryTiles.Reset();
}

TSubclassOf<AActor> AMapCreater::PickRandomSceneryClass() const
{
	TArray<TSubclassOf<AActor>> Loaded;
	Loaded.Reserve(SceneryClasses.Num());
	for (const TSoftClassPtr<AActor>& SoftClass : SceneryClasses)
	{
		if (UClass* Class = SoftClass.LoadSynchronous())
		{
			Loaded.Add(Class);
		}
	}

	if (Loaded.Num() == 0)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: no SceneryClasses loaded (expected BP_fly1~6)."));
		return nullptr;
	}

	return Loaded[FMath::RandRange(0, Loaded.Num() - 1)];
}

void AMapCreater::EnsureCarPoolCapacity()
{
	if (!CarClass)
	{
		return;
	}

	const int32 Have = ActiveCars.Num() + FreeCars.Num();
	const int32 Need = CarPoolSize - Have;
	if (Need <= 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < Need; ++i)
	{
		ACar* Car = GetWorld()->SpawnActor<ACar>(CarClass, GetActorLocation(), GetActorRotation(), Params);
		if (!Car)
		{
			continue;
		}

		Car->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Car->SetActorHiddenInGame(true);
		Car->SetActorEnableCollision(false);
		Car->SetActorTickEnabled(false);
		FreeCars.Add(Car);
	}
}

ACar* AMapCreater::AcquireCar()
{
	EnsureCarPoolCapacity();
	if (FreeCars.Num() == 0)
	{
		return nullptr;
	}

	ACar* Car = FreeCars.Pop(EAllowShrinking::No);
	ActiveCars.Add(Car);
	return Car;
}

void AMapCreater::ReleaseCar(ACar* Car)
{
	if (!Car)
	{
		return;
	}

	ActiveCars.Remove(Car);
	if (!FreeCars.Contains(Car))
	{
		FreeCars.Add(Car);
	}
}

void AMapCreater::SetCarSpawningEnabled(bool bEnabled)
{
	bCarSpawningEnabled = bEnabled;
	if (bCarSpawningEnabled)
	{
		CarSpawnTimer = CarSpawnInterval;
		// Don't fire SFX for the tile the player is already standing on when a run starts.
		LastSFXLayerIndex = CurrentPlayerLayerIndex;
	}
	else
	{
		StopAllLayerSFX();
		LastSFXLayerIndex = CurrentPlayerLayerIndex;
	}
}

void AMapCreater::RecycleAllCars()
{
	TArray<TObjectPtr<ACar>> Snapshot = ActiveCars;
	for (ACar* Car : Snapshot)
	{
		if (Car)
		{
			Car->DeactivateCar();
		}
	}
}

void AMapCreater::TickCarSpawning(float DeltaSeconds)
{
	if (!CarClass || !bCarSpawningEnabled || HasPlayerReachedGoal())
	{
		return;
	}

	CarSpawnTimer -= DeltaSeconds;
	if (CarSpawnTimer > 0.f)
	{
		return;
	}

	CarSpawnTimer = CarSpawnInterval;
	SpawnCarsBurst();
}

float AMapCreater::GetLaneLocalY(int32 LaneIndex) const
{
	const int32 ClampedCount = FMath::Max(LaneCount, 2);
	const int32 ClampedIndex = FMath::Clamp(LaneIndex, 0, ClampedCount - 1);

	// Centered road: lanes span [-Half+Margin, +Half-Margin]
	const float Half = RoadWidth * 0.5f;
	const float Margin = FMath::Clamp(LaneEdgeMargin, 0.f, Half);
	const float Y0 = -Half + Margin;
	const float Y1 = Half - Margin;
	const float Alpha = static_cast<float>(ClampedIndex) / static_cast<float>(ClampedCount - 1);
	return FMath::Lerp(Y0, Y1, Alpha);
}

int32 AMapCreater::GetNearestLaneIndex(float LocalY) const
{
	const int32 ClampedCount = FMath::Max(LaneCount, 2);
	int32 Best = 0;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < ClampedCount; ++i)
	{
		const float Dist = FMath::Abs(GetLaneLocalY(i) - LocalY);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

TArray<int32> AMapCreater::PickRandomUniqueLanes(int32 Count) const
{
	TArray<int32> Pool;
	const int32 ClampedCount = FMath::Max(LaneCount, 2);
	Pool.Reserve(ClampedCount);
	for (int32 i = 0; i < ClampedCount; ++i)
	{
		Pool.Add(i);
	}

	// Fisher–Yates shuffle
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	const int32 Take = FMath::Clamp(Count, 0, Pool.Num());
	TArray<int32> Result;
	Result.Reserve(Take);
	for (int32 i = 0; i < Take; ++i)
	{
		Result.Add(Pool[i]);
	}
	return Result;
}

void AMapCreater::SpawnCarsBurst()
{
	if (HasPlayerReachedGoal())
	{
		return;
	}

	APawn* Player = FindPlayerPawn();
	if (!Player)
	{
		return;
	}

	const FVector PlayerLocal = WorldToRoadLocal(Player->GetActorLocation());
	const float GoalLocalX = GetGoalLocalX();

	// Spawn ahead of the player, but never past the goal tile.
	// If the player is within CarSpawnAheadTiles of the goal, cars lock to the goal tile.
	float StartLocalX = PlayerLocal.X + static_cast<float>(CarSpawnAheadTiles) * LayerLength;
	StartLocalX = FMath::Min(StartLocalX, GoalLocalX);

	// Already at / past the goal spawn line — nothing useful to spawn.
	if (StartLocalX <= PlayerLocal.X + KINDA_SMALL_NUMBER)
	{
		return;
	}

	const int32 MinCount = FMath::Min(CarSpawnCountMin, CarSpawnCountMax);
	const int32 MaxCount = FMath::Max(CarSpawnCountMin, CarSpawnCountMax);
	const int32 SpawnCount = FMath::RandRange(MinCount, MaxCount);
	const TArray<int32> Lanes = PickRandomUniqueLanes(SpawnCount);

	for (int32 LaneIdx : Lanes)
	{
		ACar* Car = AcquireCar();
		if (!Car)
		{
			UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: car pool exhausted."));
			break;
		}

		Car->ActivateCar(this, StartLocalX, LaneIdx, CarSpawnZ, CarMoveSpeed);
	}
}

void AMapCreater::EnsureSFXPoolCapacity()
{
	const int32 Have = FreeSFXPlayers.Num() + ActiveSFXPlayers.Num();
	const int32 Need = SFXPoolSize - Have;
	if (Need <= 0)
	{
		return;
	}

	for (int32 i = 0; i < Need; ++i)
	{
		UAudioComponent* Comp = NewObject<UAudioComponent>(this, NAME_None, RF_Transient);
		if (!Comp)
		{
			continue;
		}

		Comp->SetupAttachment(SceneRoot);
		Comp->bAutoActivate = false;
		Comp->bAutoDestroy = false;
		Comp->bAllowSpatialization = false;
		Comp->RegisterComponent();
		FreeSFXPlayers.Add(Comp);
	}
}

void AMapCreater::TickLayerEnterSFX()
{
	if (!bCarSpawningEnabled)
	{
		LastSFXLayerIndex = CurrentPlayerLayerIndex;
		return;
	}

	if (CurrentPlayerLayerIndex > LastSFXLayerIndex)
	{
		// One random note per newly entered tile (cap burst if multiple tiles skip in one frame).
		const int32 Crossed = FMath::Min(CurrentPlayerLayerIndex - LastSFXLayerIndex, 3);
		for (int32 i = 0; i < Crossed; ++i)
		{
			PlayLayerEnterSound();
		}
		LastSFXLayerIndex = CurrentPlayerLayerIndex;
	}
	else if (CurrentPlayerLayerIndex < LastSFXLayerIndex)
	{
		// Reset / pushed backward — just resync, no SFX.
		LastSFXLayerIndex = CurrentPlayerLayerIndex;
	}
}

void AMapCreater::PlayLayerEnterSound()
{
	USoundBase* Sound = PickRandomLayerSound();
	if (!Sound)
	{
		return;
	}

	UAudioComponent* Comp = AcquireSFXPlayer();
	if (!Comp)
	{
		return;
	}

	Comp->Stop();
	Comp->SetSound(Sound);
	Comp->SetVolumeMultiplier(LayerEnterVolume);
	Comp->Play();
}

USoundBase* AMapCreater::PickRandomLayerSound() const
{
	TArray<USoundBase*> Loaded;
	Loaded.Reserve(LayerEnterSounds.Num());
	for (const TSoftObjectPtr<USoundBase>& SoftSound : LayerEnterSounds)
	{
		if (USoundBase* Sound = SoftSound.LoadSynchronous())
		{
			Loaded.Add(Sound);
		}
	}

	if (Loaded.Num() == 0)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("MapCreater: no LayerEnterSounds loaded (expected /Game/Game/Audio/piano1~4)."));
		return nullptr;
	}

	return Loaded[FMath::RandRange(0, Loaded.Num() - 1)];
}

UAudioComponent* AMapCreater::AcquireSFXPlayer()
{
	EnsureSFXPoolCapacity();

	UAudioComponent* Comp = nullptr;
	if (FreeSFXPlayers.Num() > 0)
	{
		Comp = FreeSFXPlayers.Pop(EAllowShrinking::No);
	}
	else if (ActiveSFXPlayers.Num() > 0)
	{
		// Pool exhausted: steal the oldest still-playing player.
		Comp = ActiveSFXPlayers[0];
		ActiveSFXPlayers.RemoveAt(0, 1, EAllowShrinking::No);
		if (Comp)
		{
			Comp->Stop();
			Comp->OnAudioFinishedNative.Clear();
		}
	}
	else
	{
		return nullptr;
	}

	if (!Comp)
	{
		return nullptr;
	}

	ActiveSFXPlayers.Add(Comp);

	TWeakObjectPtr<AMapCreater> WeakThis(this);
	TWeakObjectPtr<UAudioComponent> WeakComp(Comp);
	Comp->OnAudioFinishedNative.Clear();
	Comp->OnAudioFinishedNative.AddLambda([WeakThis, WeakComp](UAudioComponent* /*FinishedComp*/)
	{
		if (AMapCreater* Self = WeakThis.Get())
		{
			Self->ReleaseSFXPlayer(WeakComp.Get());
		}
	});

	return Comp;
}

void AMapCreater::ReleaseSFXPlayer(UAudioComponent* Comp)
{
	if (!Comp)
	{
		return;
	}

	Comp->OnAudioFinishedNative.Clear();
	Comp->Stop();
	Comp->SetSound(nullptr);
	ActiveSFXPlayers.Remove(Comp);
	if (!FreeSFXPlayers.Contains(Comp))
	{
		FreeSFXPlayers.Add(Comp);
	}
}

void AMapCreater::StopAllLayerSFX()
{
	TArray<TObjectPtr<UAudioComponent>> Snapshot = ActiveSFXPlayers;
	for (UAudioComponent* Comp : Snapshot)
	{
		ReleaseSFXPlayer(Comp);
	}
}
