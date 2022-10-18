// Copyright Wild Montage, LLC. All Rights Reserved.

#include "AnimNode_MotionMatcher.h"
#include "PoseMatch.h"
#include "Animation/AnimInstanceProxy.h"

//nclude "Animation/BoneControllers/AnimNode_TwoBoneIK.h"
#include "DrawDebugHelpers.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "MotionMatcher_Component.h"
#include "MotionMatcherInterface.h"

#include "RootMotionSource_Custom.h"
#include "TwoBoneIK.h"

#include "Spring/Spring.h"

#if WITH_EDITOR
#include "Debug/DebugWidget.h"
#endif

// Live recording rate for past trajectory.
#define RECORD_SAMPLE_RATE 0.03f

// Performance profiling. Enter in console "stat motionmatching" to display perf dat. "stat none" to disable.
DECLARE_STATS_GROUP(TEXT("MotionMatching"), STATGROUP_MotionMatching, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Index Search"), STAT_MMIndexSearch, STATGROUP_MotionMatching);

FAnimNode_MotionMatcher::FAnimNode_MotionMatcher()
{
}

void FAnimNode_MotionMatcher::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	GetEvaluateGraphExposedInputs().Execute(Context);

	FPoseMatchModule* module = &FPoseMatchModule::Get();

	if (module)
	{
		UE_LOG(LogTemp, Warning, TEXT("Singleton located, global float: %f"), module->GetFloat());
	}

	bHasValidMotionData = false;

	if (!MotionDataAsset) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher has no motion data."));
#endif
	}
	else if (MotionDataAsset->States.Num() == 0) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher data asset has no animation states added."));
#endif
	}
	else if (!MotionDataAsset->States[0].Animation) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("First state has no animation associated."));
#endif
	}
	else if (MotionDataAsset->bOutdatedMotionCache) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher is using an outdated motion cache. Use the \"Rebuild Motion Cache\" action from the content browser's context-menu."));
#endif
	}
	else if (MotionDataAsset->TrajectoryTimings.Num() == 0) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher data asset has no trajectory timings added."));
#endif
	}
	else if (MotionDataAsset->PoseMatchingBones.Num() == 0) {
#if WITH_EDITOR
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher data asset has no pose matching bones listed."));
#endif
	}
	else {
		bHasValidMotionData = true;
	}

#if WITH_EDITOR
	if (GEngine && MotionDataAsset && MotionDataAsset->States.Num() != 0 && MotionDataAsset->bOutdatedMotionCache)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, TEXT("Motion Matcher is using an outdated motion cache. Use the \"Rebuild Motion Cache\" action from the content browser's context-menu."));
#endif

	if (!bHasValidMotionData) return;

	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		owningActor = AnimInstance->GetOwningActor();

#if WITH_EDITOR
		FStringClassReference MyWidgetClassRef("WidgetBlueprint'/PoseMatch/debugHUD.debugHUD_C'");
		if (UClass* MyWidgetClass = MyWidgetClassRef.TryLoadClass<UDebugWidget>())
			debugWidget = CreateWidget<UDebugWidget>(owningActor->GetGameInstance(), MyWidgetClassRef.ResolveClass());

		if(debugWidget)
		{
			debugWidget->BindProps(&currentPose, MotionDataAsset);
			debugWidget->animBones.AddDefaulted(MotionDataAsset->PoseMatchingBones.Num());

			DECLARE_DEBUG_VECTOR(rootVel, "Anim Root Velocity")
			DECLARE_DEBUG_VECTOR(inputDir, "Input Direction")
			DECLARE_DEBUG_FLOAT(desiredFacing, "Desired Facing")
			DECLARE_DEBUG_FLOAT(naturalBias, "Natural Bias")
			DECLARE_DEBUG_FLOAT(loopBias, "Loop Bias")
			DECLARE_DEBUG_FLOAT(steadyBias, "Input Steadiness")
			DECLARE_DEBUG_FLOAT(rootRotWarp, "Rotation Error (steering)")
			DECLARE_DEBUG_FLOAT(lastBestCost, "Latest Cost")

			debugWidget->AddToViewport();
		}
#endif

		FTransform actorT = owningActor->GetTransform();
		FTransform componentToWorldT = AnimInstance->GetOwningComponent()->GetComponentTransform();
		csToWs = componentToWorldT;

		localMeshCompPos = actorT.InverseTransformPosition(componentToWorldT.GetLocation());
		localMeshCompRot = actorT.InverseTransformRotation(componentToWorldT.GetRotation()).Rotator().Euler();

		componentDeltaTransform = componentToWorldT;

		inertializationNodes.Empty();

		//
		// Pose matching stuff
		//

		PoseMatchingBones.Empty(MotionDataAsset->PoseMatchingBones.Num());

		for (FBoneReference boneRef : MotionDataAsset->PoseMatchingBones)
			PoseMatchingBones.Add(boneRef);

		CenterOfMassBone = MotionDataAsset->CenterOfMassBone;

		footLockReceivers.Empty(2);
		footLockReceivers.Add(MotionDataAsset->LeftFootLockReceiver);
		footLockReceivers.Add(MotionDataAsset->RightFootLockReceiver);

		footLockFeet.Empty(2);
		footLockFeet.Add(MotionDataAsset->LeftFoot);
		footLockFeet.Add(MotionDataAsset->RightFoot);

		// Trajectory stuff

		// Reserve some space for the desired trajectory.
		desiredTrajectory.Empty();
		desiredTrajectory.AddDefaulted(MotionDataAsset->TrajectoryTimings.Num());

		firstFutureTrajectoryTiming = 0;
		for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
		{
			if (MotionDataAsset->TrajectoryTimings[i] < 0.f) continue;

			firstFutureTrajectoryTiming = i;
			break;
		}

		lastTrajectoryTime = MotionDataAsset->TrajectoryTimings[MotionDataAsset->TrajectoryTimings.Num() - 1];

		//
		// Past trajectory recording
		//

		// The first element. We always assume these timings are sorted.
		const float& firstTiming = MotionDataAsset->TrajectoryTimings[0];

		if (firstTiming < 0.f)
		{
			maxHistoryLength = FMath::Abs(firstTiming);

			int32 historyCount = FMath::CeilToInt(maxHistoryLength / RECORD_SAMPLE_RATE);

			pastHistory.Empty();
			pastHistory.AddDefaulted(historyCount + 1);
		}

		//
		// Custom Root Motion
		//

		moveComp = Cast<UCharacterMovementComponent>(owningActor->GetComponentByClass(UCharacterMovementComponent::StaticClass()));

		if (moveComp)
		{
			TSharedPtr<FRootMotionSource_Custom> customRootMotion = MakeShared<FRootMotionSource_Custom>();
			customRootMotion->InstanceName = "MM_ROOT_WARPING";
			customRootMotion->AccumulateMode = ERootMotionAccumulateMode::Override;
			customRootMotion->Priority = 500;
			customRootMotion->bInLocalSpace = false;
			customRootMotion->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
			RootMotionSourceID = moveComp->ApplyRootMotionSource(customRootMotion);
			UE_LOG(LogTemp, Warning, TEXT("FOUND MOVECOMP"));
		}
	}
}

void FAnimNode_MotionMatcher::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)

	FAnimNode_Base::CacheBones_AnyThread(Context);

	FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();

	for (FBoneReference& boneRef : PoseMatchingBones)
	{
		boneRef.Initialize(BoneContainer);
		//if (boneRef.IsValidToEvaluate()){}
	}

	for (FBoneReference& boneRef : footLockReceivers)
		boneRef.Initialize(BoneContainer);

	for (FBoneReference& boneRef : footLockFeet) {
		boneRef.Initialize(BoneContainer);
		UE_LOG(LogTemp, Warning, TEXT("footLockFeet INITIALIZED is valid?: %d"), boneRef.IsValidToEvaluate());
	}

	CenterOfMassBone.Initialize(BoneContainer);

	//MotionDataAsset->LeftFoot
	//BasePose.CacheBones(Context);
}

float InterpFacing(float desired, float dt)
{
	float newSpeed = (desired < 0.0f) ? -180.f : 180.f;
	const float AngleTolerance = 0.1f;

	if (FMath::IsNearlyEqual(desired, 0.0f, AngleTolerance))
	{
		newSpeed = 0.0f;
	}

	float inputDerived = desired * 2.0f;
	newSpeed = FMath::Lerp(newSpeed, inputDerived, 0.9f);

	return newSpeed;
}

