// Copyright Epic Games, Inc. All Rights Reserved.

#include "Prank/KeepGoingUpPushPrank.h"
#include "KeepGoingUpCharacter.h"
#include "KeepGoingUpGameMode.h"
#include "KeepGoingUp.h"
#include "Map/MapCreater.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/ChildActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float SpawnDistanceUU = 500.f;
	constexpr float ApproachSpeed = 2800.f;
	constexpr float CatchDistance = 80.f;
	constexpr float TurtlePrepSeconds = 0.8f;
	constexpr float CatTurtlePushSeconds = 3.f;
	constexpr float RocketMissilePushSeconds = 5.f;
	constexpr float BoarLeafPushSeconds = 8.f;
	constexpr float SaiyanPushSeconds = 10.f;
	constexpr float NarutoSasukePushSeconds = 12.f;

	struct FPushPrankProfile
	{
		const TCHAR* ClassPath = nullptr;
		const TCHAR* LogName = nullptr;
		/** Player local +X. Negative is behind the player. */
		float SpawnAlongRoad = 0.f;
		bool bFlyToPlayer = false;
		/** Stay on the spawn offset for the whole push. Does not fly onto the player. */
		bool bStayAtOffset = false;
		bool bPushesForward = true;
		bool bPlaySpawnAnim = false;
		/** Play the skeletal mesh, keep the beam off, then open it and start pushing. */
		bool bOpenBeamAfterAnim = false;
		float IntroCameraYaw = 0.f;
		float PushSeconds = CatTurtlePushSeconds;
		const TCHAR* OneShotSound = nullptr;
		const TCHAR* LoopSoundPath = nullptr;
	};

	const FPushPrankProfile& ProfileFor(EKeepGoingUpPushPrankKind Kind)
	{
		static const FPushPrankProfile Cat{
			TEXT("/Game/Game/Code/Barrage/BP_Cat.BP_Cat_C"), TEXT("RainbowCat"),
			-SpawnDistanceUU, true, false, true, false, false, 0.f, CatTurtlePushSeconds,
			TEXT("/Game/Game/Audio/彩虹猫.彩虹猫"), nullptr};
		static const FPushPrankProfile Turtle{
			TEXT("/Game/Game/Code/Barrage/BP_TMNT.BP_TMNT_C"), TEXT("NinjaTurtle"),
			SpawnDistanceUU, true, false, false, true, false, 0.f, CatTurtlePushSeconds,
			TEXT("/Game/Game/Audio/忍者神龟.忍者神龟"), nullptr};
		static const FPushPrankProfile Rocket{
			TEXT("/Game/Game/Code/Barrage/BP_Rocket.BP_Rocket_C"), TEXT("Rocket"),
			-SpawnDistanceUU, true, false, true, false, false, 0.f, RocketMissilePushSeconds,
			nullptr, TEXT("/Game/Game/Audio/火箭.火箭")};
		static const FPushPrankProfile Missile{
			TEXT("/Game/Game/Code/Barrage/BP_DaoDan.BP_DaoDan_C"), TEXT("Missile"),
			SpawnDistanceUU, true, false, false, false, false, 0.f, RocketMissilePushSeconds,
			nullptr, TEXT("/Game/Game/Audio/导弹.导弹")};
		static const FPushPrankProfile Boar{
			TEXT("/Game/Game/Code/Barrage/BP_ZhuTuMengJin.BP_ZhuTuMengJin_C"), TEXT("ZhuTuMengJin"),
			-SpawnDistanceUU, true, false, true, false, false, 0.f, BoarLeafPushSeconds,
			TEXT("/Game/Game/Audio/猪突猛进.猪突猛进"), TEXT("/Game/Game/Audio/剪刀.剪刀")};
		static const FPushPrankProfile Leaf{
			TEXT("/Game/Game/Code/Barrage/BPLuoYeZhan.BPLuoYeZhan_C"), TEXT("LuoYeZhan"),
			SpawnDistanceUU, true, false, false, false, false, 0.f, BoarLeafPushSeconds,
			nullptr, nullptr};
		static const FPushPrankProfile Saiyan{
			TEXT("/Game/Game/Code/Barrage/BP_Saiyaren.BP_Saiyaren_C"), TEXT("SaiYaRen"),
			-SpawnDistanceUU, false, true, true, false, true, -135.f, SaiyanPushSeconds,
			TEXT("/Game/Game/Audio/孙悟空.孙悟空"), TEXT("/Game/Game/Audio/孙悟空循环.孙悟空循环")};
		static const FPushPrankProfile Vegeta{
			TEXT("/Game/Game/Code/Barrage/BP_BeiJiTa.BP_BeiJiTa_C"), TEXT("BeiJiTa"),
			SpawnDistanceUU, false, true, false, false, true, -45.f, SaiyanPushSeconds,
			TEXT("/Game/Game/Audio/贝吉塔.贝吉塔"), TEXT("/Game/Game/Audio/贝吉塔循环.贝吉塔循环")};
		static const FPushPrankProfile Naruto{
			TEXT("/Game/Game/Code/Barrage/BP_liudao.BP_liudao_C"), TEXT("Naruto"),
			-SpawnDistanceUU, true, false, true, false, false, 0.f, NarutoSasukePushSeconds,
			TEXT("/Game/Game/Audio/鸣人.鸣人"), TEXT("/Game/Game/Audio/鸣人循环.鸣人循环")};
		static const FPushPrankProfile Sasuke{
			TEXT("/Game/Game/Code/Barrage/BP_ZuoZhu.BP_ZuoZhu_C"), TEXT("Sasuke"),
			SpawnDistanceUU, true, false, false, false, false, 0.f, NarutoSasukePushSeconds,
			nullptr, nullptr};
		switch (Kind)
		{
		case EKeepGoingUpPushPrankKind::NinjaTurtle: return Turtle;
		case EKeepGoingUpPushPrankKind::Rocket: return Rocket;
		case EKeepGoingUpPushPrankKind::Missile: return Missile;
		case EKeepGoingUpPushPrankKind::ZhuTuMengJin: return Boar;
		case EKeepGoingUpPushPrankKind::LuoYeZhan: return Leaf;
		case EKeepGoingUpPushPrankKind::SaiYaRen: return Saiyan;
		case EKeepGoingUpPushPrankKind::BeiJiTa: return Vegeta;
		case EKeepGoingUpPushPrankKind::Naruto: return Naruto;
		case EKeepGoingUpPushPrankKind::Sasuke: return Sasuke;
		default: return Cat;
		}
	}
}

