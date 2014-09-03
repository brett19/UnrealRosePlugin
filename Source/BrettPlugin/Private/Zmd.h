#pragma once

#include "Common.h"

class Zmd {
public:
    struct Bone {
        uint32 parent;
        char name[256];
        FVector translation;
        FQuat rotation;
    };

    Zmd(const TCHAR *Filename) {
        FFileHelper::LoadFileToArray(rh.data, Filename);

        auto header = rh.read<char[7]>();
        uint32 version = 0;
        if (strncmp(header, "ZMD0002", 7) == 0) {
            version = 2;
        } else if (strncmp(header, "ZMD0003", 7) == 0) {
            version = 3;
        } else {
            DebugBreak();
        }

        auto boneCount = rh.read<uint32>();
        for (uint32 i = 0; i < boneCount; ++i) {
            Bone b;
            b.parent = rh.read<uint32>();
            strcpy_s(b.name, 256, rh.readStr());
            b.translation = rtuPosition(rh.read<FVector>());
            b.rotation = rtuRotation(rh.readBadQuat());
            bones.Add(b);
        }

        auto dummyCount = rh.read<uint32>();
        for (uint32 i = 0; i < dummyCount; ++i) {
            Bone b;
            b.parent = rh.read<uint32>();
            strcpy_s(b.name, 256, rh.readStr());
            b.translation = rtuPosition(rh.read<FVector>());
            b.rotation = (version == 3) ? rtuRotation(rh.readBadQuat()) : FQuat::Identity;
            dummies.Add(b);
        }
    }

    TArray<Bone> bones;
    TArray<Bone> dummies;

private:
    ReadHelper rh;
};