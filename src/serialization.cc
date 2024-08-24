#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include "project.h"


const uint32_t file_header_magic=0x776f656d;
const uint32_t track_header_magic=0x206b7274;
const uint32_t waveform_header_magic=0x65766177;


CEREAL_CLASS_VERSION(Waveform::Frame, 1);

template<typename Archive>
void Waveform::Frame::serialize(Archive& ar, uint32_t ver)
{
    ar(position, pitch, energy);
}


CEREAL_CLASS_VERSION(Waveform, 1);

template<typename Archive>
void Waveform::load(Archive& ar, uint32_t ver)
{
    uint32_t magic;
    ar(magic);
    if (magic!=waveform_header_magic)
        throw std::runtime_error("Bad file format");

    ar(length, samplerate);

    data=new float[length];
    ar(cereal::binary_data(data, length*sizeof(float)));

    ar(frames);
}


template<typename Archive>
void Waveform::save(Archive& ar, uint32_t ver) const
{
    ar(waveform_header_magic);
    ar(length, samplerate);
    ar(cereal::binary_data(data, length*sizeof(float)));
    ar(frames);
}


CEREAL_CLASS_VERSION(Track::HermiteSplinePoint, 1);

template<typename Archive>
void Track::HermiteSplinePoint::serialize(Archive& ar, uint32_t ver)
{
    ar(t, y, dy);
}


CEREAL_CLASS_VERSION(Track::Chunk, 1);

template<typename Archive>
void Track::Chunk::serialize(Archive& ar, uint32_t ver)
{
    ar(beginframe, endframe);
    ar(begin, end);
    ar(pitch, voiced, elastic);
    ar(pitchcontour);
}


CEREAL_CLASS_VERSION(Track, 1);

template<typename Archive>
void Track::load(Archive& ar, uint32_t ver)
{
    uint32_t magic;
    ar(magic);
    if (magic!=track_header_magic)
        throw std::runtime_error("Bad file format");

    ar(name, volume, panning, mute, solo, color);

    ar(wave);

    firstchunk=lastchunk=new Chunk;
    ar(*firstchunk);

    for (;;) {
        bool anotherchunk;
        ar(anotherchunk);
        if (!anotherchunk) break;

        Chunk* chunk=new Chunk;
        chunk->prev=lastchunk;
        lastchunk->next=chunk;
        lastchunk=chunk;

        ar(*chunk);
    }
}


template<typename Archive>
void Track::save(Archive& ar, uint32_t ver) const
{
    ar(track_header_magic);

    ar(name, volume, panning, mute, solo, color);

    ar(wave);

    for (Chunk* chunk=firstchunk; chunk; chunk=chunk->next)
        ar(*chunk, !!chunk->next);
}


CEREAL_CLASS_VERSION(Project, 1);

template<typename Archive>
void Project::serialize(Archive& ar, uint32_t ver)
{
    if (ver>cereal::detail::Version<Project>::version)
        throw std::runtime_error("Bad file version");

    ar(bpm, beat_subdivisions);
    ar(tracks);
}


void Project::read(std::istream& is)
{
    cereal::BinaryInputArchive ar(is);

    uint32_t magic;
    ar(magic);
    if (magic!=file_header_magic)
        throw std::runtime_error("Bad file format");

    ar(*this);

    for (auto& track: tracks)
        track->compute_synth_frames();
}


void Project::write(std::ostream& os)
{
    cereal::BinaryOutputArchive ar(os);

    ar(file_header_magic);
    ar(*this);
}
