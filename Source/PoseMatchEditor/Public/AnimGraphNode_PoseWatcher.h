// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimNode_PoseWatcher.h"
#include "AnimGraphNode_PoseWatcher.generated.h"

UCLASS()
class POSEMATCHEDITOR_API UAnimGraphNode_PoseWatcher : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_PoseWatcher Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	virtual FString GetNodeCategory() const override;
	//~ End UAnimGraphNode_Base Interface

	UAnimGraphNode_PoseWatcher(const FObjectInitializer& ObjectInitializer);
};