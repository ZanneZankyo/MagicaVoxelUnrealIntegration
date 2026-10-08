// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MagicaVoxelData.h"
#include "RenderCommandFence.h"
#include "RHIUtilities.h"
#include "UObject/Object.h"
#include "MagicaVoxelNiagaraData.generated.h"

class FRHICommandListBase;

struct MAGICAVOXELUNREALINTEGRATION_API FMagicaVoxelNiagaraDataRenderResources
{
	~FMagicaVoxelNiagaraDataRenderResources();

	void Release();
	void Update(FRHICommandListBase& RHICmdList, TConstArrayView<FVector4f> InPositionsAndColorIndices, TConstArrayView<FVector4f> InColors, const FIntVector& InModelSize);

	int32 VoxelCount = 0;
	FVector3f ModelSize = FVector3f::ZeroVector;
	FReadBuffer PositionAndColorIndexBuffer;
	FReadBuffer ColorBuffer;
};

UCLASS(BlueprintType)
class MAGICAVOXELUNREALINTEGRATION_API UMagicaVoxelNiagaraData : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel|Niagara")
	TArray<FVector4f> PositionsAndColorIndices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel|Niagara")
	TArray<FVector4f> Colors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel|Niagara")
	FIntVector ModelSize = FIntVector::ZeroValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MagicaVoxel|Niagara")
	TObjectPtr<UMagicaVoxelData> SourceVoxelData = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MagicaVoxel|Niagara", meta = (ClampMin = "0"))
	int32 SourceModelIndex = INDEX_NONE;
	
	UPROPERTY(EditDefaultsOnly, Category = "MagicaVoxel|Niagara")
	float VoxelSize = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, Category = "MagicaVoxel|Niagara")
	bool bUseCellCenters = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "MagicaVoxel|Niagara")
	bool bCenterModel = false;

	bool BuildFromVoxelData(
		const UMagicaVoxelData* VoxelData,
		int32 ModelIndex);
	bool RebuildFromSource();

	void UpdateRenderResources(FRHICommandListBase& RHICmdList);
	void UpdateRenderResources(
		FRHICommandListBase& RHICmdList,
		TConstArrayView<FVector4f> InPositionsAndColorIndices,
		TConstArrayView<FVector4f> InColors,
		const FIntVector& InModelSize);
	void ReleaseRenderResources();

	const FMagicaVoxelNiagaraDataRenderResources& GetRenderResources() const
	{
		return RenderResources;
	}

	static bool BuildPackedVoxelData(
		const UMagicaVoxelData* VoxelData,
		int32 ModelIndex,
		const float VoxelSize,
		const bool bUseCellCenters,
		const bool bCenterModel,
		TArray<FVector4f>& OutPositionsAndColorIndices,
		TArray<FVector4f>& OutColors,
		FIntVector& OutModelSize);

private:
	void QueueRenderResourcesUpdate();

	FMagicaVoxelNiagaraDataRenderResources RenderResources;
	FRenderCommandFence ReleaseRenderResourcesFence;
};
