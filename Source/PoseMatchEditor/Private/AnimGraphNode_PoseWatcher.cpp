#include "AnimGraphNode_PoseWatcher.h"
#include "PoseMatchEditor.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_PoseWatcher::UAnimGraphNode_PoseWatcher(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FLinearColor UAnimGraphNode_PoseWatcher::GetNodeTitleColor() const
{
	return FLinearColor::FromSRGBColor(FColor(255, 128, 0));
}

FText UAnimGraphNode_PoseWatcher::GetTooltipText() const
{
	return LOCTEXT("PoseWatcherGraphNode_Tooltip", "Allows for more accurate blended pose matching by saving the current pose and sending it to the nearest Motion Matcher node to use during its pose search.\n\nPlace this AFTER inertialization nodes.");
}

FText UAnimGraphNode_PoseWatcher::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PoseWatcherGraphNode", "Pose Watcher");
}

FString UAnimGraphNode_PoseWatcher::GetNodeCategory() const
{
	return TEXT("Motion Matching");
}
