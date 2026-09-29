// Copyright Zankyo Studio. All Rights Reserved.

#include "MagicaVoxelStaticMeshUtility.h"

#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

DECLARE_LOG_CATEGORY_CLASS(LogMagicaVoxelStaticMeshUtility, Log, All);

namespace
{
	struct FGreedyFace
	{
		int32 Axis = 0;
		int32 Direction = 1;
		int32 Plane = 0;
		int32 U = 0;
		int32 V = 0;
		int32 USize = 1;
		int32 VSize = 1;
		uint8 ColorIndex = 0;
	};

	FIntVector InferVoxelBounds(const TArray<FMagicaVoxelVoxel>& Voxels)
	{
		FIntVector Bounds = FIntVector::ZeroValue;

		for (const FMagicaVoxelVoxel& Voxel : Voxels)
		{
			Bounds.X = FMath::Max(Bounds.X, static_cast<int32>(Voxel.X) + 1);
			Bounds.Y = FMath::Max(Bounds.Y, static_cast<int32>(Voxel.Y) + 1);
			Bounds.Z = FMath::Max(Bounds.Z, static_cast<int32>(Voxel.Z) + 1);
		}

		return Bounds;
	}

	void AddVoxelMap(const TArray<FMagicaVoxelVoxel>& Voxels, TMap<FIntVector, uint8>& OutVoxelColors, FIntVector& InOutBounds)
	{
		OutVoxelColors.Empty(Voxels.Num());

		for (const FMagicaVoxelVoxel& Voxel : Voxels)
		{
			const FIntVector Position(Voxel.X, Voxel.Y, Voxel.Z);
			OutVoxelColors.Add(Position, Voxel.ColorIndex);

			InOutBounds.X = FMath::Max(InOutBounds.X, Position.X + 1);
			InOutBounds.Y = FMath::Max(InOutBounds.Y, Position.Y + 1);
			InOutBounds.Z = FMath::Max(InOutBounds.Z, Position.Z + 1);
		}
	}

	bool HasVoxelAt(const TMap<FIntVector, uint8>& VoxelColors, const FIntVector& Position)
	{
		return VoxelColors.Contains(Position);
	}

	int32 GetAxisValue(const FIntVector& Vector, const int32 Axis)
	{
		return Axis == 0 ? Vector.X : Axis == 1 ? Vector.Y : Vector.Z;
	}

	void SetAxisValue(FIntVector& Vector, const int32 Axis, const int32 Value)
	{
		if (Axis == 0)
		{
			Vector.X = Value;
		}
		else if (Axis == 1)
		{
			Vector.Y = Value;
		}
		else
		{
			Vector.Z = Value;
		}
	}

	void GetFaceAxes(const int32 Axis, int32& OutUAxis, int32& OutVAxis)
	{
		if (Axis == 0)
		{
			OutUAxis = 1;
			OutVAxis = 2;
		}
		else if (Axis == 1)
		{
			OutUAxis = 0;
			OutVAxis = 2;
		}
		else
		{
			OutUAxis = 0;
			OutVAxis = 1;
		}
	}

	void BuildFaceMask(
		const TMap<FIntVector, uint8>& VoxelColors,
		const FIntVector& Bounds,
		const int32 Axis,
		const int32 Direction,
		const int32 Plane,
		const int32 UAxis,
		const int32 VAxis,
		TArray<int32>& OutMask)
	{
		const int32 UCount = GetAxisValue(Bounds, UAxis);
		const int32 VCount = GetAxisValue(Bounds, VAxis);

		OutMask.Init(INDEX_NONE, UCount * VCount);

		for (int32 V = 0; V < VCount; ++V)
		{
			for (int32 U = 0; U < UCount; ++U)
			{
				FIntVector Position = FIntVector::ZeroValue;
				SetAxisValue(Position, UAxis, U);
				SetAxisValue(Position, VAxis, V);

				if (Direction > 0)
				{
					if (Plane <= 0)
					{
						continue;
					}

					SetAxisValue(Position, Axis, Plane - 1);
				}
				else
				{
					if (Plane >= GetAxisValue(Bounds, Axis))
					{
						continue;
					}

					SetAxisValue(Position, Axis, Plane);
				}

				const uint8* ColorIndex = VoxelColors.Find(Position);
				if (!ColorIndex)
				{
					continue;
				}

				FIntVector Neighbor = Position;
				SetAxisValue(Neighbor, Axis, GetAxisValue(Position, Axis) + Direction);

				if (!HasVoxelAt(VoxelColors, Neighbor))
				{
					OutMask[(V * UCount) + U] = *ColorIndex;
				}
			}
		}
	}

