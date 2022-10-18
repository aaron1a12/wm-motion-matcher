#include "RootMotionSource_Custom.h"

FRootMotionSource_Custom::FRootMotionSource_Custom()
{
}

void FRootMotionSource_Custom::PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();
	RootMotionParams.Set(RootMotion);	

	SetTime(GetTime() + SimulationTime);
}

UScriptStruct* FRootMotionSource_Custom::GetScriptStruct() const
{
	return FRootMotionSource_Custom::StaticStruct();
}
