// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FMagicaVoxelUnrealIntegrationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
