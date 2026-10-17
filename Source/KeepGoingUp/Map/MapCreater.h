// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapCreater.generated.h"

class AMapLayer;
class AMapEnd;
class ACar;
class UAudioComponent;
class USoundBase;

/**
 * Spawns and pools BP_Layer segments along local +X, and pools BP_Car obstacles on 16 fixed lanes.
 */
UCLASS(Blueprintable)
class AMapCreater : public AActor
{
	GENERATED_BODY()

public:
	AMapCreater();

	UFUNCTION(BlueprintPure, Category = "Map")
	int32 GetPlayerLayerIndex() const { return CurrentPlayerLayerIndex; }

	UFUNCTION(BlueprintPure, Category = "Map")
	FLinearColor GetPlayerLayerColor() const;

	UFUNCTION(BlueprintPure, Category = "Map")
	FVector WorldToRoadLocal(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "Map")
	FVector RoadLocalToWorld(const FVector& LocalLocation) const;

	UFUNCTION(BlueprintPure, Category = "Map")
	int32 LocalXToLayerIndex(float LocalX) const;

	FORCEINLINE float GetLayerLength() const { return LayerLength; }
	FORCEINLINE int32 GetTotalLayers() const { return TotalLayers; }
	FORCEINLINE int32 GetCarTravelTiles() const { return CarTravelTiles; }
	FORCEINLINE int32 GetCarPushTiles() const { return CarPushTiles; }
	FORCEINLINE float GetTrackStartLocalX() const { return 0.f; }
	/** Local X of the goal tile (last layer). */
	FORCEINLINE float GetGoalLocalX() const { return static_cast<float>(FMath::Max(TotalLayers - 1, 0)) * LayerLength; }
	FORCEINLINE int32 GetLaneCount() const { return LaneCount; }

	UFUNCTION(BlueprintPure, Category = "Map")
	bool HasPlayerReachedGoal() const { return CurrentPlayerLayerIndex >= TotalLayers; }

	UFUNCTION(BlueprintPure, Category = "Map")
	AMapEnd* GetMapEnd() const { return MapEndActor; }

	/** Ensure BP_End exists at the goal tile (accounts for TotalLayers + road tilt). */
	UFUNCTION(BlueprintCallable, Category = "Map")
	AMapEnd* GetOrCreateMapEnd();

	/** When false, no new car waves spawn (menu / finished). */
	void SetCarSpawningEnabled(bool bEnabled);

	FORCEINLINE bool IsCarSpawningEnabled() const { return bCarSpawningEnabled; }

	/** Deactivate every active car back into the pool. */
	void RecycleAllCars();

	/** Reset / hide the end tile (menu restart). */
	void ResetMapEnd();

	/** Recycle roadside scenery and re-seed spawn spacing. */
	void ResetScenery();

	UFUNCTION(BlueprintPure, Category = "Cars|Lanes")
	float GetLaneLocalY(int32 LaneIndex) const;

	UFUNCTION(BlueprintPure, Category = "Cars|Lanes")
	int32 GetNearestLaneIndex(float LocalY) const;

	const TArray<TObjectPtr<ACar>>& GetActiveCars() const { return ActiveCars; }

	void ReleaseCar(ACar* Car);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void EnsurePoolCapacity();
	void SyncLayersToPlayer();
	void PlaceLayer(AMapLayer* Layer, int32 LayerIndex);
	AMapLayer* AcquireLayer();
	APawn* FindPlayerPawn() const;

	void EnsureCarPoolCapacity();
	void TickCarSpawning(float DeltaSeconds);
	void SpawnCarsBurst();
	ACar* AcquireCar();
	TArray<int32> PickRandomUniqueLanes(int32 Count) const;

	void EnsureSFXPoolCapacity();
	void TickLayerEnterSFX();
	void PlayLayerEnterSound();
	UAudioComponent* AcquireSFXPlayer();
	void ReleaseSFXPlayer(UAudioComponent* Comp);
	void StopAllLayerSFX();
	USoundBase* PickRandomLayerSound() const;

	void EnsureMapEndPlaced();

