#pragma once

#include "CoreMinimal.h"
//#include "UObject/ObjectMacros.h"
//#include "UObject/Object.h"
#include "PoseMatchSettings.generated.h"


UCLASS(config = Game, defaultconfig)
class UPoseMatchSettings
	: public UObject
{
	GENERATED_BODY()

public:

	void GetBonesToMatch(TArray<FName>& BoneNames) const;

private:

	UPROPERTY(config, EditAnywhere, Category = "Matching Bones")
	TArray<FName> BoneNames;
};