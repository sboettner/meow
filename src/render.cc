#include <deque>
#include "audio.h"
#include "render.h"


class RenderAudioProvider:public IAudioProvider {
    const Track&        track;
    const Waveform&     wave;

    Track::Chunk*       firstchunk;
    Track::Chunk*       lastchunk;
    Track::Chunk*       curchunk;
    
    Track::PitchContourIterator pitch_contour_position;

    long                ptr;
    int                 nextframe;
    double              nextframepos;
    double              nextperiod;

    struct ActiveFrame {
        double  t;
        double  tstep;
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
    curchunk(firstchunk),
    pitch_contour_position(firstchunk, 0)
{
    nextframe   =firstchunk->beginframe;
    nextframepos=firstchunk->begin;
    nextperiod  =track.get_frame(nextframe).period; // FIXME: evaluate pitch contour instead

    ptr=firstchunk->begin;
    if (nextframe>0)
        ptr-=lrint(track.get_frame(nextframe).position - track.get_frame(nextframe-1).position);
}


unsigned long RenderAudioProvider::provide(float* buffer, unsigned long count)
{
    if (terminating && active.empty())
        return 0;
    
    unsigned long done=0;

    const double scale=1.0;

    while (count--) {
        if (!terminating) {
            while (ptr+nextperiod/scale>=nextframepos) {
                double t=(nextframepos-curchunk->begin) / (curchunk->end-curchunk->begin);
                if (0<=t && t<1) {
                    double s=track.get_frame(curchunk->beginframe).position*(1.0-t) + track.get_frame(curchunk->endframe).position*t;
                    while (nextframe+1<curchunk->endframe && track.get_frame(nextframe+1).position<=s) nextframe++;

                    active.push_back({
                        (ptr-nextframepos)*scale + track.get_frame(nextframe).position,
                        scale,
                        track.get_frame(nextframe-1).position,
                        track.get_frame(nextframe).position,
                        track.get_frame(nextframe+1).position
                    });
                }

                nextframepos+=nextperiod;

                if (curchunk->voiced) {
                    auto next_pitch_contour_position=pitch_contour_position + 1;

                    while (next_pitch_contour_position && ptr>next_pitch_contour_position->t) {
                        pitch_contour_position=next_pitch_contour_position;
                        next_pitch_contour_position=pitch_contour_position + 1;
                    }

                    Track::HermiteInterpolation interp;
                    
                    if (next_pitch_contour_position)
                        interp=Track::HermiteInterpolation(*pitch_contour_position, *next_pitch_contour_position);
                    else
                        interp=Track::HermiteInterpolation(pitch_contour_position->y);

                    nextperiod=track.get_samplerate() / (expf((interp(double(ptr)) - 69.0f) * M_LN2 / 12) * 440.0f);
                }
                else
                    nextperiod=float(track.get_frame(nextframe+1).position - track.get_frame(nextframe).position);
            }
        }

        float out=0.0f;

        for (auto& af: active) {
            if (af.t<=af.tbegin)
                ;   // silence before frame
            else if (af.t<=af.tmid) {
                float s=float((af.t-af.tbegin) / (af.tmid-af.tbegin));
                out+=wave(af.t) * (1.0f-cosf(M_PI*s)) * 0.5f;
            }
            else if (af.t<af.tend) {
                float s=float((af.t-af.tmid) / (af.tend-af.tmid));
                out+=wave(af.t) * (1.0f+cosf(M_PI*s)) * 0.5f;
            }

            af.t+=af.tstep;
        }

        buffer[done++]=out;
        ptr++;

        while (!active.empty() && active[0].t>=active[0].tend)
            active.pop_front();

        while (nextframepos>=curchunk->end) {
            if (!curchunk->next) {
                terminating=true;
                break;
            }
            else {
                curchunk=curchunk->next;

                if (curchunk->voiced && !curchunk->prev->voiced)
                    pitch_contour_position=Track::PitchContourIterator(curchunk, 0);
            }
        }
    }

    return done;
}
