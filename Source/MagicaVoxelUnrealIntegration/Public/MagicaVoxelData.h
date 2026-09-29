// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MagicaVoxelData.generated.h"

USTRUCT(BlueprintType)
struct FMagicaVoxelVoxel
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	uint8 X = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	uint8 Y = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	uint8 Z = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	uint8 ColorIndex = 0;
};

USTRUCT(BlueprintType)
struct FMagicaVoxelModel
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	FIntVector Size = FIntVector::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	TArray<FMagicaVoxelVoxel> Voxels;
};

UCLASS(BlueprintType)
class MAGICAVOXELUNREALINTEGRATION_API UMagicaVoxelData : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	int32 Version = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	int32 DeclaredModelCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	TArray<FMagicaVoxelModel> Models;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MagicaVoxel")
	TArray<FColor> Palette;
};
