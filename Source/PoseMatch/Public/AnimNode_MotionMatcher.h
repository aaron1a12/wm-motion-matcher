// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Animation/AnimNodeBase.h"
#include "MotionData.h"
#include "AnimNode_PoseWatcher.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_Inertialization.h"

#include "AnimNode_MotionMatcher.generated.h"
/**
 *
 */

class UCharacterMovementComponent;
class UMotionMatcherComponent;

class UMotionMatcherInterface;
class UDebugWidget;

USTRUCT(BlueprintType)
struct FMMatcherInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector DesiredVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DesiredFacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Aiming;
};

struct FPastSnapshot
{
	FVector Position;
	FVector Velocity;
	FRotator Rotation;
	float Facing;
	float TimeSince;

	FPastSnapshot() :Position(0), Velocity(0), Rotation(), Facing(0), TimeSince(0) { }
};

/**
 * Matched state along with extra info.
 */
struct FMMatcherStatePlayData
{
	int32 MatchedStateIndex = 0;
	int32 MatchedPoseIndex = 0;
	float MatchedTime = 0.0f;


	// The time at which the animation is currently playing.
	float CurrentPlayTime = 0.0f;
	int32 CurrentPointIndex = 0;

	// Marker tick record for this play through
	FMarkerTickRecord MarkerTickRecord;

	float CurrentTime = 0.0f;
	float PlayRate = 0.0f;

	float Cost = 0.0f;

	float BlendTime = MOTION_MATCHING_BLEND_TIME;
};

struct FSpringPoint
{
	FVector Position = FVector::ZeroVector;
	FVectorSpringState SpringState;
};

struct FInertBlendStates
{
	FAnimNode_Inertialization* Node = nullptr;
	TArray<float> ActiveBlends = {};
};

USTRUCT(BlueprintInternalUseOnly)
struct POSEMATCH_API FAnimNode_MotionMatcher : public FAnimNode_Base
{
	GENERATED_BODY()

	// Not used. TODO: Remove this.
	UPROPERTY()
	FPoseLink CurrentCachedPose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation Set", meta = (NeverAsPin))
	UMotionData* MotionDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (AlwaysAsPin))
	FMMatcherInput Input;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "Interface", meta = (AlwaysAsPin, DisplayName = "Matcher Interface", ToolTip = "Allows data retrieval from this node."))
	UMotionMatcherInterface* MotionMatcherInterface;

	/** This is the name of the skeletal curve that receives the alpha value for foot locking. (Left)*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault, DisplayName = "Left IK Alpha Curve"))
	FName LeftIKAlphaCurveName;

	/** This is the name of the skeletal curve that receives the alpha value for foot locking. (Right)*/
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault, DisplayName = "Right IK Alpha Curve"))
	FName RightIKAlphaCurveName;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	float SpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	float TimeScale = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (PinHiddenByDefault))
	uint8 bDebugMode : 1;

public:
	FAnimNode_MotionMatcher();

	// Fired once, before everything else.
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;

	// This fires when the RequiredBones array changes and cached bones indices have to be refreshed.
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;

	// Tick update
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;

	// Here is where the pose is actually computed and runs parallel.
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;

	// Where you can add data that is shown on-screen when you use "ShowDebug", "ShowDebug Bones" or "ShowDebug Animation" cmds.
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	// Gets the currently running state.
	const int32& GetStateIndex();