void FAnimNode_MotionMatcher::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread)
	GetEvaluateGraphExposedInputs().Execute(Context); // This lets us use the connected inputs.

	if (!owningActor || !bHasValidMotionData) return;

	//
	// Forward declarations and variable updating
	//

	if (bFirstUpdate)
	{
		bFirstUpdate = false;

		// This object let's us communicate with the outside world!

		if (MotionMatcherInterface)
			MotionMatcherInterface->PairAnimNode(this);

		// We support multiple inertialization nodes to support a high number of concurrent transitions.
		// Too bad GetAncestor() only returns 1 node so we're gonna have to go through the node stack here.

		using FKey = FObjectKey;
		using FNodeStack = TArray<FAnimNode_Base*, TInlineAllocator<4>>;
		const FNodeStack* Stack = Context.GetSharedContext()->AncestorTracker.Map.Find(FKey(FAnimNode_Inertialization::StaticStruct()));

		if (Stack)
		{
			inertializationNodes.AddDefaulted(Stack->Num());

			for (int32 i = 0; i != inertializationNodes.Num(); ++i)
			{
				if ((*Stack).IsValidIndex(i))
					inertializationNodes[i].Node = static_cast<FAnimNode_Inertialization*>((*Stack)[i]);
			}
		}

		// Setup our pose watcher if it exists.
		poseWatcher = Context.GetAncestor<FAnimNode_PoseWatcher>();

		if (poseWatcher)
			poseWatcher->SetupBones(MotionDataAsset->PoseMatchingBones);
	}

	float deltaTime = Context.GetDeltaTime();
	timeSinceLastMatch += deltaTime;
	timeSinceLastSave += deltaTime;
	timeSinceLastBlend += deltaTime;
	bAnimChanged = false;

	FTransform actorT = owningActor->GetActorTransform();

	FRotator actorRotDelta = actorT.GetRotation().Rotator() - lastRotation;
	actorRotDelta.Normalize();

	lastPosition = actorT.GetLocation();//owningActor->GetActorLocation();
	lastRotation = actorT.GetRotation().Rotator();

	float actorYaw = lastRotation.Yaw + (localMeshCompRot.Z);

	currentRootVelocity = owningActor->GetVelocity();
	turnSpeed = actorRotDelta.Yaw / deltaTime;

	// Smooth the input
	desiredVecA = FMath::VInterpTo(desiredVecA, Input.DesiredVector * (SpeedMultiplier * 120), Context.GetDeltaTime(), 12.f);

	float stoppingBias = FVector::DistSquared2D(FVector::ZeroVector, desiredVecA);
	avgInputDir = UKismetMathLibrary::DynamicWeightedMovingAverage_FVector(Input.DesiredVector * (SpeedMultiplier * 120), avgInputDir, 1.02f, 0.0f, 0.08f);

	// Change interpolation speed to tweak instability sensitivity.
	inputSteady = FMath::VInterpTo(inputSteady, Input.DesiredVector, Context.GetDeltaTime(), 0.4f);

	float inputSteadiness = FVector::DistSquared(Input.DesiredVector, inputSteady);
	float facingSteadiness = FMath::Abs(Input.DesiredFacing) * 0.02f;
	steadyBias = 1.0f - FMath::Clamp(inputSteadiness + facingSteadiness, 0.f, 1.0f);

	// Update our blends that are supposedly active in our inert nodes array. May not be accurate.

	for (auto& inertialization : inertializationNodes)
	{
		for (int32 i = 0; i != inertialization.ActiveBlends.Num(); ++i)
			inertialization.ActiveBlends[i] -= deltaTime;

		inertialization.ActiveBlends.RemoveAll([](float Val) {
			return !(Val >= 0.f);
		});
	}

	//
	// Build the Desired Trajectory
	//

	FVector currentVelocity[32];

	for (int32 i = firstFutureTrajectoryTiming - firstFutureTrajectoryTiming, y = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i, ++y)
	{
		if (!MotionDataAsset->TrajectoryTimings.IsValidIndex(i)) continue;
		currentVelocity[y] = currentRootVelocity * MotionDataAsset->TrajectoryTimings[i];
	}

	float currentYaw = 0.0f;

	for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
	{
		const float& timing = MotionDataAsset->TrajectoryTimings[i];
		desiredTrajectory[i].Position = FVector::ZeroVector;

		if (timing < 0.f) // Add past trajectory to the desired trajectory.
		{
			float rewindTime = FMath::Abs(timing);
			int32 historyIndex = FMath::FloorToInt(rewindTime / RECORD_SAMPLE_RATE);

			desiredTrajectory[i].Position = FVector::ZeroVector;

			if (pastHistory.IsValidIndex(historyIndex)) {
				FPastSnapshot& snap = pastHistory[historyIndex];

				if (snap.Position != FVector::ZeroVector) // Most likely a default snapshot.
				{
					FVector position = snap.Position + ((snap.Velocity) * (snap.TimeSince) - snap.Velocity * rewindTime)
						- lastPosition;
					position = position.RotateAngleAxis(90 - owningActor->GetActorRotation().Yaw, FVector::UpVector);

					FRotator rotDelta = snap.Rotation - lastRotation;
					rotDelta.Normalize();

					desiredTrajectory[i].Position = position;
					desiredTrajectory[i].Facing = rotDelta.Yaw;
				}
			}

			if (i == firstFutureTrajectoryTiming - 1) // last historical point
			{
				currentYaw = -1.f * desiredTrajectory[i].Facing / MotionDataAsset->TrajectoryTimings[i];
			}
		}
		else // Future desired trajectory. I've tried using springs but nothing looked as good as simply blending with velocity.
		{
			const float& timingDelta = MotionDataAsset->TrajectoryTimings[i]; // e.g., -0.2, 0.5, 1.8, etc.
			const float& timingDeltaAlpha = MotionDataAsset->TrajectoryTimings[i] / lastTrajectoryTime;

			currentVelocity[i] = currentVelocity[i].RotateAngleAxis(-1 * actorYaw, FVector::UpVector);

			desiredTrajectory[i].Position = FMath::Lerp(currentVelocity[i], desiredVecA * timingDelta, timingDelta / lastTrajectoryTime);
			desiredTrajectory[i].Facing = FMath::Clamp(FMath::FInterpTo(turnSpeed * timingDelta, Input.DesiredFacing * timingDelta, timingDelta / lastTrajectoryTime, 3.f), -90.f, 90.f);//InterpFacing(Input.DesiredFacing, timingDelta)* timingDelta;


			if (i == MotionDataAsset->TrajectoryTimings.Num() - 1)
			{
				desiredTrajectory[i].Facing = Input.DesiredFacing;
			}

			//desiredTrajectory[i].Facing = UKismetMathLibrary::FInterpEaseInOut(currentYaw, Input.DesiredFacing, timingDeltaAlpha, 2.f);

//			UE_LOG(LogTemp, Warning, TEXT("speed: %s"), *f(desiredTrajectory[i].Facing));


			//desiredTrajectory[i].Facing = 45.f * timingDelta;

			//desiredTrajectory[i].Facing = UKismetMathLibrary::FloatSpringInterp(turnSpeed * timingDelta, Input.DesiredFacing, fakeState, 25.0f, 1.0f, timingDelta / lastTrajectoryTime, 1.0f);
			//desiredTrajectory[i].Facing = FMath::Lerp(turnSpeed * timingDelta, Input.DesiredFacing * timingDelta, timingDelta / lastTrajectoryTime);
			//desiredTrajectory[i].Facing = FMath::FInterpTo(turnSpeed * timingDelta, Input.DesiredFacing, timingDelta / lastTrajectoryTime, 12.f);
			//desiredTrajectory[i].Facing = FMath::FInterpConstantTo(turnSpeed * timingDelta, Input.DesiredFacing, timingDelta / lastTrajectoryTime, 2200.f);
			//UE_LOG(LogTemp, Warning, TEXT("Time: %f, Facing: %f"), turnSpeed * timingDelta, Input.DesiredFacing);

			//CubicInterp::
			//FMath::FInterpConstantTo();
		}
	}

	yawStep = InterpFacing(Input.DesiredFacing, deltaTime) * deltaTime;

	// Past trajectory history

	if (timeSinceLastSave > RECORD_SAMPLE_RATE && firstFutureTrajectoryTiming != 0)
	{
		timeSinceLastSave = 0.f;

		FPastSnapshot snapshot;
		snapshot.Position = owningActor->GetActorLocation();
		snapshot.Rotation = owningActor->GetActorRotation();
		snapshot.Velocity = owningActor->GetVelocity();
		// snapshot.Facing ???
		snapshot.Facing = turnSpeed;
		snapshot.TimeSince = 0.f;

		pastHistory.Pop();
		pastHistory.Insert(snapshot, 0);
	}

	for (auto& record : pastHistory)
		record.TimeSince += deltaTime;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject()))
	{
		// Find out our current motion.
		EvaluatePoseSample(currentPlayData.MatchedStateIndex, currentPlayData.CurrentPlayTime, currentPose);

		// Pose sample pre-process includes normalization so we need to normalize and apply weights to our new trajectory.
		MotionDataAsset->NormalizeTrajectory(desiredTrajectory);

		// use real velocity?
		currentPose.RootVelocity = currentRootVelocity;
		currentPose.RootVelocity = currentPose.RootVelocity.RotateAngleAxis(-1 * actorYaw, FVector::UpVector);
		MotionDataAsset->NormalizeFeature(currentPose.RootVelocity, MotionDataAsset->NormalData_RootVelocity);
		currentPose.RootVelocity *= MotionDataAsset->RootVelocityWeight;

		//
		// Update foot IK locking from current pose
		//

		UpdateFootLock(deltaTime, AnimInstance);

		if (timeSinceLastMatch > MOTION_MATCHING_INTERVAL)
		{
			MatchNow();
		}


		////////////////////////////////////////////////////////////////////////////////////////////////////////////


		//
		// Candiate Warping to match even better the desired pose/trajectory.
		//


		TArray<FTrajectoryPoint> rawCurrentTraj = currentPose.Trajectory;
		TArray<FTrajectoryPoint> rawDesiredTraj = desiredTrajectory;

		MotionDataAsset->UnnormalizeTrajectory(rawCurrentTraj);
		MotionDataAsset->UnnormalizeTrajectory(rawDesiredTraj);

		FTrajectoryPoint& lastDesiredPoint = rawDesiredTraj[rawDesiredTraj.Num() - 1];
		FTrajectoryPoint& lastFuturePoint = rawCurrentTraj[rawCurrentTraj.Num() - 1];


		float desiredSpeed = lastDesiredPoint.Position.Size();
		float futureSpeed = lastFuturePoint.Position.Size();

		// Only time warp if there's input. Otherwise stay at 1.0 to quickly finish the stop animation.
		if (Input.DesiredVector.SizeSquared() > 0.1f)
		{
			currentTimeScaleWarp = FMath::Clamp(desiredSpeed / futureSpeed, 0.8f, 1.2f); // clamp to 20% diff. 
		}
		else {
			currentTimeScaleWarp = 1.0f;
		}



		// Trajectory warp

		if (moveComp)
		{
			TSharedPtr<FRootMotionSource> RMS = moveComp->GetRootMotionSourceByID(RootMotionSourceID);
			if (RMS.IsValid() && RMS->GetScriptStruct() == FRootMotionSource_Custom::StaticStruct())
			{
				FRootMotionSource_Custom* customRootMotion = static_cast<FRootMotionSource_Custom*>(RMS.Get());
				if (customRootMotion)
				{
					FTransform rootT;

					FQuat rootMotionQuat = rootMotion.GetRotation();

					FVector inputPos = (lastInputVec * 1).RotateAngleAxis(localMeshCompRot.Z, FVector::UpVector);
					FVector rootPos = (rootMotion.GetLocation() * 120).RotateAngleAxis(actorYaw, FVector::UpVector);


					FVector DesiredDirection = FVector::ZeroVector;
					const float FacingAngle = FMath::DegreesToRadians(lastDesiredPoint.Facing);
					DesiredDirection = FVector(FMath::Sin(FacingAngle), 0.0f, FMath::Cos(FacingAngle)).GetSafeNormal() * -1.0f;

					//DesiredDirection.HeadingAngle()

					FVector vecA = rawCurrentTraj[firstFutureTrajectoryTiming].Position;
					FVector vecB = rawDesiredTraj[firstFutureTrajectoryTiming].Position;

					//float sizeA = FMath::Clamp(vecA.Size() * 0.016f, 0.f, 1.0f);
					// The angle for rotation error warping (or steering) doesn't matter if we're moving too slow.
					float sizeB = FMath::Clamp(vecB.Size() * 0.016f, 0.f, 1.0f);

					//Context.AnimInstanceProxy->AnimDrawDebugSphere((lastPosition - FVector(0.f, 0.f, -1 * localMeshCompPos.Z)) + vecA.RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector), sizeA * 16.f, 32, FColor::Blue);
					//Context.AnimInstanceProxy->AnimDrawDebugSphere((lastPosition - FVector(0.f, 0.f, -1 * localMeshCompPos.Z)) + vecB.RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector), sizeB * 16.f, 32, FColor::Orange);

					vecA.Normalize();
					vecB.Normalize();

					float dot = FVector::DotProduct(vecA, vecB);
					float rightDot = FVector::DotProduct(FVector::CrossProduct(FVector::UpVector, vecA), vecB);
					float angle = FMath::RadiansToDegrees(FMath::Acos(dot)) * sizeB;

					// Invert the angle if we're to the left of our trajectory.
					// We do this by seeing if vecB is pointing in the same direction as the right dot of vecA.
					if (rightDot < 0.f) angle *= -1;


					// Normally we'd get crazy numbers if the vectors aren't pointing in the right direction.
					// (Idle^Walk, StrafeRight^StrafeLeft, Forward^Backward, etc.)
					if (dot > KINDA_SMALL_NUMBER)
					{
						rootRotWarp = FMath::FInterpTo(rootRotWarp, angle, deltaTime, 3.0f);
						//rootRotWarp = angle * sizeB;
					}
					/*else
					{
						rootRotWarp = 0.f; // new stuff
					}*/

					// If we just switched animations, reset the rotation to prevent a jump.
					if (bAnimChanged)
						rootRotWarp = 0.f;

					// Character movement 

					const float halfLife = 0.4f; // 0.2f;

					FVector correctedInput = (120.0f * Input.DesiredVector).RotateAngleAxis(actorYaw, FVector::UpVector);

					traj_xv_goal = correctedInput.X;
					traj_yv_goal = correctedInput.Y;

					spring_character_update(trajx, trajxv, trajxa, traj_xv_goal, halfLife, deltaTime);
					spring_character_update(trajy, trajyv, trajya, traj_yv_goal, halfLife, deltaTime);

					//FQuat inputQuat = FQuat::MakeFromEuler(FVector(0.f, 0.f, lastDesiredPoint.Facing) * deltaTime);
					const float& facing = rawDesiredTraj[firstFutureTrajectoryTiming].Facing;

					//float pointTurnSpeed = yawStep;//facing / firstFutureTrajectoryTiming;

					FQuat inputQuat = FQuat::MakeFromEuler(FVector(0.f, 0.f, yawStep));
					//					UE_LOG(LogTemp, Warning, TEXT("pointTurnSpeed %f"), pointTurnSpeed);
										//FQuat inputQuat = FQuat::MakeFromEuler(FVector(0.f, 0.f, 0.f * deltaTime));

					rootT.SetRotation(
						//rootMotionQuat
						//FQuat::FastLerp(rootMotionQuat, inputQuat, 0.5f)
						inputQuat
					);
					rootT.SetLocation(FVector(trajxv, trajyv, 0.f));
					

					// Apply the root motion
					customRootMotion->RootMotion = rootT;
				}
			}
		} // if moveComp

		// It's time to play the animation
		FMMatcherState& State = MotionDataAsset->States[currentPlayData.MatchedStateIndex];
		UAnimSequence* anim = State.Animation;

		FAnimInstanceProxy* AnimProxy = Context.AnimInstanceProxy;
		FAnimGroupInstance* SyncGroup;
		FAnimTickRecord& TickRecord = AnimProxy->CreateUninitializedTickRecord(SyncGroup, NAME_None);

		AnimProxy->MakeSequenceTickRecord(TickRecord, anim, State.bLoop, currentTimeScaleWarp, 1.0f, currentPlayData.CurrentPlayTime, currentPlayData.MarkerTickRecord);

#if WITH_EDITOR
		if (bDebugMode) DrawDebug(Context); else if (debugWidget && debugWidget->bShow) debugWidget->bShow = 0;
#endif
	}
}

