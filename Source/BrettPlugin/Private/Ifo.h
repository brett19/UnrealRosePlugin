#pragma once

#include "Common.h"

class Ifo {
public:
	struct EBlockType {
		enum Type {
			MapInformation = 0,
			Object = 1,
			NPC = 2,
			Building = 3,
			Sound = 4,
			Effect = 5,
			Animation = 6,
			WaterPatch = 7,
			MonsterSpawn = 8,
			WaterPlane = 9,
			WarpPoint = 10,
			CollisionObject = 11,
			EventObject = 12
		};
	};

	struct FMapBlock {
		FString Name;
		uint16 WarpId;
		uint16 EventId;
		uint32 ObjectType;
		uint32 ObjectID;
		FQuat Rotation;
		FVector Position;
		FVector Scale;
	};

	struct FBuildingBlock : public FMapBlock
	{
	};

	struct FObjectBlock : public FMapBlock
	{
	};

	struct FCollisionBlock : public FMapBlock
	{
	};

	Ifo(const TCHAR *Filename) {
		FFileHelper::LoadFileToArray(rh.data, Filename);

		auto blockCount = rh.read<uint32>();
		for (uint32 i = 0; i < blockCount; ++i) {
			auto blockType = rh.read<uint32>();
			auto blockOffset = rh.read<uint32>();

			int32 nextBlock = rh.tell();
			rh.seek(blockOffset);

			if (blockType == EBlockType::Object) {
				uint32 objCount = rh.read<uint32>();
				for (uint32 j = 0; j < objCount; ++j) {
					Objects.Add(ReadBaseObject<FObjectBlock>());
				}
			} else if (blockType == EBlockType::Building) {
				uint32 objCount = rh.read<uint32>();
				for (uint32 j = 0; j < objCount; ++j) {
					Buildings.Add(ReadBaseObject<FBuildingBlock>());
				}
			} else if (blockType == EBlockType::CollisionObject) {
				uint32 objCount = rh.read<uint32>();
				for (uint32 j = 0; j < objCount; ++j) {
					Collisions.Add(ReadBaseObject<FCollisionBlock>());
				}
			}

			rh.seek(nextBlock);
		}
	}

	template<typename DerivedBlockType>
	DerivedBlockType ReadBaseObject() {
		DerivedBlockType data;
		FMapBlock* obj = &data;
		obj->Name = rh.readByteStr();
		obj->WarpId = rh.read<uint16>();
		obj->EventId = rh.read<uint16>();
		obj->ObjectType = rh.read<uint32>();
		obj->ObjectID = rh.read<uint32>();
		rh.skip(sizeof(uint32) * 2); /* MapPosition */
		obj->Rotation = rtuRotation(rh.read<FQuat>());
		obj->Position = rtuPosition(rh.read<FVector>());
		obj->Scale = rtuScale(rh.read<FVector>());
		return data;
	}

	TArray<FBuildingBlock> Buildings;
	TArray<FObjectBlock> Objects;
	TArray<FCollisionBlock> Collisions;

private:
	ReadHelper rh;
};