private:
	// Searches in the pose database for a pose with a better motion than the currently playing one.
	void MatchNow();

	// Retrieve a pose from a state, interpolating as necessary. Use this to figure out your current trajectory.
	void EvaluatePoseSample(int32 stateIndex, float time, FMMatcherPoseSample& outPoseSample);

	// Helpers for adding dimensions to a euclidean distance calculation.
	#define ATTR_DIST(a, b) (FMath::Square(a - b))
	#define ATTR_DIST_VEC(a, b) ( ATTR_DIST(a.X, b.X) + ATTR_DIST(a.Y, b.Y) + ATTR_DIST(a.Z, b.Z) )

	// Interprets two trajectories as feature vectors in a high-dimensional data space and calculates the squared euclidean distance between them.
	float TrajDistSquared(const FTrajectory& goal, const FTrajectory& candidate);

	// Interprets two pose samples as feature vectors in a high-dimensional data space and calculates the squared euclidean distance between them.
	float PoseDistSquared(const FMMatcherPoseSample& goal, const FMMatcherPoseSample& candidate);

	// Use this function to determine whether our center of mass (or a custom point) is out of the balancing area.
	// If it does, it will return false and you should unlock the nearest foot to the center to regain our balance.
	bool IsPointInBalancingArea(FVector2D point);

	void UpdateFootLock(float dt, UAnimInstance* animInst);



private:
	FAnimNode_SequencePlayer SequencePlayerNode;

	// Used to setup stuff that can't be set up in the initializer.
	bool bFirstUpdate = true;

	// Initial component position within the blueprint viewport scene.
	// Used to fix offsets if the mesh rotated -90 or underground.
	FVector localMeshCompPos;

	// Initial component rotation within the blueprint viewport scene.
	FVector localMeshCompRot;

	FTransform csToWs;

	// Inverted transform that can be used to convert between component space and other spaces. TODO!
	FTransform componentDeltaTransform;

	// Interface class to communicate between this anim node and the anim blueprint
	UMotionMatcherInterface* mmInterface = nullptr;

	// The character movement component so we can override its root motion.
	UCharacterMovementComponent* moveComp = nullptr;

	// List of connected inertialization nodes that we can use for blending between animations.
	TArray<FInertBlendStates> inertializationNodes;

	// Connected anim node that saves our bone pose after inertialization for better pose matching.
	FAnimNode_PoseWatcher* poseWatcher = nullptr;

	// Did we just switched the animation?
	bool bAnimChanged = false;

	// Time warp for candidate error correction
	float currentTimeScaleWarp = 1.0f;

	// Recommended values for optional IK
	float ik_left_alpha = 0.f;
	float ik_right_alpha = 0.f;

	TArray<FBoneReference> footLockReceivers;
	TArray<FBoneReference> footLockFeet;




	//FFootLock currentFootLock;

	// Here we keep a copy of the bones that are used for pose matching.
	// Every bone needs to be initialized with the current LOD bone cache so we
	// have to keep it separate from other instances of this MotionDataAsset.
	TArray<FBoneReference> PoseMatchingBones;

	FVector rootBonePos;
	FVector rootBoneVel;

	FMMatcherStatePlayData currentPlayData;
	FMMatcherStatePlayData nextPlayData;

	// This is the currently evaluated pose from our database.
	// It's what we match against during motion matching.
	FMMatcherPoseSample currentPose;

	// This trajectory is a blend between past and future trajectories (simple decay on velocity), as well as user input.
	// It is constantly up-to-date and used for motion matching.
	TArray<FTrajectoryPoint> desiredTrajectory;

	// This tiny thing is used to quickly remember when is the first future trajectory point in the trajectory timings array.
	int32 firstFutureTrajectoryTiming;

	// Furthest future timing. Typically 1 second into the future.
	float lastTrajectoryTime;

	// Trajectory history. Recorded in realtime.
	TArray<FPastSnapshot> pastHistory;


	float timeSinceLastSave;
	float maxHistoryLength;
	float snapshotAge;
	FVector currentPos;
	FVector previousPos;

	// Furthest point on the desired trajectory.
	FVector desiredVecA = FVector::ZeroVector;
	float desiredFacing = 0.0f;

	FFloatSpringState facingSpringState;

	// 
	// Steady bias - used to bias looping animations when input is steady.
	//

	FVector inputSteady;
	float steadyBias;

	// How sure are we that we are stopping? Used to multiply stopping bias. (0.0f <-> 1.0f)
	float stoppingCertainty = 0.0f;

	FVector avgInputDir;

	FVector lastInputVec;
	FVectorSpringState SpringState;

	FVector lastPosition;
	FRotator lastRotation = FRotator();

	// Current character velocity in worldspace.
	FVector currentRootVelocity;

	FRotator lastRot;

	float yawStep = 0.0f;
	float turnSpeed = 0.f;
	float lastYaw = 0.f;

	AActor* owningActor;

	// The id of the custom root motion source that moves the actor.
	uint16 RootMotionSourceID;


	float timeSinceLastMatch;
	float timeSinceLastBlend = 0.0f;
	float lastBestCost;

	bool bHasValidMotionData = false;

	FRotator rotDiff;
	FTransform rootMotion;

	bool bIsLeftFootLocked = false;
	bool bIsRightFootLocked = false;
	FVector lastLFootLockedPos = FVector::ZeroVector;
	FVector lastRFootLockedPos = FVector::ZeroVector;
	TArray<bool> footLocks = { false, false };
	TArray<FVector> lockedfootPositions = { FVector::ZeroVector, FVector::ZeroVector };

	bool bAdjustingBalance = false;
	int lastUnlockedFoot = -1;
	float timeSinceLock = 0.f;

	float timeStandingStill = 0.f;

	float lerpAlpha = 0.0f;
	bool bReverse = false;

	enum
	{
		TRAJ_MAX = 32,
		TRAJ_SUB = 4,
		PRED_MAX = 4,
		PRED_SUB = 4,
	};

	float trajx_prev[TRAJ_MAX];
	float trajy_prev[TRAJ_MAX];

	float predx[PRED_MAX], predy[PRED_MAX];
	float predxv[PRED_MAX], predyv[PRED_MAX];
	float predxa[PRED_MAX], predya[PRED_MAX];


	float trajx = 0.0f;
	float trajy = 0.0f;
	float trajxv = 0.0f, trajyv = 0.0f;
	float trajxa = 0.0f, trajya = 0.0f;
	float traj_xv_goal = 0.0f;
	float traj_yv_goal = 0.0f;

	float rootRotWarp = 0.f;

	// For IsPointInBalancingArea()
	FBoneReference CenterOfMassBone;
	FTransform CenterOfMassBoneTransform;
	FVector2D centerOfMass;

