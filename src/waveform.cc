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


std::shared_ptr<Waveform> Waveform::load(const char* filename)
{
    SF_INFO sfinfo;
    SNDFILE* sf=sf_open(filename, SFM_READ, &sfinfo);
    if (!sf) return nullptr;

    assert(sfinfo.channels==1);

    long length=sf_seek(sf, 0, SEEK_END);
    sf_seek(sf, 0, SEEK_SET);

    auto wave=std::make_shared<Waveform>(length, sfinfo.samplerate);

    sf_read_float(sf, wave->data, length);

    sf_close(sf);

    return wave;
}

