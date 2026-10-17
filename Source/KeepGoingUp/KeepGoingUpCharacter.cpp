// Copyright Epic Games, Inc. All Rights Reserved.

#include "KeepGoingUpCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "KeepGoingUp.h"
#include "Map/DistanceWidget.h"
#include "Map/MapCreater.h"
#include "Map/MapEnd.h"
#include "Map/Car.h"
#include "KeepGoingUpGameMode.h"
#include "Prank/KeepGoingUpPushPrank.h"
#include "UObject/UnrealType.h"

AKeepGoingUpCharacter::AKeepGoingUpCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	PrimaryActorTick.bCanEverTick = true;

	CarHitCameraYaw = -45.f;
	CarHitCameraPitch = -20.f;
	AutoDodgeLookAheadTiles = 30;
}

void AKeepGoingUpCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		WalkSpeed = MoveComp->MaxWalkSpeed;
	}
	bIsSprinting = false;
	ApplyMoveSpeed();

	if (FMath::IsNearlyEqual(CarHitCameraYaw, 135.f) || CarHitCameraYaw > 90.f)
	{
		CarHitCameraYaw = -45.f;
		CarHitCameraPitch = -20.f;
	}

	// Ensure look-ahead isn't stuck on an older BP default
	if (AutoDodgeLookAheadTiles < 30)
	{
		AutoDodgeLookAheadTiles = 30;
	}

	if (CameraBoom)
	{
		DefaultBoomRelativeLocation = CameraBoom->GetRelativeLocation();
		DefaultBoomArmLength = CameraBoom->TargetArmLength;
	}

	CacheDistanceWidget();
	CachedMapCreater = FindMapCreater();
	if (CachedMapCreater)
	{
		const FVector Local = CachedMapCreater->WorldToRoadLocal(GetActorLocation());
		AutoTargetLaneIndex = CachedMapCreater->GetNearestLaneIndex(Local.Y);
	}
	UpdateDistanceUI();

	// GameMode enables gameplay after UI_Start; stay locked until then.
	SetRunGameplayEnabled(false, false);
}

void AKeepGoingUpCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickPendingTeleport(DeltaSeconds);
	TickPushPranks(DeltaSeconds);
	if (!bHeldAtStartPit)
	{
		TryFallIntoStartPit();
		if (!bHeldAtStartPit && !bCarPushLocked && !bPushPrankLockingMove && !bPrankIntroCamera && !IsInEndSequence() && bRunGameplayEnabled)
		{
			// A finished pit / hit can leave the spring arm ignoring the mouse.
			if (CameraBoom && !CameraBoom->bUsePawnControlRotation)
			{
				RestoreGameplayCamera();
			}
			else if (APlayerController* PC = Cast<APlayerController>(GetController()))
			{
				EnableInput(PC);
			}
		}
	}
	else if (!bPushPrankLockingMove && !bStartPitFailing)
	{
		SnapToStartPit();
	}
	UpdatePrankIntroCamera();
	UpdateDistanceUI();
	OrientDistanceWidgetToCamera();
	TickEndSequence(DeltaSeconds);
	TickAutoRun(DeltaSeconds);
}

void AKeepGoingUpCharacter::CacheDistanceWidget()
{
	DistanceWidgetComponent = nullptr;
	DistanceWidget = nullptr;

	TArray<UWidgetComponent*> WidgetComps;
	GetComponents<UWidgetComponent>(WidgetComps);
	for (UWidgetComponent* Comp : WidgetComps)
	{
		if (!Comp)
		{
			continue;
		}

		const FString CompName = Comp->GetName();
		if (Comp->GetFName() == DistanceWidgetComponentName
			|| CompName.StartsWith(DistanceWidgetComponentName.ToString()))
		{
			DistanceWidgetComponent = Comp;
			break;
		}
	}

	if (!DistanceWidgetComponent)
	{
		return;
	}

	DistanceWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	DistanceWidget = Cast<UDistanceWidget>(DistanceWidgetComponent->GetUserWidgetObject());
	if (!DistanceWidget)
	{
		DistanceWidgetComponent->InitWidget();
		DistanceWidget = Cast<UDistanceWidget>(DistanceWidgetComponent->GetUserWidgetObject());
	}
}

AMapCreater* AKeepGoingUpCharacter::FindMapCreater() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AMapCreater::StaticClass(), Found);
	return Found.Num() > 0 ? Cast<AMapCreater>(Found[0]) : nullptr;
}

void AKeepGoingUpCharacter::UpdateDistanceUI()
{
	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (!CachedMapCreater)
	{
		return;
	}

	if (!DistanceWidget)
	{
		CacheDistanceWidget();
		LastDisplayedDistance = INDEX_NONE;
	}
	if (!DistanceWidget)
	{
		return;
	}

	if (bHeldAtStartPit)
	{
		DistanceWidget->ShowPitHp(StartPitHp);
		LastDisplayedDistance = INDEX_NONE;
		return;
	}

	const int32 Distance = CachedMapCreater->GetPlayerLayerIndex();
	if (Distance == LastDisplayedDistance)
	{
		return;
	}

	LastDisplayedDistance = Distance;
	DistanceWidget->SetDistanceDisplay(Distance, CachedMapCreater->GetPlayerLayerColor());
}

void AKeepGoingUpCharacter::OrientDistanceWidgetToCamera()
{
	if (!DistanceWidgetComponent)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector WidgetLocation = DistanceWidgetComponent->GetComponentLocation();
	FVector ToCamera = CameraLocation - WidgetLocation;
	if (ToCamera.IsNearlyZero())
	{
		return;
	}

	DistanceWidgetComponent->SetWorldRotation(ToCamera.Rotation());
}

void AKeepGoingUpCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AKeepGoingUpCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AKeepGoingUpCharacter::DoJumpEnd);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AKeepGoingUpCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AKeepGoingUpCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AKeepGoingUpCharacter::Look);
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}

	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AKeepGoingUpCharacter::SprintStart);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AKeepGoingUpCharacter::SprintStop);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Pressed, this, &AKeepGoingUpCharacter::SprintStart);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Released, this, &AKeepGoingUpCharacter::SprintStop);

	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AKeepGoingUpCharacter::ToggleAutoRun);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AKeepGoingUpCharacter::IncreaseGameSpeed);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AKeepGoingUpCharacter::DecreaseGameSpeed);
}

