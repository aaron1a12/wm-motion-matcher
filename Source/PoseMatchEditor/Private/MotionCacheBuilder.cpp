#include "MotionCacheBuilder.h"

#include "AssetRegistryModule.h"
#include "AnimationBlueprintLibrary.h"

#if WITH_EDITOR

// Based off GetJointTransform_RootRelative() by Kenneth Claassen.
FTransform GetRootSpaceTransform(UAnimSequence* anim, const float time, const FName boneName, bool bExtractRoot=false)
{
    FTransform T = FTransform::Identity;

    TArray<FName> bones;
    UAnimationBlueprintLibrary::FindBonePathToRoot(anim, boneName, bones);

    if(bExtractRoot)
        bones.RemoveAt(bones.Num() - 1);

    for (const FName& bone : bones)
    {

        FTransform boneTransform;
        int32 boneIndex = anim->GetAnimationTrackNames().IndexOfByKey(bone);

        if (boneIndex == INDEX_NONE)
        {
            return T;
        }

        anim->GetBoneTransform(boneTransform, boneIndex, time, false);

        T = T * boneTransform;
    }

    return T;
}

FTransform GetBoneT(UAnimSequence* anim, const float time, const FName boneName)
{
    FTransform T = FTransform::Identity;

    FTransform boneT;
    int32 boneIndex = anim->GetAnimationTrackNames().IndexOfByKey(boneName);

    if (boneIndex == INDEX_NONE)
    {
        return T;
    }

    anim->GetBoneTransform(boneT, boneIndex, time, false);

    return boneT;
}

float GetTimeAtFrame(const int32 frame, int32 totalFrames, float sequenceLength)
{
    const float FrameTime = totalFrames > 1 ? sequenceLength / (float)(totalFrames - 1) : 0.0f;
    return FMath::Clamp(FrameTime * frame, 0.0f, sequenceLength);
}


/*
*************************************************************************************************
*/

