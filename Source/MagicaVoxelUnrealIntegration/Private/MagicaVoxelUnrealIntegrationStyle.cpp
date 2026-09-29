// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelUnrealIntegrationStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FMagicaVoxelUnrealIntegrationStyle::StyleInstance = nullptr;

void FMagicaVoxelUnrealIntegrationStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FMagicaVoxelUnrealIntegrationStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FMagicaVoxelUnrealIntegrationStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("MagicaVoxelUnrealIntegrationStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FMagicaVoxelUnrealIntegrationStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("MagicaVoxelUnrealIntegrationStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("MagicaVoxelUnrealIntegration")->GetBaseDir() / TEXT("Resources"));

	Style->Set("MagicaVoxelUnrealIntegration.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));

	return Style;
}

void FMagicaVoxelUnrealIntegrationStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FMagicaVoxelUnrealIntegrationStyle::Get()
{
	return *StyleInstance;
}