void FAnimNode_MotionMatcher::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)

	// Calc root vel?
	FVector newPose = Output.Pose.GetBones()[0].GetLocation();
	rootBoneVel = newPose - rootBonePos;
	rootBonePos = newPose;

	if (!bHasValidMotionData)
	{
		Output.ResetToRefPose();
		return;
	}

	FAnimInstanceProxy* AnimProxy = Output.AnimInstanceProxy;

	UAnimSequence* anim = MotionDataAsset->States[currentPlayData.MatchedStateIndex].Animation;

	// Single animation, no blending needed.
	FAnimationPoseData AnimationPoseData(Output);

	anim->GetAnimationPose(AnimationPoseData, FAnimExtractContext(currentPlayData.CurrentPlayTime, true));//Output.AnimInstanceProxy->ShouldExtractRootMotion()
	rootMotion = anim->ExtractRootMotion(currentPlayData.CurrentPlayTime, AnimProxy->GetDeltaSeconds() * currentTimeScaleWarp, true);

	// Candidate warping of root

	// Build our desired rotation
	const FRotator DeltaRotation(rootRotWarp, 0.f, 0.f);
	const FQuat DeltaQuat(DeltaRotation);
	const FQuat MeshToComponentQuat(FRotator::ZeroRotator);

	// Convert our rotation from Component Space to Mesh Space.
	const FQuat MeshSpaceDeltaQuat = MeshToComponentQuat.Inverse() * DeltaQuat * MeshToComponentQuat;

	// Apply rotation to root bone.
	FCompactPoseBoneIndex RootBoneIndex(0);
	Output.Pose[RootBoneIndex].SetRotation(Output.Pose[RootBoneIndex].GetRotation() * DeltaQuat);
	Output.Pose[RootBoneIndex].NormalizeRotation();


	Output.Pose.GetBoneContainer();
	const FBoneContainer& BoneContainer = AnimationPoseData.GetPose().GetBoneContainer();
	
	// Save new pose?
	FCSPose<FCompactPose> CSPose;
	//CSPose.InitPose(AnimationPoseData.GetPose());
	CSPose.InitPose(Output.Pose);

	if (CenterOfMassBone.IsValidToEvaluate(BoneContainer))
	{
		float actorYaw = owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z);

		auto boneT = CSPose.GetComponentSpaceTransform(CenterOfMassBone.GetCompactPoseIndex(BoneContainer));
		FVector bonePos = boneT.GetLocation();

		centerOfMass = FVector2D(lastPosition.X, lastPosition.Y) + FVector2D(bonePos.X, bonePos.Y).GetRotated(actorYaw);
	}

	// Optional IK stuff

	auto leftCurveUID = Output.AnimInstanceProxy->GetSkeleton()->GetUIDByName(USkeleton::AnimCurveMappingName, LeftIKAlphaCurveName);
	auto rightCurveUID = Output.AnimInstanceProxy->GetSkeleton()->GetUIDByName(USkeleton::AnimCurveMappingName, RightIKAlphaCurveName);

	if(leftCurveUID)
		Output.Curve.Set(leftCurveUID, ik_left_alpha);

	if (rightCurveUID)
		Output.Curve.Set(rightCurveUID, ik_right_alpha);


	
	FTransform ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	
	//ComponentTransform.SetRotation();csToWs.GetRotation()

	//Output.CustomAttributes.
	for (int32 footIndex = 0; footIndex != 2; ++footIndex)
	{
		FBoneReference& footBone = footLockFeet[footIndex];
		FBoneReference& vBone = footLockReceivers[footIndex];

		if (footBone.IsValidToEvaluate() && vBone.IsValidToEvaluate())
		{
			FTransform NewBoneTM = CSPose.GetComponentSpaceTransform(vBone.CachedCompactPoseIndex);
			//FCompactPoseBoneIndex vBoneIndex = vBone.GetCompactPoseIndex(BoneContainer);
			
			NewBoneTM.SetTranslation(FVector(0.f, 0.f, 100.f));
			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, CSPose, NewBoneTM, vBone.CachedCompactPoseIndex, EBoneControlSpace::BCS_WorldSpace);
			FAnimationRuntime::ConvertCSTransformToBoneSpace(ComponentTransform, CSPose, NewBoneTM, vBone.CachedCompactPoseIndex, EBoneControlSpace::BCS_BoneSpace);
			//NewBoneTM.SetRotation(FQuat::MakeFromEuler(FVector(90.f, 45.f, 90.f)));

			//NewBoneTM.SetLocation(Output.Pose[vBone.CachedCompactPoseIndex].GetLocation());
			//UE_LOG(LogTemp, Warning, TEXT("NewBoneTM: %f, %f, %f"), NewBoneTM.GetScale3D().X, NewBoneTM.GetScale3D().Y, NewBoneTM.GetScale3D().Z);
			UE_LOG(LogTemp, Warning, TEXT("NewBoneTM: %f, %f, %f"), NewBoneTM.GetLocation().X, NewBoneTM.GetLocation().Y, NewBoneTM.GetLocation().Z);
			
			

			// Convert back to Component Space.
			//FAnimationRuntime::ConvertBoneSpaceTransformToCS(ComponentTransform, CSPose, NewBoneTM, vBone.CachedCompactPoseIndex, EBoneControlSpace::BCS_WorldSpace);

			//CSPose.SetComponentSpaceTransform(vBone.CachedCompactPoseIndex, NewBoneTM);

			//const FVector& vec = foo;
			//CSPose.GetPose()[vBone.CachedCompactPoseIndex].;
			// Get the bone position from the animation. TODO: make this worldspace.
			Output.Pose[vBone.CachedCompactPoseIndex].SetLocation(NewBoneTM.GetLocation()); //Output.Pose[footBone.CachedCompactPoseIndex].GetLocation()
		}
	}
}

