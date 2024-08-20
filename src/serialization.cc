#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include "project.h"


CEREAL_CLASS_VERSION(Track::Frame, 1);

template<typename Archive>
void Track::Frame::serialize(Archive& ar, uint32_t ver)
{
    ar(position, period, pitch);
}


CEREAL_CLASS_VERSION(Waveform, 1);

template<typename Archive>
void Waveform::load(Archive& ar, uint32_t ver)
{
    ar(length, samplerate);

    data=new float[length];
    ar(cereal::binary_data(data, length*sizeof(float)));
}


template<typename Archive>
void Waveform::save(Archive& ar, uint32_t ver) const
{
    ar(length, samplerate);
    ar(cereal::binary_data(data, length*sizeof(float)));
}


CEREAL_CLASS_VERSION(Track::HermiteSplinePoint, 1);

template<typename Archive>
void Track::HermiteSplinePoint::serialize(Archive& ar, uint32_t ver)
{
    ar(t, y, dy);
}


CEREAL_CLASS_VERSION(Track::Chunk, 1);

template<typename Archive>
void Track::Chunk::load(Archive& ar, uint32_t ver)
{
    ar(beginframe, endframe);
    ar(begin, end);
    ar(pitch, voiced, elastic);
    ar(pitchcontour);

    bool havenext;
    ar(havenext);

    if (havenext) {
        next=new Chunk;
        next->prev=this;

        ar(*next);
    }
}


template<typename Archive>
void Track::Chunk::save(Archive& ar, uint32_t ver) const
{
    ar(beginframe, endframe);
    ar(begin, end);
    ar(pitch, voiced, elastic);
    ar(pitchcontour);

    if (next)
        ar(true, *next);
    else
        ar(false);
}


CEREAL_CLASS_VERSION(Track, 1);

template<typename Archive>
void Track::load(Archive& ar, uint32_t ver)
{
    ar(wave);
    ar(frames);

    firstchunk=lastchunk=new Chunk;
    ar(*firstchunk);

    while (lastchunk->next) lastchunk=lastchunk->next;
}


template<typename Archive>
void Track::save(Archive& ar, uint32_t ver) const
{
    ar(wave);
    ar(frames);

    ar(*firstchunk);
}


CEREAL_CLASS_VERSION(Project, 1);

template<typename Archive>
void Project::serialize(Archive& ar, uint32_t ver)
{
    ar(tracks);
}


void Project::read(std::istream& is)
{
    cereal::PortableBinaryInputArchive ar(is);

    ar(*this);

    for (auto& track: tracks)
        track->compute_synth_frames();
}


void Project::write(std::ostream& os)
{
    cereal::PortableBinaryOutputArchive ar(os);

    ar(*this);
}
