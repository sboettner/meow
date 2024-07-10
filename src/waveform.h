#pragma once

#include <assert.h>

class Waveform {
    float*  data;
    long    length;
    int     samplerate;

public:
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

    long get_length() const
    {
        return length;
    }

    int get_samplerate() const
    {
        return samplerate;
    }

    static Waveform* load(const char* filename);
};

