// Copyright Zankyo Studio. All Rights Reserved.

#include "VoxFactory.h"
#include "MagicaVoxelData.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_CLASS(LogVoxFactory, Log, All);

namespace
{
	struct FVoxChunkHeader
	{
		char Id[5] = {};
		int32 ContentByteSize = 0;
		int32 ChildrenChunksByteSize = 0;
	};

	class FVoxBinaryReader
	{
	public:
		FVoxBinaryReader(const uint8* InCursor, const uint8* InEnd)
			: Cursor(InCursor)
			, End(InEnd)
		{
		}

		bool AtEnd() const
		{
			return Cursor == End;
		}

		int64 Remaining() const
		{
			return End - Cursor;
		}

		const uint8* GetPosition() const
		{
			return Cursor;
		}

		bool CanRead(const int64 ByteCount) const
		{
			return ByteCount >= 0 && Cursor <= End && ByteCount <= Remaining();
		}

		bool ReadBytes(uint8* OutBytes, const int32 ByteCount)
		{
			if (!CanRead(ByteCount))
			{
				return false;
			}

			FMemory::Memcpy(OutBytes, Cursor, ByteCount);
			Cursor += ByteCount;
			return true;
		}

		bool ReadInt32(int32& OutValue)
		{
			if (!CanRead(sizeof(int32)))
			{
				return false;
			}

			const uint32 Value =
				(static_cast<uint32>(Cursor[0]) << 0) |
				(static_cast<uint32>(Cursor[1]) << 8) |
				(static_cast<uint32>(Cursor[2]) << 16) |
				(static_cast<uint32>(Cursor[3]) << 24);
			OutValue = static_cast<int32>(Value);
			Cursor += sizeof(int32);
			return true;
		}

		bool ReadChunkHeader(FVoxChunkHeader& OutHeader)
		{
			if (!CanRead(12))
			{
				return false;
			}

			FMemory::Memcpy(OutHeader.Id, Cursor, 4);
			OutHeader.Id[4] = '\0';
			Cursor += 4;

			return ReadInt32(OutHeader.ContentByteSize)
				&& ReadInt32(OutHeader.ChildrenChunksByteSize)
				&& OutHeader.ContentByteSize >= 0
				&& OutHeader.ChildrenChunksByteSize >= 0;
		}

		bool Skip(const int64 ByteCount)
		{
			if (!CanRead(ByteCount))
			{
				return false;
			}

			Cursor += ByteCount;
			return true;
		}

	private:
		const uint8* Cursor = nullptr;
		const uint8* End = nullptr;
	};

	bool IsChunkId(const FVoxChunkHeader& Header, const char* ExpectedId)
	{
		return FMemory::Memcmp(Header.Id, ExpectedId, 4) == 0;
	}

	FString ChunkIdToString(const FVoxChunkHeader& Header)
	{
		return FString(ANSI_TO_TCHAR(Header.Id));
	}

	bool ReadSizeChunk(const FVoxChunkHeader& Header, FVoxBinaryReader& ContentReader, FIntVector& OutSize)
	{
		if (Header.ContentByteSize != 12)
		{
			UE_LOG(LogVoxFactory, Error, TEXT("Invalid SIZE chunk size: %d bytes."), Header.ContentByteSize);
			return false;
		}

		return ContentReader.ReadInt32(OutSize.X)
			&& ContentReader.ReadInt32(OutSize.Y)
			&& ContentReader.ReadInt32(OutSize.Z);
	}

	bool ReadPackChunk(const FVoxChunkHeader& Header, FVoxBinaryReader& ContentReader, int32& OutModelCount)
	{
		if (Header.ContentByteSize != 4)
		{
			UE_LOG(LogVoxFactory, Error, TEXT("Invalid PACK chunk size: %d bytes."), Header.ContentByteSize);
			return false;
		}

		return ContentReader.ReadInt32(OutModelCount);
	}