void FAnimNode_MotionMatcher::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData);

	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);

	UE_LOG(LogTemp, Warning, TEXT("DEBUG"));

	//BasePose.GatherDebugData(DebugData);
}

const int32& FAnimNode_MotionMatcher::GetStateIndex()
{
	return currentPlayData.MatchedStateIndex;
}

void FAnimNode_MotionMatcher::MatchNow()
{
	SCOPE_CYCLE_COUNTER(STAT_MMIndexSearch);
	timeSinceLastMatch = 0.f;

	float bestCost = MAX_FLT;
	int32 bestStateIndex = 0;
	int32 bestPoseIndex = 0;
	float bestPoseTime = 0.f;

	bool bFindLoop = false;

	FMMatcherState& currentState = MotionDataAsset->States[currentPlayData.MatchedStateIndex];
	if (currentState.Animation)
	{
		float timeLeft = currentState.Animation->GetPlayLength() - currentPlayData.CurrentPlayTime;

		// If the anim clip is ending and it is not a loop (transition), don't even consider it for the next match. Only search for loops in this case.
		if (timeLeft < MOTION_MATCHING_BLEND_TIME && !currentState.bLoop)
			bFindLoop = true;
	}

	//
	// Pose search
	//

	for (int32 stateIndex = 0; stateIndex < MotionDataAsset->States.Num(); stateIndex++)
	{
		FMMatcherState& state = MotionDataAsset->States[stateIndex];

		if (bFindLoop && !state.bLoop)
			continue; // early exit

		for (int32 poseIndex = 0; poseIndex < state.CachedPoses.Num(); poseIndex++)
		{
			const FMMatcherPoseSample& candidatePose = state.CachedPoses[poseIndex];

			float cost = PoseDistSquared(currentPose, candidatePose);

			if (cost < bestCost)
			{
				bestCost = cost;
				bestStateIndex = stateIndex;
				bestPoseIndex = poseIndex;
				bestPoseTime = state.CachedPoses[poseIndex].Time;
			}
		}
	}

	// If the current and matched animation are the same and it is a looping anim, then don't jump anywhere.
	bool bLooping = bestStateIndex == currentPlayData.MatchedStateIndex &&
		MotionDataAsset->States[currentPlayData.MatchedStateIndex].bLoop;

	bool bWinnerAtSameLocation = bestStateIndex == currentPlayData.MatchedStateIndex &&
		FMath::Abs(currentPlayData.CurrentPlayTime - bestPoseTime) < 1.0f || currentPlayData.MatchedPoseIndex == bestPoseIndex;
	//FMath::Abs(lastBestCost - bestCost) < 10.f;
//bool bWinnerAtSameLocation = bestStateIndex == currentPlayData.MatchedStateIndex;

	int32 totalBlends = 0;

	for (auto& node : inertializationNodes)
	{
		totalBlends += node.ActiveBlends.Num();
	}


	if (!bWinnerAtSameLocation && !bLooping && timeSinceLastBlend > 0.1f)
	{
		for (auto& node : inertializationNodes)
		{
			if (node.ActiveBlends.Num() < 2)
			{
				// custom blend time
				if (currentState.CustomBlendTimes.Contains(bestStateIndex))
					currentPlayData.BlendTime = currentState.CustomBlendTimes[bestStateIndex];
				else
					currentPlayData.BlendTime = MotionDataAsset->BlendTime;

				node.ActiveBlends.Push(currentPlayData.BlendTime);

				//UE_LOG(LogTemp, Warning, TEXT("SWITCH! Old Anim: %d, New Anim: %d. Time since last switch: %f, pending blends: %d, requested blend time: %f"), currentPlayData.MatchedPoseIndex, bestPoseIndex, timeSinceLastBlend, totalBlends, currentPlayData.BlendTime);
				timeSinceLastBlend = 0.0f;


				// Switch animation
				currentPlayData.MatchedStateIndex = bestStateIndex;
				currentPlayData.MatchedPoseIndex = bestPoseIndex;
				currentPlayData.CurrentPlayTime = MotionDataAsset->States[currentPlayData.MatchedStateIndex].CachedPoses[currentPlayData.MatchedPoseIndex].Time;
				lastBestCost = bestCost;

				node.Node->RequestInertialization(currentPlayData.BlendTime);

				bAnimChanged = true;
				break;
			}
		}
	}
}

