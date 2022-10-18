// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseMatch.h"
#include "PoseMatchSettings.h"

#if WITH_EDITOR
    #include "ISettingsModule.h"
    #include "ISettingsSection.h"
#endif

#define LOCTEXT_NAMESPACE "FPoseMatchModule"

FPoseMatchModule* FPoseMatchModule::Singleton = NULL;

FPoseMatchModule& FPoseMatchModule::Get()
{
    if (Singleton == NULL)
    {
        check(IsInGameThread());
        FModuleManager::LoadModuleChecked<FPoseMatchModule>("PoseMatch");
    }
    check(Singleton != NULL);
    return *Singleton;
}

float FPoseMatchModule::GetFloat()
{
    return globalFloat;
}

void FPoseMatchModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    Singleton = this;

    globalFloat = 256.f;
	

#if WITH_EDITOR
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "PoseMatch",
			LOCTEXT("PoseMatchSettingsName", "Pose Matching"),
			LOCTEXT("PoseMatchSettingsDescription", "Configure the Pose Matching plug-in."),
			GetMutableDefault<UPoseMatchSettings>()
		);

		if (SettingsSection.IsValid())
		{
			//SettingsSection->OnModified().BindRaw(this, &FTcpMessagingModule::HandleSettingsSaved);
		}
	}
#endif // WITH_EDITOR

	const UPoseMatchSettings* Settings = GetDefault<UPoseMatchSettings>();

	TArray<FName> myBones;
	Settings->GetBonesToMatch(myBones);

	if (myBones.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Bone: %s"), *myBones[0].ToString());
	}
}

void FPoseMatchModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
    Singleton = nullptr;

#if WITH_EDITOR
	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "PoseMatch");
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPoseMatchModule, PoseMatch)


void UPoseMatchSettings::GetBonesToMatch(TArray<FName>& Bones) const
{
	for (const FName& BoneName : BoneNames)
	{
		Bones.Add(BoneName);
	}
}