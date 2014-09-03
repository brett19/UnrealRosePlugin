#pragma once

#include "Common.h"

class Him {
public:
    Him(const TCHAR *Filename) {
        FFileHelper::LoadFileToArray(rh.data, Filename);

        auto width = rh.read<uint32>();
        auto height = rh.read<uint32>();
        auto patchGridCount = rh.read<uint32>();
        auto patchSize = rh.read<float>();
        
        check(width == 65 && height == 65);

        heights.AddZeroed(width * height);
        for (uint32 y = 0; y < height; ++y) {
            for (uint32 x = 0; x < width; ++x) {
                heights[y*width+x] = rh.read<float>();
            }
        }
    }

    TArray<float> heights;

private:
    ReadHelper rh;
};