void FAnimNode_MotionMatcher::EvaluatePoseSample(int32 stateIndex, float time, FMMatcherPoseSample& outPoseSample)
{
	const TArray<FMMatcherPoseSample>& poseLibrary = MotionDataAsset->States[stateIndex].CachedPoses;

	if (poseLibrary.Num() == 0) return;

	// This gives us the pose sample index with floating point accuracy. Round to find nearest sample.
	float poseIndexValue = (time / MotionDataAsset->MotionCacheSamplingRate);

	// These rounded indices should be valid as long as they're in range of the array.
	int32 earliestPoseIndex = FMath::FloorToInt(poseIndexValue);
	int32 latestPoseIndex = FMath::CeilToInt(poseIndexValue);
	float tweenAlpha = poseIndexValue - earliestPoseIndex;

	if (!poseLibrary.IsValidIndex(earliestPoseIndex) || !poseLibrary.IsValidIndex(latestPoseIndex))
	{
		// early exit if no poses to blend from/to (out-of-range).
		outPoseSample = poseLibrary[poseLibrary.Num() - 1];
		return;
	}

	const FMMatcherPoseSample& currentSample = poseLibrary[earliestPoseIndex];
	const FMMatcherPoseSample& nextSample = poseLibrary[latestPoseIndex];
	const FMMatcherPoseSample& closestSample = poseLibrary[FMath::RoundToInt(poseIndexValue)];

	//
	// Interpolation
	//

	outPoseSample.Id = poseLibrary[FMath::RoundToInt(poseIndexValue)].Id; // Assume the closest Id.
	outPoseSample.StateId = closestSample.StateId;
	outPoseSample.Time = time;
	outPoseSample.bLFootLock = closestSample.bLFootLock;
	outPoseSample.bRFootLock = closestSample.bRFootLock;
	outPoseSample.FootLocks = closestSample.FootLocks;
	outPoseSample.RootVelocity = FMath::Lerp(currentSample.RootVelocity, nextSample.RootVelocity, tweenAlpha);
	outPoseSample.FacingAxis = FMath::Lerp(currentSample.FacingAxis, nextSample.FacingAxis, tweenAlpha);

	// While we could do fancy quaternion interpolation to prevent rotational glitches, here we save computation time by assuming that
	// the rotational speed will never be too fast and go into negative. E.g., flipping from 179* to -179*
	outPoseSample.RootRotationSpeed = FMath::Lerp(currentSample.RootRotationSpeed, nextSample.RootRotationSpeed, tweenAlpha);

	// Pose Matching Bones.
	outPoseSample.BoneData.Empty();
	outPoseSample.BoneData.AddDefaulted(currentSample.BoneData.Num());

	if (poseWatcher) // Use live bone data instead of offline data.
	{
		TArray<FMMatcherBoneData> boneData = poseWatcher->GetBoneData();

		for (int32 i = 0; i != outPoseSample.BoneData.Num(); ++i)
		{
			if(boneData.IsValidIndex(i))
			{				
				FMMatcherBoneData& outBone = outPoseSample.BoneData[i];
				outBone = boneData[i];

				MotionDataAsset->NormalizeFeature(outBone.Position, MotionDataAsset->NormalData_BonePosition[i]);
				MotionDataAsset->NormalizeFeature(outBone.Velocity, MotionDataAsset->NormalData_BoneVelocity[i]);
			}
		}
	}
	else // Use offline data (skips with no blending).
	{
		for (int32 i = 0; i != currentSample.BoneData.Num(); ++i)
		{
			FMMatcherBoneData& outBone = outPoseSample.BoneData[i];
			const FMMatcherBoneData& currentBone = currentSample.BoneData[i];
			const FMMatcherBoneData& nextBone = nextSample.BoneData[i];

			outBone.Position = FMath::Lerp(currentBone.Position, nextBone.Position, tweenAlpha);
			outBone.Velocity = FMath::Lerp(currentBone.Velocity, nextBone.Velocity, tweenAlpha);
		}
	}

	// Trajectory
	outPoseSample.Trajectory.Empty();
	outPoseSample.Trajectory.AddDefaulted(currentSample.Trajectory.Num());

	for (int32 i = 0; i != currentSample.Trajectory.Num(); ++i)
	{
		FTrajectoryPoint& outPoint = outPoseSample.Trajectory[i];
		const FTrajectoryPoint& currentPoint = currentSample.Trajectory[i];
		const FTrajectoryPoint& nextPoint = nextSample.Trajectory[i];

		outPoint.Position = FMath::Lerp(currentPoint.Position, nextPoint.Position, tweenAlpha);

		FQuat currentQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(currentPoint.Facing));
		FQuat nextQuat = FQuat(FVector::UpVector, FMath::DegreesToRadians(nextPoint.Facing));

		FQuat lerpedQuat = FQuat::FastLerp(currentQuat, nextQuat, tweenAlpha);

		outPoint.Facing = lerpedQuat.Rotator().Yaw;
	}
}

float FAnimNode_MotionMatcher::TrajDistSquared(const FTrajectory& goal, const FTrajectory& candidate)
{
	float dist = 0.f;

	for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
	{
		dist += ATTR_DIST_VEC(goal[i].Position, candidate[i].Position);
		dist += ATTR_DIST(goal[i].Facing, candidate[i].Facing);
	}

	return dist;
}

float FAnimNode_MotionMatcher::PoseDistSquared(const FMMatcherPoseSample& goal, const FMMatcherPoseSample& candidate)
{
	const FMMatcherState& state = MotionDataAsset->States[candidate.StateId];

	float dist = 0.f;

	// Add all features as dimensions
	dist += ATTR_DIST_VEC(goal.RootVelocity, candidate.RootVelocity);

	for (int32 i = 0; i != goal.BoneData.Num(); ++i)
	{
		dist += ATTR_DIST_VEC(goal.BoneData[i].Position, candidate.BoneData[i].Position);
		dist += ATTR_DIST_VEC(goal.BoneData[i].Velocity, candidate.BoneData[i].Velocity);
	}

	for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
	{
		dist += ATTR_DIST_VEC(desiredTrajectory[i].Position, candidate.Trajectory[i].Position);
		dist += ATTR_DIST(desiredTrajectory[i].Facing, candidate.Trajectory[i].Facing);
	}

	// biases

	if (goal.StateId == candidate.StateId)
		dist *= (1.0f - ((goal.Id == candidate.Id) ? MotionDataAsset->NaturalBias : MotionDataAsset->NaturalBias * 0.5f));
		
	if (state.bLoop)
		dist *= (1.0f - (MotionDataAsset->LoopBias * steadyBias));

	/*if (currentPose.StateId == candidatePose.StateId)
		dist -= (currentPose.Id == candidatePose.Id) ? MotionDataAsset->NaturalBias * 2.0f : MotionDataAsset->NaturalBias;

	if (state.bLoop)
		dist -= MotionDataAsset->LoopBias * steadyBias; // loop bias is multiplied by how steady our input is.*/

	//dist += state.bStopping ? 0.0f : MotionDataAsset->StoppingBias;

	return dist;
}