	void SyncSceneryToPlayer(int32 DesiredMin, int32 DesiredMax);
	void SpawnSceneryClusterAtTile(int32 TileIndex);
	void EnsureSceneryPoolCapacity();
	AActor* AcquireScenery(TSubclassOf<AActor> Class);
	void ReleaseScenery(AActor* Actor);
	void RecycleAllScenery();
	TSubclassOf<AActor> PickRandomSceneryClass() const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSubclassOf<AMapLayer> LayerClass;

	/** Final tile actor (BP_End). Placed at TotalLayers instead of BP_Layer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	TSubclassOf<AMapEnd> EndClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1.0"))
	float LayerLength = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "1"))
	int32 PoolRadius = 100;

	/**
	 * Map length in tiles. Last tile (this index) is BP_End; tiles 1..TotalLayers-1 are BP_Layer.
	 * Change this (or BP_MapCreater → Total Layers) to adjust goal distance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (ClampMin = "2"))
	int32 TotalLayers = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map")
	float EnterOffset = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars")
	TSubclassOf<ACar> CarClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarPoolSize = 32;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarSpawnAheadTiles = 50;

	/** Without a hit, recycle after sliding this many tiles. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarTravelTiles = 100;

	/** After hitting the player, push this many tiles then recycle (ignores travel distance left). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarPushTiles = 50;

	/** Number of fixed lateral routes (Y). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars|Lanes", meta = (ClampMin = "2"))
	int32 LaneCount = 16;

	/**
	 * Road width along local Y, centered on 0 (so usable Y ≈ ±RoadWidth/2).
	 * 16 lanes are evenly placed between the edged margins — NOT 100~1700
	 * (that range sits entirely on +Y and spills off a centered road).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars|Lanes", meta = (ClampMin = "100.0"))
	float RoadWidth = 1800.f;

	/** Keep this many uu away from each road edge when placing lanes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars|Lanes", meta = (ClampMin = "0.0"))
	float LaneEdgeMargin = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars")
	float CarSpawnZ = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1.0"))
	float CarMoveSpeed = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "0.1"))
	float CarSpawnInterval = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarSpawnCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cars", meta = (ClampMin = "1"))
	int32 CarSpawnCountMax = 4;

	/** Random one-shots when the player enters a new tile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Layer Enter")
	TArray<TSoftObjectPtr<USoundBase>> LayerEnterSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Layer Enter", meta = (ClampMin = "1"))
	int32 SFXPoolSize = 8;

	/** 1.0 = full asset volume; default is ~2/3. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Layer Enter", meta = (ClampMin = "0.0"))
	float LayerEnterVolume = 0.67f;

	/** BP_fly1~6 roadside scenery. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery")
	TArray<TSoftClassPtr<AActor>> SceneryClasses;

	/** Spawn a scenery cluster every this many tiles (randomized each time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "1"))
	int32 SceneryIntervalMin = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "1"))
	int32 SceneryIntervalMax = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "1"))
	int32 SceneryCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "1"))
	int32 SceneryCountMax = 2;

	/** Extra distance beyond road half-width for left/right placement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "0.0"))
	float ScenerySidePadding = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery")
	float SceneryZ = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenery", meta = (ClampMin = "1"))
	int32 SceneryPoolSize = 48;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMapLayer>> ActiveLayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMapLayer>> FreeLayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACar>> ActiveCars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACar>> FreeCars;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> FreeSFXPlayers;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ActiveSFXPlayers;

	UPROPERTY(Transient)
	TObjectPtr<AMapEnd> MapEndActor;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ActiveScenery;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> FreeScenery;

	/** Tile index each active scenery was spawned for (parallel to ActiveScenery). */
	TArray<int32> ActiveSceneryTiles;

	int32 ActiveMinIndex = 0;
	int32 ActiveMaxIndex = 0;
	int32 CurrentPlayerLayerIndex = 1;
	int32 LastSFXLayerIndex = 1;
	int32 NextSceneryTile = 5;

	float CarSpawnTimer = 0.f;
	bool bCarSpawningEnabled = false;
};