void NormalizeCache()
{
    auto getVectorSum = [&](FVector& v) {
        return v.X + v.Y + v.Z;
    };

    auto getVectorSqrDeviations = [&](FVector& v, FFeatureNormalData& normalData) {
        return FMath::Square(v.X - normalData.Mean) + FMath::Square(v.Y - normalData.Mean) + FMath::Square(v.Z - normalData.Mean);
    };

    auto normalizeFloat = [&](float& f, FFeatureNormalData& normalData) {
        f = (f - normalData.Mean) / normalData.StandardDeviation;
    };

    // normalizes a 3D feature using Z-score standardization 
    auto normalizeVector = [&](FVector& v, FFeatureNormalData& normalData) {
        v.X = (v.X - normalData.Mean) / normalData.StandardDeviation;
        v.Y = (v.Y - normalData.Mean) / normalData.StandardDeviation;
        v.Z = (v.Z - normalData.Mean) / normalData.StandardDeviation;
    };

    //
    // Calculate the normalization data
    //

    // set everything to zero to not mess up our arithmetic
    MotionData->NormalData_RootVelocity.SetDefaultValues();
    MotionData->NormalData_RootRotationSpeed.SetDefaultValues();

    // DEPRECATED
    MotionData->NormalData_Trajectory.Empty();
    MotionData->NormalData_Trajectory.AddDefaulted(2);
    //

    MotionData->NormalData_TrajectoryPosition.Empty();
    MotionData->NormalData_TrajectoryPosition.AddDefaulted(MotionData->TrajectoryTimings.Num());

    MotionData->NormalData_TrajectoryFacing.Empty();
    MotionData->NormalData_TrajectoryFacing.AddDefaulted(MotionData->TrajectoryTimings.Num());

    MotionData->NormalData_BonePosition.Empty();
    MotionData->NormalData_BonePosition.AddDefaulted(MotionData->PoseMatchingBones.Num());

    MotionData->NormalData_BoneVelocity.Empty();
    MotionData->NormalData_BoneVelocity.AddDefaulted(MotionData->PoseMatchingBones.Num());

    for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
    {
        MotionData->NormalData_BonePosition[i].SetDefaultValues();
        MotionData->NormalData_BoneVelocity[i].SetDefaultValues();
    }

    // sum and means

    for (auto& State : MotionData->States)
    {
        for (auto& PoseSample : State.CachedPoses)
        {
            MotionData->NormalData_RootVelocity.Count += 3; // 3 because vector 3D
            MotionData->NormalData_RootVelocity.Sum += getVectorSum(PoseSample.RootVelocity);

            MotionData->NormalData_RootRotationSpeed.Count++;
            MotionData->NormalData_RootRotationSpeed.Sum += PoseSample.RootRotationSpeed;

            // trajectory

            for (int32 i = 0; i != MotionData->TrajectoryTimings.Num(); ++i)
            {
                FTrajectoryPoint& TrajPoint = PoseSample.Trajectory[i];

                // Position
                MotionData->NormalData_TrajectoryPosition[i].Count += 3; // 3d vector
                MotionData->NormalData_TrajectoryPosition[i].Sum += getVectorSum(TrajPoint.Position);

                MotionData->NormalData_TrajectoryFacing[i].Count++;
                MotionData->NormalData_TrajectoryFacing[i].Sum += TrajPoint.Facing;
            }

            /*for (auto& TrajPoint : PoseSample.Trajectory)
            {
                // Position
                MotionData->NormalData_Trajectory[0].Count++;
                MotionData->NormalData_Trajectory[0].Sum += getVectorSum(TrajPoint.Position);

                // Facing
                MotionData->NormalData_Trajectory[1].Count++;
                MotionData->NormalData_Trajectory[1].Sum += TrajPoint.Facing;
            }*/

            // Bones

            for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
            {
                MotionData->NormalData_BonePosition[i].Count += 3; // 3d vector
                MotionData->NormalData_BonePosition[i].Sum += getVectorSum(PoseSample.BoneData[i].Position);

                MotionData->NormalData_BoneVelocity[i].Count += 3; // 3d vector
                MotionData->NormalData_BoneVelocity[i].Sum += getVectorSum(PoseSample.BoneData[i].Velocity);
            }
        }
    }

    MotionData->NormalData_RootVelocity.Mean = MotionData->NormalData_RootVelocity.Sum / MotionData->NormalData_RootVelocity.Count; 
    MotionData->NormalData_RootRotationSpeed.Mean = MotionData->NormalData_RootRotationSpeed.Sum / MotionData->NormalData_RootRotationSpeed.Count;
    //MotionData->NormalData_Trajectory[0].Mean = MotionData->NormalData_Trajectory[0].Sum / MotionData->NormalData_Trajectory[0].Count;
    //MotionData->NormalData_Trajectory[1].Mean = MotionData->NormalData_Trajectory[1].Sum / MotionData->NormalData_Trajectory[1].Count;
    //MotionData->NormalData_TrajectoryPosition

    for (int32 i = 0; i != MotionData->TrajectoryTimings.Num(); ++i)
    {
        auto& fNormalPos = MotionData->NormalData_TrajectoryPosition[i];
        auto& fNormalFacing = MotionData->NormalData_TrajectoryFacing[i];

        fNormalPos.Mean = fNormalPos.Sum / fNormalPos.Count;
        fNormalFacing.Mean = fNormalFacing.Sum / fNormalFacing.Count;
    }

    for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
    {
        MotionData->NormalData_BonePosition[i].Mean = MotionData->NormalData_BonePosition[i].Sum / MotionData->NormalData_BonePosition[i].Count;
        MotionData->NormalData_BoneVelocity[i].Mean = MotionData->NormalData_BoneVelocity[i].Sum / MotionData->NormalData_BoneVelocity[i].Count;
    }

    // variances

    for (auto& State : MotionData->States)
    {
        for (auto& PoseSample : State.CachedPoses)
        {
            MotionData->NormalData_RootVelocity.Variance += getVectorSqrDeviations(PoseSample.RootVelocity, MotionData->NormalData_RootVelocity);
            MotionData->NormalData_RootRotationSpeed.Variance += FMath::Square(PoseSample.RootRotationSpeed - MotionData->NormalData_RootRotationSpeed.Mean);


            /*for (auto& TrajPoint : PoseSample.Trajectory)
            {
                // Position
                MotionData->NormalData_Trajectory[0].Variance += getVectorSqrDeviations(TrajPoint.Position, MotionData->NormalData_Trajectory[0]);

                // Facing
                MotionData->NormalData_Trajectory[1].Variance += FMath::Square(TrajPoint.Facing - MotionData->NormalData_Trajectory[1].Mean);
            }*/

            for (int32 i = 0; i != PoseSample.Trajectory.Num(); ++i)
            {
                auto& trajPoint = PoseSample.Trajectory[i];

                auto& fNormalPos = MotionData->NormalData_TrajectoryPosition[i];
                auto& fNormalFacing = MotionData->NormalData_TrajectoryFacing[i];

                fNormalPos.Variance += getVectorSqrDeviations(trajPoint.Position, fNormalPos);
                fNormalFacing.Variance += FMath::Square(trajPoint.Facing - fNormalFacing.Mean);
            }

            for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
            {
                MotionData->NormalData_BonePosition[i].Variance += getVectorSqrDeviations(PoseSample.BoneData[i].Position, MotionData->NormalData_BonePosition[i]);
                MotionData->NormalData_BoneVelocity[i].Variance += getVectorSqrDeviations(PoseSample.BoneData[i].Velocity, MotionData->NormalData_BoneVelocity[i]);
            }            
        }
    }

    MotionData->NormalData_RootVelocity.Variance = MotionData->NormalData_RootVelocity.Variance / MotionData->NormalData_RootVelocity.Count;
    MotionData->NormalData_RootRotationSpeed.Variance = MotionData->NormalData_RootRotationSpeed.Variance / MotionData->NormalData_RootRotationSpeed.Count;

    for (int32 i = 0; i != MotionData->TrajectoryTimings.Num(); ++i)
    {
        auto& fNormalPos = MotionData->NormalData_TrajectoryPosition[i];
        auto& fNormalFacing = MotionData->NormalData_TrajectoryFacing[i];

        fNormalPos.Variance = fNormalPos.Variance / fNormalPos.Count;
        fNormalFacing.Variance = fNormalFacing.Variance / fNormalFacing.Count;
    }

    for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
    {
        MotionData->NormalData_BonePosition[i].Variance = MotionData->NormalData_BonePosition[i].Variance / MotionData->NormalData_BonePosition[i].Count;
        MotionData->NormalData_BoneVelocity[i].Variance = MotionData->NormalData_BoneVelocity[i].Variance / MotionData->NormalData_BoneVelocity[i].Count;
    }

    //
    // standard deviations
    //

    // SD may be NaN if variance=0 so here we fall back to zero if so.
    MotionData->NormalData_RootVelocity.StandardDeviation = (MotionData->NormalData_RootVelocity.Variance) ? FMath::Sqrt(MotionData->NormalData_RootVelocity.Variance) : 1.0f;
    MotionData->NormalData_RootVelocity.StdInverse = 1.0f / MotionData->NormalData_RootVelocity.StandardDeviation;
    MotionData->NormalData_RootVelocity.UserWeight = MotionData->RootVelocityWeight;

    MotionData->NormalData_RootRotationSpeed.StandardDeviation = (MotionData->NormalData_RootRotationSpeed.Variance) ? FMath::Sqrt(MotionData->NormalData_RootRotationSpeed.Variance) : 1.0f;
    MotionData->NormalData_RootRotationSpeed.StdInverse = 1.0f / MotionData->NormalData_RootRotationSpeed.StandardDeviation;
    MotionData->NormalData_RootRotationSpeed.UserWeight = 1.0f; // TODO: no custom weight?

    for (int32 i = 0; i != MotionData->TrajectoryTimings.Num(); ++i)
    {
        auto& fNormalPos = MotionData->NormalData_TrajectoryPosition[i];
        auto& fNormalFacing = MotionData->NormalData_TrajectoryFacing[i];

        fNormalPos.StandardDeviation = (fNormalPos.Variance) ? FMath::Sqrt(fNormalPos.Variance) : 1.0f;
        fNormalPos.StdInverse = 1.0f / fNormalPos.StandardDeviation;
        fNormalPos.UserWeight = MotionData->TrajectoryWeight * MotionData->TrajectoryWeights[i];

        fNormalFacing.StandardDeviation = (fNormalFacing.Variance) ? FMath::Sqrt(fNormalFacing.Variance) : 1.0f;
        fNormalFacing.StdInverse = 1.0f / fNormalFacing.StandardDeviation;
        fNormalFacing.UserWeight = MotionData->TrajectoryFacingWeights[i] * MotionData->TrajectoryFacingWeight;
    }

    for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
    {
        MotionData->NormalData_BonePosition[i].StandardDeviation = (MotionData->NormalData_BonePosition[i].Variance) ? FMath::Sqrt(MotionData->NormalData_BonePosition[i].Variance) : 1.0f;
        MotionData->NormalData_BonePosition[i].StdInverse = 1.0f / MotionData->NormalData_BonePosition[i].StandardDeviation;
        MotionData->NormalData_BonePosition[i].UserWeight = MotionData->BonePositionWeight;

        MotionData->NormalData_BoneVelocity[i].StandardDeviation = (MotionData->NormalData_BoneVelocity[i].Variance) ? FMath::Sqrt(MotionData->NormalData_BoneVelocity[i].Variance) : 1.0f;
        MotionData->NormalData_BoneVelocity[i].StdInverse = 1.0f / MotionData->NormalData_BoneVelocity[i].StandardDeviation;
        MotionData->NormalData_BoneVelocity[i].UserWeight = MotionData->BoneVelocityWeight;
    }

    //
    // custom weights
    //


    //
    // Normalize the features independently
    //

    for (auto& State : MotionData->States)
    {
        for (auto& PoseSample : State.CachedPoses)
        {
            MotionData->NormalizeFeature(PoseSample.RootVelocity, MotionData->NormalData_RootVelocity);
            MotionData->NormalizeFeature(PoseSample.RootRotationSpeed, MotionData->NormalData_RootRotationSpeed);

            MotionData->NormalizeTrajectory(PoseSample.Trajectory);

            for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
            {
                MotionData->NormalizeFeature(PoseSample.BoneData[i].Position, MotionData->NormalData_BonePosition[i]);
                MotionData->NormalizeFeature(PoseSample.BoneData[i].Velocity, MotionData->NormalData_BoneVelocity[i]);
            }
        }
    }
}

