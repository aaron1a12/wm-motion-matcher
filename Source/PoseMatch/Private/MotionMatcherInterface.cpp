#include "MotionMatcherInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNode_MotionMatcher.h"
#include "MotionMatcher_Component.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"

#if WITH_EDITOR
#include "Debug/DebugWidget.h"
#endif

UMotionMatcherInterface::UMotionMatcherInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMotionMatcherInterface::Tick(float DeltaTime)
{
	if(!bSetupComplete) return;
	/*
#if WITH_EDITOR
	if (debugWidget) {
		if(animNode->bDebugMode && !bDebugVisible) {
			debugWidget->AddToViewport();
			bDebugVisible = true;
		}
		else if (!animNode->bDebugMode && bDebugVisible) {
			debugWidget->RemoveFromViewport();
			bDebugVisible = false;
		}

		if (bDebugVisible) {
			debugWidget->animRoot = TargetFootLock.LFootLocation;
			//debugWidget->SetDebugValues(TargetFootLock.LFootLocation);
			
		}
	}
#endif
*/

	FTransform actorT = character->GetTransform();
	FVector actorPos = actorT.GetLocation();
	//ACharacter* character = (ACharacter*)owningActor;
	//ACharacter* character = Cast<ACharacter>(owningActor);

	FTransform footL_T = character->GetMesh()->GetSocketTransform("l_ankle", ERelativeTransformSpace::RTS_World);
	FTransform footR_T = character->GetMesh()->GetSocketTransform("r_ankle", ERelativeTransformSpace::RTS_World);

	auto calcIKPos = [&](FVector& actorPos, FTransform& t, FVector& ikPos) {
		FVector bonePos = t.GetLocation();
		FRotator boneRot = t.GetRotation().Rotator();

		ikPos = FVector::ForwardVector * -100.f;
		ikPos = ikPos.RotateAngleAxis(boneRot.Euler().Z, FVector::UpVector).RotateAngleAxis(90.f, FVector::UpVector) + actorPos + FVector(0.f, 0.f, -50.f);
	};

	calcIKPos(actorPos, footL_T, CurrentFootLock.LLegIKLocation);
	calcIKPos(actorPos, footR_T, CurrentFootLock.RLegIKLocation);

	if (TargetFootLock.LFoot_LockStrength == 1.f)
	{
		CurrentFootLock.LFoot_LockStrength = FMath::FInterpTo(CurrentFootLock.LFoot_LockStrength, TargetFootLock.LFoot_LockStrength, DeltaTime, 8.0f);
		CurrentFootLock.LFootLocation = FMath::VInterpTo(CurrentFootLock.LFootLocation, TargetFootLock.LFootLocation, DeltaTime, 4.0f);
	}
	else
	{
		CurrentFootLock.LFoot_LockStrength = FMath::FInterpTo(CurrentFootLock.LFoot_LockStrength, TargetFootLock.LFoot_LockStrength, DeltaTime, 3.0f);
		CurrentFootLock.LFootLocation = FMath::VInterpTo(CurrentFootLock.LFootLocation, footL_T.GetLocation(), DeltaTime, 28.0f);

		FTransform socketTAlt = character->GetMesh()->GetSocketTransform("l_ankle", ERelativeTransformSpace::RTS_Component);
		FRotator rot = socketTAlt.GetRotation().Rotator();
		CurrentFootLock.LLegIKLocation = FVector::ForwardVector * -100.f;
		CurrentFootLock.LLegIKLocation = CurrentFootLock.LLegIKLocation.RotateAngleAxis(rot.Euler().Z, FVector::UpVector).RotateAngleAxis(actorT.GetRotation().Rotator().Euler().Z, FVector::UpVector);
		CurrentFootLock.LLegIKLocation += actorT.GetLocation() + FVector(0.f, 0.f, -50.f);
	}

	if (TargetFootLock.RFoot_LockStrength == 1.f)
	{
		CurrentFootLock.RFoot_LockStrength = FMath::FInterpTo(CurrentFootLock.RFoot_LockStrength, TargetFootLock.RFoot_LockStrength, DeltaTime, 8.0f);
		CurrentFootLock.RFootLocation = FMath::VInterpTo(CurrentFootLock.RFootLocation, TargetFootLock.RFootLocation, DeltaTime, 4.0f);
	}
	else
	{
		CurrentFootLock.RFoot_LockStrength = FMath::FInterpTo(CurrentFootLock.RFoot_LockStrength, TargetFootLock.RFoot_LockStrength, DeltaTime, 3.0f);
		CurrentFootLock.RFootLocation = FMath::VInterpTo(CurrentFootLock.RFootLocation, footR_T.GetLocation(), DeltaTime, 28.0f);
	}
}

