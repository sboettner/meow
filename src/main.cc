#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <sndfile.h>
#include "correlation.h"

template<typename T>
T sqr(T x)
{
    return x*x;
}


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


Waveform::Waveform(long length, int samplerate):length(length), samplerate(samplerate)
{
    data=new float[length];
}


Waveform::~Waveform()
{
    delete[] data;
}


Waveform* Waveform::load(const char* filename)
{
    SF_INFO sfinfo;
    SNDFILE* sf=sf_open(filename, SFM_READ, &sfinfo);
    if (!sf) return nullptr;

    assert(sfinfo.channels==1);

    long length=sf_seek(sf, 0, SEEK_END);
    sf_seek(sf, 0, SEEK_SET);

    Waveform* wave=new Waveform(length, sfinfo.samplerate);

    sf_read_float(sf, wave->data, length);

    sf_close(sf);

    return wave;
}


int main()
{
    Waveform* wave=Waveform::load("testdata/example1.wav");

    const int count=1024;
    const int overlap=24;


    ICorrelationService* corrsvc=ICorrelationService::create(count);

    long offs=count - overlap;

    float tmp[count], tmp2[count];
    corrsvc->run(*wave+offs-overlap, *wave+offs-count+overlap, tmp);

    float y0=0.0f;
    for (int i=-overlap;i<overlap;i++)
        y0+=sqr((*wave)[offs+i]);
    // FIXME: should be same as tmp[2*overlap]

    float y1=y0;

    printf("%f vs. %f\n", tmp[2*overlap], y0);

    tmp2[0]=1.0f;

    for (int i=1;i<count-2*overlap;i++) {
        y0+=sqr((*wave)[offs+overlap+i-1]);
        y1+=sqr((*wave)[offs-overlap-i]);

        tmp2[i]=tmp[2*overlap+i-1] / sqrt(y0*y1);
    }

    //printf("%g, %g, %g, %g\n", tmp[1], tmp[2], tmp[3], tmp[4]);
    printf("%f, %f, %f, %f\n", tmp2[1], tmp2[2], tmp2[3], tmp2[4]);

    float dtmp=0.0f;
    int zerocrossings=0;

    for (int i=0;i<(count-overlap)/4 /* FIXME */;i++) {
        float d=(tmp2[i] - 8*tmp2[i+1] + 8*tmp2[i+3] - tmp2[i+4])/12;
        if (d*dtmp<0)
            zerocrossings++;

        dtmp=d;
    }

    printf("%d zerocrossings\n", zerocrossings);

    return 0;
}
