// Copyright Zankyo Studio. All Rights Reserved.

using UnrealBuildTool;

public class MagicaVoxelUnrealIntegration : ModuleRules
{
	public MagicaVoxelUnrealIntegration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Niagara",
				"Projects",
				"RenderCore",
				"RHI",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MeshDescription",
				"StaticMeshDescription",
			}
		);
	}
}
