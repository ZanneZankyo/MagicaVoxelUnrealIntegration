// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MagicaVoxelData.h"
#include "NiagaraDataInterface.h"
#include "MagicaVoxelNiagaraDataInterface.generated.h"

/**
 * Niagara data interface for reading imported MagicaVoxel model data in CPU and GPU simulations.
 *
 * Typical Niagara usage:
 * - Use GetVoxelCount in an emitter spawn module to drive burst count.
 * - Use ExecutionIndex with GetVoxelPosition/GetVoxelColor during particle spawn.
 * - Render particles with a cube mesh renderer scaled to VoxelSize.
 */
UCLASS(EditInlineNew, Category = "MagicaVoxel", CollapseCategories, meta = (DisplayName = "MagicaVoxel Data"))
class MAGICAVOXELUNREALINTEGRATION_API UMagicaVoxelNiagaraDataInterface : public UNiagaraDataInterface
{
	GENERATED_BODY()

public:
	UMagicaVoxelNiagaraDataInterface(FObjectInitializer const& ObjectInitializer);

	virtual void PostInitProperties() override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;

#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicaVoxel")
	TObjectPtr<UMagicaVoxelData> VoxelData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicaVoxel", meta = (ClampMin = "0"))
	int32 ModelIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicaVoxel", meta = (ClampMin = "0.0001"))
	float VoxelSize = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicaVoxel")
	bool bUseCellCenters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MagicaVoxel")
	bool bCenterModel = true;

	void VMGetVoxelCount(FVectorVMExternalFunctionContext& Context);
	void VMGetModelSize(FVectorVMExternalFunctionContext& Context);
	void VMIsValidVoxelIndex(FVectorVMExternalFunctionContext& Context);
	void VMGetVoxelPosition(FVectorVMExternalFunctionContext& Context);
	void VMGetVoxelColor(FVectorVMExternalFunctionContext& Context);
	void VMGetVoxelColorIndex(FVectorVMExternalFunctionContext& Context);
	void VMGetVoxel(FVectorVMExternalFunctionContext& Context);

private:
	void BuildPackedVoxelData(TArray<FVector4f>& OutPositionsAndColorIndices, TArray<FVector4f>& OutColors, FIntVector& OutModelSize) const;
	const FMagicaVoxelModel* GetModel() const;
	bool GetVoxel(int32 VoxelIndex, const FMagicaVoxelVoxel*& OutVoxel, const FMagicaVoxelModel*& OutModel) const;
	FVector3f MakeVoxelPosition(const FMagicaVoxelVoxel& Voxel, const FMagicaVoxelModel& Model) const;
	FLinearColor GetVoxelColor(const FMagicaVoxelVoxel& Voxel) const;
};
