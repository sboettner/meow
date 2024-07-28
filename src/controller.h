#pragma once

#include "track.h"
#include "audio.h"

class Controller {
    Track&  track;

    std::unique_ptr<IAudioDevice>   audiodev;

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
};