namespace
{
	constexpr float GameSpeedGears[] = {1.f, 2.f, 3.f};
	constexpr int32 GameSpeedGearCount = UE_ARRAY_COUNT(GameSpeedGears);
}

void AKeepGoingUpCharacter::IncreaseGameSpeed()
{
	if (!bRunGameplayEnabled)
	{
		return;
	}
	SpeedGear = FMath::Min(SpeedGear + 1, GameSpeedGearCount - 1);
	ApplyGameSpeed();
}

void AKeepGoingUpCharacter::DecreaseGameSpeed()
{
	if (!bRunGameplayEnabled)
	{
		return;
	}
	SpeedGear = FMath::Max(SpeedGear - 1, 0);
	ApplyGameSpeed();
}

void AKeepGoingUpCharacter::ApplyGameSpeed() const
{
	const float Scale = GameSpeedGears[FMath::Clamp(SpeedGear, 0, GameSpeedGearCount - 1)];
	UGameplayStatics::SetGlobalTimeDilation(this, Scale);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.2f, FColor::White, FString::Printf(TEXT("x%d"), FMath::RoundToInt(Scale)));
	}
}

void AKeepGoingUpCharacter::ResetGameSpeed()
{
	SpeedGear = 0;
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
}

void AKeepGoingUpCharacter::SetRunGameplayEnabled(bool bEnabled, bool bAllowCamera)
{
	bRunGameplayEnabled = bEnabled;
	bAllowCameraLook = bAllowCamera;

	if (!bRunGameplayEnabled)
	{
		bIsSprinting = false;
		bAutoRunEnabled = false;
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}

	// Always restore walk/sprint speed — never leave MaxWalkSpeed at 0 after menu/finish.
	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::SprintStart()
{
	if (bCarPushLocked || !bRunGameplayEnabled)
	{
		return;
	}
	bIsSprinting = true;
	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::SprintStop()
{
	// Auto-run keeps sprint even if Shift is released
	if (bAutoRunEnabled)
	{
		return;
	}
	bIsSprinting = false;
	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::ApplyMoveSpeed() const
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const bool bSprint = bIsSprinting || bAutoRunEnabled;
		MoveComp->MaxWalkSpeed = bSprint ? SprintSpeed : WalkSpeed;
	}
}

void AKeepGoingUpCharacter::ToggleAutoRun()
{
	if (!bRunGameplayEnabled || IsInEndSequence())
	{
		return;
	}

	// Allow toggling off even while locked; toggling on while locked just arms resume
	bAutoRunEnabled = !bAutoRunEnabled;

	if (bAutoRunEnabled)
	{
		bIsSprinting = true;
		bAutoDodgeCommitted = false;
		if (CachedMapCreater)
		{
			const FVector Local = CachedMapCreater->WorldToRoadLocal(GetActorLocation());
			AutoTargetLaneIndex = CachedMapCreater->GetNearestLaneIndex(Local.Y);
		}
		UE_LOG(LogKeepGoingUp, Log, TEXT("Auto-run ON"));
	}
	else
	{
		bIsSprinting = false;
		bAutoDodgeCommitted = false;
		UE_LOG(LogKeepGoingUp, Log, TEXT("Auto-run OFF"));
	}

	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::BeginEndSequence(AMapEnd* EndActor)
{
	if (!EndActor || EndPhase != EKeepGoingUpEndPhase::None)
	{
		return;
	}

	ActiveMapEnd = EndActor;
	EndPhase = EKeepGoingUpEndPhase::WalkingToEndPos;
	SetIsEndFlag(false);

	// Cancel auto-run / sprint first so EndCarPush won't re-arm sprint.
	if (bAutoRunEnabled)
	{
		UE_LOG(LogKeepGoingUp, Log, TEXT("End sequence: cancelling auto-run"));
	}
	bAutoRunEnabled = false;
	bIsSprinting = false;

	EndCarPush();
	CancelAllPushPranks();
	SetRunGameplayEnabled(false, false);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->MaxWalkSpeed = WalkSpeed;
		MoveComp->bOrientRotationToMovement = true;
	}

	// Full world EndPos (MapCreater tilt included). Do not reuse player Z — on a pitched
	// road that puts the walk target under the mesh.
	EndWalkTarget = EndActor->GetEndPosWorldLocation();

	if (CachedMapCreater)
	{
		CachedMapCreater->SetCarSpawningEnabled(false);
		CachedMapCreater->RecycleAllCars();
	}

	UE_LOG(LogKeepGoingUp, Log, TEXT("End sequence: walking to EndPos"));
}

void AKeepGoingUpCharacter::ResetEndSequence()
{
	EndPhase = EKeepGoingUpEndPhase::None;
	ActiveMapEnd = nullptr;
	EndCameraTurnElapsed = 0.f;
	SetIsEndFlag(false);

	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;
	}
}

void AKeepGoingUpCharacter::TickEndSequence(float DeltaSeconds)
{
	if (EndPhase == EKeepGoingUpEndPhase::None || EndPhase == EKeepGoingUpEndPhase::Complete)
	{
		return;
	}

	if (EndPhase == EKeepGoingUpEndPhase::WalkingToEndPos)
	{
		FVector ToTarget = EndWalkTarget - GetActorLocation();
		ToTarget.Z = 0.f;
		const float DistSq = ToTarget.SizeSquared();
		if (DistSq <= FMath::Square(EndWalkArriveRadius))
		{
			FinishEndWalkAndStartCameraTurn();
			return;
		}

		const FVector Dir = ToTarget.GetSafeNormal();
		AddMovementInput(Dir, 1.f);
		return;
	}

	if (EndPhase == EKeepGoingUpEndPhase::TurningCamera)
	{
		EndCameraTurnElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(EndCameraTurnElapsed / FMath::Max(EndCameraTurnDuration, 0.01f), 0.f, 1.f);
		const float Smooth = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

		if (AController* C = GetController())
		{
			const FQuat NewQuat = FQuat::Slerp(
				EndCameraStartRot.Quaternion(),
				EndCameraTargetRot.Quaternion(),
				Smooth);
			C->SetControlRotation(NewQuat.Rotator());
		}

		if (Alpha >= 1.f)
		{
			CompleteEndSequence();
		}
	}
}

