// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "MagicaVoxelUnrealIntegrationStyle.h"

class FMagicaVoxelUnrealIntegrationCommands : public TCommands<FMagicaVoxelUnrealIntegrationCommands>
{
public:

	FMagicaVoxelUnrealIntegrationCommands()
		: TCommands<FMagicaVoxelUnrealIntegrationCommands>(TEXT("MagicaVoxelUnrealIntegration"), NSLOCTEXT("Contexts", "MagicaVoxelUnrealIntegration", "MagicaVoxelUnrealIntegration Plugin"), NAME_None, FMagicaVoxelUnrealIntegrationStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};