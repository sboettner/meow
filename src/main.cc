#include <vector>
#include <memory>
#include <stdio.h>
#include <math.h>
#include "correlation.h"
#include "waveform.h"

template<typename T>
T sqr(T x)
{
    return x*x;
}


struct Frame {
    double  position;
    float   period;     // zero if unvoiced
    float   pitch;
};


std::vector<Frame> compute_frame_decomposition(const Waveform& wave, int blocksize, int overlap)
{
    std::unique_ptr<ICorrelationService> corrsvc(ICorrelationService::create(blocksize));

    std::vector<Frame> frames;

    frames.push_back({ 0.0, 0.0f, 0.0f });

    double position=blocksize - overlap;

    for (;;) {
        const long offs=lrint(position);
        if (offs+blocksize>=wave.get_length()) break;

        float correlation[blocksize];
        float normalized[blocksize];    // normalized correlation, same as Pearson correlation coefficient

        corrsvc->run(wave+offs-overlap, wave+offs-blocksize+overlap, correlation);

        float y0=0.0f;
        for (int i=-overlap;i<overlap;i++)
            y0+=sqr(wave[offs+i]);
        // FIXME: should be same as correlation[2*overlap]

        float y1=y0;

        normalized[0]=1.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            y0+=sqr(wave[offs+overlap+i-1]);
            y1+=sqr(wave[offs-overlap-i]);

            normalized[i]=correlation[2*overlap+i-1] / sqrt(y0*y1);
        }

        float dtmp=0.0f;
        int zerocrossings=0;

        for (int i=0;i<blocksize;i++) {
            // 4th order 1st derivative finite difference approximation
            float d=normalized[i] - 8*normalized[i+1] + 8*normalized[i+3] - normalized[i+4];
            if (d*dtmp<0)
                zerocrossings++;

            dtmp=d;
        }

        if (zerocrossings>blocksize/32) {
            // many zerocrossing of the 1st derivative indicate an unvoiced frame
            printf("\e[35;1m%d zerocrossings\n", zerocrossings);
            frames.push_back({ position, 0.0f, 0.0f });
            position=offs + blocksize/4;
            continue;
        }

        bool pastnegative=false;
        float bestpeakval=0.0f;
        float bestperiod=0.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            pastnegative|=normalized[i] < 0;

            if (pastnegative && normalized[i]>normalized[i-1] && normalized[i]>normalized[i+1]) {
                // local maximum, determine exact location by quadratic interpolation
                float a=(normalized[i-1]+normalized[i+1])/2 - normalized[i];
                float b=(normalized[i+1]-normalized[i-1])/2;

                float peakval=normalized[i] - b*b/a/4;
                if (peakval>bestpeakval) {
                    bestperiod=i - b/a/2;
                    bestpeakval=peakval;
                }
            }
        }

        if (bestperiod==0.0f) {
            printf("\e[31;1m%d zerocrossings\n", zerocrossings);
            frames.push_back({ position, 0.0f, 0.0f });
            position=offs + blocksize/4;
        }
        else {
            printf("\e[32;1mperiod=%.1f  freq=%.1f  val=%.4f\n", bestperiod, wave.get_samplerate()/bestperiod, bestpeakval);

            float freq=wave.get_samplerate() / bestperiod;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            frames.push_back({ position, bestperiod, pitch });
            position+=bestperiod;
        }
    }

    frames.push_back({ (double) wave.get_length(), 0.0f, 0.0f });

    return frames;
}


int main()
{
    Waveform* wave=Waveform::load("testdata/example2.wav");

    auto frames=compute_frame_decomposition(*wave, 1024, 24);
    printf("%ld frames\n", frames.size());

    return 0;
}
