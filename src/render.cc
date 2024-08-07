#include <deque>
#include "audio.h"
#include "render.h"


class RenderAudioProvider:public IAudioProvider {
    const Track&        track;
    const Waveform&     wave;

    Track::Chunk*       firstchunk;
    Track::Chunk*       lastchunk;
    Track::Chunk*       curchunk;
    
    long                ptr;
    int                 nextsynth;

    struct ActiveFrame {
        double  s;
        double  sstep;
        double  tbegin; // start of window
        double  tmid;   // center/peak of window
        double  tend;   // end of window
    };

    std::deque<ActiveFrame> active;

public:
    RenderAudioProvider(const Track& track, Track::Chunk* firstchunk, Track::Chunk* lastchunk);
    
    virtual unsigned long provide(float* buffer, unsigned long count) override;
};


IAudioProvider* create_render_audio_provider(const Track& track, Track::Chunk* first, Track::Chunk* last)
{
    return new RenderAudioProvider(track, first, last);
}


RenderAudioProvider::RenderAudioProvider(const Track& track, Track::Chunk* firstchunk, Track::Chunk* lastchunk):
    track(track), 
    wave(track.get_waveform()),
    firstchunk(firstchunk),
    lastchunk(lastchunk),
    curchunk(firstchunk)
{
    nextsynth=0;
    ptr=lrint(firstchunk->synth[0].tbegin);
}


unsigned long RenderAudioProvider::provide(float* buffer, unsigned long count)
{
    if (terminating && active.empty())
        return 0;
    
    unsigned long done=0;

    while (count--) {
        if (!terminating) {
            while (ptr>=curchunk->synth[nextsynth].tbegin) {
                double t=track.get_frame(curchunk->synth[nextsynth].frame).position;

                active.push_back({
                    ptr - curchunk->synth[nextsynth].tmid + t,
                    1.0,
                    curchunk->synth[nextsynth].tbegin,
                    curchunk->synth[nextsynth].tmid,
                    curchunk->synth[nextsynth].tend
                });

                nextsynth++;
                while (nextsynth>=curchunk->synth.size()) {
                    nextsynth=0;
                    curchunk=curchunk->next;
                    if (!curchunk) {
                        terminating=true;
                        break;
                    }
                }

                if (!curchunk) break;
            }
        }

        float out=0.0f;

        for (auto& af: active) {
            if (ptr<=af.tmid) {
                float u=float(ptr-af.tbegin) / float(af.tmid-af.tbegin);
                out+=wave(af.s) * (1.0f-cosf(M_PI*u)) * 0.5f;
            }
            else if (ptr<af.tend) {
                float u=float(ptr-af.tmid) / float(af.tend-af.tmid);
                out+=wave(af.s) * (1.0f+cosf(M_PI*u)) * 0.5f;
            }

            af.s+=af.sstep;
        }

        buffer[done++]=out;
        ptr++;

        while (!active.empty() && ptr>=active[0].tend)
            active.pop_front();
    }

    return done;
}
