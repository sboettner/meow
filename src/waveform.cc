#include <sndfile.h>
#include "waveform.h"


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