bool FAnimNode_MotionMatcher::IsPointInBalancingArea(FVector2D point)
{
	// The balancing area is an obround/capsule shape with the left/right foot vectors as endpoints.
	// We start by checking for the max distance between the point and the two left/right vectors, and
	// then we check if the point is in a rectangle between the two left/right vectors.

	// The radiuses of the left/right endpoints as well as the half-height of the rect.
	const float radius = 12.f;

	FVector& lFootPos = lockedfootPositions[0];
	FVector& rFootPos = lockedfootPositions[1];

	FVector2D leftFoot = FVector2D(lFootPos.X, lFootPos.Y);
	FVector2D rightFoot = FVector2D(rFootPos.X, rFootPos.Y);
	FVector2D areaCenter = FMath::Lerp(leftFoot, rightFoot, 0.5f);
	float sideLength = FVector2D::Distance(leftFoot, rightFoot);

	// Calculate the perpendicular vector (normal)

	FVector2D normal = leftFoot - areaCenter;
	normal = FVector2D(normal.Y * -1.f, normal.X).GetSafeNormal();

	// Slightly offset our shape since humans can tilt forward more than backwards.
	leftFoot += normal * 4.f; // 4cm
	rightFoot += normal * 4.f;

	// First check the left/right end-points

	float posLeftFootDist = FVector2D::Distance(point, leftFoot);
	float posRightFootDist = FVector2D::Distance(point, rightFoot);

	if (posLeftFootDist < radius || posRightFootDist < radius)
		return true;

	// Now check the rectangle

	/*
		vA--------------vB
		|                |
		|                |
		|                |
		vC--------------vD
	*/

	FVector2D vA = leftFoot + (normal * radius);
	FVector2D vB = rightFoot + (normal * radius);
	FVector2D vC = leftFoot + (normal * radius * (-1.f));
	FVector2D vD = rightFoot + (normal * radius * (-1.f));
	float rectArea = sideLength * radius * 2.f;

	/*
		Point in Rectangle problem. We start by splitting the rect into 3 triangles with our test vector
		as the third point in each triangle. If the area sum of all tris is larger than the area of our
		rect then the point is outside.

		vA---------------vB
		|    \  ABP  /    |
		| CAP \.---./ BDP |
		|    /  CDP  \    |
		vC---------------vD
	*/

	FVector2D vP = point;

	float ABP = 0.5f * abs(vA.X * (vB.Y - vP.Y) + vB.X * (vP.Y - vA.Y) + vP.X * (vA.Y - vB.Y));
	float CAP = 0.5f * abs(vC.X * (vA.Y - vP.Y) + vA.X * (vP.Y - vC.Y) + vP.X * (vC.Y - vA.Y));
	float CDP = 0.5f * abs(vD.X * (vC.Y - vP.Y) + vC.X * (vP.Y - vD.Y) + vP.X * (vD.Y - vC.Y));
	float BDP = 0.5f * abs(vB.X * (vD.Y - vP.Y) + vD.X * (vP.Y - vB.Y) + vP.X * (vB.Y - vD.Y));

	// The 1.0f fixes issues with floating point rounding errors.
	bool bPointInRec = !((rectArea + 1.0f) < (ABP + CAP + CDP + BDP));

	if (bPointInRec)
		return true;

	// not in rectangle or both end points. Must be out then.
	return false;
}

