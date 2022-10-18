#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "MotionDataFactory.generated.h"

UCLASS()
class POSEMATCHEDITOR_API UMotionDataFactoryNew : public UFactory
{
    GENERATED_BODY()

public:
    UMotionDataFactoryNew();

    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

    virtual uint32 GetMenuCategories() const override;
    virtual bool ShouldShowInNewMenu() const override;
};