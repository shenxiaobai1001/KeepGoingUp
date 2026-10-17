// Copyright Epic Games, Inc. All Rights Reserved.

#include "Map/MapLayer.h"
#include "Components/StaticMeshComponent.h"
#include "KeepGoingUpGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "KeepGoingUp.h"

AMapLayer::AMapLayer()
{
	PrimaryActorTick.bCanEverTick = false;
	InactiveRoadOpacity = 0.35f;
	ActiveRoadOpacity = 0.85f;
	InactiveRoadColor = FLinearColor(0.45f, 0.45f, 0.48f, 1.f);
}

void AMapLayer::BeginPlay()
{
	Super::BeginPlay();

	// Override stale BP defaults that still use pure white
	if (InactiveRoadColor.R > 0.9f && InactiveRoadColor.G > 0.9f && InactiveRoadColor.B > 0.9f)
	{
		InactiveRoadColor = FLinearColor(0.45f, 0.45f, 0.48f, 1.f);
		InactiveRoadOpacity = 0.35f;
	}

	CacheMeshComponents(true);
}

void AMapLayer::CacheMeshComponents(bool bForce)
{
	if (bMeshesCached && !bForce && LeftMesh && RightMesh && RoadMesh)
	{
		return;
	}

	LeftMesh = nullptr;
	RightMesh = nullptr;
	RoadMesh = nullptr;

	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);

	UE_LOG(LogKeepGoingUp, Log, TEXT("MapLayer[%s] StaticMeshComponent count=%d"), *GetName(), Meshes.Num());

	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		const FString Name = Mesh->GetName();
		const FString ParentName = Mesh->GetAttachParent() ? Mesh->GetAttachParent()->GetName() : TEXT("(none)");
		UE_LOG(LogKeepGoingUp, Log, TEXT("  SMC name='%s' parent='%s'"), *Name, *ParentName);

		const bool bNameRoad = Name.Contains(TEXT("Road"), ESearchCase::IgnoreCase);
		const bool bParentRoad = ParentName.Contains(TEXT("Road"), ESearchCase::IgnoreCase);
		const bool bNameLeft = Name.Contains(TEXT("Left"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Box"));
		const bool bParentLeft = ParentName.Contains(TEXT("Left"), ESearchCase::IgnoreCase);
		const bool bNameRight = Name.Contains(TEXT("Right"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("Box1"));
		const bool bParentRight = ParentName.Contains(TEXT("Right"), ESearchCase::IgnoreCase);

		if (bNameRoad || bParentRoad)
		{
			RoadMesh = Mesh;
		}
		else if (bNameLeft || bParentLeft)
		{
			LeftMesh = Mesh;
		}
		else if (bNameRight || bParentRight)
		{
			RightMesh = Mesh;
		}
	}

	if ((!LeftMesh || !RightMesh) && Meshes.Num() > 0)
	{
		TArray<UStaticMeshComponent*> Walls;
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh != RoadMesh)
			{
				Walls.Add(Mesh);
			}
		}

		Walls.Sort([](const UStaticMeshComponent& A, const UStaticMeshComponent& B)
		{
			return A.GetRelativeLocation().Y < B.GetRelativeLocation().Y;
		});

		if (Walls.Num() >= 1 && !LeftMesh)
		{
			LeftMesh = Walls[0];
		}
		if (Walls.Num() >= 2 && !RightMesh)
		{
			RightMesh = Walls.Last();
		}
		if (Walls.Num() == 1 && !RightMesh)
		{
			RightMesh = Walls[0];
		}
	}

	if (!RoadMesh)
	{
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh != LeftMesh && Mesh != RightMesh)
			{
				RoadMesh = Mesh;
				break;
			}
		}
	}

	bMeshesCached = true;

	UE_LOG(LogKeepGoingUp, Warning, TEXT("MapLayer[%s] resolved Left=%s Right=%s Road=%s"),
		*GetName(), *GetNameSafe(LeftMesh), *GetNameSafe(RightMesh), *GetNameSafe(RoadMesh));
}

void AMapLayer::AssignMaterialToMesh(UStaticMeshComponent* Mesh, UMaterialInterface* Material) const
{
	if (!Mesh || !Material)
	{
		return;
	}

	Mesh->bDisallowNanite = true;
	const int32 NumMats = FMath::Max(Mesh->GetNumMaterials(), 1);
	for (int32 i = 0; i < NumMats; ++i)
	{
		Mesh->SetMaterial(i, Material);
	}
	Mesh->MarkRenderStateDirty();
}

UMaterialInterface* AMapLayer::ResolveRoadMaterial() const
{
	if (UMaterialInterface* Loaded = RoadMaterial.LoadSynchronous())
	{
		return Loaded;
	}
	return RoadMesh ? RoadMesh->GetMaterial(0) : nullptr;
}

