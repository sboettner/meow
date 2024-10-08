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
        float   pitch;
        float   energy=0.0f;    // reserved for future use

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
        if (ptr<0)
            return ptr<-1 ? 0.0f : data[0]*t;
        else if (ptr+1>=length)
            return ptr>=length ? 0.0f : data[ptr]*(1.0f-t);
        else
            return data[ptr]*(1.0f-t) + data[ptr+1]*t;
    }

    int64_t get_length() const
    {
        return length;
    }

    int32_t get_samplerate() const
    {
        return samplerate;
    }

    const Frame& get_frame(int i) const
    {
        assert(0<=i && i<frames.size());
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

