#pragma once

#include "Common.h"

class Til {
public:
#pragma pack(push, 1)
	struct FTile {
		uint8 Brush;
		uint8 TileIndex;
		uint8 TileSet;
		uint32 Tile;
	};
#pragma pack(pop)

	Til(const TCHAR *Filename) {
		FFileHelper::LoadFileToArray(rh.data, Filename);

		Width = rh.read<uint32>();
		Height = rh.read<uint32>();
		Data.AddZeroed(Width * Height);
		for (uint32 i = 0; i < Width * Height; ++i) {
			Data[i] = rh.read<FTile>();
		}
	}

	uint32 Width;
	uint32 Height;
	TArray<FTile> Data;

private:
	ReadHelper rh;
};