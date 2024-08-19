#pragma once

#include <memory>
#include <assert.h>
#include <math.h>

class Waveform {
    float*  data=nullptr;
    int64_t length=0;
    int32_t samplerate=0;

public:
    Waveform() {}
    Waveform(long length, int samplerate);
    ~Waveform();

    const float* operator+(long offset) const
    {
        assert(0<=offset && offset<length);
        return data+offset;
    }

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

    static std::shared_ptr<Waveform> load(const char* filename);

    template<typename Archive>
    void load(Archive& ar, uint32_t);

    template<typename Archive>
    void save(Archive& ar, uint32_t) const;
};

