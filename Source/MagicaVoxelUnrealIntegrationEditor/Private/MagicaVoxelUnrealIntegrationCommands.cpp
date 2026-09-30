// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelUnrealIntegrationCommands.h"

#define LOCTEXT_NAMESPACE "FMagicaVoxelUnrealIntegrationModule"

void FMagicaVoxelUnrealIntegrationCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "MagicaVoxelUnrealIntegration", "Bring up MagicaVoxelUnrealIntegration window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