void AKeepGoingUpCharacter::FinishEndWalkAndStartCameraTurn()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}

	// Face toward EndPos on XY (character looks along the walk-in direction)
	if (ActiveMapEnd)
	{
		FVector ToEnd = ActiveMapEnd->GetEndPosWorldLocation() - GetActorLocation();
		ToEnd.Z = 0.f;
		if (!ToEnd.IsNearlyZero())
		{
			SetActorRotation(ToEnd.Rotation());
		}
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		CompleteEndSequence();
		return;
	}

	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;
	}

	EndCameraStartRot = PC->GetControlRotation();
	// Camera in front of the player looking at their face: yaw = character yaw + 180
	EndCameraTargetRot = FRotator(EndCameraPitch, GetActorRotation().Yaw + 180.f, 0.f);
	EndCameraTurnElapsed = 0.f;
	EndPhase = EKeepGoingUpEndPhase::TurningCamera;

	UE_LOG(LogKeepGoingUp, Log, TEXT("End sequence: turning camera to front"));
}

void AKeepGoingUpCharacter::CompleteEndSequence()
{
	EndPhase = EKeepGoingUpEndPhase::Complete;
	SetIsEndFlag(true);

	if (AController* C = GetController())
	{
		C->SetControlRotation(EndCameraTargetRot);
	}

	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GM->NotifyPlayerEndSequenceComplete();
		}
	}

	UE_LOG(LogKeepGoingUp, Log, TEXT("End sequence complete: IsEnd=true, UI_End"));
}

void AKeepGoingUpCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ChuansongActor)
	{
		ChuansongActor->Destroy();
		ChuansongActor = nullptr;
	}
	PendingTeleportKind = 0;
	Super::EndPlay(EndPlayReason);
}

bool AKeepGoingUpCharacter::TeleportToRoadStart()
{
	return BeginChuansongTeleport(false);
}

bool AKeepGoingUpCharacter::TeleportToRoadEnd()
{
	return BeginChuansongTeleport(true);
}

bool AKeepGoingUpCharacter::BeginChuansongTeleport(bool bToRoadEnd)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (bToRoadEnd)
	{
		if (!CachedMapCreater)
		{
			CachedMapCreater = FindMapCreater();
		}
		if (!CachedMapCreater || !CachedMapCreater->GetOrCreateMapEnd())
		{
			UE_LOG(LogKeepGoingUp, Warning, TEXT("TeleportToRoadEnd: MapCreater or MapEnd missing."));
			return false;
		}
	}
	else
	{
		AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>();
		FTransform StartXf;
		if (!GM || !GM->GetPlayerStartTransform(StartXf))
		{
			UE_LOG(LogKeepGoingUp, Warning, TEXT("TeleportToRoadStart: no cached PlayerStart transform."));
			return false;
		}
	}

	if (ChuansongActor)
	{
		ChuansongActor->Destroy();
		ChuansongActor = nullptr;
	}

	if (UClass* VisualClass = LoadClass<AActor>(nullptr, TEXT("/Game/Game/Code/BP_Chuansong.BP_Chuansong_C")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		ChuansongActor = World->SpawnActor<AActor>(VisualClass, GetActorTransform(), Params);
		if (ChuansongActor)
		{
			ChuansongActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			ChuansongActor->SetActorRelativeLocation(FVector::ZeroVector);
			ChuansongActor->SetActorRelativeRotation(FRotator::ZeroRotator);

			TArray<UPrimitiveComponent*> Prims;
			ChuansongActor->GetComponents<UPrimitiveComponent>(Prims);
			for (UPrimitiveComponent* Prim : Prims)
			{
				if (Prim)
				{
					Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogKeepGoingUp, Error, TEXT("Teleport: failed to load BP_Chuansong"));
	}

	PendingTeleportKind = bToRoadEnd ? 2 : 1;
	PendingTeleportLeft = 1.f;
	return true;
}

void AKeepGoingUpCharacter::TickPendingTeleport(float DeltaSeconds)
{
	if (PendingTeleportKind == 0)
	{
		return;
	}

	PendingTeleportLeft -= DeltaSeconds;
	if (PendingTeleportLeft > 0.f)
	{
		return;
	}

	const uint8 Kind = PendingTeleportKind;
	PendingTeleportKind = 0;
	PendingTeleportLeft = 0.f;
	if (ChuansongActor)
	{
		ChuansongActor->Destroy();
		ChuansongActor = nullptr;
	}

	if (Kind == 2)
	{
		ExecuteTeleportToRoadEnd();
	}
	else
	{
		ExecuteTeleportToRoadStart();
	}
}

bool AKeepGoingUpCharacter::ExecuteTeleportToRoadStart()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>();
	if (!GM)
	{
		return false;
	}

	// Preserve auto-run across the teleport (end sequence normally clears it).
	const bool bKeepAutoRun = bAutoRunEnabled;

	ClearStartPit();
	GM->EnsurePlayingForPrank();
	GM->AbortEndCountdownForPrank();
	EndCarPush();
	CancelAllPushPranks();
	ResetEndSequence();

	FTransform StartXf;
	if (!GM->GetPlayerStartTransform(StartXf))
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("TeleportToRoadStart: no cached PlayerStart transform."));
		return false;
	}

	const FVector Loc = StartXf.GetLocation();
	const FRotator Rot = StartXf.GetRotation().Rotator();
	TeleportTo(Loc, Rot, false, true);
	if (AController* C = GetController())
	{
		C->SetControlRotation(Rot);
	}

	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (CachedMapCreater)
	{
		const FVector Local = CachedMapCreater->WorldToRoadLocal(GetActorLocation());
		AutoTargetLaneIndex = CachedMapCreater->GetNearestLaneIndex(Local.Y);
	}

	bAutoRunEnabled = bKeepAutoRun;
	bAutoDodgeCommitted = false;
	bIsSprinting = bKeepAutoRun;
	ApplyMoveSpeed();
	SetRunGameplayEnabled(true, true);

	UE_LOG(LogKeepGoingUp, Log, TEXT("Prank: TeleportToRoadStart (AutoRun=%s)"),
		bKeepAutoRun ? TEXT("keep") : TEXT("off"));
	return true;
}

