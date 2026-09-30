// Copyright Zankyo Studio. All Rights Reserved.

#include "SMagicaVoxelPreviewWindow.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "ContentBrowserModule.h"
#include "EditorViewportClient.h"
#include "Engine/StaticMesh.h"
#include "IContentBrowserSingleton.h"
#include "MagicaVoxelData.h"
#include "MagicaVoxelUtility.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"
#include "PreviewScene.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/AppStyle.h"
#include "UnrealWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMagicaVoxelPreviewWindow"

namespace
{
	class FMagicaVoxelPreviewViewportClient : public FEditorViewportClient
	{
	public:
		FMagicaVoxelPreviewViewportClient(FPreviewScene* InPreviewScene, const TSharedRef<SMagicaVoxelMeshViewport>& InViewport)
			: FEditorViewportClient(nullptr, InPreviewScene, StaticCastSharedRef<SEditorViewport>(InViewport))
		{
			SetViewMode(VMI_Lit);
			SetViewLocation(FVector(-128.0, -128.0, 96.0));
			SetViewRotation(FRotator(-25.0, 45.0, 0.0));
			EngineShowFlags.SetSelectionOutline(false);
			EngineShowFlags.SetGrid(true);
			EngineShowFlags.SetSnap(false);
			DrawHelper.bDrawGrid = true;
			DrawHelper.bDrawPivot = false;
			bUsingOrbitCamera = true;

			if (Widget)
			{
				Widget->SetDefaultVisibility(false);
			}
		}

		virtual void Tick(float DeltaSeconds) override
		{
			FEditorViewportClient::Tick(DeltaSeconds);

			if (PreviewScene && PreviewScene->GetWorld())
			{
				PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
			}
		}
	};

	FString MakeDefaultMeshAssetName(const UMagicaVoxelData* VoxelData, const int32 ModelIndex)
	{
		const FString BaseName = VoxelData ? VoxelData->GetName() : TEXT("MagicaVoxelMesh");
		return FString::Printf(TEXT("%s_Model_%d_StaticMesh"), *BaseName, ModelIndex);
	}

	const FMagicaVoxelModel* GetSelectedModel(const UMagicaVoxelData* VoxelData, const int32 ModelIndex)
	{
		if (!VoxelData || !VoxelData->Models.IsValidIndex(ModelIndex))
		{
			return nullptr;
		}

		return &VoxelData->Models[ModelIndex];
	}
}

void SMagicaVoxelMeshViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeUnique<FPreviewScene>(
		FPreviewScene::ConstructionValues()
		.SetLightBrightness(4.0f)
		.SetSkyBrightness(0.75f));

	PreviewComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), TEXT("MagicaVoxelPreviewComponent"), RF_Transient);
	PreviewComponent->SetMobility(EComponentMobility::Movable);
	PreviewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewScene->AddComponent(PreviewComponent, FTransform::Identity);

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SMagicaVoxelMeshViewport::~SMagicaVoxelMeshViewport()
{
	if (PreviewScene.IsValid() && PreviewComponent)
	{
		PreviewScene->RemoveComponent(PreviewComponent);
	}
}

void SMagicaVoxelMeshViewport::SetPreviewMesh(UStaticMesh* StaticMesh)
{
	if (!PreviewComponent)
	{
		return;
	}

	PreviewComponent->SetStaticMesh(StaticMesh);
	PreviewComponent->SetVisibility(StaticMesh != nullptr);
	PreviewComponent->UpdateBounds();
	PreviewComponent->MarkRenderStateDirty();
	FocusPreviewMesh();
	Invalidate();
}

void SMagicaVoxelMeshViewport::SetPreviewMaterials(const TArray<UMaterialInterface*>& Materials)
{
	if (!PreviewComponent)
	{
		return;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		PreviewComponent->SetMaterial(MaterialIndex, Materials[MaterialIndex]);
	}

	PreviewComponent->MarkRenderStateDirty();
	Invalidate();
}

TSharedRef<FEditorViewportClient> SMagicaVoxelMeshViewport::MakeEditorViewportClient()
{
	if (!PreviewViewportClient.IsValid())
	{
		PreviewViewportClient = MakeShared<FMagicaVoxelPreviewViewportClient>(PreviewScene.Get(), SharedThis(this));
	}

	return PreviewViewportClient.ToSharedRef();
}

void SMagicaVoxelMeshViewport::FocusPreviewMesh()
{
	if (!PreviewViewportClient.IsValid() || !PreviewComponent || !PreviewComponent->GetStaticMesh())
	{
		return;
	}

	const FBoxSphereBounds Bounds = PreviewComponent->Bounds;
	const FBox FocusBox = Bounds.GetBox().ExpandBy(FVector(8.0));
	PreviewViewportClient->FocusViewportOnBox(FocusBox, true);
}

