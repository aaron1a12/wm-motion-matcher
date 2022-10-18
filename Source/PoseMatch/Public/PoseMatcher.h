#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PoseDataCache.h"

#include "PoseMatcher.generated.h"

/**
 * Vector structure used to cache current and previous info from a single bone.
 */
USTRUCT()
struct POSEMATCH_API FSingleBonePose
{
	GENERATED_BODY()
public:

	FSingleBonePose(FVector t = FVector(0.f, 0.f, 0.f), FVector r = FVector(0.f, 0.f, 0.f), FVector v = FVector(0.f, 0.f, 0.f)) : Translation(t), Rotation(r), TranslationalVelocity(v) {}

	UPROPERTY()
	FVector Translation;

	UPROPERTY()
	FVector Rotation;

	UPROPERTY()
	FVector TranslationalVelocity;
};


/**
 * The Pose Matcher Object. Here we actually compute and match the poses.
 */
UCLASS(ClassGroup = PoseMatch, Blueprintable)
class UPoseMatcher
	: public UObject
{
	GENERATED_BODY()

	UPoseMatcher(const class FObjectInitializer& PCIP);

private:
	UAnimInstance* animInst = nullptr;
	USkeletalMeshComponent* skelMesh = nullptr;

	float matchedTime = -1;

	bool bBusySearching = false;

	float currentRoll = 0.f;
	float previousRoll = 0.f;

	// Fast copy of the bones we have to match. (Taken from Settings)
	TSet<FName> boneNames;


	TMap<FName, FSingleBonePose> previousPose;
	TMap<FName, FSingleBonePose> currentPose;

public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Initialize", Keywords = "init pose"), Category = "Pose Matching")
	void Initialize(UAnimInstance* AnimInstance);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Update Pose", Keywords = "update pose"), Category = "Pose Matching")
	void UpdatePose();

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (BlueprintThreadSafe, DisplayName = "Is Pose Ready", Keywords = "seek pose"), Category = "Pose Matching")
	float FindPoseMatch(UPoseDataCache* poseCache);

	float GetLatestMatchedTime();
};