AKeepGoingUpPushPrank::AKeepGoingUpPushPrank()
{
	PrimaryActorTick.bCanEverTick = false;
	PushTimeLeft = CatTurtlePushSeconds;
}

void AKeepGoingUpPushPrank::InitPrank(EKeepGoingUpPushPrankKind InKind, AKeepGoingUpCharacter* InTarget)
{
	Kind = InKind;
	Target = InTarget;
	Phase = EKeepGoingUpPushPrankPhase::Approach;
	ApproachElapsed = 0.f;
	PushTimeLeft = ProfileFor(Kind).PushSeconds;
	bBeamOpened = false;
	static int32 NextSerial = 1;
	Serial = NextSerial++;
	StartPrankSound();
}

bool AKeepGoingUpPushPrank::WantsIntroCamera() const
{
	return ProfileFor(Kind).bOpenBeamAfterAnim && Phase == EKeepGoingUpPushPrankPhase::Approach && Visual != nullptr;
}

bool AKeepGoingUpPushPrank::UsesExtendedPushCamera() const
{
	return ProfileFor(Kind).bOpenBeamAfterAnim && Phase == EKeepGoingUpPushPrankPhase::Pushing;
}

float AKeepGoingUpPushPrank::GetIntroCameraYaw() const
{
	return ProfileFor(Kind).IntroCameraYaw;
}

bool AKeepGoingUpPushPrank::IsPushingForward() const
{
	return Phase == EKeepGoingUpPushPrankPhase::Pushing && ProfileFor(Kind).bPushesForward;
}

bool AKeepGoingUpPushPrank::IsPushingBackward() const
{
	return Phase == EKeepGoingUpPushPrankPhase::Pushing && !ProfileFor(Kind).bPushesForward;
}