void SMagicaVoxelPreviewWindow::Construct(const FArguments& InArgs)
{
	StatusText = LOCTEXT("NoVoxelDataSelected", "Select a MagicaVoxel data asset to preview.");

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UMagicaVoxelData::StaticClass())
				.AllowClear(true)
				.DisplayThumbnail(true)
				.ObjectPath(this, &SMagicaVoxelPreviewWindow::GetSelectedVoxelDataPath)
				.OnObjectChanged(this, &SMagicaVoxelPreviewWindow::OnVoxelDataChanged)
				.OnShouldSetAsset(this, &SMagicaVoxelPreviewWindow::CanUseVoxelDataAsset)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveStaticMeshButton", "Save Static Mesh"))
				.IsEnabled(this, &SMagicaVoxelPreviewWindow::CanSavePreviewMesh)
				.OnClicked(this, &SMagicaVoxelPreviewWindow::SavePreviewMesh)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SMagicaVoxelPreviewWindow::GetModelLabel)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SSlider)
				.IsEnabled(this, &SMagicaVoxelPreviewWindow::IsModelSliderEnabled)
				.Value(this, &SMagicaVoxelPreviewWindow::GetModelSliderValue)
				.OnValueChanged(this, &SMagicaVoxelPreviewWindow::OnModelSliderChanged)
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Brushes.Panel"))
			.Padding(0.0f)
			[
				SAssignNew(Viewport, SMagicaVoxelMeshViewport)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.Text(this, &SMagicaVoxelPreviewWindow::GetStatusText)
		]
	];
}

FString SMagicaVoxelPreviewWindow::GetSelectedVoxelDataPath() const
{
	return SelectedVoxelData.IsValid() ? SelectedVoxelData->GetPathName() : FString();
}

void SMagicaVoxelPreviewWindow::OnVoxelDataChanged(const FAssetData& AssetData)
{
	SelectedVoxelData.Reset();
	SelectedModelIndex = 0;

	if (UMagicaVoxelData* VoxelData = Cast<UMagicaVoxelData>(AssetData.GetAsset()))
	{
		SelectedVoxelData = TStrongObjectPtr<UMagicaVoxelData>(VoxelData);
	}

	RefreshPreviewMesh();
}

bool SMagicaVoxelPreviewWindow::CanUseVoxelDataAsset(const FAssetData& AssetData) const
{
	const UClass* AssetClass = AssetData.GetClass();
	return !AssetData.IsValid() || (AssetClass && AssetClass->IsChildOf(UMagicaVoxelData::StaticClass()));
}

int32 SMagicaVoxelPreviewWindow::GetModelCount() const
{
	return SelectedVoxelData.IsValid() ? SelectedVoxelData->Models.Num() : 0;
}

int32 SMagicaVoxelPreviewWindow::GetSelectedModelIndex() const
{
	const int32 ModelCount = GetModelCount();
	return ModelCount > 0 ? FMath::Clamp(SelectedModelIndex, 0, ModelCount - 1) : 0;
}

FText SMagicaVoxelPreviewWindow::GetModelLabel() const
{
	const int32 ModelCount = GetModelCount();
	if (ModelCount <= 0)
	{
		return LOCTEXT("NoModelLabel", "Model: none");
	}

	return FText::Format(LOCTEXT("ModelLabel", "Model: {0} / {1}"), FText::AsNumber(GetSelectedModelIndex() + 1), FText::AsNumber(ModelCount));
}

float SMagicaVoxelPreviewWindow::GetModelSliderValue() const
{
	const int32 ModelCount = GetModelCount();
	if (ModelCount <= 1)
	{
		return 0.0f;
	}

	return static_cast<float>(GetSelectedModelIndex()) / static_cast<float>(ModelCount - 1);
}

void SMagicaVoxelPreviewWindow::OnModelSliderChanged(float NewValue)
{
	const int32 ModelCount = GetModelCount();
	if (ModelCount <= 1)
	{
		return;
	}

	const int32 NewModelIndex = FMath::Clamp(FMath::RoundToInt(NewValue * static_cast<float>(ModelCount - 1)), 0, ModelCount - 1);
	if (NewModelIndex != SelectedModelIndex)
	{
		SelectedModelIndex = NewModelIndex;
		RefreshPreviewMesh();
	}
}

bool SMagicaVoxelPreviewWindow::IsModelSliderEnabled() const
{
	return GetModelCount() > 1;
}