UMaterialInstanceDynamic* AMapLayer::MakeRoadMID(
	UStaticMeshComponent* Mesh,
	UMaterialInterface* SourceMaterial,
	const FLinearColor& Color,
	float Opacity) const
{
	if (!Mesh || !SourceMaterial)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("MapLayer MakeRoadMID failed: Mesh=%s Source=%s"),
			*GetNameSafe(Mesh), *GetNameSafe(SourceMaterial));
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(SourceMaterial, const_cast<AMapLayer*>(this));
	if (!MID)
	{
		return nullptr;
	}

	MID->SetVectorParameterValue(ColorParameterName, Color);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	if (!OpacityParameterName.IsNone())
	{
		MID->SetScalarParameterValue(OpacityParameterName, Opacity);
	}

	AssignMaterialToMesh(Mesh, MID);
	return MID;
}

FLinearColor AMapLayer::GetColorForLayerIndex(int32 InLayerIndex)
{
	static const FLinearColor Bases[7] = {
		FLinearColor(1.f, 0.f, 0.f),
		FLinearColor(1.f, 0.5f, 0.f),
		FLinearColor(1.f, 1.f, 0.f),
		FLinearColor(0.f, 1.f, 0.f),
		FLinearColor(0.f, 1.f, 1.f),
		FLinearColor(0.f, 0.f, 1.f),
		FLinearColor(0.6f, 0.f, 1.f)
	};

	const int32 ZeroBased = FMath::Max(InLayerIndex, 1) - 1;
	const int32 Cycle = ZeroBased % 35;
	const int32 Segment = Cycle / 5;
	const int32 Step = Cycle % 5;
	const int32 NextSegment = (Segment + 1) % 7;
	const float Alpha = static_cast<float>(Step) / 5.f;

	FLinearColor Color = FMath::Lerp(Bases[Segment], Bases[NextSegment], Alpha);
	Color.A = 1.f;
	return Color;
}

void AMapLayer::SetupLayer(int32 InLayerIndex, float InLayerLength)
{
	CacheMeshComponents(true);

	LayerIndex = InLayerIndex;
	LayerLength = InLayerLength;
	ActiveRoadColor = GetColorForLayerIndex(LayerIndex);
	bRoadActive = false;

	ApplyWallColors();
	ApplyRoadColor(false);
}

void AMapLayer::ApplyWallColors()
{
	// 1-based: odd = white material, even = black material (no MID / no parameters)
	const bool bOdd = (LayerIndex % 2) != 0;
	UMaterialInterface* WallMat = bOdd
		? WallWhiteMaterial.LoadSynchronous()
		: WallBlackMaterial.LoadSynchronous();

	if (!WallMat)
	{
		UE_LOG(LogKeepGoingUp, Error,
			TEXT("MapLayer[%s] missing WallWhite/WallBlack. Expected /Game/Game/Materials/WallWhite and WallBlack."),
			*GetName());
		return;
	}

	AssignMaterialToMesh(LeftMesh, WallMat);
	AssignMaterialToMesh(RightMesh, WallMat);

	UE_LOG(LogKeepGoingUp, Warning, TEXT("MapLayer[%s] walls index=%d -> %s (mat=%s) Left=%s Right=%s"),
		*GetName(),
		LayerIndex,
		bOdd ? TEXT("WHITE") : TEXT("BLACK"),
		*GetNameSafe(WallMat),
		*GetNameSafe(LeftMesh),
		*GetNameSafe(RightMesh));
}

void AMapLayer::ApplyRoadColor(bool bActive)
{
	UMaterialInterface* Source = ResolveRoadMaterial();
	const FLinearColor Color = bActive ? ActiveRoadColor : InactiveRoadColor;
	const float Opacity = bActive ? ActiveRoadOpacity : InactiveRoadOpacity;
	RoadMID = MakeRoadMID(RoadMesh, Source, Color, Opacity);
	bRoadActive = bActive;
}

void AMapLayer::UpdateRoadByPlayerLocalX(float PlayerLocalX, float EnterOffset)
{
	const float SegmentStartX = (LayerIndex - 1) * LayerLength;
	const bool bShouldActivate = PlayerLocalX >= (SegmentStartX - EnterOffset);

	if (bShouldActivate != bRoadActive)
	{
		ApplyRoadColor(bShouldActivate);
		if (bShouldActivate)
		{
			PlayRandomPiano();
		}
	}
}

void AMapLayer::PlayRandomPiano()
{
	const int32 Index = FMath::RandRange(1, 8);
	const FString Path = FString::Printf(TEXT("/Game/Game/Audio/piano%d.piano%d"), Index, Index);
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
	{
		float Volume = 1.f;
		if (UWorld* World = GetWorld())
		{
			if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
			{
				Volume = GameMode->GetTileSoundVolume();
			}
		}
		UGameplayStatics::PlaySound2D(this, Sound, Volume);
		return;
	}

	UE_LOG(LogKeepGoingUp, Error, TEXT("MapLayer: missing sound %s"), *Path);
}
