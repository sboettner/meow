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


    void begin_move_chunk(Track::Chunk*, double t, float y);
    void do_move_chunk(Track::Chunk*, double t, float y);
    void finish_move_chunk(Track::Chunk*, double t, float y);

    bool insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y);
    bool delete_pitch_contour_control_point(Track::PitchContourIterator cp);

private:
    class ChunkModifier;

    Track&  track;

    std::unique_ptr<IAudioDevice>   audiodev;
    std::shared_ptr<IAudioProvider> audioprovider;

    // state while moving chunk
    bool                            moving=false;
    double                          moving_pitch_offset=0.0;
};