bool AKeepGoingUpCharacter::ExecuteTeleportToRoadEnd()
{
	UWorld* World = GetWorld();
	AKeepGoingUpGameMode* GM = World ? World->GetAuthGameMode<AKeepGoingUpGameMode>() : nullptr;
	if (GM)
	{
		GM->EnsurePlayingForPrank();
	}

	ClearStartPit();
	CancelAllPushPranks();

	if (IsInEndSequence())
	{
		// Allow re-trigger from PrankUI 测试: cancel current end walk and restart.
		ResetEndSequence();
		if (GM)
		{
			GM->AbortEndCountdownForPrank();
			GM->EnsurePlayingForPrank();
		}
	}

	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (!CachedMapCreater)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("TeleportToRoadEnd: MapCreater missing."));
		return false;
	}

	AMapEnd* MapEnd = CachedMapCreater->GetOrCreateMapEnd();
	if (!MapEnd)
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("TeleportToRoadEnd: MapEnd missing (assign EndClass on MapCreater)."));
		return false;
	}

	MapEnd->ResetEndTrigger();

	// EndPos is on MapEnd attached to MapCreater — world transform already includes the
	// road tilt (e.g. Pitch/Y ≈ 5°). Must use full XYZ; keeping the player's old Z
	// drops them under the slope when TotalLayers is large.
	const FVector EndPos = MapEnd->GetEndPosWorldLocation();
	const FRotator RoadRot = CachedMapCreater->GetActorRotation();
	TeleportTo(EndPos, RoadRot, false, true);
	if (AController* C = GetController())
	{
		C->SetControlRotation(RoadRot);
	}

	MapEnd->NotifyEndSequenceStarted();
	BeginEndSequence(MapEnd);

	UE_LOG(LogKeepGoingUp, Log, TEXT("Prank: TeleportToRoadEnd → EndPos=%s (road tilt applied)"), *EndPos.ToCompactString());
	return true;
}

void AKeepGoingUpCharacter::SetIsEndFlag(bool bValue)
{
	IsEnd = bValue;

	// Also set a Blueprint-only variable named IsEnd / Is_End if present on BP_Player.
	static const FName Names[] = { TEXT("IsEnd"), TEXT("Is_End") };
	for (const FName& Name : Names)
	{
		if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(GetClass(), Name))
		{
			BoolProp->SetPropertyValue_InContainer(this, bValue);
		}
	}
}

void AKeepGoingUpCharacter::StartRainbowCatPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::RainbowCat);
}

void AKeepGoingUpCharacter::StartNinjaTurtlePrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::NinjaTurtle);
}

void AKeepGoingUpCharacter::StartRocketPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::Rocket);
}

void AKeepGoingUpCharacter::StartMissilePrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::Missile);
}

void AKeepGoingUpCharacter::StartZhuTuMengJinPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::ZhuTuMengJin);
}

void AKeepGoingUpCharacter::StartLuoYeZhanPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::LuoYeZhan);
}

void AKeepGoingUpCharacter::StartSaiYaRenPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::SaiYaRen);
}

void AKeepGoingUpCharacter::StartBeiJiTaPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::BeiJiTa);
}

void AKeepGoingUpCharacter::StartNarutoPrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::Naruto);
}

void AKeepGoingUpCharacter::StartSasukePrank()
{
	StartPushPrank(EKeepGoingUpPushPrankKind::Sasuke);
}

void AKeepGoingUpCharacter::StartPushPrank(EKeepGoingUpPushPrankKind Kind)
{
	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GM->EnsurePlayingForPrank();
		}
	}

	if (!GetWorld())
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;
	AKeepGoingUpPushPrank* Prank = GetWorld()->SpawnActor<AKeepGoingUpPushPrank>(
		AKeepGoingUpPushPrank::StaticClass(), GetActorTransform(), Params);
	if (!Prank)
	{
		return;
	}

	Prank->InitPrank(Kind, this);
	ActivePushPranks.Add(Prank);
	UE_LOG(LogKeepGoingUp, Log, TEXT("Prank: kind %d spawned (active=%d)"), static_cast<int32>(Kind), ActivePushPranks.Num());
}

void AKeepGoingUpCharacter::SetHeldPitHp(int32 NewHp)
{
	if (bHeldAtStartPit && !bStartPitFailing)
	{
		StartPitHp = FMath::Max(1, NewHp);
	}
}

void AKeepGoingUpCharacter::ApplyPrankSoundVolume(float Volume)
{
	for (AKeepGoingUpPushPrank* Prank : ActivePushPranks)
	{
		if (Prank)
		{
			Prank->ApplySoundVolume(Volume);
		}
	}
}

void AKeepGoingUpCharacter::CancelAllPushPranks()
{
	for (AKeepGoingUpPushPrank* Prank : ActivePushPranks)
	{
		if (Prank)
		{
			Prank->Recycle();
		}
	}
	ActivePushPranks.Reset();
	bPushPrankLockingMove = false;
	bWasPushStalemate = false;
	UpdatePushPrankCamera(false, 0.f);
	if (bPrankUsingCarHit)
	{
		bPrankUsingCarHit = false;
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->GravityScale = SavedGravityScale;
			MoveComp->bOrientRotationToMovement = bSavedOrientToMovement;
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->StopMovementImmediately();
		}
		EndCarPush();
	}
	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::ClearStartPit()
{
	bHeldAtStartPit = false;
	bStartPitFailing = false;
	StartPitReleaseDelay = 0.f;
	StartPitHp = 10000;
	if (UWorld* World = GetWorld())
	{
		if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			StartPitHp = GameMode->GetStartPitMaxHp();
		}
	}
	LastDisplayedDistance = INDEX_NONE;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (!bPrankUsingCarHit)
		{
			MoveComp->GravityScale = 1.f;
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->StopMovementImmediately();
		}
	}
}

bool AKeepGoingUpCharacter::FindStartPitTransform(FTransform& OutTransform) const
{
	UClass* StartClass = LoadClass<AActor>(nullptr, TEXT("/Game/Game/Code/Scene/BP_Start.BP_Start_C"));
	if (!StartClass)
	{
		return false;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, StartClass, Found);
	for (AActor* StartActor : Found)
	{
		if (!StartActor)
		{
			continue;
		}

		TArray<USceneComponent*> Comps;
		StartActor->GetComponents<USceneComponent>(Comps);
		for (USceneComponent* Comp : Comps)
		{
			if (Comp && Comp->GetName().Contains(TEXT("PlayerPos")))
			{
				OutTransform = Comp->GetComponentTransform();
				return true;
			}
		}
	}
	return false;
}

