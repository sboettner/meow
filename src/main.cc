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


template<typename T>
class Array2D {
    T*  data;
    int ni;
    int nj;

public:
    Array2D(int ni, int nj):ni(ni), nj(nj)
    {
        data=new T[ni*nj];
    }

    ~Array2D()
    {
        delete[] data;
    }

    T& operator()(int i, int j)
    {
        assert(0<=i && i<ni);
        assert(0<=j && j<nj);
        return data[i*nj + j];
    }
};


// analysis frame
struct Frame {
    double  position;
    float   period;     // zero if unvoiced
    float   pitch;
};


struct Chunk {
    int     begin;
    int     end;
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
                if (peakval>bestpeakval + 0.01f) {
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
            float freq=wave.get_samplerate() / bestperiod;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            printf("\e[32;1mperiod=%.1f  freq=%.1f  val=%.4f  pitch=%.2f\n", bestperiod, wave.get_samplerate()/bestperiod, bestpeakval, pitch);

            frames.push_back({ position, bestperiod, pitch });
            position+=bestperiod;
        }
    }

    frames.push_back({ (double) wave.get_length(), 0.0f, 0.0f });

    return frames;
}


void detect_notes(const std::vector<Frame>& frames)
{
    const int n=frames.size();

    struct Node {
        int     pitch;
        int     back;
        float   cost;
    };

    Array2D<Node> nodes(n, 5);

    for (int j=0;j<5;j++) {
        nodes(0, j).pitch=-1;
        nodes(0, j).back=-1;
        nodes(0, j).cost=0.0f;
    }

    for (int i=1;i<n;i++) {
        if (frames[i].pitch>0) {
            int p=lrintf(frames[i].pitch) - 2;

            for (int j=0;j<5;j++, p++) {
                float bestcost=INFINITY;
                int bestback=0;

                for (int k=0;k<5;k++) {
                    float cost=nodes(i-1, k).cost;

                    if (nodes(i-1, k).pitch!=p)
                        cost+=10.0f / abs(nodes(i-1, k).pitch-p); // change penalty
                    
                    cost+=sqr(frames[i].pitch - p);

                    if (cost<bestcost) {
                        bestcost=cost;
                        bestback=k;
                    }
                }

                nodes(i, j).pitch=p;
                nodes(i, j).back=bestback;
                nodes(i, j).cost=bestcost;
            }
        }
        else {
            for (int j=0;j<5;j++) {
                nodes(i, j).pitch=nodes(i-1, j).pitch;
                nodes(i, j).back=j;
                nodes(i, j).cost=nodes(i-1, j).cost;
            }
        }
    }

    int i=n-1, j=0;
    for (int k=1;k<5;k++)
        if (nodes(i, k).cost < nodes(i, j).cost)
            j=k;

    printf("total cost: %f\n", nodes(i, j).cost);
    
    static const char* notenames[]={ "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

    while (i>=0) {
        if (frames[i].pitch>0 && nodes(i, j).pitch>=0)
            printf("i=%d  %s-%d %+.1f\n", i, notenames[nodes(i, j).pitch%12], nodes(i, j).pitch/12, 100.0f*(frames[i].pitch-nodes(i, j).pitch));

        j=nodes(i--, j).back;
    }
}


int main()
{
    Waveform* wave=Waveform::load("testdata/example2.wav");

    auto frames=compute_frame_decomposition(*wave, 1024, 24);
    
    detect_notes(frames);

    return 0;
}
