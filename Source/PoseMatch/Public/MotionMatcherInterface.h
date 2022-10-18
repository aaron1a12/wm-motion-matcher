#pragma once

#include "CoreMinimal.h"
#include "MotionMatcher_Component.h"
#include "MotionMatcherInterface.generated.h"

struct FAnimNode_MotionMatcher;
struct FFootLock;
class UAnimInstance;
class UDebugWidget;

/** Interface to the motion matcher node. Can get foot IK info from here.
*/
UCLASS(BlueprintType)
class POSEMATCH_API UMotionMatcherInterface : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UMotionMatcherInterface(const FObjectInitializer& ObjectInitializer);

private:
	FAnimNode_MotionMatcher* animNode = nullptr;
	ACharacter* character = nullptr;

	bool bSetupComplete = false;

	FFootLock CurrentFootLock;
	FFootLock TargetFootLock;


#if WITH_EDITOR
	UDebugWidget* debugWidget;
	bool bDebugVisible = false;
#endif

public:
	// Tick function
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return bSetupComplete; };
	virtual TStatId GetStatId() const override { return UObject::GetStatID(); };
	// End overrides

	// Associate an anim node instance with this interface
	void PairAnimNode(FAnimNode_MotionMatcher* motionMatcher);

	void UpdateFootLock(int foot, bool bLock, FVector newPos);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Motion Matching")
	const FFootLock& GetFootLockData();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Motion Matching")
	int32 GetStateIndex();

	void DrawDebug();


};