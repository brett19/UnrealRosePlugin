#pragma once

#include "Common.h"

class Zmo {
public:
    struct ChannelType
    {
        enum Type {
            None = 1 << 0,
            Position = 1 << 1,
            Rotation = 1 << 2,
            Normal = 1 << 3,
            Alpha = 1 << 4,
            UV1 = 1 << 5,
            UV2 = 1 << 6,
            UV3 = 1 << 7,
            UV4 = 1 << 8,
            TexAnim = 1 << 9,
            Scale = 1 << 10
        };
    };

    struct Channel {
        uint32 index;
        virtual ChannelType::Type type() = 0;
    };

    struct PositionChannel : public Channel {
        ChannelType::Type type() override { return ChannelType::Position; }
        TArray<FVector> frames;
    };
    struct RotationChannel : public Channel {
        ChannelType::Type type() override { return ChannelType::Rotation; }
        TArray<FQuat> frames;
    };
    struct ScaleChannel : public Channel {
        ChannelType::Type type() override { return ChannelType::Scale; }
        TArray<FVector> frames;
    };

    Zmo(const TCHAR *Filename) {
        FFileHelper::LoadFileToArray(rh.data, Filename);

        auto header = rh.readStr();

        framesPerSecond = rh.read<uint32>();
        frameCount = rh.read<uint32>();
        auto channelCount = rh.read<uint32>();

        for (uint32 i = 0; i < channelCount; ++i) {
            Channel * channel = nullptr;

            auto type = rh.read<uint32>();
            if (type == ChannelType::Position) {
                channel = new PositionChannel();
            } else if (type == ChannelType::Rotation) {
                channel = new RotationChannel();
            } else if (type == ChannelType::Scale) {
                channel = new ScaleChannel();
            } else {
                DebugBreak();
            }

            channel->index = rh.read<uint32>();
            channels.Add(channel);
        }

        for (uint32 j = 0; j < frameCount; ++j) {
            for (uint32 i = 0; i < channelCount; ++i) {
                Channel *channel = channels[i];
                if (channel->type() == ChannelType::Position) {
                    auto& frames = ((PositionChannel*)channel)->frames;
                    frames.Add(rtuPosition(rh.read<FVector>()));
                } else if (channel->type() == ChannelType::Rotation) {
                    auto& frames = ((RotationChannel*)channel)->frames;
                    //frames.Add(rtuRotation(rh.read<FQuat>()));
					frames.Add(rtuRotation(rh.readBadQuat()));
                } else if (channel->type() == ChannelType::Scale) {
                    auto& frames = ((ScaleChannel*)channel)->frames;
                    frames.Add(rtuScale(rh.read<FVector>()));
                } else {
                    DebugBreak();
                }
            }
        }
    }

    uint32 framesPerSecond;
    uint32 frameCount;
    TArray<Channel*> channels;

private:
    ReadHelper rh;
};