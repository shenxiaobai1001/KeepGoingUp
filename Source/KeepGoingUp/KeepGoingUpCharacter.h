// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "KeepGoingUpCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWidgetComponent;
class UDistanceWidget;
class AMapCreater;
class AMapEnd;
class ACar;
class AKeepGoingUpPushPrank;
enum class EKeepGoingUpPushPrankKind : uint8;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EKeepGoingUpEndPhase : uint8
{
	None,
	WalkingToEndPos,
	TurningCamera,
	Complete
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Third person character with sprint, car-hit lock, and Q auto-run / lane dodge.
 */
UCLASS(abstract)
class AKeepGoingUpCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category="Distance UI")
	FName DistanceWidgetComponentName = TEXT("ui_distance");

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> DistanceWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDistanceWidget> DistanceWidget;

	UPROPERTY(Transient)
	TObjectPtr<AMapCreater> CachedMapCreater;

	int32 LastDisplayedDistance = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 900.f;

	bool bIsSprinting = false;

	bool bCarPushLocked = false;
	bool bRunGameplayEnabled = false;
	bool bAllowCameraLook = false;
	bool bSavedBoomUsePawnControlRotation = true;
	FRotator SavedBoomRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Car Hit")
	float CarHitCameraYaw = -45.f;

	UPROPERTY(EditAnywhere, Category = "Car Hit")
	float CarHitCameraPitch = -20.f;

	/** Q toggles this. Stays on across car hits; only Q turns it off. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Auto Run")
	bool bAutoRunEnabled = false;

	/** How far ahead (tiles) to scan cars when scoring lane safety. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "1"))
	int32 AutoDodgeLookAheadTiles = 30;

	/**
	 * If the nearest car ahead on the current lane is closer than this (tiles), dodge.
	 * Destination lanes are scored by how far their nearest car is (farther = safer).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "1"))
	int32 AutoDodgeThreatTiles = 18;

	/** Prefer destinations whose nearest car is at least this far (tiles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "1"))
	int32 AutoDodgeSafeClearanceTiles = 25;

	/** Lateral move strength while dodging between lanes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "0.1"))
	float AutoDodgeLateralStrength = 1.35f;

	/** Extra lateral strength per extra lane when jumping multiple lanes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "0.0"))
	float AutoDodgeMultiLaneBoost = 0.4f;

	/** Consider arrived at target lane within this local-Y distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "1.0"))
	float AutoLaneArriveThreshold = 40.f;

	/**
	 * Cars within this local-Y distance of a path are treated as on a collision course
	 * (lane index alone is not enough — cars/player often sit slightly off-center).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Run", meta = (ClampMin = "10.0"))
	float AutoDodgeHitHalfWidth = 120.f;

	int32 AutoTargetLaneIndex = 0;
	bool bAutoDodgeCommitted = false;

	/** Driven by end sequence; AnimBP can read this for the victory pose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Sequence")
	bool IsEnd = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Sequence", meta = (ClampMin = "1.0"))
	float EndWalkArriveRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Sequence", meta = (ClampMin = "0.1"))
	float EndCameraTurnDuration = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "End Sequence")
	float EndCameraPitch = -12.f;

	EKeepGoingUpEndPhase EndPhase = EKeepGoingUpEndPhase::None;

	UPROPERTY(Transient)
	TObjectPtr<AMapEnd> ActiveMapEnd;

	FVector EndWalkTarget = FVector::ZeroVector;
	FRotator EndCameraStartRot = FRotator::ZeroRotator;
	FRotator EndCameraTargetRot = FRotator::ZeroRotator;
	float EndCameraTurnElapsed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prank|Push", meta = (ClampMin = "100.0"))
	float PrankPushSpeed = 1100.f;

	UPROPERTY()
	TArray<TObjectPtr<AKeepGoingUpPushPrank>> ActivePushPranks;

	bool bPushPrankLockingMove = false;
	bool bPrankUsingCarHit = false;
	float SavedGravityScale = 1.f;
	bool bWasPushStalemate = false;
	bool bSavedOrientToMovement = true;
	FRotator StalemateFaceRotation = FRotator::ZeroRotator;
	bool bStalemateShaking = false;
	float StalemateShakeTime = 0.f;
	FVector SavedBoomRelativeLocation = FVector::ZeroVector;

	bool bPrankIntroCamera = false;
	bool bIntroSavedBoomUsePawnControl = true;
	bool bIntroSavedBoomDoCollisionTest = true;
	FRotator IntroSavedBoomRelativeRotation = FRotator::ZeroRotator;
	FVector DefaultBoomRelativeLocation = FVector::ZeroVector;
	float DefaultBoomArmLength = 400.f;

	/** Pushed off the road start and locked to BP_Start/PlayerPos. */
	/** 0 = none, 1 = road start, 2 = road end. BP_Chuansong follows for one second first. */
	uint8 PendingTeleportKind = 0;
	float PendingTeleportLeft = 0.f;

	UPROPERTY()
	TObjectPtr<AActor> ChuansongActor;

	bool bHeldAtStartPit = false;
	bool bStartPitFailing = false;
	int32 StartPitHp = 10000;

