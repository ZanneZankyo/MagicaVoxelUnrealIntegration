// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelUnrealIntegrationEditor.h"
#include "MagicaVoxelUnrealIntegrationStyle.h"
#include "MagicaVoxelUnrealIntegrationCommands.h"
#include "SMagicaVoxelPreviewWindow.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"

static const FName MagicaVoxelUnrealIntegrationTabName("MagicaVoxelUnrealIntegration");

#define LOCTEXT_NAMESPACE "FMagicaVoxelUnrealIntegrationEditorModule"

void FMagicaVoxelUnrealIntegrationEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FMagicaVoxelUnrealIntegrationStyle::Initialize();
	FMagicaVoxelUnrealIntegrationStyle::ReloadTextures();

	FMagicaVoxelUnrealIntegrationCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FMagicaVoxelUnrealIntegrationCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FMagicaVoxelUnrealIntegrationEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FMagicaVoxelUnrealIntegrationEditorModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MagicaVoxelUnrealIntegrationTabName, FOnSpawnTab::CreateRaw(this, &FMagicaVoxelUnrealIntegrationEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FMagicaVoxelUnrealIntegrationTabTitle", "MagicaVoxelUnrealIntegration"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FMagicaVoxelUnrealIntegrationEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FMagicaVoxelUnrealIntegrationStyle::Shutdown();

	FMagicaVoxelUnrealIntegrationCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MagicaVoxelUnrealIntegrationTabName);
}

TSharedRef<SDockTab> FMagicaVoxelUnrealIntegrationEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SMagicaVoxelPreviewWindow)
		];
}

void FMagicaVoxelUnrealIntegrationEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(MagicaVoxelUnrealIntegrationTabName);
}

void FMagicaVoxelUnrealIntegrationEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FMagicaVoxelUnrealIntegrationCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FMagicaVoxelUnrealIntegrationCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMagicaVoxelUnrealIntegrationEditorModule, MagicaVoxelUnrealIntegrationEditor)
