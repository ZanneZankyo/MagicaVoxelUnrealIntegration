// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MagicaVoxelData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SEditorViewport.h"
#include "Widgets/SCompoundWidget.h"

class FEditorViewportClient;
class FPreviewScene;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

class SMagicaVoxelMeshViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SMagicaVoxelMeshViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMagicaVoxelMeshViewport() override;

	void SetPreviewMesh(UStaticMesh* StaticMesh);
	void SetPreviewMaterials(const TArray<UMaterialInterface*>& Materials);

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	void FocusPreviewMesh();

private:
	TUniquePtr<FPreviewScene> PreviewScene;
	TSharedPtr<FEditorViewportClient> PreviewViewportClient;
	TObjectPtr<UStaticMeshComponent> PreviewComponent = nullptr;
};

class SMagicaVoxelPreviewWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMagicaVoxelPreviewWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString GetSelectedVoxelDataPath() const;
	void OnVoxelDataChanged(const FAssetData& AssetData);
	bool CanUseVoxelDataAsset(const FAssetData& AssetData) const;

	int32 GetModelCount() const;
	int32 GetSelectedModelIndex() const;
	FText GetModelLabel() const;
	float GetModelSliderValue() const;
	void OnModelSliderChanged(float NewValue);
	bool IsModelSliderEnabled() const;

	FReply SavePreviewMesh();
	bool CanSavePreviewMesh() const;
	FReply SavePackedVoxelData();
	bool CanSavePackedVoxelData() const;

	void RefreshPreviewMesh();
	void ApplyPreviewMaterials();
	FText GetStatusText() const;

private:
	TSharedPtr<SMagicaVoxelMeshViewport> Viewport;
	TStrongObjectPtr<UMagicaVoxelData> SelectedVoxelData;
	TStrongObjectPtr<UStaticMesh> PreviewMesh;
	TArray<TStrongObjectPtr<UMaterialInstanceDynamic>> PreviewMaterials;
	int32 SelectedModelIndex = 0;
	FText StatusText;
};
