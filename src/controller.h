#pragma once

#include "track.h"
#include "audio.h"

class Controller {
public:
    Controller(Track&);
    ~Controller();

    Track& get_track()
    {
        return track;
    }

    IAudioDevice& get_audio_device()
    {
        return *audiodev;
    }


    class IChunkModifier {
    public:
        virtual ~IChunkModifier();

        virtual void finish() = 0;
        virtual void move_to(double t, double y) = 0;
    };

    std::unique_ptr<IChunkModifier> begin_modify_chunk(Track::Chunk*, double t, double y);

private:
    class ChunkModifier;

    Track&  track;

    std::unique_ptr<IAudioDevice>   audiodev;
};
