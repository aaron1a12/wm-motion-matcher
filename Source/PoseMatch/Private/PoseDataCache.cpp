#include "PoseDataCache.h"

#include "Math/UnrealMathUtility.h"

UPoseDataCache::UPoseDataCache(const FObjectInitializer& ObjectInitializer)
{
	//poseCurveData.UpdateOrAddKey(FVector(0.f, 0.f, 0.f), 0.f);
}

void UPoseDataCache::SetInfo(TArray<FName> boneArray, int32 numFrames, float length)
{
	frames = numFrames;
	sequenceLength = length;

	Bones.Reserve(boneArray.Num());

	for (auto& boneName : boneArray)
	{
		Bones.Emplace(boneName, FPoseBoneData());
	}
}

void UPoseDataCache::SetKey(FName boneName, float time, FVector translation, FVector rotation)
{
	FPoseBoneData* boneDat = Bones.Find(boneName);

	if (boneDat)
	{
		boneDat->Translation.UpdateOrAddKey(translation, time);
		boneDat->Rotation.UpdateOrAddKey(rotation, time);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("COULD NOT FIND BONE."));
	}
}

int32 UPoseDataCache::GetNumFrames()
{
	return frames;
}

int32 UPoseDataCache::GetSequenceLength()
{
	return sequenceLength;
}

float UPoseDataCache::GetTimeAtFrame(const int32 frame)
{
	const float FrameTime = frames > 1 ? sequenceLength / (float)(frames - 1) : 0.0f;
	return FMath::Clamp(FrameTime * frame, 0.0f, sequenceLength);
}

FVector UPoseDataCache::GetVectorValue(float Time)
{
	return FVector(0.f, 0.f, 0.f);
	//return poseCurveData.Evaluate(Time, 1.0f);
}

/*FPoseBoneData UPoseDataCache::GetBoneData(FName boneName)
{
	//return TSharedPtr<FPoseBoneData>();
	auto boneDat = Bones.Find(boneName);

	return *NewObject<FPoseBoneData>();

	//return boneDat;
}*/

/*FPoseBoneData UPoseDataCache::GetBoneData(FName boneName)
{
	auto boneDat = Bones.Find(boneName);

	if (!boneDat)
	{
		return *NewObject<FPoseBoneData>();
	}
	else
	{
		return *boneDat;
	}
}*/

/*FPoseBoneData& UPoseDataCache::GetBoneData(FName boneName)
{
	auto boneDat = Bones.Find(boneName);

	if (!boneDat)
	{
		return *NewObject<FPoseBoneData>();
	}
	else
	{
		return *boneDat;
	}
}*/

// Why we have to include this symbol in the editor build I have no idea.
#if WITH_EDITORONLY_DATA
FVector FVectorCurve::Evaluate(float CurrentTime, float BlendWeight) const
{
	FVector Value;

	Value.X = FloatCurves[(int32)EIndex::X].Eval(CurrentTime) * BlendWeight;
	Value.Y = FloatCurves[(int32)EIndex::Y].Eval(CurrentTime) * BlendWeight;
	Value.Z = FloatCurves[(int32)EIndex::Z].Eval(CurrentTime) * BlendWeight;

	return Value;
}
#endif