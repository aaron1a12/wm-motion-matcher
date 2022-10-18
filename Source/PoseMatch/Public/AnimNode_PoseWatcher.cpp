// Copyright Wild Montage, LLC. All Rights Reserved.

#include "AnimNode_PoseWatcher.h"
#include "Animation/AnimInstanceProxy.h"

FAnimNode_PoseWatcher::FAnimNode_PoseWatcher()
{
}

void FAnimNode_PoseWatcher::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	FAnimNode_Base::Initialize_AnyThread(Context);	
	Pose.Initialize(Context);

	AnimProxy = Context.AnimInstanceProxy;

	BoneWatchList.Empty();
	BoneData.Empty();
}

void FAnimNode_PoseWatcher::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread);

	Pose.CacheBones(Context);
	FAnimNode_Base::CacheBones_AnyThread(Context);

	CacheBonesNow();
}

void FAnimNode_PoseWatcher::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);

	FScopedAnimNodeTracker Tracked = Context.TrackAncestor(this);

	Pose.Update(Context);
}

void FAnimNode_PoseWatcher::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	Pose.Evaluate(Output);

	// avoid race condition
	if (BoneWatchList.Num() != BoneData.Num())
		return;

	// Component space
	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);

	const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();

	for (int32 i = 0; i != BoneWatchList.Num(); ++i)
	{
		FBoneReference& boneRef = BoneWatchList[i];
		FMMatcherBoneData& boneData = BoneData[i];

		if (boneRef.IsValidToEvaluate(BoneContainer))
		{
			auto boneT = CSPose.GetComponentSpaceTransform(boneRef.GetCompactPoseIndex(BoneContainer));
			FVector newBonePos = boneT.GetLocation();

			boneData.Velocity = (newBonePos - boneData.Position) / AnimProxy->GetDeltaSeconds();
			boneData.Position = newBonePos;
		}
	}
}

void FAnimNode_PoseWatcher::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData);

	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine);
	Pose.GatherDebugData(DebugData);
}

void FAnimNode_PoseWatcher::CacheBonesNow()
{
	if (!AnimProxy)	return;

	FBoneContainer& BoneContainer = AnimProxy->GetRequiredBones();

	for (FBoneReference& boneRef : BoneWatchList)
	{
		boneRef.Initialize(BoneContainer);
	}
}

void FAnimNode_PoseWatcher::SetupBones(TArray<FBoneReference>& bonesToWatch)
{
	BoneWatchList = bonesToWatch;
	BoneData.AddDefaulted(BoneWatchList.Num());

	CacheBonesNow();
}

TArray<FMMatcherBoneData>& FAnimNode_PoseWatcher::GetBoneData()
{
	return BoneData;
}
