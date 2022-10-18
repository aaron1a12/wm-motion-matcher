// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseMatchEditor.h"
#include "AssetToolsModule.h"

#include "AssetTypeActions_MotionData.h"

#define LOCTEXT_NAMESPACE "FPoseMatchEditorModule"

void FPoseMatchEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TSharedRef<IAssetTypeActions> Action = MakeShareable(new FAssetTypeActions_MotionData());
	
	AssetTools.RegisterAssetTypeActions(Action);
	//CreatedAssetTypeActions.Add(Action);
	//RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_MotionField()));
}

void FPoseMatchEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPoseMatchEditorModule, PoseMatchEditor)