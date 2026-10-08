// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelNiagaraDataInterface.h"

#include "MagicaVoxelNiagaraData.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraTypes.h"
#include "GlobalRenderResources.h"
#include "VectorVM.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MagicaVoxelNiagaraDataInterface)

#define LOCTEXT_NAMESPACE "MagicaVoxelNiagaraDataInterface"

DECLARE_LOG_CATEGORY_CLASS(LogMagicaVoxelNiagaraDataInterface, Log, All);

namespace MagicaVoxelNiagaraDataInterfaceLocal
{
	const FName NAME_GetVoxelCount(TEXT("GetVoxelCount"));
	const FName NAME_GetModelSize(TEXT("GetModelSize"));
	const FName NAME_IsValidVoxelIndex(TEXT("IsValidVoxelIndex"));
	const FName NAME_GetVoxelPosition(TEXT("GetVoxelPosition"));
	const FName NAME_GetVoxelColor(TEXT("GetVoxelColor"));
	const FName NAME_GetVoxelColorIndex(TEXT("GetVoxelColorIndex"));
	const FName NAME_GetVoxel(TEXT("GetVoxel"));

	const TCHAR* TemplateShaderFilePath = TEXT("/Plugin/MagicaVoxelUnrealIntegration/Private/MagicaVoxelNiagaraDataInterfaceTemplate.ush");

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int32, VoxelCount)
		SHADER_PARAMETER(FVector3f, ModelSize)
		SHADER_PARAMETER_SRV(Buffer<float4>, PositionAndColorIndexBuffer)
		SHADER_PARAMETER_SRV(Buffer<float4>, ColorBuffer)
	END_SHADER_PARAMETER_STRUCT()

	struct FInstanceData_GameThread
	{
		const UMagicaVoxelNiagaraData* NiagaraData = nullptr;
	};

	struct FInstanceData_RenderThread
	{
		const FMagicaVoxelNiagaraDataRenderResources* RenderResources = nullptr;
	};

	struct FGameToRenderInstanceData
	{
		const FMagicaVoxelNiagaraDataRenderResources* RenderResources = nullptr;
	};

	struct FProxy : public FNiagaraDataInterfaceProxy
	{
		virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
		{
			return sizeof(FGameToRenderInstanceData);
		}

		virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override
		{
			FGameToRenderInstanceData* GameToRenderInstanceData = reinterpret_cast<FGameToRenderInstanceData*>(PerInstanceData);
			FInstanceData_RenderThread& InstanceData = PerInstanceData_RenderThread.FindOrAdd(Instance);
			InstanceData.RenderResources = GameToRenderInstanceData->RenderResources;
			GameToRenderInstanceData->~FGameToRenderInstanceData();
		}

		TMap<FNiagaraSystemInstanceID, FInstanceData_RenderThread> PerInstanceData_RenderThread;
	};
}

UMagicaVoxelNiagaraDataInterface::UMagicaVoxelNiagaraDataInterface(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new MagicaVoxelNiagaraDataInterfaceLocal::FProxy());
}

void UMagicaVoxelNiagaraDataInterface::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

bool UMagicaVoxelNiagaraDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UMagicaVoxelNiagaraDataInterface* OtherTyped = CastChecked<const UMagicaVoxelNiagaraDataInterface>(Other);
	return OtherTyped->NiagaraData == NiagaraData;
}

bool UMagicaVoxelNiagaraDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UMagicaVoxelNiagaraDataInterface* DestinationTyped = CastChecked<UMagicaVoxelNiagaraDataInterface>(Destination);
	DestinationTyped->NiagaraData = NiagaraData;
	return true;
}

bool UMagicaVoxelNiagaraDataInterface::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	return Target == ENiagaraSimTarget::CPUSim || Target == ENiagaraSimTarget::GPUComputeSim;
}

