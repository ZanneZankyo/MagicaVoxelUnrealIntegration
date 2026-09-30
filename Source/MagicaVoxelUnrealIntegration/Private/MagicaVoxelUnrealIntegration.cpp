// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelUnrealIntegration.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FMagicaVoxelUnrealIntegrationModule"

void FMagicaVoxelUnrealIntegrationModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MagicaVoxelUnrealIntegration"));
	if (Plugin.IsValid())
	{
		AddShaderSourceDirectoryMapping(
			TEXT("/Plugin/MagicaVoxelUnrealIntegration"),
			FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
	}
}

void FMagicaVoxelUnrealIntegrationModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMagicaVoxelUnrealIntegrationModule, MagicaVoxelUnrealIntegration)
