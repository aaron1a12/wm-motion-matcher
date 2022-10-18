// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimNode_MotionMatcher.h"
#include "AnimGraphNode_MotionMatcher.generated.h"

UCLASS()
class POSEMATCHEDITOR_API UMotionMatcherGraphNode : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_MotionMatcher Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	virtual FString GetNodeCategory() const override;
	/** UAnimGraphNode_Base interface */
	virtual void OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;

	virtual void CreateOutputPins() override;

	//~ End UAnimGraphNode_Base Interface

	UMotionMatcherGraphNode(const FObjectInitializer& ObjectInitializer);
};