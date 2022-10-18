#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

#include "MotionMatcher_Component.generated.h"

enum MM_FOOT { LEFT, RIGHT };

USTRUCT(BlueprintType)
struct POSEMATCH_API FFootLock
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	FVector LFootLocation;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	FVector RFootLocation;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	float LFoot_LockStrength = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	float RFoot_LockStrength = 0.f;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	FVector LLegIKLocation;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	FVector RLegIKLocation;
};

UCLASS(BlueprintType, Category = "Motion Matcher Component", meta = (BlueprintSpawnableComponent))
class POSEMATCH_API UMotionMatcherComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/**
		* Default UObject constructor.
		*/
	//UMotionMatcherComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	bool debugMode = false;

	FFootLock CurrentFootLock;

	FFootLock TargetFootLock;

	bool bFootLockChanged = false;

	float blendAlpha = 0.0f;

public:

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void UpdateFootLock(int foot, bool bLock, FVector newPos);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Get Foot Lock", Keywords = "motion match"), Category = "Motion Matching")
	FFootLock GetFootLock();

	UFUNCTION(BlueprintCallable, Category = Puppet, meta = (AllowPrivateAccess = "true"))
	void EnableDebug(bool enable);
};