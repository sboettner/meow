#pragma once

#include <vector>
#include "waveform.h"

class Track {
public:
    // analysis frame marker
    struct Frame {
        double  position;
        float   period;     // zero if unvoiced
        float   pitch;
    };

    // synthesis chunk
    struct Chunk {
        Chunk*  prev;
        Chunk*  next;

        int     beginframe;
        int     endframe;

        int     begin;
        int     end;
        
        bool    voiced;
        float   avgpitch;
        float   newpitch;
    };


    Track(Waveform*);
    ~Track();

    void compute_frame_decomposition(int blocksize, int overlap);
    void detect_chunks();

    int get_samplerate() const
    {
        return wave->get_samplerate();
    }

    const Waveform& get_waveform() const
    {
        return *wave;
    }

    const Frame& get_frame(int i) const
    {
        return frames[i];
    }

    int get_frame_count() const
    {
        return frames.size();
    }

    Chunk*  get_first_chunk()
    {
        return firstchunk;
    }

private:
    Waveform* const     wave;

    std::vector<Frame>  frames;

    Chunk*              firstchunk=nullptr;
    Chunk*              lastchunk =nullptr;
};
