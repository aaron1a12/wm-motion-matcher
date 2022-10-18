#include "MotionDataFactory.h"
#include "AssetTypeCategories.h"
#include "MotionData.h"
#include "PoseDataCache.h"

UMotionDataFactoryNew::UMotionDataFactoryNew()
{
	SupportedClass = UMotionData::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UMotionDataFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UMotionData>(InParent, InClass, InName, Flags);
}

uint32 UMotionDataFactoryNew::GetMenuCategories() const
{
	return EAssetTypeCategories::Animation;
}

bool UMotionDataFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}
