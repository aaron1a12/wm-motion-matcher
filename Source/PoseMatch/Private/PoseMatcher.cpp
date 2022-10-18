#include "PoseMatcher.h"
#include "PoseDataCache.h"
#include "PoseMatchSettings.h"
#include "GenericPlatform/GenericPlatformMath.h"

//#include "Animation/AnimSequence.h"
//#include "Animation/AnimCurveCompressionCodec_UniformIndexable.h"
#include "Misc/DateTime.h"

UPoseMatcher::UPoseMatcher(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

void UPoseMatcher::Initialize(UAnimInstance* AnimInstance)
{
	animInst = AnimInstance;
	skelMesh = animInst->GetSkelMeshComponent();
	//skelMesh->GetBoneName()

	const UPoseMatchSettings* Settings = GetDefault<UPoseMatchSettings>();

	TArray<FName> myBones;
	Settings->GetBonesToMatch(myBones);

	//UE_LOG(LogTemp, Warning, TEXT("Number of bones: %i"), myBones.Num());

	// preallocate to speed up stuff
	boneNames.Reserve(myBones.Num());

	previousPose.Reserve(myBones.Num());
	currentPose.Reserve(myBones.Num());



	UE_LOG(LogTemp, Warning, TEXT("Initializing pose matcher."));

	for (auto& name : myBones)
	{
		//int32 boneIndex = skelMesh->GetBoneIndex(name);
		UE_LOG(LogTemp, Warning, TEXT("Found %s bone."), *name.ToString());

		boneNames.Add(name);

		previousPose.Emplace(name, FSingleBonePose());
		currentPose.Emplace(name, FSingleBonePose());
	}
	


	

	//auto curves = AnimInstance->GetAnimationCurveList(EAnimCurveType::AttributeCurve);
	//AnimInstance->GetSkelMeshComponent()->getbon
}

void UPoseMatcher::UpdatePose()
{
	if(animInst)
	{
		/*FVector thighPosL;
		FRotator thighRotL;
		FVector thighPosR;
		FRotator thighRotR;

		skelMesh->TransformToBoneSpace("Bip_Thigh_L", skelMesh->GetComponentLocation(), skelMesh->GetComponentRotation(), thighPosL, thighRotL);
		skelMesh->TransformToBoneSpace("Bip_Thigh_R", skelMesh->GetComponentLocation(), skelMesh->GetComponentRotation(), thighPosR, thighRotR);

		float dist = FVector::DistSquared(thighPosL, thighPosR) * 12;

		UE_LOG(LogTemp, Warning, TEXT("dist: %f"), dist);*/

		for (auto& boneName : boneNames)
		{
			//FName boneName = skelMesh->GetBoneName(boneIndex);
			//previousPose.Reserve
			//previousPose.Emplace(boneIndex)
			//FSingleBonePose* currentPose

			// Store a copy of the current pose in our previous pose (so we can calculate velocity.)
			previousPose.Emplace(boneName, *currentPose.Find(boneName));

			//FTransform boneT = skelMesh->GetBoneTransform(boneIndex);

			FVector pos;
			FRotator rot;			

			skelMesh->TransformToBoneSpace(boneName, skelMesh->GetComponentLocation(), skelMesh->GetComponentRotation(), pos, rot);

			FQuat boneQuat = skelMesh->GetBoneQuaternion(boneName, EBoneSpaces::WorldSpace);
			FQuat parentBoneQuat = skelMesh->GetBoneQuaternion(skelMesh->GetParentBone(boneName), EBoneSpaces::WorldSpace);

			FQuat finalQuat = parentBoneQuat.Inverse() * boneQuat;
			FRotator finalRot = finalQuat.Rotator();
			
			
			//skelMesh->TransformFromBoneSpace()

			currentPose.Emplace(
				boneName,
				FSingleBonePose(pos, FVector(finalRot.Roll, finalRot.Pitch, finalRot.Yaw))
			);

			/*if(boneName=="Bip_Thigh_L")
				UE_LOG(LogTemp, Warning, TEXT("Bone (%s), Roll: %f, Pitch: %f, Yaw: %f"), *boneName.ToString(), finalRot.Roll, finalRot.Pitch, finalRot.Yaw);*/


			//auto boneQuat = skelMesh->GetBoneQuaternion("Bip_Thigh_L", EBoneSpaces::WorldSpace);
		}


		/*auto skelMesh = animInst->GetSkelMeshComponent();

		int boneIndex = skelMesh->GetBoneIndex("Bip_Thigh_L");

		auto boneQuat = skelMesh->GetBoneQuaternion("Bip_Thigh_L", EBoneSpaces::WorldSpace);
		FTransform boneT = skelMesh->GetBoneTransform(boneIndex);

		previousRoll = currentRoll;
		//currentRoll = boneT.GetRotation().Rotator().Roll;
		currentRoll = skelMesh->GetSocketRotation("Bip_Thigh_L").Roll;*/


		//skelMesh->GetBoneTransform()
		//UE_LOG(LogTemp, Warning, TEXT("Anim Curve: %f, Extracted Bone Roll: %f"), animInst->GetCurveValue("Bip_Thigh_L_x"), currentRoll);
	}
}


float getVectorAvgDiff( FVector A, FVector B)
{	
	float a = FGenericPlatformMath::Abs(A.X - B.X);
	float b = FGenericPlatformMath::Abs(A.Y - B.Y);
	float c = FGenericPlatformMath::Abs(A.Z - B.Z);

	return (a + b + c) / 3;
}

float UPoseMatcher::FindPoseMatch(UPoseDataCache* poseCache)
{
	if (!animInst || !poseCache) return -1;

	// thread safety. don't continue if there is a left-over result from a previous match
	if (matchedTime != -1) {
		matchedTime = -1;
		UE_LOG(LogTemp, Warning, TEXT("Found leftover info"));
		return matchedTime;
	}

	if (bBusySearching) return -1;
	bBusySearching = true;

	// Compute velocity for each bone in currentPose
	for (auto& item : currentPose)
	{
		auto boneLastPose = previousPose.Find(item.Key);

		if (boneLastPose)
		{
			item.Value.TranslationalVelocity = item.Value.Translation - boneLastPose->Translation;
		}
	}

	float bestMatch = MAX_FLT;
	float bestWeight = MAX_FLT;
	int matchedFrame = 0;

	matchedTime = -1;

	int numFrames = poseCache->GetNumFrames();

	//auto activePoser = currentPose.Find("Bip_Thigh_L");
	//UE_LOG(LogTemp, Warning, TEXT("Bip_Thigh_L Roll: %f"), activePoser->Rotation.X);

	// Loop through the pose data cache,
	// using i = 1 to skip first frame.
	for (int i = 1; i < numFrames; i++) {

		float time = poseCache->GetTimeAtFrame(i);
		float time4PrevFrame = poseCache->GetTimeAtFrame(i - 1);

		TSet<float> weights;
		weights.Reserve(poseCache->Bones.Num());

		// Go through each bone
		for (auto& poseCacheItem : poseCache->Bones)
		{
			// Find the same bone on our current pose.
			auto activePose = currentPose.Find(poseCacheItem.Key);

			if (activePose)
			{
				FVector activeTranslation = activePose->Translation;
				FVector activeRotation = activePose->Rotation;
				FVector activeTranslationalVelocity = activePose->TranslationalVelocity;

				FVector cachedTranslation = poseCacheItem.Value.Translation.Evaluate(time, 1.f);
				FVector cachedRotation = poseCacheItem.Value.Rotation.Evaluate(time, 1.f);
				FVector cachedTranslationalVelocity = cachedTranslation - poseCacheItem.Value.Translation.Evaluate(time4PrevFrame, 1.f);

				//
				// Compute and compare the differences
				//

				// == Typical ranges ==
				// Dist:			0.14<->0.65
				// Dist Squared:	0.01<->0.44
				// Roll:			  21<->-23

				float avgT = getVectorAvgDiff(activeTranslation, cachedTranslation);
				float avgTV = getVectorAvgDiff(activeTranslationalVelocity, cachedTranslationalVelocity);
				float avgR = getVectorAvgDiff(activeRotation, cachedRotation) * 0.01f;

				float avgDiff = (avgT + avgTV + avgR) / 3.f;

				//UE_LOG(LogTemp, Warning, TEXT("avgDiff: %f"), avgDiff);

				/*if(poseCacheItem.Key=="Bip_Thigh_L")
					UE_LOG(LogTemp, Warning, TEXT("Frame: %d | Current: %f, Cached: %f"), i, activeRotation.X, cachedRotation.X);*/

				weights.Emplace(avgDiff);
				//float dist = FVector::DistSquared(activeTranslation, cachedTranslation);
			}
		}

		
		float combinedWeight = 0.f;
		for (auto& weight : weights) { combinedWeight += weight; }

		float avgWeight = combinedWeight / weights.Num();

		if (avgWeight < bestWeight)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Found a better weight (%f) at frame %d"), avgWeight, i);

			bestWeight = avgWeight;
			bestMatch = time;
			matchedFrame = i;
		}
	}

	//UE_LOG(LogTemp, Warning, TEXT("Matched time: %f, frame: %d"), bestMatch, matchedFrame);

	bBusySearching = false;

	matchedTime = bestMatch;// +0.1f;
	return matchedTime;
	return -1;

	
	/* Current roll
	auto skelMesh = animInst->GetSkelMeshComponent();
	int boneIndex = skelMesh->GetBoneIndex("Bip_Thigh_L"); //Bip_Thigh_L

	auto thighQuat = skelMesh->GetBoneQuaternion("Bip_Thigh_L", EBoneSpaces::WorldSpace);
	auto pelvisQuat = skelMesh->GetBoneQuaternion("Bip_Pelvis", EBoneSpaces::WorldSpace);
	auto rootQuat = skelMesh->GetBoneQuaternion("Root", EBoneSpaces::WorldSpace);

	FQuat local1 = pelvisQuat.Inverse() * thighQuat;
	FQuat local2 = rootQuat.Inverse() * local2;

//	skelMesh->GetParentBone()
	//currentRoll = boneT.GetRotation().Rotator().Roll;
	currentRoll = local1.Rotator().Roll;


	FSmartName CurveSmartName;
	nextAnimation->GetSkeleton()->GetSmartNameByName(USkeleton::AnimCurveMappingName, "Bip_Thigh_L_x", CurveSmartName);

	FAnimCurveBufferAccess CurveBuffer = FAnimCurveBufferAccess(nextAnimation, CurveSmartName.UID);

	//boneT + boneT;

	UE_LOG(LogTemp, Warning, TEXT("Time: %d, Roll: %f, Curve: %f"), FDateTime::Now().GetMillisecond(), currentRoll, animInst->GetCurveValue("Bip_Thigh_L_x"));

	//UE_LOG(LogTemp, Warning, TEXT("Current: %f, In Next Anim: %f, Time: %d"), currentRoll, animRoll, FDateTime::Now().GetMillisecond());

	return -1;
	//UAnimSequence* anim;

	//anim.getbon
	//anim->sequ
	//anim->GetRawNumberOfFrames();
	//auto curves = anim->GetCurveData();

	//curves.


	/*if (!animInst || !PoseCache) return -1;

	// thread safety. don't continue if there is a left-over result from a previous match
	if (matchedTime != -1) {
		matchedTime = -1;
		return matchedTime;
	}

	if (bBusySearching) return -1;
	bBusySearching = true;

	float bestMatch = MAX_FLT;
	float bestWeight = MAX_FLT;
	int matchedFrame = 0;

	int numFrames = PoseCache->GetNumFrames();

	for (int i = 0; i < numFrames; i++) {
		if (i != 0) { // skip first frame
			float time = PoseCache->GetTimeAtFrame(i);
			FVector vecVal = PoseCache->GetVectorValue(time);
			float curveVal = animInst->GetCurveValue("Bip_Thigh_L_x");

			float diff = FGenericPlatformMath::Abs(vecVal.X - curveVal);

			if (diff < bestWeight)
			{
				bestWeight = diff;
				bestMatch = time;
				matchedFrame = i;
			}
		}
	}

	return bestMatch;*/


	bBusySearching = false;
	return -1;
}

float UPoseMatcher::GetLatestMatchedTime()
{
	float found = matchedTime;

	matchedTime = -1;

	//UE_LOG(LogTemp, Warning, TEXT("GetLatestMatchedTime(): %f"), found);

	return found;
}
