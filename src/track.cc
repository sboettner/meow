#include <memory>
#include <algorithm>
#include <stdio.h>
#include <math.h>
#include "track.h"
#include "correlation.h"


template<typename T>
T sqr(T x)
{
    return x*x;
}


Track::Track(Waveform* wave):wave(wave)
{
}


Track::~Track()
{
    delete wave;

    for (auto* cf: crudeframes)
        delete cf;

    while (firstchunk) {
        Chunk* tmp=firstchunk;
        firstchunk=tmp->next;

        delete tmp;
    }
}


void Track::compute_frame_decomposition(int blocksize, int overlap)
{
    std::unique_ptr<ICorrelationService> corrsvc(ICorrelationService::create(blocksize));

    assert(crudeframes.empty());
    crudeframes.push_back(new CrudeFrame(0.0));
    crudeframes[0]->cost[0]=0.0f;

    double position=blocksize - overlap;

    for (;;) {
        const long offs=lrint(position);
        if (offs+blocksize>=wave->get_length()) break;

        float correlation[blocksize];
        float normalized[blocksize];    // normalized correlation, same as Pearson correlation coefficient

        corrsvc->run(*wave+offs-overlap, *wave+offs-blocksize+overlap, correlation);

        float y0=0.0f;
        for (int i=-overlap;i<overlap;i++)
            y0+=sqr((*wave)[offs+i]);
        // FIXME: should be same as correlation[2*overlap]

        float y1=y0;

        normalized[0]=1.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            y0+=sqr((*wave)[offs+overlap+i-1]);
            y1+=sqr((*wave)[offs-overlap-i]);

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
            CrudeFrame* cf=new CrudeFrame(position);
            cf->zerocrossings=zerocrossings;
            cf->cost[0]=0.0f;
            crudeframes.push_back(cf);

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
            CrudeFrame* cf=new CrudeFrame(position);
            cf->zerocrossings=zerocrossings;
            cf->cost[0]=0.0f;
            crudeframes.push_back(cf);

            position=offs + blocksize/4;
        }
        else {
            float freq=get_samplerate() / bestperiod;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            CrudeFrame* cf=new CrudeFrame(position);
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

            crudeframes.push_back(cf);

            position+=bestperiod;
        }
    }

    crudeframes.push_back(new CrudeFrame(double(wave->get_length())));
    crudeframes.back()->cost[0]=0.0f;
}