void ApplyWeights()
{
    for (auto& State : MotionData->States)
    {
        for (auto& PoseSample : State.CachedPoses)
        {
            PoseSample.RootVelocity *= MotionData->RootVelocityWeight;

            int32 x = 0;
            for (auto& TrajPoint : PoseSample.Trajectory)
            {
                TrajPoint.Position *= MotionData->TrajectoryWeight;
                //TrajPoint.Facing *= MotionData->TrajectoryFacingWeight;
                TrajPoint.Facing *= MotionData->TrajectoryFacingWeight * MotionData->TrajectoryFacingWeights[x];
                x++;
            }

            for (int32 i = 0; i != MotionData->PoseMatchingBones.Num(); ++i)
            {
                PoseSample.BoneData[i].Position *= MotionData->BonePositionWeight;
                PoseSample.BoneData[i].Velocity *= MotionData->BoneVelocityWeight;
            }
        }
    }
}

void BuildMotionCache(const TWeakObjectPtr<UMotionData> motionDataAsset)
{
    MotionData = motionDataAsset;
    StateCount = 0;
    SampleCount = 0;

    // Ensure the timings are sorted chronologically.
    MotionData->TrajectoryTimings.Sort([](const float& A, const float& B) { return A < B; });

    // Loop through the states.
    for (auto& State : motionDataAsset->States)
    {
        BuildMotionCache_PerState(State);
    }

    // Normalize features (feature scaling because some ranges are vastly different in size)

    NormalizeCache();

    // Apply user weights (makes features matter more/less)

    //ApplyWeights();

    //float rotSpeedSum
    //motionDataAsset->NormalData_RootVelocity.Sum;

    // Set our cache as up-to-date.
    motionDataAsset->bOutdatedMotionCache = false;

    // Mark as dirty so the editor can save the data.
    //motionDataAsset->PostEditChange();
    motionDataAsset->MarkPackageDirty();

    UE_LOG(LogTemp, Warning, TEXT("Total poses cached: %d"), SampleCount);
}

