// Copyright Zankyo Studio. All Rights Reserved.

using UnrealBuildTool;

public class MagicaVoxelUnrealIntegrationEditor : ModuleRules
{
	public MagicaVoxelUnrealIntegrationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"MagicaVoxelUnrealIntegration",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"PropertyEditor",
				"ContentBrowser",
				"AssetRegistry",
				"Slate",
				"SlateCore",
			}
		);
	}
}
