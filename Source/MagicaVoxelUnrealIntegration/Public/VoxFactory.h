// Copyright Zankyo Studio. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "VoxFactory.generated.h"

UCLASS()
class UVoxFactory : public UFactory
{
	GENERATED_BODY()

public:
	UVoxFactory();

	// UFactory Interface
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	virtual bool DoesSupportClass(UClass* InClass) override;
};
