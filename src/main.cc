#include <stdio.h>
#include <math.h>
#include "correlation.h"
#include "waveform.h"

template<typename T>
T sqr(T x)
{
    return x*x;
}


int main()
{
    Waveform* wave=Waveform::load("testdata/example2.wav");

    const int count=1024;
    const int overlap=24;


    ICorrelationService* corrsvc=ICorrelationService::create(count);

    long offs=count - overlap;

    while (offs+count<wave->get_length()) {
        float correlation[count];
        float normalized[count];    // normalized correlation, same as Pearson correlation coefficient

        corrsvc->run(*wave+offs-overlap, *wave+offs-count+overlap, correlation);

        float y0=0.0f;
        for (int i=-overlap;i<overlap;i++)
            y0+=sqr((*wave)[offs+i]);
        // FIXME: should be same as correlation[2*overlap]

        float y1=y0;

        normalized[0]=1.0f;

        for (int i=1;i<count-2*overlap;i++) {
            y0+=sqr((*wave)[offs+overlap+i-1]);
            y1+=sqr((*wave)[offs-overlap-i]);

            normalized[i]=correlation[2*overlap+i-1] / sqrt(y0*y1);
        }

        float dtmp=0.0f;
        int zerocrossings=0;

        for (int i=0;i<count;i++) {
            // 4th order 1st derivative finite difference approximation
            float d=normalized[i] - 8*normalized[i+1] + 8*normalized[i+3] - normalized[i+4];
            if (d*dtmp<0)
                zerocrossings++;

            dtmp=d;
        }

        if (zerocrossings>count/32) {
            // many zerocrossing of the 1st derivative indicate an unvoiced frame
            printf("\e[35;1m%d zerocrossings\n", zerocrossings);
            offs+=count/4;
            continue;
        }

        bool pastnegative=false;
        float bestpeakval=0.0f;
        float bestperiod=0.0f;

        for (int i=1;i<count-2*overlap;i++) {
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
            offs+=count/4;
        }
        else {
            printf("\e[32;1mperiod=%.1f  freq=%.1f  val=%.4f\n", bestperiod, wave->get_samplerate()/bestperiod, bestpeakval);
            offs+=lrintf(bestperiod);
        }
    }

    return 0;
}