void AKeepGoingUpPushPrank::Advance(float DeltaSeconds, AMapCreater* MapCreater)
{
	if (Phase == EKeepGoingUpPushPrankPhase::Finished || !Target || !MapCreater)
	{
		return;
	}

	if (!bVisualSpawned)
	{
		SpawnVisual(MapCreater);
	}
	else if (Visual && (Kind == EKeepGoingUpPushPrankKind::Naruto || Kind == EKeepGoingUpPushPrankKind::Sasuke))
	{
		// Those two blueprints call PlayAnimation with looping on.
		StopVisualAnimLoop(Visual);
	}

	const FPushPrankProfile& Profile = ProfileFor(Kind);

	if (Phase == EKeepGoingUpPushPrankPhase::Approach)
	{
		if (!bAttachedToPlayer)
		{
			AttachVisualAtLocalOffset(Profile.SpawnAlongRoad);
		}

		if (Profile.bOpenBeamAfterAnim)
		{
			ApproachElapsed += DeltaSeconds;
			if (!bBeamOpened && IsIntroAnimFinished())
			{
				SetBeamActive(true);
				bBeamOpened = true;
				Phase = EKeepGoingUpPushPrankPhase::Pushing;
				UE_LOG(LogKeepGoingUp, Log, TEXT("%s: anim done, beam on, start pushing"), Profile.LogName);
			}
		}
		else if (Profile.bFlyToPlayer && Visual)
		{
			FVector Rel = Visual->GetRootComponent() ? Visual->GetRootComponent()->GetRelativeLocation() : FVector(Profile.SpawnAlongRoad, 0.f, 0.f);
			const float Dist = FMath::Abs(Rel.X);
			if (Dist <= CatchDistance)
			{
				SnapVisualToPlayer();
				Phase = EKeepGoingUpPushPrankPhase::Pushing;
				UE_LOG(LogKeepGoingUp, Log, TEXT("%s: start pushing"), Profile.LogName);
			}
			else
			{
				const float Step = FMath::Min(ApproachSpeed * DeltaSeconds, Dist);
				Rel.X -= FMath::Sign(Rel.X) * Step;
				Visual->SetActorRelativeLocation(Rel);
			}
		}
		else
		{
			ApproachElapsed += DeltaSeconds;
			if (ApproachElapsed >= TurtlePrepSeconds)
			{
				SnapVisualToPlayer();
				Phase = EKeepGoingUpPushPrankPhase::Pushing;
				UE_LOG(LogKeepGoingUp, Log, TEXT("%s: start pushing"), Profile.LogName);
			}
		}
		return;
	}

	if (Phase == EKeepGoingUpPushPrankPhase::Pushing)
	{
		if (!bAttachedToPlayer)
		{
			AttachVisualAtLocalOffset(Profile.bStayAtOffset ? Profile.SpawnAlongRoad : 0.f);
		}
		else if (!Profile.bStayAtOffset)
		{
			SnapVisualToPlayer();
		}

		PushTimeLeft -= DeltaSeconds;
		if (PushTimeLeft <= 0.f)
		{
			UE_LOG(LogKeepGoingUp, Log, TEXT("%s: push time over, recycle"), ProfileFor(Kind).LogName);
			Recycle();
		}
	}
}

void AKeepGoingUpPushPrank::SpawnVisual(AMapCreater* MapCreater)
{
	bVisualSpawned = true;
	if (!GetWorld() || !Target || !MapCreater)
	{
		return;
	}

	const FPushPrankProfile& Profile = ProfileFor(Kind);
	const TCHAR* ClassPath = Profile.ClassPath;

	UClass* VisualClass = LoadClass<AActor>(nullptr, ClassPath);
	if (!VisualClass)
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("PushPrank: failed to load %s"), ClassPath);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	Visual = GetWorld()->SpawnActor<AActor>(VisualClass, Target->GetActorTransform(), Params);
	if (!Visual)
	{
		return;
	}

	DisableVisualCollision(Visual);
	StopVisualAnimLoop(Visual);
	AttachVisualAtLocalOffset(Profile.SpawnAlongRoad);
	if (Profile.bPlaySpawnAnim)
	{
		PlayTurtleIntro();
	}
	if (Profile.bOpenBeamAfterAnim)
	{
		RaiseEffectSortPriority(Visual);
		SetBeamActive(false);
	}
	ApplyVisualAnimation(Visual);
}