	void AppendGreedyFacesForMask(
		TArray<int32>& Mask,
		const int32 UCount,
		const int32 VCount,
		const int32 Axis,
		const int32 Direction,
		const int32 Plane,
		const bool bUseGreedyMeshing,
		TArray<FGreedyFace>& OutFaces)
	{
		for (int32 V = 0; V < VCount; ++V)
		{
			for (int32 U = 0; U < UCount;)
			{
				const int32 ColorIndex = Mask[(V * UCount) + U];
				if (ColorIndex == INDEX_NONE)
				{
					++U;
					continue;
				}

				int32 Width = 1;
				if (bUseGreedyMeshing)
				{
					while (U + Width < UCount && Mask[(V * UCount) + U + Width] == ColorIndex)
					{
						++Width;
					}
				}

				int32 Height = 1;
				if (bUseGreedyMeshing)
				{
					bool bCanGrow = true;
					while (V + Height < VCount && bCanGrow)
					{
						for (int32 TestU = 0; TestU < Width; ++TestU)
						{
							if (Mask[((V + Height) * UCount) + U + TestU] != ColorIndex)
							{
								bCanGrow = false;
								break;
							}
						}

						if (bCanGrow)
						{
							++Height;
						}
					}
				}

				for (int32 ClearV = 0; ClearV < Height; ++ClearV)
				{
					for (int32 ClearU = 0; ClearU < Width; ++ClearU)
					{
						Mask[((V + ClearV) * UCount) + U + ClearU] = INDEX_NONE;
					}
				}

				FGreedyFace Face;
				Face.Axis = Axis;
				Face.Direction = Direction;
				Face.Plane = Plane;
				Face.U = U;
				Face.V = V;
				Face.USize = Width;
				Face.VSize = Height;
				Face.ColorIndex = static_cast<uint8>(ColorIndex);
				OutFaces.Add(Face);

				U += Width;
			}
		}
	}

	TArray<FGreedyFace> BuildGreedyFaces(const TMap<FIntVector, uint8>& VoxelColors, const FIntVector& Bounds, const bool bUseGreedyMeshing)
	{
		TArray<FGreedyFace> Faces;
		TArray<int32> Mask;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			int32 UAxis = 0;
			int32 VAxis = 0;
			GetFaceAxes(Axis, UAxis, VAxis);

			const int32 PlaneCount = GetAxisValue(Bounds, Axis);
			const int32 UCount = GetAxisValue(Bounds, UAxis);
			const int32 VCount = GetAxisValue(Bounds, VAxis);

			for (int32 Direction : { -1, 1 })
			{
				for (int32 Plane = 0; Plane <= PlaneCount; ++Plane)
				{
					BuildFaceMask(VoxelColors, Bounds, Axis, Direction, Plane, UAxis, VAxis, Mask);
					AppendGreedyFacesForMask(Mask, UCount, VCount, Axis, Direction, Plane, bUseGreedyMeshing, Faces);
				}
			}
		}

