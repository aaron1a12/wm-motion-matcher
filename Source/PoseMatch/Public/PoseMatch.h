// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FPoseMatchModule : public IModuleInterface
{
public:

	float globalFloat = 0.f;

	/** singleton for the module while loaded and available */
	static FPoseMatchModule* Singleton;

	static FPoseMatchModule& Get();

	float GetFloat();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
