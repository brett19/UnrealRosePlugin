#pragma once

#include "Common.h"

class Chr {
public:
    struct Animation {
        uint16 type;
        uint16 animationIdx;
    };

    struct Effect {
        uint16 boneIdx;
        uint16 effectIdx;
    };

    struct Character {
        bool enabled;

        uint16 skeletonIdx;
        FString name;
        
        TArray<uint16> models;
        TArray<Animation> animations;
        TArray<Effect> effects;
    };

    Chr(const TCHAR *Filename) {
        FFileHelper::LoadFileToArray(rh.data, Filename);

        auto skeletonCount = rh.read<uint16>();
        for (uint16 i = 0; i < skeletonCount; ++i) {
            skeletons.Add(rh.readStr());
        }

        auto animationCount = rh.read<uint16>();
        for (uint16 i = 0; i < animationCount; ++i) {
            animations.Add(rh.readStr());
        }

        auto effectCount = rh.read<uint16>();
        for (uint16 i = 0; i < effectCount; ++i) {
            effects.Add(rh.readStr());
        }

        auto characterCount = rh.read<uint16>();
        for (uint16 i = 0; i < characterCount; ++i) {
            Character c;
            c.enabled = rh.read<uint8>() != 0;

            if (c.enabled) {
                c.skeletonIdx = rh.read<uint16>();
                c.name = rh.readStr();

                auto modelCount = rh.read<uint16>();
                for (uint16 j = 0; j < modelCount; ++j) {
                    c.models.Add(rh.read<uint16>());
                }

                auto animationCount = rh.read<uint16>();
                for (uint16 j = 0; j < animationCount; ++j) {
                    Animation a;
                    a.type = rh.read<uint16>();
                    a.animationIdx = rh.read<uint16>();
                    c.animations.Add(a);
                }

                auto effectCount = rh.read<uint16>();
                for (uint16 j = 0; j < effectCount; ++j) {
                    Effect e;
                    e.boneIdx = rh.read<uint16>();
                    e.effectIdx = rh.read<uint16>();
                    c.effects.Add(e);
                }
            }

            characters.Add(c);
        }
    }

    TArray<FString> skeletons;
    TArray<FString> animations;
    TArray<FString> effects;
    TArray<Character> characters;

private:
    ReadHelper rh;
};