#pragma once


#include "Core.h"

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "MotionData.generated.h"

#define MOTION_MATCHING_INTERVAL  0.03f
#define MOTION_MATCHING_BLEND_TIME 0.4f

typedef TArray<float> FFeatureVector;
typedef TArray<FTrajectoryPoint> FTrajectory;

USTRUCT(BlueprintType)
struct POSEMATCH_API FTrajectoryPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	float Facing = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float TimeOffset = 0.f;
};

/**
 * Bone data for pose matching
 */
USTRUCT(BlueprintType)
struct POSEMATCH_API FMMatcherBoneData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position;

	UPROPERTY()
	FVector Velocity;
};

/**
 * Bone data for pose matching
 */
USTRUCT(BlueprintType)
struct POSEMATCH_API FFeatureNormalData
{
	GENERATED_BODY()

	UPROPERTY()
	float Count = 0.f;

	UPROPERTY()
	float Sum = 0.f;

	UPROPERTY()
	float Mean = 0.f;

	UPROPERTY()
	float Variance = 0.f;

	UPROPERTY()
	float StandardDeviation = 0.f;

	// Used to speed up 1.0f / StD operations
	UPROPERTY()
	float StdInverse = 0.f;

	// Custom scaling. (Makes feature matter more/less.)
	UPROPERTY()
	float UserWeight = 1.f;

public:
	// Reset everything to zero.
	void SetDefaultValues();
};

/**
 * Data for a single pose frozen in time. Contains past/future information.
 */
USTRUCT(BlueprintType) //BlueprintInternalUseOnly?
struct POSEMATCH_API FMMatcherPoseSample
{
	GENERATED_BODY()

	/** Unique identifier. */
	UPROPERTY()
	int32 Id;

	/** State identifier. */
	UPROPERTY()
	int32 StateId;

	/** Time within the animation sequence. */
	UPROPERTY()
	float Time;

	/** Select bone data for pose matching. */
	UPROPERTY()
	TArray<FMMatcherBoneData> BoneData;

	/** Velocity of the root bone. In cm/s. */
	UPROPERTY()
	FVector RootVelocity;

	/** How fast is the root rotating. In degrees/s. */
	UPROPERTY()
	float RootRotationSpeed;

	/** Positional and facing information over time. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FTrajectoryPoint> Trajectory;

	/* DEPRECATED */
	UPROPERTY()
	uint8 bLFootLock : 1;

	UPROPERTY()
	uint8 bRFootLock : 1;

	UPROPERTY()
	TArray<uint8> FootLocks = {1, 1};

	/** TLOU2-inspired facing axis of the spine. */
	UPROPERTY()
	FVector FacingAxis;

	/*
	An array of floats, each representing a feature axis in a high-dimensional data space.
	To match a pose, simply compare the squared euclidean distance between two feature vectors.
	Should specific features in the vector matter more than others, you can multiply them by a weight?
	
	These floats should be normalized to assist in weight balancing.

	["Data-Driven Responsive Control of Physics-Based Characters", SIGGRAPH Asia 2019]
	*/
	UPROPERTY()
	TArray<float> FeatureVector;
};


/**
 * Single animation or "state" that you would place in a traditional state machine. (e.g., idle, start walking, walking, etc.)
 */
USTRUCT(BlueprintType)
struct POSEMATCH_API FMMatcherState
{
	GENERATED_BODY()

	/** Unique identifier. */
	UPROPERTY()
	int32 Id;

	/** */
	UPROPERTY(EditAnywhere)
	UAnimSequence* Animation;

	/** Is this a moving loop or idle animation? */
	UPROPERTY(EditAnywhere)
	uint8 bLoop : 1;

	/** Is this a transition to a stop/idle? Incompatble with "Loop." */
	UPROPERTY(EditAnywhere, DisplayName = "Stopping Animation")
	uint8 bStopping : 1;

	/** These cached poses are what get matched against. */
	UPROPERTY()
	TArray<FMMatcherPoseSample> CachedPoses;

	/** Blend times for particular state pairs. Enter state id and blend time in seconds. */
	UPROPERTY(EditAnywhere)
	TMap<int32, float> CustomBlendTimes;
};


/**
 * Motion Data Asset.
 * This file contains everything you need to motion match, including settings and an animation set.
 */
UCLASS(BlueprintType)
class POSEMATCH_API UMotionData : public UObject, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:

	UMotionData(const FObjectInitializer& ObjectInitializer);
	virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError) override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;

	UPROPERTY(BlueprintInternalUseOnly)
	bool bOutdatedMotionCache = false;
#endif

private:
	UPROPERTY(AssetRegistrySearchable)
	TSubclassOf<UMotionData> NativeClass;