void FAnimNode_MotionMatcher::UpdateFootLock(float dt, UAnimInstance* animInst)
{
	timeSinceLock += dt;


	bool bLeftLocked = (bool)currentPose.FootLocks[0]; // Lock in the animation
	bool bRightLocked = (bool)currentPose.FootLocks[1]; // Lock in the animation

	if (bLeftLocked)
		ik_left_alpha = 1.0f;
	else
		ik_left_alpha = 0.f;

	if (bRightLocked)
		ik_right_alpha = 1.0f;
	else
		ik_right_alpha = 0.f;

	if (!MotionMatcherInterface) return;


	float actorYaw = owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z);
	float speed = currentRootVelocity.SizeSquared2D() + FMath::Abs(turnSpeed);

	if (speed < 0.1f)
	{
		timeStandingStill += dt;
	}
	else
	{
		timeStandingStill = 0.f;
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////////

	if (MotionMatcherInterface && currentPose.Id != 0)
	{
		TArray<FMMatcherBoneData> animBoneData = currentPose.BoneData;
		MotionDataAsset->UnnormalizeBoneData(animBoneData);

		//float actorYaw = owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z);


		for (int32 foot = 0; foot != 2; ++foot)
		{
			FBoneReference& footBone = footLockFeet[foot];
			bool bPoseLocked = (bool)currentPose.FootLocks[foot]; // Lock in the animation
			bool& bLocked = footLocks[foot]; // is it currently locked?
			bool bLockNow = bPoseLocked && !bLocked;
			bool bUnlockNow = !bPoseLocked && bLocked;

			if (bLockNow || bUnlockNow)
			{
				// Find the bone

				for (int32 boneIndex = 0; boneIndex != PoseMatchingBones.Num(); ++boneIndex)
				{
					if (PoseMatchingBones[boneIndex].BoneName == footBone.BoneName)
					{
						UE_LOG(LogTemp, Warning, TEXT("Pose: %d"), currentPose.Id);


						const FMMatcherBoneData& boneData = animBoneData[boneIndex];
						FVector bonePos = lastPosition + localMeshCompPos + boneData.Position.RotateAngleAxis(actorYaw, FVector::UpVector);
						FVector boneVel = lastPosition + localMeshCompPos + (boneData.Position + (boneData.Velocity / 4)).RotateAngleAxis(actorYaw, FVector::UpVector);
						UE_LOG(LogTemp, Warning, TEXT("New lock: %f, %f, %f"), bonePos.X, bonePos.Y, bonePos.Z);

						bLocked = bPoseLocked;
						lockedfootPositions[foot] = bonePos;

						MotionMatcherInterface->UpdateFootLock(foot, bLocked, lockedfootPositions[foot]);

						break;
					}
				}
			}
		}		
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	return;

	/*
	if (MotionMatcherInterface)
	{
		bool foot = MM_FOOT::RIGHT;

	nextFoot:
		foot = !foot;
		FName footBoneName = (foot == MM_FOOT::LEFT) ? MotionDataAsset->LeftFoot.BoneName : MotionDataAsset->RightFoot.BoneName;

		bool bOutOfBalance = !IsPointInBalancingArea(centerOfMass);

		for (int32 i = 0; i != PoseMatchingBones.Num(); ++i)
		{
			FBoneReference& boneRef = PoseMatchingBones[i];
			FMMatcherBoneData& boneData = animBoneData[i];

			if (boneRef.BoneName == footBoneName)
			{
				bool bPoseLocked = (bool)currentPose.FootLocks[foot]; // Lock in the animation
				bool& bLocked = footLocks[foot]; // is it currently locked?
				bool bSupposedToBeLocked = bPoseLocked && !bLocked;
				bool bNotSupposedToBeLocked = !bPoseLocked && bLocked;
				bool& bOtherFootLocked = footLocks[(foot == 0) ? 1 : 0];
				bool otherFoot = !foot;

				FVector leftFootPos = boneData.Position.RotateAngleAxis(actorYaw, FVector::UpVector) + lastPosition + localMeshCompPos;
				FTransform boneT = animInst->GetSkelMeshComponent()->GetSocketTransform(footBoneName, ERelativeTransformSpace::RTS_World);
				FVector bonePos = boneT.GetLocation();

				const FVector& currentLockedPos = lockedfootPositions[foot];
				const FVector& currentLockedPosOther = lockedfootPositions[!foot];

				// distance between the foot in the animation vs the current foot. TODO: What about IK-modified foot? Our target should be adjusted here.
				float dist = FVector::DistSquared(leftFootPos, boneT.GetLocation());

				//bOutOfBalance = bOutOfBalance || (timeStandingStill > 2.f && dist > 10.f);
				bOutOfBalance = bOutOfBalance || dist > 100.f || (dist > 10.f && timeStandingStill > 1.5f);

				// distance between the foot and the pelvis (Z axis ignored)
				float distPelvis = FVector2D::DistSquared(centerOfMass, FVector2D(bonePos.X, bonePos.Y));

				// When to lock
				if (bSupposedToBeLocked && !bAdjustingBalance)
				{
					bLocked = true;
					lockedfootPositions[foot] = bonePos;

					MotionMatcherInterface->UpdateFootLock(foot, bLocked, lockedfootPositions[foot]);
				}
				else if (bNotSupposedToBeLocked || bPoseLocked && ((bOutOfBalance) && bOtherFootLocked && !bAdjustingBalance || distPelvis > 1000.f)) // || bNotSupposedToBeLocked// (bOutOfBalance) && bOtherFootLocked && !bAdjustingBalance || distPelvis > 1000.f
				{
					bool bContinue = true;
					//bAdjustingBalance = true;

					if (bOutOfBalance && bPoseLocked)//??poe locked? why
					{
						bAdjustingBalance = true;

						float footDistA = FVector2D::DistSquared(centerOfMass, FVector2D(currentLockedPos.X, currentLockedPos.Y));
						float footDistB = FVector2D::DistSquared(centerOfMass, FVector2D(currentLockedPosOther.X, currentLockedPosOther.Y));

						if (footDistB <= footDistA && !foot != lastUnlockedFoot || lastUnlockedFoot == (int)foot)
						{
							bAdjustingBalance = false;
							bContinue = false;
						}
					}


					if (bContinue)
					{
						footLocks[foot] = false;
						MotionMatcherInterface->UpdateFootLock(foot, footLocks[foot], FVector::ZeroVector);
						lastUnlockedFoot = foot;
					}
				}
				else if (dist < 5.f && bSupposedToBeLocked)
				{
					bLocked = true;
					lockedfootPositions[foot] = bonePos;

					MotionMatcherInterface->UpdateFootLock(foot, bLocked, lockedfootPositions[foot]);

					if (bOtherFootLocked)
						bAdjustingBalance = false;
				}

				if (foot == MM_FOOT::LEFT)
					goto nextFoot;
				else
					return;
			}
		}*/

		return;
	//}
}

#if WITH_EDITOR
void FAnimNode_MotionMatcher::DrawDebug(const FAnimationUpdateContext& context)
{
	// Note: we're not on the game thread here so we can only modify the widget but not call methods.
	if (debugWidget && !debugWidget->bShow)
		debugWidget->bShow = true;

	// 
	// Draw trajectories
	//

	TArray<FTrajectoryPoint> debugCurrentTraj = currentPose.Trajectory;
	TArray<FTrajectoryPoint> debugDesiredTraj = desiredTrajectory;
	TArray<FMMatcherBoneData> animBoneData = currentPose.BoneData;

	MotionDataAsset->UnnormalizeTrajectory(debugCurrentTraj);
	MotionDataAsset->UnnormalizeTrajectory(debugDesiredTraj);
	MotionDataAsset->UnnormalizeBoneData(animBoneData);

	DrawTrajectory(context, debugCurrentTraj, FColor(0, 0, 255));
	DrawTrajectory(context, debugDesiredTraj, FColor(255, 128, 0));
	//DrawBoneData(context, currentBoneData, FColor(0, 255, 255));
	DrawBoneData(context, animBoneData, FColor(255, 0, 255));
	DrawFootIKThreshold(context);

	if (MotionMatcherInterface)
	{
		MotionMatcherInterface->DrawDebug();
	}
	
	if (debugWidget)
	{

		/*auto fooProp = debugWidget->customProperties.Find("foo");

		if (fooProp)
		{
			fooProp->Set(1.f, 2.f, 3.f);
		}*/

		//UPDATE_DEBUG("FOO", 256.f)


		debugWidget->stateIndex = currentPlayData.MatchedStateIndex;
		auto& state = MotionDataAsset->States[currentPlayData.MatchedStateIndex];
		UAnimSequence* anim = state.Animation;
		anim->GetFName().ToString(debugWidget->animName);
		MotionDataAsset->GetName(debugWidget->stateName);

		debugWidget->animType = (EDebugAnimType)!state.bLoop;
		debugWidget->animRoot = currentPose.RootVelocity;
		debugWidget->animTime = currentPlayData.CurrentPlayTime;

		
		UPDATE_DEBUG_VECTOR(inputDir, Input.DesiredVector);
		UPDATE_DEBUG_VECTOR(rootVel, currentPose.RootVelocity);

		UPDATE_DEBUG_FLOAT(desiredFacing, Input.DesiredFacing);
		UPDATE_DEBUG_FLOAT(naturalBias, MotionDataAsset->NaturalBias);
		UPDATE_DEBUG_FLOAT(loopBias, 1.0f - (MotionDataAsset->LoopBias * steadyBias));
		UPDATE_DEBUG_FLOAT(steadyBias, steadyBias);
		UPDATE_DEBUG_FLOAT(rootRotWarp, rootRotWarp);
		UPDATE_DEBUG_FLOAT(lastBestCost, lastBestCost);

		//UE_LOG(LogTemp, Warning, TEXT("vel: %f"), debugWidget->debugVectorProps.Find("rootVel")->Value.Size());
		


		for (int32 i = 0; i != MotionDataAsset->PoseMatchingBones.Num(); ++i)
		{
			debugWidget->animBones[i] = currentPose.BoneData[i].Position;
		}

		/*if (debugWidget)
		{
			debugWidget->SetDebugValues(currentPose.RootVelocity);
		}*/


		// visualize input steadiness
		//context.AnimInstanceProxy->AnimDrawDebugSphere(lastPosition, steadyBias * 100, 32, FColor::Red);
		//context.AnimInstanceProxy->AnimDrawDebugSphere(lastPosition + avgInputDir.RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector), 100.f, 32, FColor::Blue);

		// Draw root rotation warp
		FVector posOffset = lastPosition - FVector(0.f, 0.f, -1 * localMeshCompPos.Z);
		
		FVector endFacingPoint = FVector(0.f, 100.f, 0.f).RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector) + posOffset;
		FVector endFacingPointWarped = FVector(0.f, 100.f, 0.f).RotateAngleAxis(rootRotWarp, FVector::UpVector).RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector) + posOffset;

		context.AnimInstanceProxy->AnimDrawDebugDirectionalArrow(posOffset + FVector(0.f, 0.f, 100.f), endFacingPoint + FVector(0.f, 0.f, 100.f), 256.f, FColor::Orange, false, -1.0f, 1.0f);
		context.AnimInstanceProxy->AnimDrawDebugDirectionalArrow(posOffset + FVector(0.f, 0.f, 100.f), endFacingPointWarped + FVector(0.f, 0.f, 100.f), 256.f, FColor::Blue, false, -1.0f, 1.0f);


		auto unnormalize = [&](FVector v, FFeatureNormalData& normalData) {
			return FVector(
				(normalData.StandardDeviation * v.X) + normalData.Mean,
				(normalData.StandardDeviation * v.Y) + normalData.Mean,
				(normalData.StandardDeviation * v.Z) + normalData.Mean
			);
		};




		/*

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%s @ %s"), *anim->GetFName().ToString(), *mmDataName), FColor(200, 200, 200), FVector2D(1.0f));

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("State Index:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%d"), currentPlayData.MatchedStateIndex), FColor::Green, FVector2D(1.0f));

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("State Play Time:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), currentPlayData.CurrentPlayTime), FColor::Blue, FVector2D(1.0f));

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("Time Warp:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), currentTimeScaleWarp), FColor::Blue, FVector2D(1.0f));

		/*context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("Actual Root Velocity:")), FColor::White, FVector2D(0.9f)
		);

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("\r%s     %s     %s"),
				*f(currentPose.RootVelocity.X), *f(currentPose.RootVelocity.Y), *f(currentPose.RootVelocity.Z)
			), FColor::Orange, FVector2D(1.0f)
		);*/
		/*
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("Anim Root Velocity:")), FColor::White, FVector2D(0.9f)
		);

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("\r%s     %s     %s"),
				*f(currentPose.RootVelocity.X), *f(currentPose.RootVelocity.Y), *f(currentPose.RootVelocity.Z)
			), FColor::Orange, FVector2D(1.0f)
		);

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("Anim Bones:")), FColor::White, FVector2D(0.9f)
		);

		for (int32 i = 0; i != MotionDataAsset->PoseMatchingBones.Num(); ++i)
		{
			context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
				FString::Printf(TEXT("\r%s     %s     %s"),
					*f(currentPose.BoneData[i].Position.X), *f(currentPose.BoneData[i].Position.Y), *f(currentPose.BoneData[i].Position.Z)
				), FColor::Red, FVector2D(1.0f)
			);

			context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
				FString::Printf(TEXT("\r%s     %s     %s"),
					*f(currentPose.BoneData[i].Velocity.X), *f(currentPose.BoneData[i].Velocity.Y), *f(currentPose.BoneData[i].Velocity.Z)
				), FColor::Orange, FVector2D(1.0f)
			);
		}

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("Anim Trajectory:")), FColor::White, FVector2D(0.9f)
		);

		for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
		{
			context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
				FString::Printf(TEXT("\r(%s )   %s     %s     %s     |     %s"),
					*f(MotionDataAsset->TrajectoryTimings[i]),
					*f(currentPose.Trajectory[i].Position.X), *f(currentPose.Trajectory[i].Position.Y), *f(currentPose.Trajectory[i].Position.Z), *f(currentPose.Trajectory[i].Facing)
				), FColor::Cyan, FVector2D(1.0f)
			);
		}

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
			FString::Printf(TEXT("Desired Trajectory:")), FColor::White, FVector2D(0.9f)
		);

		for (int32 i = 0; i != MotionDataAsset->TrajectoryTimings.Num(); ++i)
		{
			context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(
				FString::Printf(TEXT("\r(%s )   %s     %s     %s     |     %s"),
					*f(MotionDataAsset->TrajectoryTimings[i]),
					*f(desiredTrajectory[i].Position.X), *f(desiredTrajectory[i].Position.Y), *f(desiredTrajectory[i].Position.Z), *f(desiredTrajectory[i].Facing)
				), FColor::Cyan, FVector2D(1.0f)
			);
		}

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("Desired Facing:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), Input.DesiredFacing), FColor::Blue, FVector2D(1.0f));

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("Loop Bias:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), 1.0f - (MotionDataAsset->LoopBias * steadyBias)), FColor::Blue, FVector2D(1.0f));

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("Cost:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), lastBestCost), FColor::Blue, FVector2D(1.0f));
	

		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("Root Rotation Warp:")), FColor::White, FVector2D(0.9f));
		context.AnimInstanceProxy->AnimDrawDebugOnScreenMessage(FString::Printf(TEXT("%f"), rootRotWarp), FColor::Blue, FVector2D(1.0f));
		*/
	}
}

