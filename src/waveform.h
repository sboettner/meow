#pragma once

#include <memory>
#include <assert.h>
#include <math.h>
#include <cstdint>
#include <vector>

class IProgressMonitor;


class Waveform {
public:
    // analysis frame marker
    struct Frame {
        double  position;
        float   period;     // zero if unvoiced
        float   pitch;

        template<typename Archive>
        void serialize(Archive& ar, uint32_t ver);
    };

    Waveform() {}
    Waveform(long length, int samplerate);
    ~Waveform();

    float operator[](long offset) const
    {
        assert(0<=offset && offset<length);
        return data[offset];
    }

    float operator()(double offset) const
    {
        long ptr=(long) floor(offset);
        float t=float(offset-ptr);

        // TODO: cubic interpolation
        return data[ptr]*(1.0f-t) + data[ptr+1]*t;
    }

    long get_length() const
    {
        return length;
    }

    int get_samplerate() const
    {
        return samplerate;
    }

    const Frame& get_frame(int i) const
    {
        return frames[i];
    }

    int get_frame_count() const
    {
        return frames.size();
    }

    void compute_frame_decomposition(int blocksize, int overlap, IProgressMonitor& monitor);

    static std::shared_ptr<Waveform> load(const char* filename);

    template<typename Archive>
    void load(Archive& ar, uint32_t);

    template<typename Archive>
    void save(Archive& ar, uint32_t) const;

private:
    float*  data=nullptr;
    int64_t length=0;
    int32_t samplerate=0;

    std::vector<Frame>  frames;
};

