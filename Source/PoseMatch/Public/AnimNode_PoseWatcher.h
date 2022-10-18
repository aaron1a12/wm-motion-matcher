// Copyright Wild Montage, LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimCurveTypes.h"
#include "BonePose.h"
#include "Animation/AnimNodeBase.h"
#include "MotionData.h"
#include "AnimNode_PoseWatcher.generated.h"


USTRUCT(BlueprintInternalUseOnly)
struct POSEMATCH_API FAnimNode_PoseWatcher : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links)
	FPoseLink Pose;

private:
	FAnimInstanceProxy* AnimProxy;

	// Bones to save
	TArray<FBoneReference> BoneWatchList;

	// This is the latest bone data (positions + velocites)
	TArray<FMMatcherBoneData> BoneData;

	// Used to delay caching until we've added bones to the watch list.
	void CacheBonesNow();

public:
	FAnimNode_PoseWatcher();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

public:
	// Add bones to the watch list. Use only once.
	void SetupBones(TArray<FBoneReference>& bonesToWatch);

	// Get the latest bone data.
	TArray<FMMatcherBoneData>& GetBoneData();
};
