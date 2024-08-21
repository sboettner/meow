#include <memory>
#include <algorithm>
#include <stdio.h>
#include <math.h>
#include "track.h"


template<typename T>
T sqr(T x)
{
    return x*x;
}


template<typename T>
T unlerp(T x0, T x1, T x)
{
    return (x-x0) / (x1-x0);
}


Track::HermiteInterpolation::HermiteInterpolation(float v)
{
    t0=0.0;

    a=b=c=0.0f;
    d=v;
}


Track::HermiteInterpolation::HermiteInterpolation(const HermiteSplinePoint& p0, const HermiteSplinePoint& p1)
{
    const float dt=float(p1.t - p0.t);
    const float m=(p1.y - p0.y) / dt;

    t0=p0.t;
    a=(p0.dy+p1.dy-2*m) / sqr(dt);
    b=(3*m-2*p0.dy-p1.dy) / dt;
    c=p0.dy;
    d=p0.y;
}


Track::Track(std::shared_ptr<Waveform> wave):wave(wave)
{
    name="unnamed track";
}


Track::~Track()
{
    while (firstchunk) {
        Chunk* tmp=firstchunk;
        firstchunk=tmp->next;

        delete tmp;
    }
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
    const int n=wave->get_frame_count();

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
        const auto& frame=wave->get_frame(i);

        if (frame.pitch>0) {
            int p=lrintf(frame.pitch) - 3;

            for (int j=0;j<7;j++, p++) {
                float bestcost=INFINITY;
                int bestback=0;

                for (int k=0;k<7;k++) {
                    float cost=nodes(i-1, k).cost;

                    if (nodes(i-1, k).pitch>=0 && nodes(i-1, k).pitch!=p)
                        cost+=10.0f; // change penalty
                    
                    cost+=fabs(frame.pitch - p);

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

    static const char* notenames[]={ "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

    assert(!firstchunk && !lastchunk);

    while (i>=0) {
        int begin=i;
        int pitch=nodes(i, j).pitch;

        while (begin>=0 && nodes(begin, j).pitch==pitch)
            j=nodes(begin--, j).back;
        
        Chunk* tmp=new Chunk;
        tmp->prev  =tmp->next=nullptr;
        
        tmp->beginframe=begin+1;
        tmp->endframe  =i+1;

        tmp->begin =lrint(wave->get_frame(begin+1).position);
        tmp->end   =lrint(wave->get_frame(i    +1).position);

        tmp->pitch=pitch;
        tmp->voiced=pitch>=0;
        tmp->elastic=tmp->voiced;

        if (firstchunk) {
            firstchunk->prev=tmp;
            tmp       ->next=firstchunk;
        }
        else
            lastchunk=tmp;

        firstchunk=tmp;

        i=begin;
    }

    for (auto* ch=firstchunk; ch; ch=ch->next)
        if (!ch->voiced) {
            if (ch->prev && ch->next)
                ch->pitch=(ch->prev->pitch+ch->next->pitch) / 2;
            else if (ch->prev)
                ch->pitch=ch->prev->pitch;
            else if (ch->next)
                ch->pitch=ch->next->pitch;
            else
                ch->pitch=60;
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

        void update_slope()
        {
            update_akima_slope(
                prev && prev->prev ? &prev->prev->pt : nullptr,
                prev ? &prev->pt : nullptr,
                &pt,
                next ? &next->pt : nullptr,
                next && next->next ? &next->next->pt : nullptr
            );
        }

        float compute_error(const Waveform& wave) const
        {
            assert(next);

            HermiteInterpolation interp(pt, next->pt);

            float error=0.0f;
            for (int j=frameidx+1;j<next->frameidx;j++)
                error+=fabs(interp(wave.get_frame(j).position) - wave.get_frame(j).pitch);

            return error;
        }

        void optimize(const Waveform& wave)
        {
            assert(prev && next);

            float besterror=INFINITY;
            int bestidx=0;

            for (int i=prev->frameidx+1;i<next->frameidx;i++) {
                frameidx=i;
                pt.t=wave.get_frame(i).position;
                pt.y=wave.get_frame(i).pitch;

                this->update_slope();
                prev->update_slope();
                next->update_slope();

                const float error=
                      prev->compute_error(wave)
                    + this->compute_error(wave)
                    - 5.0f*logf(this->pt.t-prev->pt.t)
                    - 5.0f*logf(next->pt.t-this->pt.t);

                if (error<besterror) {
                    besterror=error;
                    bestidx=i;
                }
            }

            frameidx=bestidx;
            pt.t=wave.get_frame(bestidx).position;
            pt.y=wave.get_frame(bestidx).pitch;

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

    first->pt.t=wave->get_frame(from).position;
    first->pt.y=wave->get_frame(from).pitch;

    last ->pt.t=wave->get_frame(to-1).position;
    last ->pt.y=wave->get_frame(to-1).pitch;

    first->pt.dy=last->pt.dy=(last->pt.y-first->pt.y) / (last->pt.t-first->pt.t);

    for (int pass=0;pass<100;pass++) {
        Node* worst=first;
        float worsterror=0.0f;

        for (Node* node=worst; node->next; node=node->next) {
            float error=node->compute_error(*wave);
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

        split->optimize(*wave);

        for (Node* node=split->prev; node->prev; node=node->prev)
            node->optimize(*wave);

        for (Node* node=split->next; node->next; node=node->next)
            node->optimize(*wave);
    }


    for (Node* node=first; node;) {
        while (chunk->next && chunk->next->voiced && node->pt.t>=wave->get_frame(chunk->next->beginframe).position)
            chunk=chunk->next;
        
        chunk->pitchcontour.push_back(node->pt);

        Node* next=node->next;
        delete node;
        node=next;
    }
}


void Track::compute_synth_frames()
{
    double curpos=0.0;

    synth.clear();

    for (Chunk* chunk=firstchunk; chunk; chunk=chunk->next) {
        if (chunk->voiced) {
            // Pitch-Synchronous Overlap-Add
            double t=chunk->begin;

            PitchContourIterator pci(chunk, 0);

            for (;;) {
                while (pci+1 && t>=(pci+1)->t) pci=pci+1;

                while (t>=chunk->end) {
                    if (!chunk->next || !chunk->next->voiced) goto end;
                    chunk=chunk->next;
                }

                double s=(t-chunk->begin) / (chunk->end-chunk->begin);
                
                HermiteInterpolation hi;
                if (pci+1)
                    hi=HermiteInterpolation(*pci, *(pci+1));
                else
                    hi=HermiteInterpolation(pci->y);
                
                double nextperiod=get_samplerate() / (expf((hi(t) - 69.0f) * M_LN2 / 12) * 440.0f);


                SynthFrame sf;

                int frame=lrint(chunk->beginframe*(1.0-s) + chunk->endframe*s);
                sf.smid  =wave->get_frame(frame).position;
                sf.tbegin=t + wave->get_frame(frame-1).position - sf.smid;
                sf.tmid  =t;
                sf.tend  =t + wave->get_frame(frame+1).position - sf.smid;

                sf.stretch=1.0f;
                sf.amplitude=1.0f;

                synth.push_back(sf);

                t+=nextperiod;
            }

            end:;
        }
        else {
            // simple Overlap-Add
            for (int i=chunk->beginframe;i<chunk->endframe;i++) {
                double s0=unlerp(wave->get_frame(chunk->beginframe).position, wave->get_frame(chunk->endframe).position, wave->get_frame(i  ).position);
                double s1=unlerp(wave->get_frame(chunk->beginframe).position, wave->get_frame(chunk->endframe).position, wave->get_frame(i+1).position);

                SynthFrame sf;

                sf.smid=wave->get_frame(i).position;
                sf.tmid=chunk->begin*(1.0-s0) + chunk->end*s0;
                sf.tend=chunk->begin*(1.0-s1) + chunk->end*s1;

                if (!synth.empty())
                    sf.tbegin=synth.back().tmid;
                else
                    sf.tbegin=sf.tmid;

                sf.stretch=1.0f;
                sf.amplitude=1.0f;
                
                synth.push_back(sf);
            }
        }
    }
}


int Track::get_first_synth_frame_index(const Track::Chunk* chunk) const
{
    return std::lower_bound(
        synth.begin(),
        synth.end(),
        chunk->begin,
        [] (const SynthFrame& sf, long ptr) {
            return sf.tmid < ptr;
        }
    ) - synth.begin();
}


void Track::update_akima_slope(const HermiteSplinePoint* p0, const HermiteSplinePoint* p1, HermiteSplinePoint* p2, const HermiteSplinePoint* p3, const HermiteSplinePoint* p4)
{
    // compute slope according to the rules for an Akima spline

    if (!p2) return;

    if (!p1) {
        p2->dy=(p3->y-p2->y) / (p3->t-p2->t);
        return;
    }

    if (!p3) {
        p2->dy=(p2->y-p1->y) / (p2->t-p1->t);
        return;
    }

    float m1=(p2->y-p1->y) / (p2->t-p1->t);
    float m2=(p3->y-p2->y) / (p3->t-p2->t);

    if (!p0 || !p4) {
        p2->dy=(m1+m2) / 2;
        return;
    }

    float m0=(p1->y-p0->y) / (p1->t-p0->t);
    float m3=(p4->y-p3->y) / (p4->t-p3->t);

    float a=fabs(m2-m3);
    float b=fabs(m0-m1);
    if (a+b>0)
        p2->dy=(a*m1 + b*m2) / (a+b);
    else
        p2->dy=(m1+m2) / 2;
}

