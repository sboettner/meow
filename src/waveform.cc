#include <algorithm>
#include <sndfile.h>
#include "waveform.h"
#include "correlation.h"
#include "iprogressmonitor.h"


template<typename T>
T sqr(T x)
{
    return x*x;
}


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


void Waveform::compute_frame_decomposition(int blocksize, int overlap, IProgressMonitor& monitor)
{
    assert(frames.empty());

    struct CrudeFrame {
        double  position;
        int     zerocrossings=0;
        bool    voiced=false;

        float   period=0.0f;

        float   cost[8];
        uint8_t back[8];
        float   totalcost[8];

        CrudeFrame(double pos):position(pos)
        {
            for (int i=0;i<8;i++) {
                cost[i]=INFINITY;
                totalcost[i]=INFINITY;
            }
        }
    };


    std::unique_ptr<ICorrelationService> corrsvc(ICorrelationService::create(blocksize));

    std::vector<std::unique_ptr<CrudeFrame>> crudeframes;
    crudeframes.push_back(std::make_unique<CrudeFrame>(0.0));
    crudeframes[0]->cost[0]=0.0f;

    double position=blocksize - overlap;

    for (;;) {
        const long offs=lrint(position);
        if (offs+blocksize>=length) break;

        monitor.report(position / length);
        
        float correlation[blocksize];
        float normalized[blocksize];    // normalized correlation, same as Pearson correlation coefficient

        corrsvc->run(data+offs-overlap, data+offs-blocksize+overlap, correlation);

        float y0=0.0f;
        for (int i=-overlap;i<overlap;i++)
            y0+=sqr((*this)[offs+i]);
        // FIXME: should be same as correlation[2*overlap]

        float y1=y0;

        normalized[0]=1.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            y0+=sqr((*this)[offs+overlap+i-1]);
            y1+=sqr((*this)[offs-overlap-i]);

            normalized[i]=correlation[2*overlap+i-1] / sqrt(y0*y1);
        }

        float dtmp=0.0f;
        int zerocrossings=0;

        for (int i=0;i<blocksize/4;i++) {
            // 4th order 1st derivative finite difference approximation
            float d=normalized[i] - 8*normalized[i+1] + 8*normalized[i+3] - normalized[i+4];
            if (d*dtmp<0)
                zerocrossings++;

            dtmp=d;
        }

        if (zerocrossings>blocksize/32) {
            // many zerocrossing of the 1st derivative indicate an unvoiced frame
            auto cf=std::make_unique<CrudeFrame>(position);
            cf->zerocrossings=zerocrossings;
            cf->cost[0]=0.0f;
            crudeframes.push_back(std::move(cf));

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
            auto cf=std::make_unique<CrudeFrame>(position);
            cf->zerocrossings=zerocrossings;
            cf->cost[0]=0.0f;
            crudeframes.push_back(std::move(cf));

            position=offs + blocksize/4;
        }
        else {
            float freq=get_samplerate() / bestperiod;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            auto cf=std::make_unique<CrudeFrame>(position);
            cf->zerocrossings=zerocrossings;
            cf->voiced=true;
            cf->period=bestperiod;
            cf->cost[0]=M_PI/2; // cost for making this frame unvoiced
            cf->cost[1]=bestpeakval<1.0f ? acosf(bestpeakval) : 0.0f;

            for (int i=2;i<8;i++) {
                float minpeakval=bestpeakval;

                for (int j=1;j<i;j++) {
                    float t=bestperiod * j / i;
                    int t0=(int) floorf(t);
                    t-=t0;
                    minpeakval=std::min(minpeakval, normalized[t0]*(1.0f-t)+normalized[t0+1]*t);
                }

                cf->cost[i]=minpeakval<1.0f ? acosf(minpeakval) : 0.0f;
            }

            crudeframes.push_back(std::move(cf));

            position+=bestperiod;
        }
    }

    crudeframes.push_back(std::make_unique<CrudeFrame>(double(length)));
    crudeframes.back()->cost[0]=0.0f;

    monitor.report(1.0);


    // Viterbi algorithm
    crudeframes[0]->totalcost[0]=0.0f;

    for (int i=1;i<crudeframes.size();i++) {
        for (int j=0;j<8;j++) {
            float bestcost=INFINITY;
            int bestback=0;

            for (int k=0;k<8;k++) {
                float cost=crudeframes[i-1]->totalcost[k] + crudeframes[i]->cost[j];

                if (j>0 && k>0)
                    cost+=25.0f * sqr(logf(crudeframes[i-1]->period/k) - logf(crudeframes[i]->period/j));
                else if (j>0)
                    cost+=5.0f;   // penalty for transitioning from unvoiced to voiced
                else if (k>0)
                    cost+=5.0f;   // penalty for transitioning from voiced to unvoiced

                if (cost<bestcost) {
                    bestcost=cost;
                    bestback=k;
                }
            }

            crudeframes[i]->totalcost[j]=bestcost;
            crudeframes[i]->back[j]=bestback;
        }
    }

    int i=crudeframes.size()-1, j=0;
    for (int k=0;k<8;k++)
        if (crudeframes[i]->totalcost[j] > crudeframes[i]->totalcost[k])
            j=k;

    while (i>=0) {
        printf("period=%f  state=%d  zx=%d  cost=%f\n", crudeframes[i]->period, j, crudeframes[i]->zerocrossings, crudeframes[i]->cost[j]);

        if (j==0)
            frames.push_back({ crudeframes[i]->position, 0.0f, 0.0f });
        else {
            float freq=get_samplerate() / crudeframes[i]->period * j;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            for (int k=j-1;k>=0;k--)
                frames.push_back({ crudeframes[i]->position + crudeframes[i]->period*k/j, crudeframes[i]->period/j, pitch });
        }

        j=crudeframes[i--]->back[j];
    }

    std::reverse(frames.begin(), frames.end());
}