#if WITH_EDITOR
	UDebugWidget* debugWidget;
	bool bDebugVisible = false;
	void DrawDebug(const FAnimationUpdateContext& context);
	void DrawTrajectory(const FAnimationUpdateContext& context, const TArray<FTrajectoryPoint>& trajectory, FColor color);
	void DrawBoneData(const FAnimationUpdateContext& context, const TArray<FMMatcherBoneData>& bones, FColor color);
	void DrawFootIKThreshold(const FAnimationUpdateContext& context);

	static FORCEINLINE FString f(float TheFloat, int32 Precision = true, bool IncludeLeadingZero = true) {

		//Round to integral if have something like 1.9999 within precision
		float Rounded = roundf(TheFloat);

		if (FMath::Abs(TheFloat - Rounded) < FMath::Pow(10, -1 * Precision))
		{
			TheFloat = Rounded;
		}

		FNumberFormattingOptions NumberFormat; //Text.h
		NumberFormat.MinimumIntegralDigits = (IncludeLeadingZero) ? 1 : 0;
		NumberFormat.MaximumIntegralDigits = 10000;
		NumberFormat.MinimumFractionalDigits = Precision;
		NumberFormat.MaximumFractionalDigits = Precision;

		FString out = FText::AsNumber(TheFloat, &NumberFormat).ToString();

		if (!(TheFloat < 0.0f))
		{
			return FString::Printf(TEXT("\r\r%s"), *out);
		}

		return FText::AsNumber(TheFloat, &NumberFormat).ToString();
	}
#endif
};