	bool ReadXyziChunk(const FVoxChunkHeader& Header, FVoxBinaryReader& ContentReader, const FIntVector& ModelSize, FMagicaVoxelModel& OutModel)
	{
		int32 NumVoxels = 0;
		if (!ContentReader.ReadInt32(NumVoxels) || NumVoxels < 0)
		{
			UE_LOG(LogVoxFactory, Error, TEXT("Invalid XYZI voxel count."));
			return false;
		}

		const int64 ExpectedContentSize = sizeof(int32) + (static_cast<int64>(NumVoxels) * 4);
		if (ExpectedContentSize != Header.ContentByteSize)
		{
			UE_LOG(LogVoxFactory, Error, TEXT("Invalid XYZI chunk size: expected %lld bytes, got %d bytes."), ExpectedContentSize, Header.ContentByteSize);
			return false;
		}

		OutModel.Size = ModelSize;
		OutModel.Voxels.Empty(NumVoxels);

		for (int32 VoxelIndex = 0; VoxelIndex < NumVoxels; ++VoxelIndex)
		{
			uint8 VoxelBytes[4] = {};
			if (!ContentReader.ReadBytes(VoxelBytes, UE_ARRAY_COUNT(VoxelBytes)))
			{
				UE_LOG(LogVoxFactory, Error, TEXT("Unexpected end of XYZI voxel data."));
				return false;
			}

			FMagicaVoxelVoxel Voxel;
			Voxel.X = VoxelBytes[0];
			Voxel.Y = VoxelBytes[1];
			Voxel.Z = VoxelBytes[2];
			Voxel.ColorIndex = VoxelBytes[3];
			OutModel.Voxels.Add(Voxel);
		}

		return true;
	}

	bool ReadRgbaChunk(const FVoxChunkHeader& Header, FVoxBinaryReader& ContentReader, TArray<FColor>& OutPalette)
	{
		if (Header.ContentByteSize != 256 * 4)
		{
			UE_LOG(LogVoxFactory, Error, TEXT("Invalid RGBA chunk size: %d bytes."), Header.ContentByteSize);
			return false;
		}

		OutPalette.SetNumZeroed(256);
		OutPalette[0] = FColor::Transparent;

		for (int32 ColorIndex = 0; ColorIndex < 256; ++ColorIndex)
		{
			uint8 ColorBytes[4] = {};
			if (!ContentReader.ReadBytes(ColorBytes, UE_ARRAY_COUNT(ColorBytes)))
			{
				UE_LOG(LogVoxFactory, Error, TEXT("Unexpected end of RGBA palette data."));
				return false;
			}

			if (ColorIndex < 255)
			{
				OutPalette[ColorIndex + 1] = FColor(ColorBytes[0], ColorBytes[1], ColorBytes[2], ColorBytes[3]);
			}
		}

		return true;
	}

	bool ParseMainChildren(const uint8* ChildrenStart, const uint8* ChildrenEnd, UMagicaVoxelData* OutVoxelData)
	{
		FVoxBinaryReader ChunkReader(ChildrenStart, ChildrenEnd);
		FIntVector PendingSize = FIntVector::ZeroValue;
		bool bHasPendingSize = false;
		bool bHasPackChunk = false;

		while (!ChunkReader.AtEnd())
		{
			FVoxChunkHeader Header;
			if (!ChunkReader.ReadChunkHeader(Header))
			{
				UE_LOG(LogVoxFactory, Error, TEXT("Failed to read .vox chunk header."));
				return false;
			}

			const int64 ChunkPayloadSize = static_cast<int64>(Header.ContentByteSize) + Header.ChildrenChunksByteSize;
			if (!ChunkReader.CanRead(ChunkPayloadSize))
			{
				UE_LOG(LogVoxFactory, Error, TEXT("Chunk '%s' extends beyond the end of the MAIN chunk."), *ChunkIdToString(Header));
				return false;
			}

			const uint8* ContentStart = ChunkReader.GetPosition();
			const uint8* ContentEnd = ContentStart + Header.ContentByteSize;
			FVoxBinaryReader ContentReader(ContentStart, ContentEnd);

			if (IsChunkId(Header, "PACK"))
			{
				if (!ReadPackChunk(Header, ContentReader, OutVoxelData->DeclaredModelCount) || OutVoxelData->DeclaredModelCount < 0)
				{
					return false;
				}

				bHasPackChunk = true;
			}
			else if (IsChunkId(Header, "SIZE"))
			{
				if (bHasPendingSize)
				{
					UE_LOG(LogVoxFactory, Warning, TEXT("Found SIZE chunk before the previous SIZE was paired with an XYZI chunk."));
				}

				if (!ReadSizeChunk(Header, ContentReader, PendingSize))
				{
					return false;
				}

				bHasPendingSize = true;
			}
			else if (IsChunkId(Header, "XYZI"))
			{
				if (!bHasPendingSize)
				{
					UE_LOG(LogVoxFactory, Error, TEXT("Found XYZI chunk without a preceding SIZE chunk."));
					return false;
				}

				FMagicaVoxelModel Model;
				if (!ReadXyziChunk(Header, ContentReader, PendingSize, Model))
				{
					return false;
				}

				OutVoxelData->Models.Add(MoveTemp(Model));
				PendingSize = FIntVector::ZeroValue;
				bHasPendingSize = false;
			}
			else if (IsChunkId(Header, "RGBA"))
			{
				if (!ReadRgbaChunk(Header, ContentReader, OutVoxelData->Palette))
				{
					return false;
				}
			}

			if (!ChunkReader.Skip(ChunkPayloadSize))
			{
				UE_LOG(LogVoxFactory, Error, TEXT("Failed to skip chunk '%s'."), *ChunkIdToString(Header));
				return false;
			}
		}

		if (bHasPendingSize)
		{
			UE_LOG(LogVoxFactory, Warning, TEXT("The final SIZE chunk was not paired with an XYZI chunk."));
		}

		if (OutVoxelData->Models.IsEmpty())
		{
			UE_LOG(LogVoxFactory, Error, TEXT("The .vox file did not contain any SIZE/XYZI model data."));
			return false;
		}

		if (!bHasPackChunk)
		{
			OutVoxelData->DeclaredModelCount = OutVoxelData->Models.Num();
		}
		else if (OutVoxelData->DeclaredModelCount != OutVoxelData->Models.Num())
		{
			UE_LOG(
				LogVoxFactory,
				Warning,
				TEXT("PACK declares %d model(s), but %d SIZE/XYZI model pair(s) were parsed."),
				OutVoxelData->DeclaredModelCount,
				OutVoxelData->Models.Num());
		}

		return true;
	}
}