void AKeepGoingUpCharacter::SnapToStartPit()
{
	FTransform PitTransform;
	if (!FindStartPitTransform(PitTransform))
	{
		return;
	}

	SetActorLocation(PitTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(PitTransform.Rotator());
	ApplyStartPitCamera();

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = 0.f;
		MoveComp->SetMovementMode(MOVE_Flying);
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
	}
}

void AKeepGoingUpCharacter::EnterStartPit()
{
	if (bHeldAtStartPit || bStartPitFailing || IsInEndSequence() || !bRunGameplayEnabled)
	{
		return;
	}

	FTransform PitTransform;
	if (!FindStartPitTransform(PitTransform))
	{
		UE_LOG(LogKeepGoingUp, Warning, TEXT("Start pit: BP_Start/PlayerPos not found."));
		return;
	}

	bHeldAtStartPit = true;
	StartPitReleaseDelay = 0.f;
	StartPitHp = 10000;
	if (UWorld* World = GetWorld())
	{
		if (const AKeepGoingUpGameMode* GameMode = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			StartPitHp = GameMode->GetStartPitMaxHp();
		}
	}
	LastDisplayedDistance = INDEX_NONE;
	// Hit flag drives the car-hit animation. Camera stays fixed; mouse look stays off.
	if (!bCarPushLocked)
	{
		BeginCarPush(nullptr);
	}
	SnapToStartPit();
	UE_LOG(LogKeepGoingUp, Log, TEXT("Start pit: locked to BP_Start/PlayerPos, HP=%d"), StartPitHp);
}

void AKeepGoingUpCharacter::ApplyStartPitCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	// Fixed front-right. Mouse look stays off until the player leaves the pit.
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetRelativeRotation(FRotator(CarHitCameraPitch, -135.f, 0.f));
}

void AKeepGoingUpCharacter::RestoreGameplayCamera()
{
	if (CameraBoom)
	{
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->SetRelativeRotation(FRotator::ZeroRotator);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
		FRotator Look = GetActorRotation();
		Look.Roll = 0.f;
		Look.Pitch = -15.f;
		PC->SetControlRotation(Look);
	}
}

void AKeepGoingUpCharacter::TryFallIntoStartPit()
{
	if (!bRunGameplayEnabled || IsInEndSequence() || bStartPitFailing)
	{
		return;
	}
	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (!CachedMapCreater)
	{
		return;
	}

	// First tile sits at road local X = 0. Jail as soon as world X is before that tile.
	const float FirstTileWorldX = CachedMapCreater->RoadLocalToWorld(FVector::ZeroVector).X;
	if (GetActorLocation().X < FirstTileWorldX)
	{
		EnterStartPit();
	}
}

void AKeepGoingUpCharacter::ReleaseStartPitToFirstTile()
{
	bHeldAtStartPit = false;
	bStartPitFailing = false;
	LastDisplayedDistance = INDEX_NONE;

	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (!CachedMapCreater)
	{
		return;
	}

	FVector Local(80.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			FTransform StartXf;
			if (GM->GetPlayerStartTransform(StartXf))
			{
				const FVector StartLocal = CachedMapCreater->WorldToRoadLocal(StartXf.GetLocation());
				if (StartLocal.X >= 0.f)
				{
					Local = StartLocal;
				}
			}
		}
	}
	Local.X = FMath::Max(Local.X, 80.f);

	const FVector WorldLocation = CachedMapCreater->RoadLocalToWorld(Local);
	const FRotator RoadRotation = CachedMapCreater->GetActorRotation();
	TeleportTo(WorldLocation, RoadRotation, false, true);
	StartPitReleaseDelay = 0.f;

	UE_LOG(LogKeepGoingUp, Log, TEXT("Start pit: no knockback, return to first tile."));
}

void AKeepGoingUpCharacter::FailFromStartPit()
{
	if (bStartPitFailing)
	{
		return;
	}

	bStartPitFailing = true;
	StartPitHp = 0;
	CancelAllPushPranks();
	BeginCarPush(nullptr);
	SnapToStartPit();

	if (UWorld* World = GetWorld())
	{
		if (AKeepGoingUpGameMode* GM = World->GetAuthGameMode<AKeepGoingUpGameMode>())
		{
			GM->NotifyStartPitFailed();
		}
	}
}

void AKeepGoingUpCharacter::TickStartPitHold(bool bHasKnockback, bool bKnockbackWins, float DeltaSeconds)
{
	if (bStartPitFailing)
	{
		SnapToStartPit();
		return;
	}

	if (bHasKnockback)
	{
		StartPitReleaseDelay = 0.f;
		SnapToStartPit();
		if (bKnockbackWins && StartPitHp > 0)
		{
			--StartPitHp;
			if (StartPitHp <= 0)
			{
				FailFromStartPit();
			}
		}
		return;
	}

	SnapToStartPit();
	StartPitReleaseDelay += DeltaSeconds;
	if (StartPitReleaseDelay >= 1.f)
	{
		ReleaseStartPitToFirstTile();
	}
}

