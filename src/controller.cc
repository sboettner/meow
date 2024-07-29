#include "controller.h"
#include "render.h"


class Controller::ChunkModifier:public Controller::IChunkModifier {
public:
    ChunkModifier(Controller& controller, Track::Chunk* chunk, double t, double y);
    virtual ~ChunkModifier();

    virtual void finish() override;
    virtual void move_to(double t, double y) override;

private:
    Track::Chunk*                   chunk;
    bool                            moving=false;

    double                          pitch_offset;

    std::shared_ptr<IAudioProvider> audioprovider;
};


Controller::IChunkModifier::~IChunkModifier()
{
}


Controller::ChunkModifier::ChunkModifier(Controller& controller, Track::Chunk* chunk, double t, double y):
    chunk(chunk),
    pitch_offset(chunk->avgpitch-y)
{
    audioprovider=std::shared_ptr<IAudioProvider>(create_render_audio_provider(controller.track, chunk, chunk->next->next));
    controller.audiodev->play(audioprovider);
}


Controller::ChunkModifier::~ChunkModifier()
{
}


void Controller::ChunkModifier::finish()
{
    audioprovider->terminate();
    audioprovider=nullptr;
}


void Controller::ChunkModifier::move_to(double t, double y)
{
    if (!moving) {
        if (fabs(y+pitch_offset-chunk->avgpitch) < 0.25) return;
        
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
        newpitch=y - pitch_offset;

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


Controller::Controller(Track& track):track(track)
{
    audiodev=std::unique_ptr<IAudioDevice>(IAudioDevice::create());
}


Controller::~Controller()
{
}


std::unique_ptr<Controller::IChunkModifier> Controller::begin_modify_chunk(Track::Chunk* chunk, double t, float y)
{
    return std::make_unique<ChunkModifier>(*this, chunk, t, y);
}


void Controller::insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y)
{
    auto& pc=after.get_chunk()->pitchcontour;

    pc.insert(pc.begin()+after.get_index()+1, Track::HermiteSplinePoint { t, y, 0.0f });

    Track::update_akima_slope(after-1, after, after+1, after+2, after+3);
}
