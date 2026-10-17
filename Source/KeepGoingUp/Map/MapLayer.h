// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapLayer.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Single road segment. Place Left / Right / Road meshes in the Blueprint (BP_Layer).
 * Index is 1-based. Walls alternate black/white via two solid materials; Road uses MID color/opacity.
 */
UCLASS(Blueprintable)
class AMapLayer : public AActor
{
	GENERATED_BODY()

public:
	AMapLayer();

	UFUNCTION(BlueprintCallable, Category = "MapLayer")
	void SetupLayer(int32 InLayerIndex, float InLayerLength);

	UFUNCTION(BlueprintCallable, Category = "MapLayer")
	void UpdateRoadByPlayerLocalX(float PlayerLocalX, float EnterOffset);

	UFUNCTION(BlueprintPure, Category = "MapLayer")
	static FLinearColor GetColorForLayerIndex(int32 InLayerIndex);

	FORCEINLINE int32 GetLayerIndex() const { return LayerIndex; }
	FORCEINLINE FLinearColor GetActiveRoadColor() const { return ActiveRoadColor; }
	FORCEINLINE bool IsRoadActive() const { return bRoadActive; }

protected:
	virtual void BeginPlay() override;

	void CacheMeshComponents(bool bForce = false);
	void ApplyWallColors();
	void ApplyRoadColor(bool bActive);
	void PlayRandomPiano();
	void AssignMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material) const;

	UMaterialInterface* ResolveRoadMaterial() const;
	UMaterialInstanceDynamic* MakeRoadMID(
		UStaticMeshComponent* Mesh,
		UMaterialInterface* SourceMaterial,
		const FLinearColor& Color,
		float Opacity) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapLayer")
	int32 LayerIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MapLayer")
	float LayerLength = 100.f;

	/** Odd segments (1,3,5...) use this solid material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	TSoftObjectPtr<UMaterialInterface> WallWhiteMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Game/Materials/WallWhite.WallWhite")));

	/** Even segments (2,4,6...) use this solid material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	TSoftObjectPtr<UMaterialInterface> WallBlackMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Game/Materials/WallBlack.WallBlack")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	TSoftObjectPtr<UMaterialInterface> RoadMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Game/Materials/Road.Road")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	FName ColorParameterName = TEXT("BaseColor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	FName OpacityParameterName = TEXT("Opacity");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InactiveRoadOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActiveRoadOpacity = 0.85f;

	/** Soft gray while not yet reached (avoid pure white). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MapLayer|Material")
	FLinearColor InactiveRoadColor = FLinearColor(0.45f, 0.45f, 0.48f, 1.f);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeftMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RightMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RoadMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RoadMID = nullptr;

	FLinearColor ActiveRoadColor = FLinearColor::Red;
	bool bRoadActive = false;
	bool bMeshesCached = false;
};
