#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "RootMotionSource_Custom.generated.h"

/** Custom root motion for generated motion from motion matching. */
USTRUCT()
struct POSEMATCH_API FRootMotionSource_Custom : public FRootMotionSource
{
	GENERATED_USTRUCT_BODY()

	FRootMotionSource_Custom();

	virtual ~FRootMotionSource_Custom() {}

	UPROPERTY()
	FTransform RootMotion;

	FVector LFootLockPos;

	// This lets us give the movement component custom root motion.
	virtual void PrepareRootMotion(
		float SimulationTime,
		float MovementTickTime,
		const ACharacter& Character,
		const UCharacterMovementComponent& MoveComponent
	) override;

	virtual UScriptStruct* GetScriptStruct() const override;
};