FReply SMagicaVoxelPreviewWindow::SavePreviewMesh()
{
	const UMagicaVoxelData* VoxelData = SelectedVoxelData.Get();
	const FMagicaVoxelModel* Model = GetSelectedModel(VoxelData, GetSelectedModelIndex());
	if (!Model)
	{
		return FReply::Handled();
	}

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveStaticMeshDialogTitle", "Save MagicaVoxel Static Mesh");
	SaveAssetDialogConfig.DefaultPath = TEXT("/Game");
	SaveAssetDialogConfig.DefaultAssetName = MakeDefaultMeshAssetName(VoxelData, GetSelectedModelIndex());
	SaveAssetDialogConfig.AssetClassNames.Add(UStaticMesh::StaticClass()->GetClassPathName());
	SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
	if (SaveObjectPath.IsEmpty())
	{
		return FReply::Handled();
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	const FString AssetName = FPackageName::ObjectPathToObjectName(SaveObjectPath);
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		StatusText = LOCTEXT("PackageCreateFailed", "Failed to create the target package.");
		return FReply::Handled();
	}

	UStaticMesh* SavedMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!SavedMesh || !UMagicaVoxelUtility::PopulateStaticMeshFromModel(SavedMesh, *Model))
	{
		StatusText = LOCTEXT("SaveMeshFailed", "Failed to build the static mesh asset.");
		return FReply::Handled();
	}

	FAssetRegistryModule::AssetCreated(SavedMesh);
	Package->MarkPackageDirty();
	StatusText = FText::Format(LOCTEXT("SavedMeshStatus", "Saved {0}."), FText::FromString(SaveObjectPath));

	return FReply::Handled();
}

bool SMagicaVoxelPreviewWindow::CanSavePreviewMesh() const
{
	return PreviewMesh.IsValid() && GetSelectedModel(SelectedVoxelData.Get(), GetSelectedModelIndex()) != nullptr;
}

void SMagicaVoxelPreviewWindow::RefreshPreviewMesh()
{
	PreviewMaterials.Reset();
	PreviewMesh.Reset();

	const UMagicaVoxelData* VoxelData = SelectedVoxelData.Get();
	const int32 ModelIndex = GetSelectedModelIndex();
	const FMagicaVoxelModel* Model = GetSelectedModel(VoxelData, ModelIndex);
	if (!Model)
	{
		StatusText = VoxelData
			? LOCTEXT("NoModelsInVoxelData", "The selected MagicaVoxel data asset has no models.")
			: LOCTEXT("NoVoxelDataSelected", "Select a MagicaVoxel data asset to preview.");

		if (Viewport.IsValid())
		{
			Viewport->SetPreviewMesh(nullptr);
		}
		return;
	}

	UStaticMesh* NewPreviewMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!UMagicaVoxelUtility::PopulateStaticMeshFromModel(NewPreviewMesh, *Model))
	{
		StatusText = LOCTEXT("PreviewBuildFailed", "Failed to build the preview static mesh.");
		if (Viewport.IsValid())
		{
			Viewport->SetPreviewMesh(nullptr);
		}
		return;
	}

	PreviewMesh = TStrongObjectPtr<UStaticMesh>(NewPreviewMesh);

	if (Viewport.IsValid())
	{
		Viewport->SetPreviewMesh(PreviewMesh.Get());
	}

	ApplyPreviewMaterials();
	StatusText = FText::Format(
		LOCTEXT("PreviewStatus", "Previewing model {0}: {1} voxels, size {2}."),
		FText::AsNumber(ModelIndex + 1),
		FText::AsNumber(Model->Voxels.Num()),
		FText::FromString(Model->Size.ToString()));
}

void SMagicaVoxelPreviewWindow::ApplyPreviewMaterials()
{
	if (!Viewport.IsValid() || !PreviewMesh.IsValid() || !SelectedVoxelData.IsValid())
	{
		return;
	}

	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!BaseMaterial)
	{
		BaseMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	TArray<UMaterialInterface*> Materials;
	const TArray<FStaticMaterial>& StaticMaterials = PreviewMesh->GetStaticMaterials();
	Materials.Reserve(StaticMaterials.Num());

	for (int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
	{
		const FName SlotName = StaticMaterials[MaterialIndex].MaterialSlotName;
		FString SlotNameString = SlotName.ToString();

		int32 ColorIndex = INDEX_NONE;
		if (SlotNameString.RemoveFromStart(TEXT("ColorIndex_")))
		{
			LexFromString(ColorIndex, *SlotNameString);
		}

		UMaterialInstanceDynamic* PreviewMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, GetTransientPackage());
		if (PreviewMaterial && SelectedVoxelData->Palette.IsValidIndex(ColorIndex))
		{
			PreviewMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(SelectedVoxelData->Palette[ColorIndex]));
			PreviewMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(SelectedVoxelData->Palette[ColorIndex]));
		}

		PreviewMaterials.Add(TStrongObjectPtr<UMaterialInstanceDynamic>(PreviewMaterial));
		Materials.Add(PreviewMaterial ? PreviewMaterial : BaseMaterial);
	}

	Viewport->SetPreviewMaterials(Materials);
}

FText SMagicaVoxelPreviewWindow::GetStatusText() const
{
	return StatusText;
}

#undef LOCTEXT_NAMESPACE
