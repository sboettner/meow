#include "controller.h"
#include "render.h"


Controller::Controller(Track& track):track(track)
{
    audiodev=std::unique_ptr<IAudioDevice>(IAudioDevice::create());
}


Controller::~Controller()
{
}


void Controller::begin_move_chunk(Track::Chunk* chunk, double t, float y)
{
    moving=false;
    moving_pitch_offset=chunk->avgpitch-y;

    audioprovider=std::shared_ptr<IAudioProvider>(create_render_audio_provider(track, chunk, chunk->next->next));
    audiodev->play(audioprovider);
}


void Controller::do_move_chunk(Track::Chunk* chunk, double t, float y)
{
    if (!moving) {
        if (fabs(y+moving_pitch_offset-chunk->avgpitch) < 0.25) return;
        
        moving=true;
    }

    double newpitch;

    if (chunk->type==Track::Chunk::Type::LeadingUnvoiced || chunk->type==Track::Chunk::Type::TrailingUnvoiced) {
        if (!chunk->next || (chunk->prev && fabsf(chunk->prev->avgpitch-y)<fabsf(chunk->next->avgpitch-y))) {
            newpitch=chunk->prev->avgpitch;
            chunk->type=Track::Chunk::Type::TrailingUnvoiced;
        }
        else {
            newpitch=chunk->next->avgpitch;
            chunk->type=Track::Chunk::Type::LeadingUnvoiced;
        }
    }
    else
        newpitch=y - moving_pitch_offset;

    float delta=newpitch - chunk->avgpitch;
    if (delta==0.0f) return;

    chunk->avgpitch=newpitch;

    for (auto* ch=chunk->prev; ch && ch->type==Track::Chunk::Type::LeadingUnvoiced; ch=ch->prev)
        ch->avgpitch=newpitch;

    for (auto* ch=chunk->next; ch && ch->type==Track::Chunk::Type::TrailingUnvoiced; ch=ch->next)
        ch->avgpitch=newpitch;

    for (auto& pc: chunk->pitchcontour)
        pc.y+=delta;
        // TODO: update pc.dy
}


void Controller::finish_move_chunk(Track::Chunk* chunk, double t, float y)
{
    audioprovider->terminate();
    audioprovider=nullptr;
}


void Controller::insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y)
{
    auto& pc=after.get_chunk()->pitchcontour;

    pc.insert(pc.begin()+after.get_index()+1, Track::HermiteSplinePoint { t, y, 0.0f });

    Track::update_akima_slope(after-1, after, after+1, after+2, after+3);
}
