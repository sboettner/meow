#pragma once

#include <vector>
#include "waveform.h"

class Track {
public:
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

    // analysis frame marker
    struct Frame {
        double  position;
        float   period;     // zero if unvoiced
        float   pitch;
    };

    struct HermiteSplinePoint {
        double  t;
        float   y;
        float   dy;
    };

    class HermiteInterpolation {
        double  t0;
        float   a, b, c, d;

    public:
        HermiteInterpolation() {}
        HermiteInterpolation(float);
        HermiteInterpolation(const HermiteSplinePoint& p0, const HermiteSplinePoint& p1);

        float operator()(double t) const
        {
            t-=t0;
            return d + t*(c + t*(b + t*a));
        }
    };

    // synthesis chunk
    struct Chunk {
        Chunk*  prev;
        Chunk*  next;

        int     beginframe;
        int     endframe;

        long    begin;
        long    end;
        
        bool    voiced;
        float   avgpitch;
        float   newpitch;

        std::vector<HermiteSplinePoint> pitchcontour;
    };

    class PitchContourIterator {
        Chunk*  chunk;
        int     index;

    public:
        PitchContourIterator(Chunk* chunk, int index):chunk(chunk), index(index) {}

        HermiteSplinePoint* operator->()
        {
            assert(chunk);
            return &chunk->pitchcontour[index];
        }

        operator bool()
        {
            return !!chunk;
        }

        operator HermiteSplinePoint*()
        {
            return chunk ? &chunk->pitchcontour[index] : nullptr;
        }

        HermiteSplinePoint& operator*()
        {
            assert(chunk);
            return chunk->pitchcontour[index];
        }

        bool operator==(const PitchContourIterator& rhs) const
        {
            return chunk==rhs.chunk && index==rhs.index;
        }

        PitchContourIterator operator+(int rhs) const
        {
            assert(rhs>=0);

            PitchContourIterator result=*this;

            result.index+=rhs;
            while (result.index>=result.chunk->pitchcontour.size()) {
                result.index-=result.chunk->pitchcontour.size();

                result.chunk=result.chunk->next;
                if (!result.chunk) break;
                if (!result.chunk->voiced) {
                    result.chunk=nullptr;
                    break;
                }
            }

            return result;
        }

        PitchContourIterator operator-(int rhs) const
        {
            assert(rhs>=0);

            PitchContourIterator result=*this;

            result.index-=rhs;
            while (result.index<0) {
                result.chunk=result.chunk->prev;
                if (!result.chunk) break;
                if (!result.chunk->voiced) {
                    result.chunk=nullptr;
                    break;
                }

                result.index+=result.chunk->pitchcontour.size();
            }

            return result;
        }
    };


    Track(Waveform*);
    ~Track();

    void compute_frame_decomposition(int blocksize, int overlap);
    void refine_frame_decomposition();
    void detect_chunks();
    void compute_pitch_contour();

    int get_samplerate() const
    {
        return wave->get_samplerate();
    }

    const Waveform& get_waveform() const
    {
        return *wave;
    }

    const Frame& get_frame(int i) const
    {
        return frames[i];
    }

    int get_frame_count() const
    {
        return frames.size();
    }

    Chunk*  get_first_chunk()
    {
        return firstchunk;
    }

    static void update_akima_slope(const HermiteSplinePoint* p0, const HermiteSplinePoint* p1, HermiteSplinePoint* p2, const HermiteSplinePoint* p3, const HermiteSplinePoint* p4);

private:
    Waveform* const     wave;

    std::vector<CrudeFrame*>    crudeframes;
    std::vector<Frame>  frames;

    Chunk*              firstchunk=nullptr;
    Chunk*              lastchunk =nullptr;

    void compute_pitch_contour(Chunk* chunk, int from, int to);
};
