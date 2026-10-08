// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelNiagaraData.h"

#include "RHICommandList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MagicaVoxelNiagaraData)

namespace
{
	const FMagicaVoxelModel* GetValidModel(const UMagicaVoxelData* VoxelData, const int32 ModelIndex)
	{
		if (!IsValid(VoxelData) || !VoxelData->Models.IsValidIndex(ModelIndex))
		{
			return nullptr;
		}

		const FMagicaVoxelModel& Model = VoxelData->Models[ModelIndex];
		return Model.Size.X > 0 && Model.Size.Y > 0 && Model.Size.Z > 0 ? &Model : nullptr;
	}

	FVector3f MakeVoxelPosition(
		const FMagicaVoxelVoxel& Voxel,
		const FMagicaVoxelModel& Model,
		const float VoxelSize,
		const bool bUseCellCenters,
		const bool bCenterModel)
	{
		FVector Position(
			static_cast<double>(Voxel.X),
			static_cast<double>(Voxel.Y),
			static_cast<double>(Voxel.Z));

		if (bUseCellCenters)
		{
			Position += FVector(0.5);
		}

		if (bCenterModel)
		{
			const FVector ModelSize(
				static_cast<double>(Model.Size.X),
				static_cast<double>(Model.Size.Y),
				static_cast<double>(Model.Size.Z));
			Position -= ModelSize * 0.5;
		}

		return FVector3f(Position * static_cast<double>(VoxelSize));
	}

	FLinearColor GetVoxelColor(const UMagicaVoxelData* VoxelData, const FMagicaVoxelVoxel& Voxel)
	{
		const int32 PaletteIndex = static_cast<int32>(Voxel.ColorIndex);
		if (IsValid(VoxelData) && VoxelData->Palette.IsValidIndex(PaletteIndex))
		{
			FLinearColor Color = FLinearColor::FromSRGBColor(VoxelData->Palette[PaletteIndex]);
			Color.A = 1.0f;
			return Color;
		}

		return FLinearColor::White;
	}
}

FMagicaVoxelNiagaraDataRenderResources::~FMagicaVoxelNiagaraDataRenderResources()
{
	Release();
}

void FMagicaVoxelNiagaraDataRenderResources::Release()
{
	PositionAndColorIndexBuffer.Release();
	ColorBuffer.Release();
	VoxelCount = 0;
	ModelSize = FVector3f::ZeroVector;
}

void FMagicaVoxelNiagaraDataRenderResources::Update(
	FRHICommandListBase& RHICmdList,
	TConstArrayView<FVector4f> InPositionsAndColorIndices,
	TConstArrayView<FVector4f> InColors,
	const FIntVector& InModelSize)
{
	Release();

	TArray<FVector4f> SafePositionsAndColorIndices(InPositionsAndColorIndices);
	TArray<FVector4f> SafeColors(InColors);
	if (SafePositionsAndColorIndices.IsEmpty())
	{
		SafePositionsAndColorIndices.Add(FVector4f::Zero());
	}
	if (SafeColors.IsEmpty())
	{
		SafeColors.Add(FVector4f(1.0f, 1.0f, 1.0f, 1.0f));
	}

	VoxelCount = InPositionsAndColorIndices.Num();
	ModelSize = FVector3f(
		static_cast<float>(InModelSize.X),
		static_cast<float>(InModelSize.Y),
		static_cast<float>(InModelSize.Z));

	PositionAndColorIndexBuffer.InitializeWithData(
		RHICmdList,
		TEXT("MagicaVoxelNiagaraDataPositionsAndColorIndices"),
		sizeof(FVector4f),
		SafePositionsAndColorIndices.Num(),
		PF_A32B32G32R32F,
		BUF_Static,
		[&SafePositionsAndColorIndices](FRHIBufferInitializer& Initializer)
		{
			Initializer.WriteData(SafePositionsAndColorIndices.GetData(), SafePositionsAndColorIndices.Num() * sizeof(FVector4f));
		});

	ColorBuffer.InitializeWithData(
		RHICmdList,
		TEXT("MagicaVoxelNiagaraDataColors"),
		sizeof(FVector4f),
		SafeColors.Num(),
		PF_A32B32G32R32F,
		BUF_Static,
		[&SafeColors](FRHIBufferInitializer& Initializer)
		{
			Initializer.WriteData(SafeColors.GetData(), SafeColors.Num() * sizeof(FVector4f));
		});
}

void UMagicaVoxelNiagaraData::BeginDestroy()
{
	ReleaseRenderResources();
	Super::BeginDestroy();
}

bool UMagicaVoxelNiagaraData::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && ReleaseRenderResourcesFence.IsFenceComplete();
}

bool UMagicaVoxelNiagaraData::BuildFromVoxelData(
	const UMagicaVoxelData* VoxelData,
	const int32 ModelIndex)
{
	SourceVoxelData = const_cast<UMagicaVoxelData*>(VoxelData);
	SourceModelIndex = ModelIndex;

	const bool bBuilt = BuildPackedVoxelData(
		VoxelData,
		ModelIndex,
		VoxelSize,
		bUseCellCenters,
		bCenterModel,
		PositionsAndColorIndices,
		Colors,
		ModelSize);

	if (bBuilt)
	{
		QueueRenderResourcesUpdate();
	}

	return bBuilt;
}