UVoxFactory::UVoxFactory()
{
	// Point the factory to process your custom formats
	bEditorImport = true;
	bText = false; // .vox files are binary formats
    
	// Bind the file extension to this factory 
	Formats.Add(TEXT("vox;MagicaVoxel File"));

	// Define the type of asset object this factory creates output for.
	SupportedClass = UMagicaVoxelData::StaticClass(); 
}

bool UVoxFactory::DoesSupportClass(UClass* InClass)
{
	return InClass && InClass->IsChildOf(SupportedClass);
}

UObject* UVoxFactory::FactoryCreateBinary(
	UClass* InClass, 
	UObject* InParent, 
	FName InName, 
	EObjectFlags Flags, 
	UObject* Context, 
	const TCHAR* Type, 
	const uint8*& Buffer, 
	const uint8* BufferEnd, 
	FFeedbackContext* Warn)
{
	FVoxBinaryReader Reader(Buffer, BufferEnd);

	uint8 Magic[4] = {};
	if (!Reader.ReadBytes(Magic, UE_ARRAY_COUNT(Magic)))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Vox file too small to contain a valid header."));
		return nullptr;
	}

	if (Magic[0] != 'V' || Magic[1] != 'O' || Magic[2] != 'X' || Magic[3] != ' ')
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Invalid .vox file header signature."));
		return nullptr;
	}

	int32 Version = 0;
	if (!Reader.ReadInt32(Version))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Vox file too small to contain a version number."));
		return nullptr;
	}

	FVoxChunkHeader MainChunkHeader;
	if (!Reader.ReadChunkHeader(MainChunkHeader))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Vox file is missing the MAIN chunk."));
		return nullptr;
	}

	if (!IsChunkId(MainChunkHeader, "MAIN"))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Expected MAIN chunk, found '%s'."), *ChunkIdToString(MainChunkHeader));
		return nullptr;
	}

	const int64 MainChunkPayloadSize = static_cast<int64>(MainChunkHeader.ContentByteSize) + MainChunkHeader.ChildrenChunksByteSize;
	if (!Reader.CanRead(MainChunkPayloadSize))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("MAIN chunk extends beyond the end of the file."));
		return nullptr;
	}

	if (!Reader.Skip(MainChunkHeader.ContentByteSize))
	{
		UE_LOG(LogVoxFactory, Error, TEXT("Failed to read MAIN chunk content."));
		return nullptr;
	}

	const uint8* MainChildrenStart = Reader.GetPosition();
	const uint8* MainChildrenEnd = MainChildrenStart + MainChunkHeader.ChildrenChunksByteSize;

	UClass* AssetClass = InClass && InClass->IsChildOf(UMagicaVoxelData::StaticClass())
		? InClass
		: UMagicaVoxelData::StaticClass();

	UMagicaVoxelData* NewVoxelData = NewObject<UMagicaVoxelData>(InParent, AssetClass, InName, Flags);
	NewVoxelData->Version = Version;

	if (!ParseMainChildren(MainChildrenStart, MainChildrenEnd, NewVoxelData))
	{
		return nullptr;
	}

	Buffer = BufferEnd;

	return NewVoxelData;
}
