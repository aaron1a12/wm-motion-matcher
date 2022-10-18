// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseMatchBPLibrary.h"
#include "PoseMatch.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimClassInterface.h"
#include "PoseMatcher.h"

#include "Modules/ModuleManager.h"

#include "PoseMatchSettings.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "MotionMatcher_Component.h"



UPoseMatchBPLibrary::UPoseMatchBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

float UPoseMatchBPLibrary::PoseMatchSampleFunction(float Param)
{
	return FPoseMatchModule::Get().GetFloat();
	//GetOuter()
	//GetOutermostObject()
	//FModuleManager::LoadModuleChecked<FPoseMatchModule>("PoseMatch");
	return -1;
}

bool UPoseMatchBPLibrary::IsPoseReady(bool Condition, UPoseMatcher* poseMatcher, UPoseDataCache* poseCache)
{
	if (!Condition) return false;

	float time = poseMatcher->FindPoseMatch(poseCache);

	if (time == -1) return false;

	return true;
}

float UPoseMatchBPLibrary::GetMatchedTime(UPoseMatcher* poseMatcher)
{
	return poseMatcher->GetLatestMatchedTime();
}

TArray<FName> UPoseMatchBPLibrary::GetPoseMatchingBones()
{
	const UPoseMatchSettings* Settings = GetDefault<UPoseMatchSettings>();

	TArray<FName> myBones;
	Settings->GetBonesToMatch(myBones);

	return myBones;
	//return TArray<FName>();
}

UPoseMatcher* UPoseMatchBPLibrary::CreatePoseMatcher(UAnimInstance* AnimInstance)
{
	UPoseMatcher* pmObj = NewObject<UPoseMatcher>();
	pmObj->Initialize(AnimInstance);
	return pmObj;
}

FFootLock UPoseMatchBPLibrary::GetFootLockData(UAnimInstance* AnimInstance)
{
	UMotionMatcherComponent* mmComp = Cast<UMotionMatcherComponent>(AnimInstance->GetOwningActor()->GetComponentByClass(UMotionMatcherComponent::StaticClass()));

	if (mmComp)
	{
		return mmComp->GetFootLock();
	}

	return FFootLock();

	//AnimInstance->blueprint
//	UAnimBlueprintGeneratedClass* Class = AnimBlueprint->GetAnimBlueprintGeneratedClass();
}

UMotionMatcherInterface* UPoseMatchBPLibrary::CreateMotionMatcherInterface()
{
	return NewObject<UMotionMatcherInterface>();
}