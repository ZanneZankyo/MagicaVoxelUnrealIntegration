// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicaVoxelData.h"
#include "MagicaVoxelStaticMeshUtility.generated.h"

class UStaticMesh;

UCLASS()
class MAGICAVOXELUNREALINTEGRATION_API UMagicaVoxelStaticMeshUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MagicaVoxel|StaticMesh")
	static bool PopulateStaticMeshFromModel(UStaticMesh* StaticMesh, const FMagicaVoxelModel& Model, bool bUseGreedyMeshing = true);

	UFUNCTION(BlueprintCallable, Category = "MagicaVoxel|StaticMesh")
	static bool PopulateStaticMeshFromVoxels(UStaticMesh* StaticMesh, const TArray<FMagicaVoxelVoxel>& Voxels, bool bUseGreedyMeshing = true);
};