void AKeepGoingUpPushPrank::PlayTurtleIntro()
{
	if (!Visual)
	{
		return;
	}

	UAnimSequence* Anim = LoadObject<UAnimSequence>(
		nullptr,
		TEXT("/Game/Game/Resourcess/RenZheShenGui/RenZheShenGui_Anim.RenZheShenGui_Anim"));

	TArray<UChildActorComponent*> ChildActors;
	Visual->GetComponents<UChildActorComponent>(ChildActors);
	for (UChildActorComponent* ChildComp : ChildActors)
	{
		if (ChildComp && !ChildComp->GetChildActor())
		{
			ChildComp->CreateChildActor();
		}
		AActor* Child = ChildComp ? ChildComp->GetChildActor() : nullptr;
		if (!Child)
		{
			continue;
		}

		TArray<USkeletalMeshComponent*> Meshes;
		Child->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh)
			{
				continue;
			}
			if (Anim)
			{
				Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
				Mesh->PlayAnimation(Anim, false);
			}
			Mesh->SetPosition(0.f, false);
			Mesh->bPauseAnims = false;
		}
	}
}

void AKeepGoingUpPushPrank::AttachVisualAtLocalOffset(float AlongPlayerX)
{
	if (!Visual || !Target)
	{
		return;
	}

	if (!bAttachedToPlayer)
	{
		Visual->AttachToActor(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		bAttachedToPlayer = true;
	}

	Visual->SetActorRelativeLocation(FVector(AlongPlayerX, 0.f, 0.f));
	Visual->SetActorRelativeRotation(FRotator::ZeroRotator);
}

void AKeepGoingUpPushPrank::SnapVisualToPlayer()
{
	AttachVisualAtLocalOffset(0.f);
}

void AKeepGoingUpPushPrank::SetBeamActive(bool bActive) const
{
	if (!Visual)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Visual->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (!Component || !Component->GetName().Contains(TEXT("lazzer"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (USceneComponent* Scene = Cast<USceneComponent>(Component))
		{
			Scene->SetHiddenInGame(!bActive);
			Scene->SetVisibility(bActive, true);
		}
		if (UParticleSystemComponent* Particles = Cast<UParticleSystemComponent>(Component))
		{
			if (bActive)
			{
				Particles->Activate(true);
			}
			else
			{
				Particles->Deactivate();
			}
		}
	}

	if (bActive)
	{
		RaiseEffectSortPriority(Visual);
	}
}

void AKeepGoingUpPushPrank::RaiseEffectSortPriority(AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Prims;
	Actor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (!Prim || Prim->IsA<USkeletalMeshComponent>())
		{
			continue;
		}
		Prim->TranslucencySortPriority = 1;
		Prim->MarkRenderStateDirty();
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (AActor* Child = ChildComp ? ChildComp->GetChildActor() : nullptr)
		{
			RaiseEffectSortPriority(Child);
		}
	}
}

bool AKeepGoingUpPushPrank::IsIntroAnimFinished() const
{
	if (!Visual)
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Visual->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance();
		if (!Node)
		{
			continue;
		}

		Node->SetLooping(false);
		const float Length = Node->GetLength();
		const float Time = Node->GetCurrentTime();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			return ApproachElapsed >= 0.2f;
		}
		// The first frames can report "not playing" before the single-node starts.
		if (Time <= 0.02f)
		{
			return false;
		}
		return Time >= Length - 0.05f;
	}

	return ApproachElapsed >= 8.f;
}

void AKeepGoingUpPushPrank::ApplyVisualAnimation(AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	const bool bLoop = Kind == EKeepGoingUpPushPrankKind::ZhuTuMengJin || Kind == EKeepGoingUpPushPrankKind::LuoYeZhan;
	const bool bPlayOnce = Kind == EKeepGoingUpPushPrankKind::SaiYaRen;
	if (!bLoop && !bPlayOnce)
	{
		if (Kind == EKeepGoingUpPushPrankKind::Naruto || Kind == EKeepGoingUpPushPrankKind::Sasuke)
		{
			StopVisualAnimLoop(Actor);
		}
		return;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		UAnimationAsset* Anim = Mesh->AnimationData.AnimToPlay;
		if (!Anim && bPlayOnce)
		{
			Anim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Game/Resourcess/SaiYaRen/SaiYaRen_Anim.SaiYaRen_Anim"));
		}
		else if (!Anim && Kind == EKeepGoingUpPushPrankKind::ZhuTuMengJin)
		{
			Anim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Game/Resourcess/ZhuTuMengJin/ZhuTuMengJin_Anim.ZhuTuMengJin_Anim"));
		}
		else if (!Anim && Kind == EKeepGoingUpPushPrankKind::LuoYeZhan)
		{
			Anim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Game/Resourcess/LuoYeZhan/LuoYeZhan_Anim.LuoYeZhan_Anim"));
		}
		if (!Anim)
		{
			continue;
		}

		Mesh->PlayAnimation(Anim, bLoop);
		Mesh->AnimationData.bSavedLooping = bLoop;
		Mesh->AnimationData.bSavedPlaying = true;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (AActor* Child = ChildComp ? ChildComp->GetChildActor() : nullptr)
		{
			ApplyVisualAnimation(Child);
		}
	}
}

