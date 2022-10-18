// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MotionData.h"
#include "DebugWidget.generated.h"

#define DECLARE_DEBUG_BOOL(name, displayName) { debugWidget->debugBoolProps.Add(#name).DisplayName = ##displayName##; }
#define DECLARE_DEBUG_INT(name, displayName) { debugWidget->debugIntProps.Add(#name).DisplayName = ##displayName##; }
#define DECLARE_DEBUG_FLOAT(name, displayName) { debugWidget->debugFloatProps.Add(#name).DisplayName = ##displayName##; }
#define DECLARE_DEBUG_VECTOR(name, displayName) { debugWidget->debugVectorProps.Add(#name).DisplayName = ##displayName##; }
#define DECLARE_DEBUG_STRING(name, displayName) { debugWidget->debugStringProps.Add(#name).DisplayName = ##displayName##; }

#define UPDATE_DEBUG_BOOL(name, value) { auto _prop = debugWidget->debugBoolProps.Find(#name)->Value = value; };
#define UPDATE_DEBUG_INT(name, value) { auto _prop = debugWidget->debugIntProps.Find(#name)->Value = value; };
#define UPDATE_DEBUG_FLOAT(name, value) { auto _prop = debugWidget->debugFloatProps.Find(#name)->Value = value; };
#define UPDATE_DEBUG_VECTOR(name, value) { auto _prop = debugWidget->debugVectorProps.Find(#name)->Value = value; };
#define UPDATE_DEBUG_STRING(name, value) { auto _prop = debugWidget->debugStringProps.Find(#name)->Value = value; };

typedef TArray<FVector> FVectorList;

UENUM()
enum EDebugDataType
{
	BOOL	UMETA(DisplayName = "Boolean"),
	_INT		UMETA(DisplayName = "Integer"),
	FLOAT	UMETA(DisplayName = "Float"),
	VECTOR	UMETA(DisplayName = "Vector"),
	STRING	UMETA(DisplayName = "String"),
};

UENUM(BlueprintType)
enum EDebugAnimType
{
	Loop,
	Transition,
};

USTRUCT(BlueprintType)
struct FDebugProperty
{
	GENERATED_BODY()

	void* Data;

	bool _DEBUG_BOOL;
	int32 _DEBUG_INT;
	float _DEBUG_FLOAT;
	FVector _DEBUG_VECTOR;	
	FString _DEBUG_STRING;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	TEnumAsByte<EDebugDataType> DataType;
};

USTRUCT(BlueprintType)
struct FDebugBoolProperty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	bool Value;
};

USTRUCT(BlueprintType)
struct FDebugIntProperty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	int32 Value;
};

USTRUCT(BlueprintType)
struct FDebugFloatProperty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	float Value;
};

USTRUCT(BlueprintType)
struct FDebugVectorProperty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FVector Value;
};

USTRUCT(BlueprintType)
struct FDebugStringProperty
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString Value;
};


/**
 *
 */
UCLASS()
class POSEMATCH_API UDebugWidget : public UUserWidget
{
	GENERATED_BODY()

private:
	FTrajectory* currentTrajectory;
	FTrajectory* desiredTrajectory;

	FMMatcherPoseSample* currentPose = nullptr;

	UMotionData* motionData = nullptr;
	//FMMatcherPoseSample* Pose;


public:

	UPROPERTY(Transient, BlueprintReadOnly, meta = (Category = "Motion Matching Debugging"))
	bool bPropertyBound = false;

	void BindProps(FMMatcherPoseSample* current, UMotionData* motionDataAsset) {
		currentPose = current;
		motionData = motionDataAsset;
		UE_LOG(LogTemp, Warning, TEXT("prop bound"));
		bPropertyBound = true;
	}

public:
	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite)
	bool bShow = false;

public:

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	FString stateName;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	FString animName;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	int32 stateIndex;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	TEnumAsByte<EDebugAnimType> animType;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	float animTime;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	FVector animRoot;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	TArray<FVector> animBones;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugProperty> debugProperties;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Transient, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugBoolProperty> debugBoolProps;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Transient, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugIntProperty> debugIntProps;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Transient, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugFloatProperty> debugFloatProps;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Transient, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugVectorProperty> debugVectorProps;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Transient, meta = (Category = "Motion Matching Debugging"))
	TMap<FName, FDebugStringProperty> debugStringProps;

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetDebugValues(UPARAM(ref) FVector& animRootVelocity) const;

	UFUNCTION()
	float GetFloatFromProperty(UPARAM(ref) FDebugProperty& debugProperty) { return *(float*)debugProperty.Data; };

	UFUNCTION(BlueprintCallable)
	UMotionData* GetMotionData()
	{
		if (motionData)
		{
			return motionData;
		}
		else
		{
			return nullptr;
		}
	}

	UFUNCTION(BlueprintCallable)
	FMMatcherPoseSample GetPoseSample()
	{
		if (currentPose)
		{
			return *currentPose;
		}
		else
		{
			return FMMatcherPoseSample();
		}
	}
};

UCLASS()
class POSEMATCH_API UDebugPropertyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void* propPtr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	bool bIsPropertyBound = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	FString DisplayName;

	/////////////////////////////////////////////

	template <class T>
	void BindProperty_Native(FName propertyName, T& debugProps)
	{
		auto prop = debugProps.Find(propertyName);

		if (prop)
		{
			propPtr = &prop->Value;
			DisplayName = prop->DisplayName;
			DisplayName.Append(":");
		}

		bIsPropertyBound = true;
	}

	UFUNCTION(BlueprintCallable)
	void BindBoolProperty(FName propertyName, UPARAM(ref) TMap<FName, FDebugBoolProperty>& props)
	{
		BindProperty_Native(propertyName, props);
	};

	UFUNCTION(BlueprintCallable)
	void BindIntProperty(FName propertyName, UPARAM(ref) TMap<FName, FDebugIntProperty>& props)
	{
		BindProperty_Native(propertyName, props);
	};

	UFUNCTION(BlueprintCallable)
	void BindFloatProperty(FName propertyName, UPARAM(ref) TMap<FName, FDebugFloatProperty>& props)
	{
		BindProperty_Native(propertyName, props);
	};

	UFUNCTION(BlueprintCallable)
	void BindVectorProperty(FName propertyName, UPARAM(ref) TMap<FName, FDebugVectorProperty>& props)
	{
		BindProperty_Native(propertyName, props);
	};

	UFUNCTION(BlueprintCallable)
	void BindStringProperty(FName propertyName, UPARAM(ref) TMap<FName, FDebugStringProperty>& props)
	{
		BindProperty_Native(propertyName, props);
	};

	UFUNCTION(BlueprintCallable)
	bool GetBoolProperty() const { return *(bool*)propPtr; };

	UFUNCTION(BlueprintCallable)
	float GetIntProperty() const { return *(int32*)propPtr; };

	UFUNCTION(BlueprintCallable)
	float GetFloatProperty() const { return *(float*)propPtr; };

	UFUNCTION(BlueprintCallable)
	FVector GetVectorProperty() const {	return *(FVector*)propPtr; };

	UFUNCTION(BlueprintCallable)
	FString GetStringProperty() const { return *(FString*)propPtr; };
};