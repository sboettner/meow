#pragma once

#include <stack>
#include "project.h"
#include "audio.h"


class Controller {
public:
    Controller(Project&);
    ~Controller();

    Track& get_track()
    {
        return *project.tracks[0];
    }

    IAudioDevice& get_audio_device()
    {
        return *audiodev;
    }


    void begin_move_chunk(Track::Chunk*, double t, float y);
    void do_move_chunk(Track::Chunk*, double t, float y, bool move_pitch_contour, bool move_time);
    void finish_move_chunk(Track::Chunk*, double t, float y);

    void begin_move_edge(Track::Chunk*, double t);
    void do_move_edge(Track::Chunk*, double t);
    void finish_move_edge(Track::Chunk*, double t);

    bool split_chunk(Track::Chunk*, double t);
    bool merge_chunks(Track::Chunk*);

    void begin_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y);
    void do_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y);
    void finish_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y);

    bool insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y);
    bool delete_pitch_contour_control_point(Track::PitchContourIterator cp);

    bool set_elastic(Track::Chunk*, bool);

    void undo();

private:
    Track::Chunk* backup(Track::Chunk* first, Track::Chunk* last, Track::Chunk* mid=nullptr);

    Project&                        project;

    std::unique_ptr<IAudioDevice>   audiodev;
    std::shared_ptr<IAudioProvider> audioprovider;

    Track::Chunk*                   curchunk=nullptr;
    Track::Chunk*                   curchunkbackup=nullptr;

    // state while moving chunk
    double                          moving_time_offset=0.0;

    struct BackupState {
        Track::Chunk*   first;
        Track::Chunk*   last;
    };

    std::stack<BackupState, std::vector<BackupState>>   undo_stack;
};
