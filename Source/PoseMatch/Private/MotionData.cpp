#include "MotionData.h"

UMotionData::UMotionData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NativeClass = GetClass();
}

USkeleton* UMotionData::GetSkeleton(bool& bInvalidSkeletonIsError)
{
	bInvalidSkeletonIsError = false;

	return TargetSkeleton;
}

#if WITH_EDITOR
void UMotionData::PreEditChange(FProperty* PropertyAboutToChange)
{
	//PropertyAboutToChange->GetNameCPP()
	bOutdatedMotionCache = true;
}
#endif


void UMotionData::NormalizeFeature(float& featureValue, FFeatureNormalData& normalData)
{
	featureValue = (featureValue - normalData.Mean) * normalData.StdInverse * normalData.UserWeight;
}

void UMotionData::NormalizeFeature(FVector& featureValue, FFeatureNormalData& normalData)
{
	featureValue = (featureValue - normalData.Mean) * normalData.StdInverse * normalData.UserWeight;
}

void UMotionData::NormalizeTrajectory(TArray<FTrajectoryPoint>& trajectory)
{
	for (int32 i = 0; i != TrajectoryTimings.Num(); ++i)
	{
		FFeatureNormalData& fNormalPos = NormalData_TrajectoryPosition[i];
		FFeatureNormalData& fNormalFacing = NormalData_TrajectoryFacing[i];
		
		auto& tP = trajectory[i];

		NormalizeFeature(tP.Position, fNormalPos);
		NormalizeFeature(tP.Facing, fNormalFacing);
	}
}

void UMotionData::UnnormalizeFeature(float& featureValue, FFeatureNormalData& normalData)
{
	featureValue = normalData.StandardDeviation * (featureValue / normalData.UserWeight) + normalData.Mean;
}

void UMotionData::UnnormalizeFeature(FVector& featureValue, FFeatureNormalData& normalData)
{
	featureValue = normalData.StandardDeviation * (featureValue / normalData.UserWeight) + normalData.Mean;
}

void UMotionData::UnnormalizeTrajectory(TArray<FTrajectoryPoint>& trajectory)
{
	for (int32 i = 0; i != TrajectoryTimings.Num(); ++i)
	{
		FFeatureNormalData& fNormalPos = NormalData_TrajectoryPosition[i];
		FFeatureNormalData& fNormalFacing = NormalData_TrajectoryFacing[i];

		auto& tP = trajectory[i];

		UnnormalizeFeature(tP.Position, fNormalPos);
		UnnormalizeFeature(tP.Facing, fNormalFacing);
	}
}
void UMotionData::UnnormalizeBoneData(TArray<FMMatcherBoneData>& boneData)
{
	float posWeightScale = 1.0f / BonePositionWeight;
	float velWeightScale = 1.0f / BoneVelocityWeight;

	for (int32 i = 0; i != boneData.Num(); ++i)
	{
		FMMatcherBoneData& bone = boneData[i];

		bone.Position = FVector(
			(NormalData_BonePosition[i].StandardDeviation * (bone.Position.X * posWeightScale) + NormalData_BonePosition[i].Mean),
			(NormalData_BonePosition[i].StandardDeviation * (bone.Position.Y * posWeightScale) + NormalData_BonePosition[i].Mean),
			(NormalData_BonePosition[i].StandardDeviation * (bone.Position.Z * posWeightScale) + NormalData_BonePosition[i].Mean)
		);

		bone.Velocity = FVector(
			(NormalData_BoneVelocity[i].StandardDeviation * (bone.Velocity.X * velWeightScale) + NormalData_BoneVelocity[i].Mean),
			(NormalData_BoneVelocity[i].StandardDeviation * (bone.Velocity.Y * velWeightScale) + NormalData_BoneVelocity[i].Mean),
			(NormalData_BoneVelocity[i].StandardDeviation * (bone.Velocity.Z * velWeightScale) + NormalData_BoneVelocity[i].Mean)
		);
	}
}

void FFeatureNormalData::SetDefaultValues()
{
	Count = 0.f;
	Sum = 0.f;
	Mean = 0.f;
	Variance = 0.f;
	StandardDeviation = 0.f;
	StdInverse = 0.f;
	UserWeight = 1.0f;
}
