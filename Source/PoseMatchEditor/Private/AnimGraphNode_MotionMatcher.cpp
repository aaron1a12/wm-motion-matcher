// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimGraphNode_MotionMatcher.h"
#include "PoseMatchEditor.h"

// ?
#include "IAnimBlueprintGeneratedClassCompiledData.h"
#include "IAnimBlueprintCompilationContext.h"


#include "AnimationGraphSchema.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UMotionMatcherGraphNode::UMotionMatcherGraphNode(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FLinearColor UMotionMatcherGraphNode::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor(255, 128, 0));
}

FText UMotionMatcherGraphNode::GetTooltipText() const
{
	return LOCTEXT("MotionMatcherGraphNode_Tooltip", "Compares current pose and future trajectories to automatically play from a list of animations. Best used for locomotion.");
}

FText UMotionMatcherGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("MotionMatcherGraphNode", "Motion Matcher");
}

FString UMotionMatcherGraphNode::GetNodeCategory() const
{
	return TEXT("Motion Matching");
}

void UMotionMatcherGraphNode::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	if (Node.MotionDataAsset && Node.MotionDataAsset->bOutdatedMotionCache)
	{
		InCompilationContext.GetMessageLog().Error(*LOCTEXT("OutdatedCacheMsg", "@@ has an outdated motion cache. Use the \"Rebuild Motion Cache\" action from the content browser's context-menu.").ToString(), this);
		return;
	}


	//InCompilationContext.GetBlueprint()-
	//return;

	/*
//	FAnimNotifyEvent* tt;
	//tt.Notify = UFootLockNotify::StaticClass();
	//tt->Notify->Notify();

	//OutCompiledData.FindOrAddNotify(Node.);

	//OutCompiledData.

	//Pins.Add

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->GetFName() == "Footer")
		{
			Pin->bHidden = true;
			Pin->MakeLinkTo(CreatePin(EGPD_Input, UAnimationGraphSchema::PC_Struct, TEXT(""), FFootLock::StaticStruct(), false, true, TEXT("Foot")));
			UE_LOG(LogTemp, Warning, TEXT("Connection made."));
		}

		UE_LOG(LogTemp, Warning, TEXT("PIN: %s"), *Pin->GetName());

		//Pin->MakeLinkTo

		//Pin->bHidden

		//UClass* InstClass = GetTargetSkeletonClass();
		if (FProperty* FoundProperty = FindFProperty<FProperty>(Node.StaticStruct(), Pin->PinName))
		{
			if (Pin->GetFName() == "Test")
			{
				UE_LOG(LogTemp, Warning, TEXT("ASSIGNING PIN: %s"), *Pin->GetName());
				Node.myOutputProperty = FoundProperty;
				Pin->Direction = EEdGraphPinDirection::EGPD_Output;
				Node.myOutputProperty = FoundProperty;

			}
			//FoundProperty->CopyCompleteValue();
			
			///AddSourceTargetProperties(*PrefixedName, FoundProperty->GetFName());
			//Node.Test;
		}

		
	}
	*/
}


void UMotionMatcherGraphNode::CreateOutputPins()
{
	Super::CreateOutputPins();
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->GetFName() == "MotionMatcherInterface")
		{
			Pin->bDisplayAsMutableRef = true;
			Pin->bDefaultValueIsIgnored = true;
		}
	}
}


#undef LOCTEXT_NAMESPACE