bool UMagicaVoxelNiagaraDataInterface::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	FInstanceData_GameThread* InstanceData = new(PerInstanceData) FInstanceData_GameThread();
	InstanceData->NiagaraData = NiagaraData;

	if (IsUsedWithGPUScript() && IsValid(NiagaraData))
	{
		TArray<FVector4f> PositionsAndColorIndices = NiagaraData->PositionsAndColorIndices;
		TArray<FVector4f> Colors = NiagaraData->Colors;
		const FIntVector ModelSize = NiagaraData->ModelSize;

		ENQUEUE_RENDER_COMMAND(FMagicaVoxelNiagaraDataInterface_AddProxy)
		(
			[
				Proxy_RT = GetProxyAs<FProxy>(),
				InstanceID_RT = SystemInstance->GetId(),
				NiagaraData_RT = NiagaraData.Get(),
				PositionsAndColorIndices_RT = MoveTemp(PositionsAndColorIndices),
				Colors_RT = MoveTemp(Colors),
				ModelSize_RT = ModelSize
			](FRHICommandListImmediate& RHICmdList) mutable
			{
				NiagaraData_RT->UpdateRenderResources(RHICmdList, PositionsAndColorIndices_RT, Colors_RT, ModelSize_RT);
				Proxy_RT->PerInstanceData_RenderThread.FindOrAdd(InstanceID_RT).RenderResources = &NiagaraData_RT->GetRenderResources();
			}
		);
	}

	return true;
}

void UMagicaVoxelNiagaraDataInterface::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	FInstanceData_GameThread* InstanceData = reinterpret_cast<FInstanceData_GameThread*>(PerInstanceData);
	InstanceData->~FInstanceData_GameThread();

	if (IsUsedWithGPUScript())
	{
		ENQUEUE_RENDER_COMMAND(FMagicaVoxelNiagaraDataInterface_RemoveProxy)
		(
			[Proxy_RT = GetProxyAs<FProxy>(), InstanceID_RT = SystemInstance->GetId()](FRHICommandListImmediate& RHICmdList)
			{
				Proxy_RT->PerInstanceData_RenderThread.Remove(InstanceID_RT);
			}
		);
	}
}

int32 UMagicaVoxelNiagaraDataInterface::PerInstanceDataSize() const
{
	return sizeof(MagicaVoxelNiagaraDataInterfaceLocal::FInstanceData_GameThread);
}

void UMagicaVoxelNiagaraDataInterface::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	const FInstanceData_GameThread* InstanceData = reinterpret_cast<const FInstanceData_GameThread*>(PerInstanceData);
	FGameToRenderInstanceData* GameToRenderInstanceData = new(DataForRenderThread) FGameToRenderInstanceData();
	GameToRenderInstanceData->RenderResources = IsValid(InstanceData->NiagaraData) ? &InstanceData->NiagaraData->GetRenderResources() : nullptr;
}

