#pragma once

#include "Common.h"

class Zms {
public:
	enum ZmsFormat {
		ZMSF_NONE = 1 << 0,
		ZMSF_POSITION = 1 << 1,
		ZMSF_NORMAL = 1 << 2,
		ZMSF_COLOR = 1 << 3,
		ZMSF_BLENDWEIGHT = 1 << 4,
		ZMSF_BLENDINDEX = 1 << 5,
		ZMSF_TANGENT = 1 << 6,
		ZMSF_UV1 = 1 << 7,
		ZMSF_UV2 = 1 << 8,
		ZMSF_UV3 = 1 << 9,
		ZMSF_UV4 = 1 << 10
	};

	struct BoneWeights {
		float weight[4];
		uint16 boneIdx[4];
	};

	Zms(const TCHAR *Filename) {
		FFileHelper::LoadFileToArray(rh.data, Filename);

		auto header = rh.read<char[8]>();
		auto format = rh.read<uint32>();
		rh.skip(sizeof(FVector) * 2);

		TArray<uint16> boneLookup;
		auto boneCount = rh.read<uint16>();
		boneLookup.AddZeroed(boneCount);
		for (uint16 i = 0; i < boneCount; ++i) {
			boneLookup[i] = rh.read<uint16>();
		}

		auto vertexCount = rh.read<uint16>();
		vertexPositions.AddZeroed(vertexCount);
		for (uint16 i = 0; i < vertexCount; ++i) {
			vertexPositions[i] = rtuPosition(rh.read<FVector>()) * 100;
		}

		if (format & ZMSF_NORMAL) {
			vertexNormals.AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexNormals[i] = rh.read<FVector>();
			}
		}
		if (format & ZMSF_COLOR) {
			vertexColors.AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				float alpha = rh.read<float>();
				vertexColors[i] = FLinearColor(rh.read<float>(), rh.read<float>(), rh.read<float>(), alpha);
			}
		}
		if (format & ZMSF_BLENDINDEX && format & ZMSF_BLENDWEIGHT) {
			boneWeights.AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				boneWeights[i] = rh.read<BoneWeights>();
				boneWeights[i].boneIdx[0] = boneLookup[boneWeights[i].boneIdx[0]];
				boneWeights[i].boneIdx[1] = boneLookup[boneWeights[i].boneIdx[1]];
				boneWeights[i].boneIdx[2] = boneLookup[boneWeights[i].boneIdx[2]];
				boneWeights[i].boneIdx[3] = boneLookup[boneWeights[i].boneIdx[3]];
			}
		}
		if (format & ZMSF_TANGENT) {
			vertexTangents.AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexTangents[i] = rtuPosition(rh.read<FVector>());
			}
		}
		if (format & ZMSF_UV1) {
			vertexUvs[0].AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexUvs[0][i] = rh.read<FVector2D>();
			}
		}
		if (format & ZMSF_UV2) {
			vertexUvs[1].AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexUvs[1][i] = rh.read<FVector2D>();
			}
		}
		if (format & ZMSF_UV3) {
			vertexUvs[2].AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexUvs[2][i] = rh.read<FVector2D>();
			}
		}
		if (format & ZMSF_UV4) {
			vertexUvs[3].AddZeroed(vertexCount);
			for (uint16 i = 0; i < vertexCount; ++i) {
				vertexUvs[3][i] = rh.read<FVector2D>();
			}
		}

		auto faceCount = rh.read<uint16>();
		int indexCount = faceCount * 3;
		indexes.AddZeroed(indexCount);
		for (uint16 i = 0; i < indexCount; ++i) {
			indexes[i] = rh.read<uint16>();
		}
	}

	TArray<FVector> vertexPositions;
	TArray<FLinearColor> vertexColors;
	TArray<FVector> vertexNormals;
	TArray<FVector> vertexTangents;
	TArray<FVector2D> vertexUvs[4];
	TArray<uint32> indexes;
	TArray<BoneWeights> boneWeights;

private:
	ReadHelper rh;
};
