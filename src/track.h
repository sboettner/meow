#pragma once

#include "waveform.h"


class Track {
    friend class Controller;
    
public:
    struct HermiteSplinePoint {
        double  t;
        float   y;
        float   dy;

        template<typename Archive>
        void serialize(Archive& ar, uint32_t ver);
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

    struct SynthFrame {
        double  smid;       // window center in original waveform
        double  tbegin;     // window begin in output waveform
        double  tmid;       // window center in output waveform
        double  tend;       // window end in output waveform
        float   stretch;    // time scaling factor
        float   amplitude;  // amplitude scaling factor
    };

    // synthesis chunk
    struct Chunk {
        Chunk*  prev=nullptr;
        Chunk*  next=nullptr;

        int     beginframe;
        int     endframe;

        long    begin;
        long    end;
        
        int8_t  pitch;  // midi note 0-127
        bool    voiced;
        bool    elastic;

        std::vector<HermiteSplinePoint> pitchcontour;

        template<typename Archive>
        void load(Archive& ar, uint32_t);

        template<typename Archive>
        void save(Archive& ar, uint32_t) const;
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

        Chunk* get_chunk()
        {
            return chunk;
        }

        int get_index()
        {
            return index;
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


    Track() {}
    Track(std::shared_ptr<Waveform>);
    ~Track();

    void detect_chunks();
    void compute_pitch_contour();

    void compute_synth_frames();

    int get_samplerate() const
    {
        return wave->get_samplerate();
    }

    const Waveform& get_waveform() const
    {
        return *wave;
    }

    const SynthFrame& get_synth_frame(int i) const
    {
        return synth[i];
    }

    int get_synth_frame_count() const
    {
        return synth.size();
    }

    int get_first_synth_frame_index(const Track::Chunk*) const;

    Chunk*  get_first_chunk()
    {
        return firstchunk;
    }

    static void update_akima_slope(const HermiteSplinePoint* p0, const HermiteSplinePoint* p1, HermiteSplinePoint* p2, const HermiteSplinePoint* p3, const HermiteSplinePoint* p4);

    template<typename Archive>
    void load(Archive& ar, uint32_t);

    template<typename Archive>
    void save(Archive& ar, uint32_t) const;

private:
    std::shared_ptr<Waveform>   wave;

    std::vector<SynthFrame>     synth;

    Chunk*              firstchunk=nullptr;
    Chunk*              lastchunk =nullptr;

    void compute_pitch_contour(Chunk* chunk, int from, int to);
};