bool UMagicaVoxelNiagaraData::RebuildFromSource()
{
	const bool bBuilt = BuildPackedVoxelData(
		SourceVoxelData,
		SourceModelIndex,
		VoxelSize,
		bUseCellCenters,
		bCenterModel,
		PositionsAndColorIndices,
		Colors,
		ModelSize);

	if (bBuilt)
	{
		MarkPackageDirty();
		QueueRenderResourcesUpdate();
	}

	return bBuilt;
}

void UMagicaVoxelNiagaraData::UpdateRenderResources(FRHICommandListBase& RHICmdList)
{
	RenderResources.Update(RHICmdList, PositionsAndColorIndices, Colors, ModelSize);
}

void UMagicaVoxelNiagaraData::UpdateRenderResources(
	FRHICommandListBase& RHICmdList,
	TConstArrayView<FVector4f> InPositionsAndColorIndices,
	TConstArrayView<FVector4f> InColors,
	const FIntVector& InModelSize)
{
	RenderResources.Update(RHICmdList, InPositionsAndColorIndices, InColors, InModelSize);
}

void UMagicaVoxelNiagaraData::ReleaseRenderResources()
{
	if (IsInRenderingThread())
	{
		RenderResources.Release();
		return;
	}

	FMagicaVoxelNiagaraDataRenderResources* Resources = &RenderResources;
	ENQUEUE_RENDER_COMMAND(FMagicaVoxelNiagaraData_ReleaseRenderResources)
	(
		[Resources](FRHICommandListImmediate& RHICmdList)
		{
			Resources->Release();
		}
	);
	ReleaseRenderResourcesFence.BeginFence();
}

void UMagicaVoxelNiagaraData::QueueRenderResourcesUpdate()
{
	TArray<FVector4f> PositionsAndColorIndicesCopy = PositionsAndColorIndices;
	TArray<FVector4f> ColorsCopy = Colors;
	const FIntVector ModelSizeCopy = ModelSize;

	ENQUEUE_RENDER_COMMAND(FMagicaVoxelNiagaraData_UpdateRenderResources)
	(
		[
			This = this,
			PositionsAndColorIndices_RT = MoveTemp(PositionsAndColorIndicesCopy),
			Colors_RT = MoveTemp(ColorsCopy),
			ModelSize_RT = ModelSizeCopy
		](FRHICommandListImmediate& RHICmdList) mutable
		{
			This->UpdateRenderResources(RHICmdList, PositionsAndColorIndices_RT, Colors_RT, ModelSize_RT);
		}
	);
}

#if WITH_EDITOR
void UMagicaVoxelNiagaraData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMagicaVoxelNiagaraData, VoxelSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMagicaVoxelNiagaraData, bUseCellCenters)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMagicaVoxelNiagaraData, bCenterModel)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMagicaVoxelNiagaraData, SourceVoxelData)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UMagicaVoxelNiagaraData, SourceModelIndex))
	{
		if (!RebuildFromSource())
		{
			MarkPackageDirty();
			QueueRenderResourcesUpdate();
		}
	}
}
#endif

bool UMagicaVoxelNiagaraData::BuildPackedVoxelData(
	const UMagicaVoxelData* VoxelData,
	const int32 ModelIndex,
	const float VoxelSize,
	const bool bUseCellCenters,
	const bool bCenterModel,
	TArray<FVector4f>& OutPositionsAndColorIndices,
	TArray<FVector4f>& OutColors,
	FIntVector& OutModelSize)
{
	OutPositionsAndColorIndices.Reset();
	OutColors.Reset();
	OutModelSize = FIntVector::ZeroValue;

	const FMagicaVoxelModel* Model = GetValidModel(VoxelData, ModelIndex);
	if (!Model)
	{
		return false;
	}

	OutModelSize = Model->Size;
	OutPositionsAndColorIndices.Reserve(Model->Voxels.Num());
	OutColors.Reserve(VoxelData->Palette.Num());

	for (const FMagicaVoxelVoxel& Voxel : Model->Voxels)
	{
		if (Voxel.X >= Model->Size.X || Voxel.Y >= Model->Size.Y || Voxel.Z >= Model->Size.Z)
		{
			continue;
		}

		const FVector3f Position = MakeVoxelPosition(Voxel, *Model, VoxelSize, bUseCellCenters, bCenterModel);
		OutPositionsAndColorIndices.Add(FVector4f(Position.X, Position.Y, Position.Z, static_cast<float>(Voxel.ColorIndex)));
	}
	
	for (const auto &Color : VoxelData->Palette)
	{
		FLinearColor LinearColor = FLinearColor::FromSRGBColor(Color);
		LinearColor.A = 1.0f;
		OutColors.Add(FVector4f(LinearColor.R, LinearColor.G, LinearColor.B, LinearColor.A));
	}
	
	return true;
}