public:
	/** Intended Skeleton. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	USkeleton* TargetSkeleton;

	/** Basically our data resolution, measured in seconds. Value should be larger than (1/framerate)
	* Example: 0.044, approx. every frame. Ubisoft uses the largest recommended value, 0.10 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	float MotionCacheSamplingRate = 0.044;

	/** Time spent blending between animations, measured in seconds.
	* Custom blend times for specific state pairs may be entered in the appropriate state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BlendTime = 0.250f;

	/** Time offsets to place trajectory points that will be used when caching poses and motion matching. Measured in seconds.
	*
	* Example setup:
	* -0.25
	* 0.25
	* 0.5
	* 0.75
	* 1.0
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<float> TrajectoryTimings = { -0.25f, 0.25f, 0.5f, 0.75f, 1.0f };

	/** When present pose vs future candidate pose matching, which bones should we consider.
	* If you edit this list or change its order you must rebuild the cache. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FBoneReference> PoseMatchingBones;

	/** Foot IK Bone. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference LeftFoot;

	/** Index in the "Pose Matching Bones" list. If the right foot is the first bone in the list, then enter 0.*/
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference RightFoot;

	/** Virtual bone that receives the foot placement vector for foot locking based on the playing animation clip.
	* Left foot. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference LeftFootLockReceiver;

	/** Virtual bone that receives the foot placement vector for foot locking based on the playing animation clip.
	* Right foot. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference RightFootLockReceiver;

	/** For foot locking purposes. Typically the pelvis bone. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference CenterOfMassBone;

	//
	// Weights and biases. 
	//

	UPROPERTY(EditAnywhere, Category = "Settings")
	float RootVelocityWeight = 1.0f;

	//UPROPERTY(EditAnywhere, Category = "Settings")
	//float RootRotationalVelocityWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float BonePositionWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float BoneVelocityWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float TrajectoryWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<float> TrajectoryWeights = { 0.5f, 0.5f, 0.5f, 0.5f, 1.0f };

	UPROPERTY(EditAnywhere, Category = "Settings")
	float TrajectoryFacingWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<float> TrajectoryFacingWeights = { 1.0f, 2.0f, 2.0f, 1.5f, 1.0f };

	/**	How much do we want to keep playing the same animation and frame. ["Motion Matching in The Last of Us Part II", GDC 2021] */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float NaturalBias = 1.0f;

	/** How much to prefer loops when input is steady. ["Motion Matching in The Last of Us Part II", GDC 2021] */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float LoopBias = 1.0f;

	/** How much to prefer stopping animations when stopping. ["Motion Matching in The Last of Us Part II", GDC 2021] */
	UPROPERTY(EditAnywhere, Category = "Settings")
	float StoppingBias = 1.0f;

	/** Animations or "states" that you would place in a traditional state machine. (e.g., idle, start walking, walking, etc.) 
	* After adding new animations here, you need to use the "Rebuild Motion Cache" action for it to be used properly by the system. */
	UPROPERTY(EditAnywhere, Category = "Animation Set")
	TArray<FMMatcherState> States;

public:

	/* For normalization. */

	UPROPERTY()
	FFeatureNormalData NormalData_RootVelocity;

	UPROPERTY()
	FFeatureNormalData NormalData_RootRotationSpeed;

	UPROPERTY()
	TArray<FFeatureNormalData> NormalData_BonePosition;

	UPROPERTY()
	TArray<FFeatureNormalData> NormalData_BoneVelocity;

	UPROPERTY()
	TArray<FFeatureNormalData> NormalData_Trajectory; // TODO: DEPRECATE

	UPROPERTY()
	TArray<FFeatureNormalData> NormalData_TrajectoryPosition;

	UPROPERTY()
	TArray<FFeatureNormalData> NormalData_TrajectoryFacing;

	/* Normalizes a value with provided normalization data. */
	void NormalizeFeature(float& featureValue, FFeatureNormalData& normalData);
	void NormalizeFeature(FVector& featureValue, FFeatureNormalData& normalData);

	/* Normalizes a desired trajectory with cached normalization values so it can be part of our feature query vector. */
	void NormalizeTrajectory(TArray<FTrajectoryPoint>& trajectory);

	void UnnormalizeFeature(float& featureValue, FFeatureNormalData& normalData);
	void UnnormalizeFeature(FVector& featureValue, FFeatureNormalData& normalData);

	/* Unnormalizes a trajectory to it's original raw values. */
	void UnnormalizeTrajectory(TArray<FTrajectoryPoint>& trajectory);

	void UnnormalizeBoneData(TArray<FMMatcherBoneData>& boneData);
};