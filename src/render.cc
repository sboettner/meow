#include <algorithm>
#include "audio.h"
#include "render.h"


class RenderAudioProvider:public IAudioProvider {
    const Track&        track;
    const Waveform&     wave;

    Track::Chunk*       firstchunk;
    Track::Chunk*       lastchunk;
    Track::Chunk*       curchunk;
    
    long                ptr;
    int                 synthhead;
    int                 synthtail;

public:
    RenderAudioProvider(const Track& track, Track::Chunk* firstchunk, Track::Chunk* lastchunk);
    
    virtual unsigned long provide(float* buffer, unsigned long count) override;
};


std::shared_ptr<IAudioProvider> create_render_audio_provider(const Track& track, Track::Chunk* first, Track::Chunk* last)
{
    return std::make_shared<RenderAudioProvider>(track, first, last);
}


RenderAudioProvider::RenderAudioProvider(const Track& track, Track::Chunk* firstchunk, Track::Chunk* lastchunk):
    track(track), 
    wave(track.get_waveform()),
    firstchunk(firstchunk),
    lastchunk(lastchunk),
    curchunk(firstchunk)
{
    synthhead=synthtail=track.get_first_synth_frame_index(firstchunk);

    ptr=lrint(track.get_synth_frame(synthhead).tbegin);
}


unsigned long RenderAudioProvider::provide(float* buffer, unsigned long count)
{
    if (terminating && synthhead==synthtail)
        return 0;
    
    unsigned long done=0;

    while (count--) {
        if (!terminating) {
            while (track.get_synth_frame(synthhead).tbegin<=ptr) {
                if (++synthhead==track.get_synth_frame_count()) {
                    terminating=true;
                    break;
                }
            }
        }

        float out=0.0f;

        for (int i=synthtail;i<synthhead;i++) {
            const auto& sf=track.get_synth_frame(i);

            double s=(ptr - sf.tmid)*sf.stretch + sf.smid;

            if (ptr<sf.tmid) {
                float u=float(ptr-sf.tbegin) / float(sf.tmid-sf.tbegin);
                out+=wave(s) * (1.0f-cosf(M_PI*u)) * 0.5f * sf.amplitude;
            }
            else if (ptr<sf.tend) {
                float u=float(ptr-sf.tmid) / float(sf.tend-sf.tmid);
                out+=wave(s) * (1.0f+cosf(M_PI*u)) * 0.5f * sf.amplitude;
            }
        }

        buffer[done++]=out;
        ptr++;

        while (synthtail<synthhead && track.get_synth_frame(synthtail).tend<=ptr)
            synthtail++;
    }

    return done;
}