void AKeepGoingUpCharacter::TickPushPranks(float DeltaSeconds)
{
	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}

	for (int32 Index = ActivePushPranks.Num() - 1; Index >= 0; --Index)
	{
		AKeepGoingUpPushPrank* Prank = ActivePushPranks[Index];
		if (!Prank || Prank->IsFinished())
		{
			ActivePushPranks.RemoveAtSwap(Index);
			continue;
		}
		Prank->Advance(DeltaSeconds, CachedMapCreater);
		if (Prank->IsFinished())
		{
			ActivePushPranks.RemoveAtSwap(Index);
		}
	}

	// Only the highest rank currently pushing decides the direction.
	// Cat/turtle < rocket/missile < boar/leaf < Goku/Vegeta < Naruto/Sasuke.
	// A lower rank does nothing while a higher one is pushing. Same rank on both sides stalemates.
	int32 BestRank = 0;
	bool bTopForward = false;
	bool bTopBackward = false;
	for (AKeepGoingUpPushPrank* Prank : ActivePushPranks)
	{
		if (!Prank || (!Prank->IsPushingForward() && !Prank->IsPushingBackward()))
		{
			continue;
		}

		int32 Rank = 1;
		switch (Prank->GetKind())
		{
		case EKeepGoingUpPushPrankKind::Rocket:
		case EKeepGoingUpPushPrankKind::Missile:
			Rank = 2;
			break;
		case EKeepGoingUpPushPrankKind::ZhuTuMengJin:
		case EKeepGoingUpPushPrankKind::LuoYeZhan:
			Rank = 3;
			break;
		case EKeepGoingUpPushPrankKind::SaiYaRen:
		case EKeepGoingUpPushPrankKind::BeiJiTa:
			Rank = 4;
			break;
		case EKeepGoingUpPushPrankKind::Naruto:
		case EKeepGoingUpPushPrankKind::Sasuke:
			Rank = 5;
			break;
		default:
			Rank = 1;
			break;
		}

		if (Rank > BestRank)
		{
			BestRank = Rank;
			bTopForward = Prank->IsPushingForward();
			bTopBackward = Prank->IsPushingBackward();
		}
		else if (Rank == BestRank)
		{
			bTopForward |= Prank->IsPushingForward();
			bTopBackward |= Prank->IsPushingBackward();
		}
	}

	const bool bStalemate = bTopForward && bTopBackward;
	const bool bPushing = bTopForward || bTopBackward;
	const int32 ForwardTypes = bTopForward && !bTopBackward ? 1 : 0;
	const int32 BackwardTypes = bTopBackward && !bTopForward ? 1 : 0;
	bPushPrankLockingMove = bPushing;

	if (bHeldAtStartPit)
	{
		TickStartPitHold(bTopBackward, bTopBackward && !bTopForward, DeltaSeconds);
		if (bHeldAtStartPit || bStartPitFailing)
		{
			return;
		}

		// A push that carries the player out of the pit keeps the fixed hit camera.
		if (bPushing)
		{
			bPrankUsingCarHit = false;
			bCarPushLocked = false;
		}
		else
		{
			EndCarPush();
		}
	}

	// Barrage push ignores obstacle cars: don't stay attached, and new overlaps won't capture.
	if (bPushing && CachedMapCreater)
	{
		for (ACar* Car : CachedMapCreater->GetActiveCars())
		{
			if (Car)
			{
				Car->ReleaseCapturedPlayer(this);
			}
		}
	}

	UpdatePushPrankCamera(bStalemate, DeltaSeconds);

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();

	if (!bPushing || IsInEndSequence() || !CachedMapCreater)
	{
		if (bPrankUsingCarHit)
		{
			bPrankUsingCarHit = false;
			bWasPushStalemate = false;
			if (MoveComp)
			{
				MoveComp->GravityScale = SavedGravityScale;
				MoveComp->bOrientRotationToMovement = bSavedOrientToMovement;
				MoveComp->SetMovementMode(MOVE_Walking);
				MoveComp->StopMovementImmediately();
			}
			EndCarPush();
		}
		if (!bPushing)
		{
			ApplyMoveSpeed();
		}
		return;
	}

	// Same state as a car hit: hit pose, locked input, hit camera.
	if (!bPrankUsingCarHit)
	{
		if (MoveComp)
		{
			bSavedOrientToMovement = MoveComp->bOrientRotationToMovement;
			SavedGravityScale = MoveComp->GravityScale;
			MoveComp->bOrientRotationToMovement = false;
			MoveComp->GravityScale = 0.f;
			MoveComp->SetMovementMode(MOVE_Flying);
			MoveComp->StopMovementImmediately();
		}
		BeginCarPush(nullptr);
		bPrankUsingCarHit = true;
	}

	if (!MoveComp)
	{
		return;
	}

	// Car-hit body angle (same as being stuck to a car on the road). Right-rear view is the hit camera.
	// Forced every push frame, including stalemate, so the player's previous yaw is ignored.
	SetActorRotation(CachedMapCreater->GetActorRotation());

	MoveComp->GravityScale = 0.f;
	MoveComp->SetMovementMode(MOVE_Flying);
	MoveComp->StopMovementImmediately();
	MoveComp->Velocity = FVector::ZeroVector;

	if (bStalemate)
	{
		return;
	}

	// Direct displacement along the road. No walking physics, so the slope cannot pull the player down.
	const FVector RoadForward = CachedMapCreater->GetActorTransform().TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
	const float Sign = (ForwardTypes > BackwardTypes) ? 1.f : -1.f;
	const FVector Delta = RoadForward * Sign * (PrankPushSpeed * 3.f) * DeltaSeconds;
	SetActorLocation(GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);

	// Teleport does not fire overlaps, so test cars directly and shatter HotDogCar.
	for (ACar* Car : CachedMapCreater->GetActiveCars())
	{
		if (Car && Car->IsCharacterInside(this))
		{
			Car->ShatterFromPush(GetActorForwardVector());
		}
	}
}

void AKeepGoingUpCharacter::UpdatePrankIntroCamera()
{
	if (!CameraBoom)
	{
		return;
	}

	AKeepGoingUpPushPrank* Focus = nullptr;
	for (AKeepGoingUpPushPrank* Prank : ActivePushPranks)
	{
		if (!Prank || !Prank->WantsIntroCamera() || !Prank->GetVisual())
		{
			continue;
		}
		if (!Focus || Prank->GetSerial() > Focus->GetSerial())
		{
			Focus = Prank;
		}
	}

	if (!Focus)
	{
		if (bPrankIntroCamera)
		{
			bPrankIntroCamera = false;
			CameraBoom->SetAbsolute(false, false, false);
			CameraBoom->SetRelativeLocation(DefaultBoomRelativeLocation);
			CameraBoom->SetRelativeRotation(IntroSavedBoomRelativeRotation);
			CameraBoom->bUsePawnControlRotation = bIntroSavedBoomUsePawnControl;
			CameraBoom->bDoCollisionTest = bIntroSavedBoomDoCollisionTest;
			if (bHeldAtStartPit)
			{
				ApplyStartPitCamera();
			}
			else if (bCarPushLocked || bPrankUsingCarHit)
			{
				CameraBoom->bUsePawnControlRotation = false;
				CameraBoom->SetRelativeRotation(FRotator(CarHitCameraPitch, CarHitCameraYaw, 0.f));
			}
			else if (bIntroSavedBoomUsePawnControl)
			{
				RestoreGameplayCamera();
			}
		}

		UpdateSaiyanPushCameraDistance();
		return;
	}

	if (!bPrankIntroCamera)
	{
		bPrankIntroCamera = true;
		bIntroSavedBoomUsePawnControl = CameraBoom->bUsePawnControlRotation;
		bIntroSavedBoomDoCollisionTest = CameraBoom->bDoCollisionTest;
		IntroSavedBoomRelativeRotation = CameraBoom->GetRelativeRotation();
	}

	// The boom pivot sits on the character. A collision probe that starts inside the mesh
	// shortens the arm and leaves the camera stuck in the body while the animation moves.
	CameraBoom->bDoCollisionTest = false;

	const USceneComponent* VisualRoot = Focus->GetVisual()->GetRootComponent();
	const FVector RelLoc = VisualRoot ? VisualRoot->GetRelativeLocation() : FVector::ZeroVector;
	const FRotator RelRot = VisualRoot ? VisualRoot->GetRelativeRotation() : FRotator::ZeroRotator;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->SetAbsolute(false, false, false);
	CameraBoom->SetRelativeLocation(RelLoc);
	CameraBoom->SetRelativeRotation(RelRot + FRotator(CarHitCameraPitch, Focus->GetIntroCameraYaw(), 0.f));
	UpdateSaiyanPushCameraDistance();
}

