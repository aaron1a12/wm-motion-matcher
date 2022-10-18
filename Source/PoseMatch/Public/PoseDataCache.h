#pragma once


#include "Core.h"

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimCurveTypes.h"

#include "Engine/DataTable.h"
#include "PoseDataCache.generated.h"


/*USTRUCT(Blueprintable)
struct FPoseBoneData
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FVectorCurve Translation;

	UPROPERTY()
	FVectorCurve Rotation;
};*/


/**
 * Curve database structure for a single bone. Covers the whole animation.
 */
USTRUCT(BlueprintType)
struct POSEMATCH_API FPoseBoneData
{
	GENERATED_BODY()

	UPROPERTY()
	FVectorCurve Translation;

	UPROPERTY()
	FVectorCurve Rotation;

};

UCLASS(ClassGroup = PoseMatch, Blueprintable)
class POSEMATCH_API UPoseDataCache : public UPrimaryDataAsset
{
	GENERATED_UCLASS_BODY()

	//UPROPERTY(EditAnywhere, DisplayName = "Curve Data", Category = "Pose Data", Meta = (ToolTip = "Tooltp"))
	//FVectorCurve poseCurveData;

	/** Curve database with bone names as keys. Can't use indices since anim modifiers can't access them. */
	UPROPERTY(EditAnywhere, DisplayName = "Curve Data", Category = "Pose Matching", Meta = (ToolTip = "Tooltp"))
	TMap<FName, FPoseBoneData> Bones;

	UPROPERTY(EditAnywhere)
	int32 frames;

	UPROPERTY(EditAnywhere)
	float sequenceLength;

	UFUNCTION(BlueprintCallable, Category = "Pose Matching")
	void SetInfo(TArray<FName> boneArray, int32 numFrames, float length);

	UFUNCTION(BlueprintCallable, Category = "Pose Matching")
	void SetKey(FName boneName, float time, FVector translation, FVector rotation);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pose Matching")
	int32 GetNumFrames();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pose Matching")
	int32 GetSequenceLength();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pose Matching")
	float GetTimeAtFrame(const int32 frame);

	/** DEPRECATED */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pose Matching")
	FVector GetVectorValue(float Time);

	/** NOT WORKING */
	inline const FPoseBoneData& GetBoneData(FName boneName) { return *Bones.Find(boneName); };
};