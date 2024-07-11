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

        int     begin;
        int     end;
        
        bool    voiced;
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

    Chunk*  get_first_chunk()
    {
        return firstchunk;
    }

    std::vector<Frame>::const_iterator find_nearest_frame(double position);

private:
    Waveform* const     wave;

    std::vector<Frame>  frames;

    Chunk*              firstchunk=nullptr;
    Chunk*              lastchunk =nullptr;
};