#if WITH_EDITORONLY_DATA
void UMagicaVoxelNiagaraDataInterface::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetVoxelCount;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelCount")));
		Sig.SetDescription(LOCTEXT("GetVoxelCountDesc", "Returns the number of voxels in the selected MagicaVoxel model."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetModelSize;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ModelSize")));
		Sig.SetDescription(LOCTEXT("GetModelSizeDesc", "Returns the selected model size in voxel cells."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_IsValidVoxelIndex;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelIndex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")));
		Sig.SetDescription(LOCTEXT("IsValidVoxelIndexDesc", "Returns true when VoxelIndex exists in the selected model."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetVoxelPosition;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelIndex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.SetDescription(LOCTEXT("GetVoxelPositionDesc", "Returns the selected voxel's packed local position."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetVoxelColor;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelIndex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
		Sig.SetDescription(LOCTEXT("GetVoxelColorDesc", "Returns the selected voxel's palette color."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetVoxelColorIndex;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelIndex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ColorIndex")));
		Sig.SetDescription(LOCTEXT("GetVoxelColorIndexDesc", "Returns the selected voxel's MagicaVoxel palette index."));
	}

	{
		FNiagaraFunctionSignature& Sig = OutFunctions.AddDefaulted_GetRef();
		Sig.Name = NAME_GetVoxel;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("MagicaVoxel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("VoxelIndex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ColorIndex")));
		Sig.SetDescription(LOCTEXT("GetVoxelDesc", "Returns validity, local position, palette color, and palette index for the selected voxel."));
	}
}
#endif

#if WITH_EDITORONLY_DATA
void UMagicaVoxelNiagaraDataInterface::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	const TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{TEXT("ParameterName"), ParamInfo.DataInterfaceHLSLSymbol},
	};
	AppendTemplateHLSL(OutHLSL, TemplateShaderFilePath, TemplateArgs);
}

bool UMagicaVoxelNiagaraDataInterface::GetFunctionHLSL(const FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	static const TSet<FName> TemplateFunctions =
	{
		NAME_GetVoxelCount,
		NAME_GetModelSize,
		NAME_IsValidVoxelIndex,
		NAME_GetVoxelPosition,
		NAME_GetVoxelColor,
		NAME_GetVoxelColorIndex,
		NAME_GetVoxel,
	};

	return TemplateFunctions.Contains(HlslGenContext.GetFunctionInfo().DefinitionName);
}

bool UMagicaVoxelNiagaraDataInterface::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	const bool bSuccess = Super::AppendCompileHash(InVisitor);
	InVisitor->UpdateShaderFile(TemplateShaderFilePath);
	InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}
#endif

void UMagicaVoxelNiagaraDataInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UMagicaVoxelNiagaraDataInterface::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	const FProxy& DIProxy = Context.GetProxy<FProxy>();
	const FInstanceData_RenderThread* InstanceData = DIProxy.PerInstanceData_RenderThread.Find(Context.GetSystemInstanceID());
	const FMagicaVoxelNiagaraDataRenderResources* RenderResources = InstanceData ? InstanceData->RenderResources : nullptr;

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	ShaderParameters->VoxelCount = RenderResources ? RenderResources->VoxelCount : 0;
	ShaderParameters->ModelSize = RenderResources ? RenderResources->ModelSize : FVector3f::ZeroVector;
	ShaderParameters->PositionAndColorIndexBuffer = RenderResources && RenderResources->PositionAndColorIndexBuffer.SRV
		? RenderResources->PositionAndColorIndexBuffer.SRV
		: GBlackFloat4VertexBufferWithSRV->ShaderResourceViewRHI;
	ShaderParameters->ColorBuffer = RenderResources && RenderResources->ColorBuffer.SRV
		? RenderResources->ColorBuffer.SRV
		: GBlackFloat4VertexBufferWithSRV->ShaderResourceViewRHI;
}

void UMagicaVoxelNiagaraDataInterface::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	if (BindingInfo.Name == NAME_GetVoxelCount && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetVoxelCount);
	}
	else if (BindingInfo.Name == NAME_GetModelSize && BindingInfo.GetNumOutputs() == 3)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetModelSize);
	}
	else if (BindingInfo.Name == NAME_IsValidVoxelIndex && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMIsValidVoxelIndex);
	}
	else if (BindingInfo.Name == NAME_GetVoxelPosition && BindingInfo.GetNumOutputs() == 3)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetVoxelPosition);
	}
	else if (BindingInfo.Name == NAME_GetVoxelColor && BindingInfo.GetNumOutputs() == 4)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetVoxelColor);
	}
	else if (BindingInfo.Name == NAME_GetVoxelColorIndex && BindingInfo.GetNumOutputs() == 1)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetVoxelColorIndex);
	}
	else if (BindingInfo.Name == NAME_GetVoxel && BindingInfo.GetNumOutputs() == 9)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UMagicaVoxelNiagaraDataInterface::VMGetVoxel);
	}
	else
	{
		UE_LOG(LogMagicaVoxelNiagaraDataInterface, Warning, TEXT("Could not find MagicaVoxel data interface external function. Name: %s, Inputs: %d, Outputs: %d"),
			*BindingInfo.Name.ToString(),
			BindingInfo.GetNumInputs(),
			BindingInfo.GetNumOutputs());
		OutFunc = FVMExternalFunction();
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetVoxelCount(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIOutputParam<int32> OutVoxelCount(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		OutVoxelCount.SetAndAdvance(VoxelCount);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetModelSize(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIOutputParam<FVector3f> OutModelSize(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const FVector3f ModelSize = IsValid(Data)
		? FVector3f(
			static_cast<float>(Data->ModelSize.X),
			static_cast<float>(Data->ModelSize.Y),
			static_cast<float>(Data->ModelSize.Z))
		: FVector3f::ZeroVector;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		OutModelSize.SetAndAdvance(ModelSize);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMIsValidVoxelIndex(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIInputParam<int32> InVoxelIndex(Context);
	FNDIOutputParam<bool> OutIsValid(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		const int32 VoxelIndex = InVoxelIndex.GetAndAdvance();
		OutIsValid.SetAndAdvance(VoxelIndex >= 0 && VoxelIndex < VoxelCount);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetVoxelPosition(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIInputParam<int32> InVoxelIndex(Context);
	FNDIOutputParam<FVector3f> OutPosition(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		const int32 VoxelIndex = InVoxelIndex.GetAndAdvance();
		const bool bIsValid = VoxelIndex >= 0 && VoxelIndex < VoxelCount;
		const FVector4f PackedPosition = bIsValid ? Data->PositionsAndColorIndices[VoxelIndex] : FVector4f::Zero();
		const FVector3f Position(PackedPosition.X, PackedPosition.Y, PackedPosition.Z);

		OutPosition.SetAndAdvance(Position);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetVoxelColor(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIInputParam<int32> InVoxelIndex(Context);
	FNDIOutputParam<FLinearColor> OutColor(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		const int32 VoxelIndex = InVoxelIndex.GetAndAdvance();
		const bool bIsValid = VoxelIndex >= 0 && VoxelIndex < VoxelCount;
		const int32 ColorIndex = bIsValid ? FMath::RoundToInt(Data->PositionsAndColorIndices[VoxelIndex].W) : INDEX_NONE;
		const FVector4f PackedColor = Data && Data->Colors.IsValidIndex(ColorIndex) ? Data->Colors[ColorIndex] : FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
		const FLinearColor Color(PackedColor.X, PackedColor.Y, PackedColor.Z, PackedColor.W);

		OutColor.SetAndAdvance(Color);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetVoxelColorIndex(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIInputParam<int32> InVoxelIndex(Context);
	FNDIOutputParam<int32> OutColorIndex(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		const int32 VoxelIndex = InVoxelIndex.GetAndAdvance();
		const bool bIsValid = VoxelIndex >= 0 && VoxelIndex < VoxelCount;
		OutColorIndex.SetAndAdvance(bIsValid ? FMath::RoundToInt(Data->PositionsAndColorIndices[VoxelIndex].W) : 0);
	}
}

void UMagicaVoxelNiagaraDataInterface::VMGetVoxel(FVectorVMExternalFunctionContext& Context)
{
	using namespace MagicaVoxelNiagaraDataInterfaceLocal;

	VectorVM::FUserPtrHandler<FInstanceData_GameThread> InstanceData(Context);
	FNDIInputParam<int32> InVoxelIndex(Context);
	FNDIOutputParam<bool> OutIsValid(Context);
	FNDIOutputParam<FVector3f> OutPosition(Context);
	FNDIOutputParam<FLinearColor> OutColor(Context);
	FNDIOutputParam<int32> OutColorIndex(Context);

	const UMagicaVoxelNiagaraData* Data = InstanceData.Get() ? InstanceData->NiagaraData : nullptr;
	const int32 VoxelCount = IsValid(Data) ? Data->PositionsAndColorIndices.Num() : 0;

	for (int32 InstanceIndex = 0; InstanceIndex < Context.GetNumInstances(); ++InstanceIndex)
	{
		const int32 VoxelIndex = InVoxelIndex.GetAndAdvance();
		const bool bIsValid = VoxelIndex >= 0 && VoxelIndex < VoxelCount;
		const FVector4f PackedPosition = bIsValid ? Data->PositionsAndColorIndices[VoxelIndex] : FVector4f::Zero();
		const int32 ColorIndex = bIsValid ? FMath::RoundToInt(PackedPosition.W) : INDEX_NONE;
		const FVector4f PackedColor = Data && Data->Colors.IsValidIndex(ColorIndex) ? Data->Colors[ColorIndex] : FVector4f(1.0f, 1.0f, 1.0f, 1.0f);

		OutIsValid.SetAndAdvance(bIsValid);
		OutPosition.SetAndAdvance(FVector3f(PackedPosition.X, PackedPosition.Y, PackedPosition.Z));
		OutColor.SetAndAdvance(FLinearColor(PackedColor.X, PackedColor.Y, PackedColor.Z, PackedColor.W));
		OutColorIndex.SetAndAdvance(bIsValid ? ColorIndex : 0);
	}
}

#undef LOCTEXT_NAMESPACE