		return Faces;
	}

	FVector3f MakePosition(const int32 Axis, const int32 UAxis, const int32 VAxis, const int32 Plane, const int32 U, const int32 V)
	{
		FIntVector Position = FIntVector::ZeroValue;
		SetAxisValue(Position, Axis, Plane);
		SetAxisValue(Position, UAxis, U);
		SetAxisValue(Position, VAxis, V);
		return FVector3f(static_cast<float>(Position.X), static_cast<float>(Position.Y), static_cast<float>(Position.Z));
	}

	void GetFaceVertices(const FGreedyFace& Face, FVector3f OutVertices[4], FVector3f& OutNormal)
	{
		int32 UAxis = 0;
		int32 VAxis = 0;
		GetFaceAxes(Face.Axis, UAxis, VAxis);

		const int32 U0 = Face.U;
		const int32 U1 = Face.U + Face.USize;
		const int32 V0 = Face.V;
		const int32 V1 = Face.V + Face.VSize;

		const FVector3f P00 = MakePosition(Face.Axis, UAxis, VAxis, Face.Plane, U0, V0);
		const FVector3f P10 = MakePosition(Face.Axis, UAxis, VAxis, Face.Plane, U1, V0);
		const FVector3f P11 = MakePosition(Face.Axis, UAxis, VAxis, Face.Plane, U1, V1);
		const FVector3f P01 = MakePosition(Face.Axis, UAxis, VAxis, Face.Plane, U0, V1);

		if (Face.Axis == 0)
		{
			OutNormal = FVector3f(static_cast<float>(Face.Direction), 0.0f, 0.0f);
			OutVertices[0] = P00;
			OutVertices[1] = Face.Direction < 0 ? P10 : P01;
			OutVertices[2] = P11;
			OutVertices[3] = Face.Direction < 0 ? P01 : P10;
		}
		else if (Face.Axis == 1)
		{
			OutNormal = FVector3f(0.0f, static_cast<float>(Face.Direction), 0.0f);
			OutVertices[0] = P00;
			OutVertices[1] = Face.Direction < 0 ? P01 : P10;
			OutVertices[2] = P11;
			OutVertices[3] = Face.Direction < 0 ? P10 : P01;
		}
		else
		{
			OutNormal = FVector3f(0.0f, 0.0f, static_cast<float>(Face.Direction));
			OutVertices[0] = P00;
			OutVertices[1] = Face.Direction < 0 ? P10 : P01;
			OutVertices[2] = P11;
			OutVertices[3] = Face.Direction < 0 ? P01 : P10;
		}
	}

	FVector2f GetFaceUv(const int32 VertexIndex, const FGreedyFace& Face)
	{
		if (VertexIndex == 0)
		{
			return FVector2f(0.0f, 0.0f);
		}

		if (VertexIndex == 1)
		{
			return FVector2f(static_cast<float>(Face.USize), 0.0f);
		}

		if (VertexIndex == 2)
		{
			return FVector2f(static_cast<float>(Face.USize), static_cast<float>(Face.VSize));
		}

		return FVector2f(0.0f, static_cast<float>(Face.VSize));
	}

	void AppendFaceToMeshDescription(
		const FGreedyFace& Face,
		const FPolygonGroupID PolygonGroupID,
		FMeshDescription& MeshDescription,
		TVertexAttributesRef<FVector3f>& VertexPositions,
		TVertexInstanceAttributesRef<FVector3f>& VertexInstanceNormals,
		TVertexInstanceAttributesRef<FVector2f>& VertexInstanceUVs)
	{
		FVector3f Positions[4];
		FVector3f Normal;
		GetFaceVertices(Face, Positions, Normal);

		TArray<FVertexInstanceID> VertexInstanceIDs;
		VertexInstanceIDs.Reserve(4);

		for (int32 VertexIndex = 0; VertexIndex < 4; ++VertexIndex)
		{
			const FVertexID VertexID = MeshDescription.CreateVertex();
			VertexPositions[VertexID] = Positions[VertexIndex];

			const FVertexInstanceID VertexInstanceID = MeshDescription.CreateVertexInstance(VertexID);
			VertexInstanceNormals[VertexInstanceID] = Normal;
			VertexInstanceUVs.Set(VertexInstanceID, 0, GetFaceUv(VertexIndex, Face));
			VertexInstanceIDs.Add(VertexInstanceID);
		}

		MeshDescription.CreatePolygon(PolygonGroupID, VertexInstanceIDs);
	}

	bool PopulateStaticMeshFromVoxelArray(UStaticMesh* StaticMesh, const TArray<FMagicaVoxelVoxel>& Voxels, FIntVector Bounds, const bool bUseGreedyMeshing)
	{
		if (!StaticMesh)
		{
			UE_LOG(LogMagicaVoxelStaticMeshUtility, Error, TEXT("Cannot populate a null UStaticMesh."));
			return false;
		}

		if (Voxels.IsEmpty())
		{
			UE_LOG(LogMagicaVoxelStaticMeshUtility, Warning, TEXT("Cannot build a static mesh from an empty voxel array."));
			return false;
		}

		TMap<FIntVector, uint8> VoxelColors;
		AddVoxelMap(Voxels, VoxelColors, Bounds);

		if (Bounds.X <= 0 || Bounds.Y <= 0 || Bounds.Z <= 0)
		{
			UE_LOG(LogMagicaVoxelStaticMeshUtility, Error, TEXT("Cannot build a static mesh with invalid voxel bounds: %s."), *Bounds.ToString());
			return false;
		}
		
		TSet<uint8> UniqueColorIndices;
		for (const auto& CoordsAndColorIndex : VoxelColors)
		{
			UniqueColorIndices.Add(CoordsAndColorIndex.Value);
		}

		/*
		TArray<uint8> ColorIndices;
		VoxelColors.GenerateValueArray(ColorIndices);
		ColorIndices.Sort();

		TArray<uint8> UniqueColorIndices;
		UniqueColorIndices.Reserve(ColorIndices.Num());
		for (const uint8 ColorIndex : ColorIndices)
		{
			if (UniqueColorIndices.IsEmpty() || UniqueColorIndices.Last() != ColorIndex)
			{
				UniqueColorIndices.Add(ColorIndex);
			}
		}*/

		FMeshDescription MeshDescription;
		FStaticMeshAttributes Attributes(MeshDescription);
		Attributes.Register();
		Attributes.GetVertexInstanceUVs().SetNumChannels(1);

		TPolygonGroupAttributesRef<FName> PolygonGroupMaterialSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

		TMap<uint8, FPolygonGroupID> PolygonGroupsByColorIndex;
		TArray<FStaticMaterial> StaticMaterials;
		StaticMaterials.Reserve(UniqueColorIndices.Num());

		for (const uint8 ColorIndex : UniqueColorIndices)
		{
			const FName SlotName(*FString::Printf(TEXT("ColorIndex_%d"), ColorIndex));
			const FPolygonGroupID PolygonGroupID = MeshDescription.CreatePolygonGroup();
			PolygonGroupMaterialSlotNames[PolygonGroupID] = SlotName;

			PolygonGroupsByColorIndex.Add(ColorIndex, PolygonGroupID);
			StaticMaterials.Add(FStaticMaterial(nullptr, SlotName, SlotName));
		}

		const TArray<FGreedyFace> Faces = BuildGreedyFaces(VoxelColors, Bounds, bUseGreedyMeshing);

		for (const FGreedyFace& Face : Faces)
		{
			const FPolygonGroupID* PolygonGroupID = PolygonGroupsByColorIndex.Find(Face.ColorIndex);
			if (!PolygonGroupID)
			{
				continue;
			}

			AppendFaceToMeshDescription(Face, *PolygonGroupID, MeshDescription, VertexPositions, VertexInstanceNormals, VertexInstanceUVs);
		}

		StaticMesh->Modify();
		StaticMesh->GetStaticMaterials() = MoveTemp(StaticMaterials);

		TArray<const FMeshDescription*> MeshDescriptions;
		MeshDescriptions.Add(&MeshDescription);
		StaticMesh->BuildFromMeshDescriptions(MeshDescriptions);

#if WITH_EDITOR
		StaticMesh->PostEditChange();
#endif

		if (!StaticMesh->HasAnyFlags(RF_Transient))
		{
			StaticMesh->MarkPackageDirty();
		}
		return true;
	}
}

bool UMagicaVoxelStaticMeshUtility::PopulateStaticMeshFromModel(UStaticMesh* StaticMesh, const FMagicaVoxelModel& Model, const bool bUseGreedyMeshing)
{
	return PopulateStaticMeshFromVoxelArray(StaticMesh, Model.Voxels, Model.Size, bUseGreedyMeshing);
}

bool UMagicaVoxelStaticMeshUtility::PopulateStaticMeshFromVoxels(UStaticMesh* StaticMesh, const TArray<FMagicaVoxelVoxel>& Voxels, const bool bUseGreedyMeshing)
{
	return PopulateStaticMeshFromVoxelArray(StaticMesh, Voxels, InferVoxelBounds(Voxels), bUseGreedyMeshing);
}