public:

	AKeepGoingUpCharacter();

	UFUNCTION(BlueprintPure, Category = "Car Hit")
	bool IsCarPushLocked() const { return bCarPushLocked; }

	UFUNCTION(BlueprintPure, Category = "Auto Run")
	bool IsAutoRunEnabled() const { return bAutoRunEnabled; }

	UFUNCTION(BlueprintPure, Category = "Game Flow")
	bool IsRunGameplayEnabled() const { return bRunGameplayEnabled; }

	UFUNCTION(BlueprintPure, Category = "End Sequence")
	bool IsInEndSequence() const { return EndPhase != EKeepGoingUpEndPhase::None; }

	/**
	 * Menu / finish lock.
	 * bAllowCamera: true during finish (look only); false during start menu.
	 */
	void SetRunGameplayEnabled(bool bEnabled, bool bAllowCamera);

	/** Called by BP_End / MapEnd when the player steps onto the finish tile. */
	void BeginEndSequence(AMapEnd* EndActor);

	/** Clear end-sequence state when returning to the start menu. */
	void ResetEndSequence();

	/**
	 * Prank: teleport to the road start. Keeps auto-run if it was on.
	 * If UI_End countdown is active, closes it and resumes Playing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	bool TeleportToRoadStart();

	/**
	 * Prank: teleport to the real finish EndPos (TotalLayers + MapCreater tilt),
	 * then run the normal end sequence → UI_End countdown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	bool TeleportToRoadEnd();

	/** Spawn another BP_Cat. Stacks; does not recycle existing cats. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartRainbowCatPrank();

	/** Spawn another BP_TMNT. Stacks; does not recycle existing turtles. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartNinjaTurtlePrank();

	/** Spawn another BP_Rocket five meters behind. Stacks with other forward pushes. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartRocketPrank();

	/** Spawn another BP_DaoDan five meters ahead. Stacks with other backward pushes. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartMissilePrank();

	/** Spawn another BP_ZhuTuMengJin five meters behind. Pushes forward for 8 seconds. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartZhuTuMengJinPrank();

	/** Spawn another BPLuoYeZhan five meters ahead. Pushes backward for 8 seconds. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartLuoYeZhanPrank();

	/** Spawn BP_Saiyaren five meters behind. Pushes forward for 10 seconds after its animation. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartSaiYaRenPrank();

	/** Spawn BP_BeiJiTa five meters ahead. Pushes backward for 10 seconds after its animation. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartBeiJiTaPrank();

	/** Spawn BP_liudao five meters behind. Pushes forward for 12 seconds. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartNarutoPrank();

	/** Spawn BP_ZuoZhu five meters ahead. Pushes backward for 12 seconds. */
	UFUNCTION(BlueprintCallable, Category = "Prank")
	void StartSasukePrank();

	void CancelAllPushPranks();

	/** Leave the BP_Start hold (menu, new run, or a forward push). */
	void ClearStartPit();

	/** Obstacle car reached the road start while carrying this player. */
	void EnterStartPit();

	bool IsHeldAtStartPit() const { return bHeldAtStartPit; }

	/** True while any cat/turtle is in the pushing phase (including stalemate). */
	bool IsPushPrankLockingMove() const { return bPushPrankLockingMove; }

	void BeginCarPush(ACar* Car);
	void EndCarPush();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void CacheDistanceWidget();
	void UpdateDistanceUI();
	void OrientDistanceWidgetToCamera();
	AMapCreater* FindMapCreater() const;

	void SprintStart();
	void SprintStop();
	void ApplyMoveSpeed() const;

	void ToggleAutoRun();
	void TickAutoRun(float DeltaSeconds);
	void UpdateAutoDodgeTargetLane();
	int32 FindSafeLaneIndex(int32 PreferredLane, float PlayerLocalX, float PlayerLocalY) const;
	/** Distance (uu) to nearest car ahead that overlaps PathLocalY; Max if none. */
	float GetPathClearanceAhead(float PathLocalY, float PlayerLocalX) const;

	void TickPushPranks(float DeltaSeconds);
	void UpdatePushPrankCamera(bool bStalemate, float DeltaSeconds);
	void UpdatePrankIntroCamera();
	void UpdateSaiyanPushCameraDistance();
	void TryFallIntoStartPit();
	void ReleaseStartPitToFirstTile();
	void SnapToStartPit();
	void ApplyStartPitCamera();
	void RestoreGameplayCamera();
	bool FindStartPitTransform(FTransform& OutTransform) const;
	void FailFromStartPit();
	void StartPushPrank(EKeepGoingUpPushPrankKind Kind);
	void TickStartPitHold(int32 ForwardTypes, int32 BackwardTypes);
	void TickEndSequence(float DeltaSeconds);
	void TickPendingTeleport(float DeltaSeconds);
	bool BeginChuansongTeleport(bool bToRoadEnd);
	bool ExecuteTeleportToRoadStart();
	bool ExecuteTeleportToRoadEnd();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void FinishEndWalkAndStartCameraTurn();
	void CompleteEndSequence();
	void SetIsEndFlag(bool bValue);

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

public:

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