void AKeepGoingUpCharacter::UpdateSaiyanPushCameraDistance()
{
	if (!CameraBoom)
	{
		return;
	}

	bool bExtend = false;
	for (const AKeepGoingUpPushPrank* Prank : ActivePushPranks)
	{
		if (Prank && Prank->UsesExtendedPushCamera())
		{
			bExtend = true;
			break;
		}
	}

	// Intro look stays at the default arm length. 1.5x is only the behind-the-player push view.
	CameraBoom->TargetArmLength = (bExtend && !bPrankIntroCamera) ? DefaultBoomArmLength * 1.5f : DefaultBoomArmLength;
}

void AKeepGoingUpCharacter::UpdatePushPrankCamera(bool bStalemate, float DeltaSeconds)
{
	if (!CameraBoom || bPrankIntroCamera)
	{
		return;
	}

	if (bStalemate)
	{
		if (!bStalemateShaking)
		{
			SavedBoomRelativeLocation = DefaultBoomRelativeLocation;
			bStalemateShaking = true;
			StalemateShakeTime = 0.f;
		}

		StalemateShakeTime += DeltaSeconds;
		const float Mag = 6.f;
		const FVector Offset(
			FMath::Sin(StalemateShakeTime * 48.f) * Mag,
			FMath::Cos(StalemateShakeTime * 37.f) * Mag,
			FMath::Sin(StalemateShakeTime * 29.f) * (Mag * 0.45f));
		CameraBoom->SetRelativeLocation(SavedBoomRelativeLocation + Offset);
		return;
	}

	if (bStalemateShaking)
	{
		CameraBoom->SetRelativeLocation(SavedBoomRelativeLocation);
		bStalemateShaking = false;
		StalemateShakeTime = 0.f;
	}
}

void AKeepGoingUpCharacter::TickAutoRun(float DeltaSeconds)
{
	if (!bRunGameplayEnabled || !bAutoRunEnabled || bCarPushLocked || IsInEndSequence() || bPushPrankLockingMove || bHeldAtStartPit)
	{
		return;
	}

	if (!CachedMapCreater)
	{
		CachedMapCreater = FindMapCreater();
	}
	if (!CachedMapCreater)
	{
		return;
	}

	ApplyMoveSpeed();
	UpdateAutoDodgeTargetLane();

	// Move forward along the road (+local X)
	const FVector RoadForward = CachedMapCreater->GetActorTransform().TransformVectorNoScale(FVector::ForwardVector).GetSafeNormal();
	AddMovementInput(RoadForward, 1.f);

	// Strafe toward target lane along local Y (stronger when jumping multiple lanes)
	const FVector PlayerLocal = CachedMapCreater->WorldToRoadLocal(GetActorLocation());
	const int32 CurrentLane = CachedMapCreater->GetNearestLaneIndex(PlayerLocal.Y);
	const float TargetY = CachedMapCreater->GetLaneLocalY(AutoTargetLaneIndex);
	const float DeltaY = TargetY - PlayerLocal.Y;
	if (FMath::Abs(DeltaY) > AutoLaneArriveThreshold)
	{
		const int32 LaneJump = FMath::Abs(AutoTargetLaneIndex - CurrentLane);
		const float Boost = 1.f + AutoDodgeMultiLaneBoost * static_cast<float>(FMath::Max(LaneJump - 1, 0));
		const FVector RoadRight = CachedMapCreater->GetActorTransform().TransformVectorNoScale(FVector::RightVector).GetSafeNormal();
		const float Lateral = FMath::Clamp(DeltaY / 200.f, -1.f, 1.f) * AutoDodgeLateralStrength * Boost;
		AddMovementInput(RoadRight, Lateral);
		bAutoDodgeCommitted = true;
	}
	else
	{
		bAutoDodgeCommitted = false;
	}
}

void AKeepGoingUpCharacter::UpdateAutoDodgeTargetLane()
{
	if (!CachedMapCreater)
	{
		return;
	}

	const FVector PlayerLocal = CachedMapCreater->WorldToRoadLocal(GetActorLocation());
	const int32 CurrentLane = CachedMapCreater->GetNearestLaneIndex(PlayerLocal.Y);
	const float Len = CachedMapCreater->GetLayerLength();
	const float ThreatDist = static_cast<float>(AutoDodgeThreatTiles) * Len;

	// Use the player's real Y — off-center / between-lane cars still count as threats.
	const float CurrentClearance = GetPathClearanceAhead(PlayerLocal.Y, PlayerLocal.X);
	const bool bCurrentThreatened = CurrentClearance < ThreatDist;

	// Stay committed mid-dodge unless the target lane center itself becomes dangerous.
	if (bAutoDodgeCommitted && AutoTargetLaneIndex != CurrentLane)
	{
		const float TargetY = CachedMapCreater->GetLaneLocalY(AutoTargetLaneIndex);
		const float TargetClearance = GetPathClearanceAhead(TargetY, PlayerLocal.X);
		if (TargetClearance >= ThreatDist || !bCurrentThreatened)
		{
			return;
		}
	}

	AutoTargetLaneIndex = FindSafeLaneIndex(CurrentLane, PlayerLocal.X, PlayerLocal.Y);
}

