#pragma once

#include "Common.h"

class Zsc {
public:
	struct PropertyType {
		enum Type {
			Position = 1,
			Rotation = 2,
			Scale = 3,
			AxisRotation = 4,
			BoneIndex = 5,
			DummyIndex = 6,
			Parent = 7,
			Animation = 8,
			Collision = 29,
			ConstantAnimation = 30,
			VisibleRangeSet = 31,
			UseLightmap = 32
		};
	};

	struct Texture {
		FString filePath;
		bool useSkinShader;
		bool alphaEnabled;
		bool twoSided;
		bool alphaTestEnabled;
		uint16 alphaReference;
		bool depthTestEnabled;
		bool depthWriteEnabled;
		uint16 blendType;
		bool useSpecularShader;
		float alpha;
		uint16 glowType;
		FLinearColor glowColor;
	};

	struct Part {
		uint16 meshIdx;
		uint16 texIdx;

		FVector position;
		FQuat rotation;
		FVector scale;
		FQuat axisRotation;
		uint16 parentIdx;
		uint16 collisionType;
		FString animPath;
		uint16 visibleRangeSet;
		bool useLightmap;
		uint16 boneIdx;
		uint16 dummyIdx;
	};

	struct Effect {
		uint16 effectType;
		uint16 effectIdx;

		FVector position;
		FQuat rotation;
		FVector scale;
		uint16 parentIdx;
	};

	struct Model {
		TArray<Part> parts;
		TArray<Effect> effects;
	};

	Zsc(const TCHAR *Filename) {
		FFileHelper::LoadFileToArray(rh.data, Filename);

		auto meshCount = rh.read<uint16>();
		for (uint16 i = 0; i < meshCount; ++i) {
			meshes.Add(rh.readStr());
		}

		auto textureCount = rh.read<uint16>();
		for (uint16 i = 0; i < textureCount; ++i) {
			Texture t;
			t.filePath = rh.readStr();
			t.useSkinShader = rh.read<uint16>() != 0;
			t.alphaEnabled = rh.read<uint16>() != 0;
			t.twoSided = rh.read<uint16>() != 0;
			t.alphaTestEnabled = rh.read<uint16>() != 0;
			t.alphaReference = rh.read<uint16>();
			t.depthTestEnabled = rh.read<uint16>() != 0;
			t.depthWriteEnabled = rh.read<uint16>() != 0;
			t.blendType = rh.read<uint16>() != 0;
			t.useSpecularShader = rh.read<uint16>() != 0;
			t.alpha = rh.read<float>();
			t.glowType = rh.read<uint16>();
			t.glowColor = rh.readColor3();
			textures.Add(t);
		}

		auto effectCount = rh.read<uint16>();
		for (uint16 i = 0; i < effectCount; ++i) {
			effects.Add(rh.readStr());
		}

		char AnimPathBuffer[512];
		auto modelCount = rh.read<uint16>();
		for (uint16 i = 0; i < modelCount; ++i) {
			rh.skip(sizeof(int32) * 3);

			Model m;

			auto partCount = rh.read<uint16>();
			if (partCount > 0) {
				for (uint16 j = 0; j < partCount; ++j) {
					Part p;
					p.meshIdx = rh.read<uint16>();
					p.texIdx = rh.read<uint16>();

					p.position = FVector::ZeroVector;
					p.rotation = FQuat::Identity;
					p.scale = FVector(1, 1, 1);
					p.axisRotation = FQuat::Identity;
					p.parentIdx = 0xFF;
					p.collisionType = 0;
					p.boneIdx = 0xFFFF;
					p.dummyIdx = 0xFFFF;

					uint8 propType = 0;
					while ((propType = rh.read<uint8>()) != 0) {
						auto propSize = rh.read<uint8>();

						if (propType == PropertyType::Position) {
							p.position = rtuPosition(rh.read<FVector>());
						} else if (propType == PropertyType::Rotation) {
							p.rotation = rtuRotation(rh.readBadQuat());
						} else if (propType == PropertyType::Scale) {
							p.scale = rtuScale(rh.read<FVector>());
						} else if (propType == PropertyType::AxisRotation) {
							p.axisRotation = rtuRotation(rh.readBadQuat());
						} else if (propType == PropertyType::Parent) {
							p.parentIdx = rh.read<uint16>();
						} else if (propType == PropertyType::Collision) {
							p.collisionType = rh.read<uint16>();
						} else if (propType == PropertyType::ConstantAnimation) {
							memcpy(AnimPathBuffer, rh.read(propSize), propSize);
							AnimPathBuffer[propSize] = 0;
							p.animPath = AnimPathBuffer;
						} else if (propType == PropertyType::BoneIndex) {
							p.boneIdx = rh.read<uint16>();
						} else if (propType == PropertyType::DummyIndex) {
							p.dummyIdx = rh.read<uint16>();
						} else {
							rh.skip(propSize);
						}
					}

					m.parts.Add(p);
				}

				auto effectCount = rh.read<uint16>();
				for (uint16 j = 0; j < effectCount; ++j) {
					Effect e;
					e.effectType = rh.read<uint16>();
					e.effectIdx = rh.read<uint16>();

					e.position = FVector::ZeroVector;
					e.rotation = FQuat::Identity;
					e.scale = FVector(1, 1, 1);
					e.parentIdx = 0;

					uint8 propType = 0;
					while ((propType = rh.read<uint8>()) != 0) {
						auto propSize = rh.read<uint8>();

						if (propType == PropertyType::Position) {
							e.position = rtuPosition(rh.read<FVector>());
						} else if (propType == PropertyType::Rotation) {
							e.rotation = rtuRotation(rh.readBadQuat());
						} else if (propType == PropertyType::Scale) {
							e.scale = rtuScale(rh.read<FVector>());
						} else if (propType == PropertyType::Parent) {
							e.parentIdx = rh.read<uint16>();
						} else {
							rh.skip(propSize);
						}
					}

					m.effects.Add(e);
				}

				rh.skip(sizeof(float) * 3 * 2);
			}

			models.Add(m);
		}
	}

	TArray<FString> meshes;
	TArray<Texture> textures;
	TArray<FString> effects;
	TArray<Model> models;

private:
	ReadHelper rh;
};