void FAnimNode_MotionMatcher::DrawTrajectory(const FAnimationUpdateContext& context, const TArray<FTrajectoryPoint>& trajectory, FColor color)
{
	float actorYaw = owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z); // TODO: This 90* offset shouldn't be hardcoded.
	FVector posOffset = lastPosition - FVector(0.f, 0.f, -1 * localMeshCompPos.Z);

	for (int32 i = 0; i != trajectory.Num(); ++i)
	{
		FColor currentShade = FColor(
			FMath::RoundToInt(color.R / trajectory.Num() * (1 + i)),
			FMath::RoundToInt(color.G / trajectory.Num() * (1 + i)),
			FMath::RoundToInt(color.B / trajectory.Num() * (1 + i))
		);

		if (trajectory.IsValidIndex(i)) {
			FVector pointPos = posOffset + trajectory[i].Position.RotateAngleAxis(actorYaw, FVector::UpVector);
			FVector endFacingPoint = FVector(0.f, 20.f, 0.f).RotateAngleAxis(trajectory[i].Facing, FVector::UpVector).RotateAngleAxis(actorYaw, FVector::UpVector) + pointPos;

			context.AnimInstanceProxy->AnimDrawDebugSphere(pointPos, 2.f, 8, currentShade, false, -1.0f);
			context.AnimInstanceProxy->AnimDrawDebugDirectionalArrow(pointPos, endFacingPoint, 6.f, currentShade, false, -1.0f, 1.0f);

			if (i > 0)
				context.AnimInstanceProxy->AnimDrawDebugLine(posOffset + trajectory[i - 1].Position.RotateAngleAxis(actorYaw, FVector::UpVector), pointPos, currentShade, false, -1.0f, 1.0f);
		}
	}
}

void FAnimNode_MotionMatcher::DrawBoneData(const FAnimationUpdateContext& context, const TArray<FMMatcherBoneData>& bones, FColor color)
{
	float actorYaw = owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z);

	for (int32 i = 0; i != bones.Num(); ++i)
	{
		const FMMatcherBoneData& bone = bones[i];

		FVector bonePos = lastPosition + localMeshCompPos + bone.Position.RotateAngleAxis(actorYaw, FVector::UpVector);
		FVector boneVel = lastPosition + localMeshCompPos + (bone.Position + (bone.Velocity / 4)).RotateAngleAxis(actorYaw, FVector::UpVector);
		//FVector boneVel = lastPosition + localMeshCompPos + (bone.Position + (FVector::ZeroVector / 4)).RotateAngleAxis(actorYaw, FVector::UpVector);

		// Crashes
		//DrawDebugSphere(owningActor->GetWorld(), bonePos, 2.f, 8, color, false, -1.0f, 255);
		//DrawDebugDirectionalArrow(owningActor->GetWorld(), bonePos, boneVel, 6.f, color, false, -1.0f, 255, 1.0f);

		context.AnimInstanceProxy->AnimDrawDebugSphere(bonePos, 16.f, 8, color, false, -1.0f);
		context.AnimInstanceProxy->AnimDrawDebugDirectionalArrow(bonePos, boneVel, 6.f, color, false, -1.0f, 1.0);
	}

	//context.AnimInstanceProxy->AnimDrawDebugSphere(currentBoneData[2].Position, 16.f, 8, FColor::Orange, false, -1.0f);
}

void FAnimNode_MotionMatcher::DrawFootIKThreshold(const FAnimationUpdateContext& context)
{
	bReverse = (lerpAlpha >= 1.0f || lerpAlpha <= 0.0f) ? !bReverse : bReverse;
	lerpAlpha = FMath::Clamp(lerpAlpha + ((bReverse) ? -0.5f * context.GetDeltaTime() : 0.5f * context.GetDeltaTime()), 0.0f, 1.0f);

	FColor shade;

	if (IsPointInBalancingArea(FVector2D(centerOfMass.X, centerOfMass.Y)))
		shade = FColor::Green;
	else
		shade = FColor::Red;

	context.AnimInstanceProxy->AnimDrawDebugSphere(FVector(centerOfMass.X, centerOfMass.Y, 0.f), 14.f, 4, shade, false, -1.f, 1.0f);
	//context.AnimInstanceProxy->AnimDrawDebugSphere(FVector(0.f, 0.f, 0.f), 14.f, 4, FColor::Red, false, -1.f, 1.0f);

	const float spacing = 2.f;
	const float width = 200.f;
	const float height = 200.f;

	int32 vLines = FMath::RoundToInt(height / spacing);
	int32 hLines = FMath::RoundToInt(width / spacing);

	int32 totalDots = vLines * hLines;

	FVector actorPosOffset = lastPosition;//.RotateAngleAxis(owningActor->GetActorRotation().Yaw + (localMeshCompRot.Z), FVector::UpVector);

	float initialOffsetX = actorPosOffset.X - width / 2;
	float initialOffsetY = actorPosOffset.Y - width / 2;

	float offsetX = initialOffsetX;
	float offsetY = initialOffsetY;

	// replace actor forward vector with pelvis forward vector!
	//bool bAcceptableTwist = (FVector::DotProduct(normal, owningActor->GetActorForwardVector()) > 0.3f) ? true : false;

	// pelvis forward vector? GET FROM POSE INSTEAD IN EVAL.
	//FTransform socketT = context.AnimInstanceProxy->GetSkelMeshComponent()->GetSocketTransform("pelvis", ERelativeTransformSpace::RTS_World);
	//UE_LOG(LogTemp, Warning, TEXT("%f, %f"), centerOfMass.X, centerOfMass.Y);

	//IsPointInBalancingArea(FVector2D(0.f, 0.f));

	for (int j = 0; j < vLines; j++) {

		offsetX = initialOffsetX;

		for (int k = 0; k < hLines; k++) {
			FVector pos = FVector(offsetX, offsetY, 0.0f); // pos can be stand in for center of mass

			if (IsPointInBalancingArea(FVector2D(pos.X, pos.Y)))
				context.AnimInstanceProxy->AnimDrawDebugLine(pos, pos + FVector::UpVector, FColor::Cyan, false, -1.0f, 1.f);

			offsetX += spacing;
		}

		offsetY += spacing;
	}
}
#endif