void AKeepGoingUpPushPrank::StopVisualAnimLoop(AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Actor->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}

		// Blueprint PlayAnimation often passes Looping=true and overrides the component checkbox.
		Mesh->AnimationData.bSavedLooping = false;
		if (UAnimSingleNodeInstance* Node = Mesh->GetSingleNodeInstance())
		{
			Node->SetLooping(false);
		}
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (AActor* Child = ChildComp ? ChildComp->GetChildActor() : nullptr)
		{
			StopVisualAnimLoop(Child);
		}
	}
}

void AKeepGoingUpPushPrank::DisableVisualCollision(AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Prims;
	Actor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim)
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);
	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (AActor* Child = ChildComp ? ChildComp->GetChildActor() : nullptr)
		{
			DisableVisualCollision(Child);
		}
	}
}

void AKeepGoingUpPushPrank::StartPrankSound()
{
	const FPushPrankProfile& Profile = ProfileFor(Kind);
	if (Profile.LoopSoundPath)
	{
		LoopSound = LoadObject<USoundBase>(nullptr, Profile.LoopSoundPath);
		if (!LoopSound)
		{
			UE_LOG(LogKeepGoingUp, Error, TEXT("PushPrank: missing sound %s"), Profile.LoopSoundPath);
		}
	}

	if (Profile.OneShotSound)
	{
		if (USoundBase* OneShot = LoadObject<USoundBase>(nullptr, Profile.OneShotSound))
		{
			PlayPrankSound(OneShot, false);
			return;
		}
		UE_LOG(LogKeepGoingUp, Error, TEXT("PushPrank: missing sound %s"), Profile.OneShotSound);
	}

	if (LoopSound)
	{
		PlayPrankSound(LoopSound, true);
	}
}

void AKeepGoingUpPushPrank::PlayPrankSound(USoundBase* Sound, bool bLoop)
{
	if (!Sound || Phase == EKeepGoingUpPushPrankPhase::Finished)
	{
		return;
	}

	StopPrankSound();
	bLoopingSound = bLoop;
	float Volume = 1.f;
	if (UWorld* World = GetWorld())
	{
		if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			Volume = GameMode->GetPrankSoundVolume();
		}
	}
	PrankSound = UGameplayStatics::SpawnSound2D(this, Sound, Volume, 1.f, 0.f, nullptr, false, false);
	if (PrankSound)
	{
		PrankSound->OnAudioFinished.AddUniqueDynamic(this, &AKeepGoingUpPushPrank::HandlePrankSoundFinished);
	}
}

void AKeepGoingUpPushPrank::ApplySoundVolume(float Volume)
{
	if (PrankSound)
	{
		PrankSound->SetVolumeMultiplier(FMath::Clamp(Volume, 0.f, 1.f));
	}
}

void AKeepGoingUpPushPrank::StopPrankSound()
{
	bLoopingSound = false;
	if (!PrankSound)
	{
		return;
	}

	PrankSound->OnAudioFinished.RemoveDynamic(this, &AKeepGoingUpPushPrank::HandlePrankSoundFinished);
	PrankSound->Stop();
	PrankSound = nullptr;
}

void AKeepGoingUpPushPrank::HandlePrankSoundFinished()
{
	PrankSound = nullptr;
	if (Phase == EKeepGoingUpPushPrankPhase::Finished)
	{
		bLoopingSound = false;
		return;
	}

	if (bLoopingSound && LoopSound)
	{
		PlayPrankSound(LoopSound, true);
		return;
	}

	if (LoopSound)
	{
		PlayPrankSound(LoopSound, true);
	}
}

void AKeepGoingUpPushPrank::Recycle()
{
	Phase = EKeepGoingUpPushPrankPhase::Finished;
	StopPrankSound();
	LoopSound = nullptr;
	if (Visual)
	{
		Visual->Destroy();
		Visual = nullptr;
	}
	Destroy();
}