void UMotionMatcherInterface::PairAnimNode(FAnimNode_MotionMatcher* motionMatcher)
{
	animNode = motionMatcher;

	UAnimInstance* AnimInstance = Cast<UAnimInstance>(GetOuter());
	if (AnimInstance)
	{
		character = (ACharacter*)AnimInstance->TryGetPawnOwner();

#if WITH_EDITOR
		/*
		if(character)
		{
		FStringClassReference MyWidgetClassRef("WidgetBlueprint'/PoseMatch/debugHUD.debugHUD_C'");
		if (UClass* MyWidgetClass = MyWidgetClassRef.TryLoadClass<UDebugWidget>())
			debugWidget = CreateWidget<UDebugWidget>(character->GetGameInstance(), MyWidgetClassRef.ResolveClass());
		}
		*/
#endif
	}

	bSetupComplete = (!AnimInstance || !character || !animNode) ? false : true;
}

void UMotionMatcherInterface::UpdateFootLock(int foot, bool bLock, FVector newPos)
{
	//UE_LOG(LogTemp, Warning, TEXT("New lock: %f, %f, %f"), newPos.X, newPos.Y, newPos.Z);

	if (foot == MM_FOOT::LEFT && bLock)
	{
		TargetFootLock.LFoot_LockStrength = 1.0f;
		TargetFootLock.LFootLocation = newPos;
	}

	if (foot == MM_FOOT::LEFT && !bLock)
		TargetFootLock.LFoot_LockStrength = 0.0f;

	if (foot == MM_FOOT::RIGHT && bLock)
	{
		TargetFootLock.RFoot_LockStrength = 1.0f;
		TargetFootLock.RFootLocation = newPos;
	}

	if (foot == MM_FOOT::RIGHT && !bLock)
		TargetFootLock.RFoot_LockStrength = 0.0f;

	/*if (FVector::DistSquared(GetOwner()->GetActorTransform().GetLocation() - FVector(0.f, 0.f, 85.f), TargetFootLock.LFootLocation) > 2500.f)
	{
		TargetFootLock.LFoot_LockStrength = 0.f;
	}


	if (FVector::DistSquared(GetOwner()->GetActorTransform().GetLocation() - FVector(0.f, 0.f, 85.f), TargetFootLock.RFootLocation) > 2500.f)
	{
		TargetFootLock.RFoot_LockStrength = 0.f;
	}*/
}

const FFootLock& UMotionMatcherInterface::GetFootLockData()
{
	return CurrentFootLock;
}

int32 UMotionMatcherInterface::GetStateIndex()
{
	return (!animNode) ? 0 : animNode->GetStateIndex();
}

void UMotionMatcherInterface::DrawDebug()
{
	if (bSetupComplete)
	{
		DrawDebugSphere(character->GetWorld(), CurrentFootLock.LFootLocation, 2.f + (8 * CurrentFootLock.LFoot_LockStrength), 6, FColor::Red, false, -1.0f, 255, 0.5f);
		DrawDebugSphere(character->GetWorld(), CurrentFootLock.RFootLocation, 2.f + (8 * CurrentFootLock.RFoot_LockStrength), 6, FColor::Red, false, -1.0f, 255, 0.5f);

		DrawDebugSphere(character->GetWorld(), CurrentFootLock.LLegIKLocation, 4.f, 2, FColor::Red, false, -1.0f, 255, 0.5f);
		DrawDebugSphere(character->GetWorld(), CurrentFootLock.RLegIKLocation, 4.f, 2, FColor::Green, false, -1.0f, 255, 0.5f);
	}
}
