#pragma once

#include "CoreMinimal.h"
#include "MotionData.h"

#if WITH_EDITOR
TWeakObjectPtr<UMotionData> MotionData;
int32 StateCount;
int32 SampleCount;

void BuildMotionCache(const TWeakObjectPtr<UMotionData> motionDataAsset);
void BuildMotionCache_PerState(FMMatcherState& state);

void AddAnimations(const TWeakObjectPtr<UMotionData> motionDataAsset);
void ChangeAnimationRate(const TWeakObjectPtr<UMotionData> motionDataAsset, bool bReset);
#endif