float AKeepGoingUpCharacter::GetPathClearanceAhead(float PathLocalY, float PlayerLocalX) const
{
	if (!CachedMapCreater)
	{
		return TNumericLimits<float>::Max();
	}

	const float LookAhead = static_cast<float>(AutoDodgeLookAheadTiles) * CachedMapCreater->GetLayerLength();
	const float HitHalf = FMath::Max(AutoDodgeHitHalfWidth, 10.f);
	float BestDx = TNumericLimits<float>::Max();

	for (ACar* Car : CachedMapCreater->GetActiveCars())
	{
		if (!Car || !Car->IsActive())
		{
			continue;
		}

		const float Dx = Car->GetRoadLocalX() - PlayerLocalX;
		if (Dx <= 0.f || Dx > LookAhead)
		{
			continue;
		}

		// Geometric overlap on Y — not strict lane-index equality.
		if (FMath::Abs(Car->GetRoadLocalY() - PathLocalY) > HitHalf)
		{
			continue;
		}

		BestDx = FMath::Min(BestDx, Dx);
	}

	return BestDx;
}

int32 AKeepGoingUpCharacter::FindSafeLaneIndex(int32 PreferredLane, float PlayerLocalX, float PlayerLocalY) const
{
	if (!CachedMapCreater)
	{
		return PreferredLane;
	}

	const float Len = CachedMapCreater->GetLayerLength();
	const float ThreatDist = static_cast<float>(AutoDodgeThreatTiles) * Len;
	const int32 LaneCount = CachedMapCreater->GetLaneCount();

	// Threat against where the player actually is (handles between-lane / misaligned cars).
	const float PreferredClearance = GetPathClearanceAhead(PlayerLocalY, PlayerLocalX);
	if (PreferredClearance >= ThreatDist)
	{
		return PreferredLane;
	}

	// Prefer the *nearest* lane that is already clear enough (Clearance >= ThreatDist).
	// Do not jump to a far lane just because it has more open distance
	// (e.g. cars on 1/2/4 → dodge to 3, not all the way to 5).
	int32 BestSafeLane = INDEX_NONE;
	int32 BestSafeDist = TNumericLimits<int32>::Max();
	float BestSafeClearance = -1.f;

	int32 BestAnyLane = PreferredLane;
	float BestAnyClearance = GetPathClearanceAhead(CachedMapCreater->GetLaneLocalY(PreferredLane), PlayerLocalX);
	int32 BestAnyDist = 0;

	for (int32 Lane = 0; Lane < LaneCount; ++Lane)
	{
		const float LaneY = CachedMapCreater->GetLaneLocalY(Lane);
		const float Clearance = GetPathClearanceAhead(LaneY, PlayerLocalX);
		const int32 LaneDist = FMath::Abs(Lane - PreferredLane);

		if (Clearance >= ThreatDist)
		{
			if (LaneDist < BestSafeDist
				|| (LaneDist == BestSafeDist && Clearance > BestSafeClearance + 1.f))
			{
				BestSafeLane = Lane;
				BestSafeDist = LaneDist;
				BestSafeClearance = Clearance;
			}
		}

		if (Clearance > BestAnyClearance + 1.f
			|| (FMath::Abs(Clearance - BestAnyClearance) <= Len * 0.5f && LaneDist < BestAnyDist))
		{
			BestAnyLane = Lane;
			BestAnyClearance = Clearance;
			BestAnyDist = LaneDist;
		}
	}

	return (BestSafeLane != INDEX_NONE) ? BestSafeLane : BestAnyLane;
}

void AKeepGoingUpCharacter::BeginCarPush(ACar* /*Car*/)
{
	if (bCarPushLocked)
	{
		return;
	}

	bCarPushLocked = true;
	bIsSprinting = false;

	if (CameraBoom)
	{
		bSavedBoomUsePawnControlRotation = CameraBoom->bUsePawnControlRotation;
		SavedBoomRelativeRotation = CameraBoom->GetRelativeRotation();
		CameraBoom->bUsePawnControlRotation = false;
		CameraBoom->SetRelativeRotation(FRotator(CarHitCameraPitch, CarHitCameraYaw, 0.f));
	}

	DisableInput(Cast<APlayerController>(GetController()));
}

void AKeepGoingUpCharacter::EndCarPush()
{
	if (!bCarPushLocked)
	{
		return;
	}

	bCarPushLocked = false;
	RestoreGameplayCamera();

	// Auto-run stays armed across hits and prank pushes, and starts moving again here.
	if (bAutoRunEnabled && !bHeldAtStartPit)
	{
		bIsSprinting = true;
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->bOrientRotationToMovement = true;
		}
	}

	ApplyMoveSpeed();
}

void AKeepGoingUpCharacter::Move(const FInputActionValue& Value)
{
	if (!bRunGameplayEnabled || bCarPushLocked || bAutoRunEnabled || IsInEndSequence() || bPushPrankLockingMove || bHeldAtStartPit)
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AKeepGoingUpCharacter::Look(const FInputActionValue& Value)
{
	if (bHeldAtStartPit || bCarPushLocked || IsInEndSequence())
	{
		return;
	}
	if (!bRunGameplayEnabled && !bAllowCameraLook)
	{
		return;
	}

	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AKeepGoingUpCharacter::DoMove(float Right, float Forward)
{
	if (!bRunGameplayEnabled || bCarPushLocked || bAutoRunEnabled || IsInEndSequence() || bPushPrankLockingMove || bHeldAtStartPit || GetController() == nullptr)
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void AKeepGoingUpCharacter::DoLook(float Yaw, float Pitch)
{
	if (bHeldAtStartPit || bCarPushLocked || IsInEndSequence() || GetController() == nullptr)
	{
		return;
	}
	if (!bRunGameplayEnabled && !bAllowCameraLook)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AKeepGoingUpCharacter::DoJumpStart()
{
	if (!bRunGameplayEnabled || bCarPushLocked || IsInEndSequence())
	{
		return;
	}
	Jump();
}

void AKeepGoingUpCharacter::DoJumpEnd()
{
	if (!bRunGameplayEnabled || bCarPushLocked || IsInEndSequence())
	{
		return;
	}
	StopJumping();
}