void BuildMotionCache_PerState(FMMatcherState& state)
{
    if (!state.Animation) return;

    state.Id = StateCount;

    UAnimSequence* anim = state.Animation;
    int32 frames = anim->GetNumberOfFrames();

    // We'll save trajactory points every 3 frames
    const int32 SAVE_INTERVAL = 3;
    int32 totalPoints = FMath::CeilToInt(frames / SAVE_INTERVAL);

    // Delete the old data.
    state.CachedPoses.Empty();
    //state.CachedPoses.Reserve(totalPoses);

    float endTime = anim->GetPlayLength() - MOTION_MATCHING_INTERVAL - 0.250f;  // TRIM THE LAST QUARTER SECOND? WHO NEEDS THIS!!?

    // Trajectory bone.
    FName rootBoneName = anim->GetSkeleton()->GetReferenceSkeleton().GetBoneName(0);

    //anim->GetSkeleton()->required


    //FTransform trans = GetRootSpaceTransform(anim, 0.0f, "root");

    //UE_LOG(LogTemp, Warning, TEXT("Roll: %f, Pitch: %f, Yaw: %f"), botRot.Roll, botRot.Pitch, botRot.Yaw);

    for (float time = 0.f; time < endTime; time += MotionData->MotionCacheSamplingRate)
    {
        // Create a new pose sample
        FMMatcherPoseSample pose;
        pose.Id = SampleCount;
        pose.StateId = state.Id;
        pose.Time = time;
       
        // The trajectory transforms are relative to this current root transform.
        //FTransform currentRootT = GetRootSpaceTransform(anim, time, rootBoneName);
        FTransform currentRootT = anim->ExtractRootMotion(0.f, time, false); // alternate method?

        // Future sample for root velocity.
        //FTransform futureRootT = GetRootSpaceTransform(anim, time + MOTION_MATCHING_INTERVAL, rootBoneName);
        FTransform futureRootT = anim->ExtractRootMotion(0.f, time + MOTION_MATCHING_INTERVAL, false); // alternate method?

        pose.RootVelocity = (futureRootT.GetLocation() - currentRootT.GetLocation()) / MOTION_MATCHING_INTERVAL;
        pose.RootVelocity = pose.RootVelocity.RotateAngleAxis(-1 * currentRootT.GetRotation().Rotator().Yaw, FVector::UpVector);
        pose.RootRotationSpeed = (futureRootT.GetRotation().Rotator().Yaw - currentRootT.GetRotation().Rotator().Yaw) / MOTION_MATCHING_INTERVAL;


        // Foot locking

        auto shouldFootLock = [&] (FName& bN) { 
            FTransform rBallT = GetRootSpaceTransform(anim, time, bN);
            FTransform rBallT2 = GetRootSpaceTransform(anim, time + MOTION_MATCHING_INTERVAL, bN);

            float toeSpeed = FMath::Abs(rBallT2.GetLocation().Size() - rBallT.GetLocation().Size());
            //UE_LOG(LogTemp, Warning, TEXT("\nSPEED: %f."), toeSpeed);
            if (toeSpeed < 0.5f) {
               
                return true;
            }
            else {
                return false;
            }
        };
        
        if (MotionData->LeftFoot.BoneIndex != 0) {
            //pose.bLFootLock = shouldFootLock(MotionData->LeftFoot.BoneName);
            pose.FootLocks[0] = shouldFootLock(MotionData->LeftFoot.BoneName);

            //UE_LOG(LogTemp, Warning, TEXT("Left lock: %d"), pose.FootLocks[0]);
        }

        if (MotionData->RightFoot.BoneIndex != 0) {
            //pose.bRFootLock = shouldFootLock(MotionData->RightFoot.BoneName);
            pose.FootLocks[1] = shouldFootLock(MotionData->RightFoot.BoneName);
        }

        FTransform spine = GetRootSpaceTransform(anim, time, "spined");
        FVector spineFacing = spine.GetUnitAxis(EAxis::Y);

        pose.FacingAxis = spineFacing;

        //
        // Bone data for pose matching
        //

        pose.BoneData.AddDefaulted(MotionData->PoseMatchingBones.Num());

        for (int32 i = 0; i != pose.BoneData.Num(); ++i)
        {
            FMMatcherBoneData& boneData = pose.BoneData[i];
            const FBoneReference& boneRef = MotionData->PoseMatchingBones[i];
            FName boneName = boneRef.BoneName;

            FTransform boneT = GetRootSpaceTransform(anim, time, boneName);
            FTransform futureBoneT = GetRootSpaceTransform(anim, time + MOTION_MATCHING_INTERVAL, boneName);
            
            
            //boneData.Position = boneT.GetLocation();
            //boneData.Position = GetRootSpaceTransform(anim, time, boneName, true).GetLocation();
            boneData.Position = (currentRootT.InverseTransformPosition(boneT.GetLocation()));
            boneData.Velocity = (futureRootT.InverseTransformPosition(futureBoneT.GetLocation()) - boneData.Position) / MOTION_MATCHING_INTERVAL;
            //boneData.Velocity = FMath::Lerp(boneData.Velocity, boneData.Velocity.RotateAngleAxis(-1 * currentRootT.GetRotation().Rotator().Yaw, FVector::UpVector), 0.5f);
            //boneData.Velocity = boneData.Velocity.RotateAngleAxis(-1 * currentRootT.GetRotation().Rotator().Yaw, FVector::UpVector);

            //FVector boneVel = lastPosition + localMeshCompPos + (bone.Position + (bone.Velocity / 8)).RotateAngleAxis(actorYaw, FVector::UpVector);

            FTransform test;
            UAnimationBlueprintLibrary::GetBonePoseForTime(anim, boneName, time, true, test);
            //boneData.Position = test.GetLocation();

            /*UE_LOG(LogTemp, Warning, TEXT("\nBONE POS: %f | %f | %f"), //\nBONE VEL: %f | %f | %f
                boneData.Position.X, boneData.Position.Y, boneData.Position.Z,
                boneData.Velocity.X, boneData.Velocity.Y, boneData.Velocity.Z);*/
        }

        //
        // Trajectory stuff
        //

        for (auto& timeOffset : MotionData->TrajectoryTimings)
        {
            FTrajectoryPoint trajPoint;

            float absPointTime = time + timeOffset;

            if(absPointTime < 0.f || absPointTime > endTime) // Point lands outside of our animation clip, let's estimate by extrapolating.
            {                
                // We'll take a sample and see how much the vectors move. This is how long that sample is.
                float sampleTime = 0.10f; // 0.10f

                float startTimeA, startTimeB;

                if (absPointTime > endTime) // Forward extrapolation
                {
                    startTimeA = anim->GetPlayLength() - sampleTime; // Start a bit before the end
                    startTimeB = anim->GetPlayLength(); // And stop at the very end
                    
                    FVector a = GetRootSpaceTransform(anim, startTimeA, rootBoneName).GetLocation();
                    FVector b = GetRootSpaceTransform(anim, startTimeB, rootBoneName).GetLocation();

                    // Speed is in cm/s.
                    FVector velocity = (b - a) / sampleTime;

                    float rot = currentRootT.GetRotation().Rotator().Yaw;
                    velocity = velocity.RotateAngleAxis(-1*rot, FVector::UpVector);

                    // Apply velocity
                    trajPoint.Position = velocity * FMath::Abs(timeOffset);
                    trajPoint.Facing = 0.f; // assume no facing change.
                }
                else
                {
                    startTimeA = sampleTime; // Start a bit after the beginning
                    startTimeB = 0.f; // and stop at the very beginning

                    FVector a = GetRootSpaceTransform(anim, startTimeA, rootBoneName).GetLocation();
                    FVector b = GetRootSpaceTransform(anim, startTimeB, rootBoneName).GetLocation();

                    // Speed is in cm/s.
                    FVector velocity = (b - a) / sampleTime;

                    float rot = currentRootT.GetRotation().Rotator().Yaw;
//                    velocity = velocity.RotateAngleAxis(rot - rot - rot, FVector::UpVector);

                    // Apply velocity
                    trajPoint.Position = (velocity * FMath::Abs(timeOffset));// + currentRootT.GetLocation() - currentRootT.GetLocation() - currentRootT.GetLocation();
                    trajPoint.Position = FMath::Lerp(trajPoint.Position, trajPoint.Position.RotateAngleAxis(-1*rot, FVector::UpVector), 0.5f);  // rot - rot - rot
                    trajPoint.Facing = 0.f; // assume no facing change.
                }
            }
            else
            {                
                FTransform newRootT = GetRootSpaceTransform(anim, time + timeOffset, rootBoneName);

                FVector posChange = newRootT.GetLocation() - currentRootT.GetLocation();
                float rot = currentRootT.GetRotation().Rotator().Yaw;
               
                trajPoint.Position = posChange.RotateAngleAxis(-1*rot, FVector::UpVector);
                //trajPoint.Facing = newRootT.GetRotation().Rotator().Yaw - currentRootT.GetRotation().Rotator().Yaw;


                FRotator rotDiff = newRootT.GetRotation().Rotator() - currentRootT.GetRotation().Rotator();
                rotDiff.Normalize();

                trajPoint.Facing = rotDiff.Yaw;
                //trajPoint.Facing = 0.f;

                /*if(timeOffset==0.6f) //-0.4f
                    UE_LOG(LogTemp, Warning, TEXT("\nFacing: %f"), trajPoint.Facing);*/
            }
            
            trajPoint.TimeOffset = timeOffset; // do we need this?

            pose.Trajectory.Add(trajPoint);
        }

        //
        // Save the pose.
        //

        state.CachedPoses.Add(pose);
        SampleCount++;
    }  

    for (auto& boneRef : MotionData->PoseMatchingBones)
    {
        FTransform boneTransform = GetRootSpaceTransform(anim, 0.f, boneRef.BoneName);
        FVector boneLocation = boneTransform.GetLocation();

        FRotator botRot = boneTransform.GetRotation().Rotator();
    }
    
    StateCount++;
}
void AddAnimations(const TWeakObjectPtr<UMotionData> motionDataAsset)
{
    ///motionDataAsset.

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    TArray<FAssetData> AssetData;

    const UClass* Class = UAnimSequence::StaticClass();

    AssetRegistryModule.Get().GetAssetsByClass("AnimSequence", AssetData);

    auto skeletalAssetId = motionDataAsset->TargetSkeleton->GetPrimaryAssetId();

    bool bAddedAny = false;

    for (FAssetData asset : AssetData) {
        UAnimSequence* loadedAsset = (UAnimSequence*)asset.GetAsset();

        if(loadedAsset && loadedAsset->GetSkeleton())
        {
            if (loadedAsset->GetSkeleton()->GetPrimaryAssetId() == skeletalAssetId)
            {
                loadedAsset->bEnableRootMotion = true;
                loadedAsset->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;

                loadedAsset->MarkPackageDirty();

                FMMatcherState state;

                state.Animation = loadedAsset;

                motionDataAsset->States.Add(state);
                bAddedAny = true;
            }
        }
    }

    if (bAddedAny)
    {
        motionDataAsset->MarkPackageDirty();
    }
    
}
void ChangeAnimationRate(const TWeakObjectPtr<UMotionData> motionDataAsset, bool bReset)
{
    // Loop through the states.
    for (auto& State : motionDataAsset->States)
    {
        State.Animation->RateScale = (bReset) ? 1.f : 1.25f;
        State.Animation->MarkPackageDirty();
    }
}
#endif