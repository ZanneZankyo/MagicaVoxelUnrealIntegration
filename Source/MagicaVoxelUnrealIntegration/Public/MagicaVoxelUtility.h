// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicaVoxelData.h"
#include "MagicaVoxelUtility.generated.h"

class UStaticMesh;
class UNiagaraComponent;

UCLASS()
class MAGICAVOXELUNREALINTEGRATION_API UMagicaVoxelUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MagicaVoxel|StaticMesh")
	static bool PopulateStaticMeshFromModel(UStaticMesh* StaticMesh, const FMagicaVoxelModel& Model, bool bUseGreedyMeshing = true);

	UFUNCTION(BlueprintCallable, Category = "MagicaVoxel|StaticMesh")
	static bool PopulateStaticMeshFromVoxels(UStaticMesh* StaticMesh, const TArray<FMagicaVoxelVoxel>& Voxels, bool bUseGreedyMeshing = true);

	UFUNCTION(BlueprintCallable, Category = "MagicaVoxel|Niagara")
	static bool InitializeVoxelModel(UNiagaraComponent* NiagaraComponent, const UMagicaVoxelData* Data, int32 ModelIndex, float VoxelSize = 1.f);
};
