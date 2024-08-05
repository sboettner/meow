#include <algorithm>
#include <cstdio>
#include "controller.h"
#include "render.h"


template<typename T>
T lerp(T a, T b, T x)
{
    return a*(T(1)-x) + b*x;
}


template<typename T>
T unlerp(T a, T b, T y)
{
    return (y-a) / (b-a);
}


Controller::Controller(Track& track):track(track)
{
    audiodev=std::unique_ptr<IAudioDevice>(IAudioDevice::create());
}


Controller::~Controller()
{
}


void Controller::begin_move_chunk(Track::Chunk* chunk, double t, float y)
{
    curchunk=chunk;

    Track::Chunk *first=chunk, *last=chunk;
    if (chunk->type==Track::Chunk::Type::TrailingUnvoiced) {
        while (first->prev && first->prev->type==Track::Chunk::Type::Voiced)
            first=first->prev;
    }

    if (chunk->type==Track::Chunk::Type::LeadingUnvoiced) {
        while (last->next && last->next->type==Track::Chunk::Type::Voiced)
            last=last->next;
    }

    curchunkbackup=backup(first, last, curchunk);

    moving_time_offset =chunk->begin - t;

    audioprovider=std::shared_ptr<IAudioProvider>(create_render_audio_provider(track, chunk, chunk->next->next));
    audiodev->play(audioprovider);
}


void Controller::do_move_chunk(Track::Chunk* chunk, double t, float y)
{
    if (curchunk->type==Track::Chunk::Type::TrailingUnvoiced) {
        const int len=curchunk->end - curchunk->begin;
        curchunk->begin=lrint(moving_time_offset + t);
        curchunk->end=curchunk->begin + len;

        double firstt=undo_stack.top().first->begin;

        Track::Chunk* cur=curchunk->prev;
        Track::Chunk* bup=curchunkbackup->prev;

        if (cur)
            cur->end=curchunk->begin;

        while (cur && bup && cur!=bup) {
            cur->begin=lrint(lerp(firstt, (double) curchunk->begin, unlerp(firstt, (double) curchunkbackup->begin, (double) bup->begin)));

            if (cur->prev && cur->prev->type==Track::Chunk::Type::Voiced)
                cur->prev->end=cur->begin;
            
            for (int i=0;i<cur->pitchcontour.size();i++)
                cur->pitchcontour[i].t=lerp(firstt, (double) curchunk->begin, unlerp(firstt, (double) curchunkbackup->begin, bup->pitchcontour[i].t));

            cur=cur->prev;
            bup=bup->prev;
        }
    }

    chunk->pitch=lrintf(y);
}


void Controller::finish_move_chunk(Track::Chunk* chunk, double t, float y)
{
    audioprovider->terminate();
    audioprovider=nullptr;

    curchunk=curchunkbackup=nullptr;
}


void Controller::begin_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y)
{
    backup(cp.get_chunk(), cp.get_chunk());
}


void Controller::do_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y)
{
    if (cp-1 && cp+1)
        cp->t=std::clamp(t, (cp-1)->t+48.0, (cp+1)->t-48.0);

    cp->y=y;

    Track::update_akima_slope(cp-4, cp-3, cp-2, cp-1, cp);
    Track::update_akima_slope(cp-3, cp-2, cp-1, cp, cp+1);
    Track::update_akima_slope(cp-2, cp-1, cp, cp+1, cp+2);
    Track::update_akima_slope(cp-1, cp, cp+1, cp+2, cp+3);
    Track::update_akima_slope(cp, cp+1, cp+2, cp+3, cp+4);
}


void Controller::finish_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y)
{
}


bool Controller::insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y)
{
    auto& pc=after.get_chunk()->pitchcontour;

    pc.insert(pc.begin()+after.get_index()+1, Track::HermiteSplinePoint { t, y, 0.0f });

    Track::update_akima_slope(after-1, after, after+1, after+2, after+3);

    return true;
}


bool Controller::delete_pitch_contour_control_point(Track::PitchContourIterator cp)
{
    if (cp-1 && cp+1) {
        cp.get_chunk()->pitchcontour.erase(cp.get_chunk()->pitchcontour.begin() + cp.get_index());
        return true;
    }
    else
        return false;
}


Track::Chunk* Controller::backup(Track::Chunk* first, Track::Chunk* last, Track::Chunk* mid)
{
    BackupState bs;
    Track::Chunk* midbackup=nullptr;

    bs.first=bs.last=new Track::Chunk(*first);

    if (first==mid)
        midbackup=bs.first;

    while (first!=last) {
        first=first->next;

        Track::Chunk* tmp=new Track::Chunk(*first);
        tmp->prev=bs.last;
        bs.last->next=tmp;
        bs.last=tmp;

        if (first==mid)
            midbackup=tmp;
    }

    undo_stack.push(bs);

    return midbackup;
}


void Controller::undo()
{
    if (undo_stack.empty()) return;

    BackupState& bs=undo_stack.top();

    if (bs.first->prev)
        bs.first->prev->next=bs.first;
    else
        track.firstchunk=bs.first;
    
    if (bs.last->next)
        bs.last->next->prev=bs.last;
    else
        track.lastchunk=bs.last;

    undo_stack.pop();
}