void Track::refine_frame_decomposition()
{
    assert(frames.empty());

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


void Track::detect_chunks()
{
    const int n=frames.size();

    struct Node {
        int     pitch;
        int     back;
        float   cost;
    };

    Array2D<Node> nodes(n, 7);

    for (int j=0;j<7;j++) {
        nodes(0, j).pitch=-1;
        nodes(0, j).back=-1;
        nodes(0, j).cost=0.0f;
    }

    for (int i=1;i<n;i++) {
        if (frames[i].pitch>0) {
            int p=lrintf(frames[i].pitch) - 3;

            for (int j=0;j<7;j++, p++) {
                float bestcost=INFINITY;
                int bestback=0;

                for (int k=0;k<7;k++) {
                    float cost=nodes(i-1, k).cost;

                    if (nodes(i-1, k).pitch>=0 && nodes(i-1, k).pitch!=p)
                        cost+=10.0f; // change penalty
                    
                    cost+=fabs(frames[i].pitch - p);

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
            for (int j=0;j<7;j++) {
                nodes(i, j).pitch=-1;
                nodes(i, j).back=j;
                nodes(i, j).cost=nodes(i-1, j).cost;
            }
        }
    }

    int i=n-2, j=0;
    for (int k=1;k<7;k++)
        if (nodes(i, k).cost < nodes(i, j).cost)
            j=k;

    printf("total cost: %f\n", nodes(i, j).cost);
    
    static const char* notenames[]={ "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

    assert(!firstchunk && !lastchunk);

    while (i>=0) {
        int begin=i;
        int pitch=nodes(i, j).pitch;

        while (begin>=0 && nodes(begin, j).pitch==pitch)
            j=nodes(begin--, j).back;
        
        if (pitch>=0)
            printf("\e[32;1mchunk %.2f-%.2f: %s-%d\n", frames[begin+1].position, frames[i+1].position, notenames[pitch%12], pitch/12);
        else
            printf("\e[35;1mchunk %.2f-%.2f: unvoiced\n", frames[begin+1].position, frames[i+1].position);

        Chunk* tmp=new Chunk;
        tmp->prev  =tmp->next=nullptr;
        
        tmp->beginframe=begin+1;
        tmp->endframe  =i+1;

        tmp->begin =lrint(frames[begin+1].position);
        tmp->end   =lrint(frames[i    +1].position);

        tmp->voiced=pitch>=0;

        float avgperiod=float(frames[i+1].position - frames[begin+1].position) / (i - begin);
        float avgfreq=get_samplerate() / avgperiod;
        tmp->avgpitch=
        tmp->newpitch=logf(avgfreq / 440.0f) / M_LN2 * 12.0f + 69.0f;

        if (firstchunk) {
            firstchunk->prev=tmp;
            tmp       ->next=firstchunk;
        }
        else
            lastchunk=tmp;

        firstchunk=tmp;

        i=begin;
    }
}


void Track::compute_pitch_contour()
{
    for (Chunk* ch=firstchunk; ch; ch=ch->next) {
        if (!ch->voiced) continue;

        Chunk* from=ch;
        while (ch->next && ch->next->voiced)
            ch=ch->next;

        compute_pitch_contour(from, from->beginframe, ch->endframe);
    }
}


void Track::compute_pitch_contour(Chunk* chunk, int from, int to)
{
    const int n=to-from;

    printf("block %d to %d, n=%d\n", from, to-1, n);

    struct Node {
        Node*               next=nullptr;
        Node*               prev=nullptr;
        int                 frameidx;
        HermiteSplinePoint  pt;

        void hermite_coeffs(float coeffs[4]) const
        {
            float dt=float(next->pt.t-pt.t);
            float m=(next->pt.y-pt.y) / dt;

            coeffs[0]=pt.y;
            coeffs[1]=pt.dy;
            coeffs[2]=(3*m-2*pt.dy-next->pt.dy) / dt;
            coeffs[3]=(pt.dy+next->pt.dy-2*m) / sqr(dt);
        }

        void update_slope()
        {
            // compute slope according to the rules for an Akima spline

            if (!prev) {
                pt.dy=(next->pt.y-pt.y) / (next->pt.t-pt.t);
                return;
            }

            if (!next) {
                pt.dy=(pt.y-prev->pt.y) / (pt.t-prev->pt.t);
                return;
            }

            float m1=(pt.y-prev->pt.y) / (next->pt.t-pt.t);
            float m2=(next->pt.y-pt.y) / (pt.t-prev->pt.t);

            if (!prev->prev || !next->next) {
                pt.dy=(m1+m2) / 2;
                return;
            }

            float m0=(prev->pt.y-prev->prev->pt.y) / (prev->pt.t-prev->prev->pt.t);
            float m3=(next->next->pt.y-next->pt.y) / (next->next->pt.t-next->pt.y);

            float a=fabs(m2-m3);
            float b=fabs(m0-m1);
            if (a+b>0)
                pt.dy=(a*m1 + b*m2) / (a+b);
            else
                pt.dy=(m1+m2) / 2;
        }

        float compute_error(const Track& track) const
        {
            assert(next);

            float hc[4];
            hermite_coeffs(hc);

            float error=0.0f;
            for (int j=frameidx+1;j<next->frameidx;j++) {
                float s=float(track.frames[j].position - pt.t);
                error+=fabs(hc[0] + s*(hc[1] + s*(hc[2] + s*hc[3])) - track.frames[j].pitch);
            }

            return error;
        }

        void optimize(const Track& track)
        {
            assert(prev && next);

            float besterror=INFINITY;
            int bestidx=0;

            for (int i=prev->frameidx+1;i<next->frameidx;i++) {
                frameidx=i;
                pt.t=track.frames[i].position;
                pt.y=track.frames[i].pitch;

                this->update_slope();
                prev->update_slope();
                next->update_slope();

                const float error=
                      prev->compute_error(track)
                    + this->compute_error(track)
                    - 5.0f*logf(this->pt.t-prev->pt.t)
                    - 5.0f*logf(next->pt.t-this->pt.t);

                if (error<besterror) {
                    besterror=error;
                    bestidx=i;
                }
            }

            frameidx=bestidx;
            pt.t=track.frames[bestidx].position;
            pt.y=track.frames[bestidx].pitch;

            this->update_slope();
            prev->update_slope();
            next->update_slope();

            if (prev->prev) prev->prev->update_slope();
            if (next->next) next->next->update_slope();
        }
    };

    Node* first=new Node;
    Node* last =new Node;

    first->next=last;
    last ->prev=first;

    first->frameidx=from;
    last ->frameidx=to-1;

    first->pt.t=frames[from].position;
    first->pt.y=frames[from].pitch;

    last ->pt.t=frames[to-1].position;
    last ->pt.y=frames[to-1].pitch;

    first->pt.dy=last->pt.dy=(last->pt.y-first->pt.y) / (last->pt.t-first->pt.t);

    for (int pass=0;pass<100;pass++) {
        Node* worst=first;
        float worsterror=0.0f;

        for (Node* node=worst; node->next; node=node->next) {
            float error=node->compute_error(*this);
            if (error>worsterror) {
                worsterror=error;
                worst=node;
            }
        }

        printf("error=%f\n", worsterror);
        if (worsterror<5.0f) break;

        // insert new node
        Node* split=new Node;
        split->prev=worst;
        split->next=worst->next;
        split->prev->next=split;
        split->next->prev=split;

        split->optimize(*this);

        for (Node* node=split->prev; node->prev; node=node->prev)
            node->optimize(*this);

        for (Node* node=split->next; node->next; node=node->next)
            node->optimize(*this);
    }


    for (Node* node=first; node;) {
        while (chunk->next && chunk->next->voiced && node->pt.t>=frames[chunk->next->beginframe].position)
            chunk=chunk->next;
        
        chunk->pitchcontour.push_back(node->pt);

        Node* next=node->next;
        delete node;
        node=next;
    }
}
