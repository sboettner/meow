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
    if (!chunk->elastic) {
        while (first->prev && first->prev->elastic)
            first=first->prev;
        while (last->next && last->next->elastic)
            last=last->next;
    }

    curchunkbackup=backup(first, last, curchunk);

    moving_time_offset =chunk->begin - t;

    audioprovider=std::shared_ptr<IAudioProvider>(create_render_audio_provider(track, chunk, chunk->next->next));
    audiodev->play(audioprovider);
}


void Controller::do_move_chunk(Track::Chunk* chunk, double t, float y, bool move_pitch_contour)
{
    if (!curchunk->elastic) {
        const int len=curchunk->end - curchunk->begin;
        curchunk->begin=lrint(moving_time_offset + t);
        curchunk->end=curchunk->begin + len;

        double firstt=undo_stack.top().first->begin;
        double lastt =undo_stack.top().last ->end;

        for (Track::Chunk *cur=curchunk->prev, *bup=curchunkbackup->prev; cur && bup && cur!=bup; cur=cur->prev, bup=bup->prev) {
            cur->begin=lrint(lerp(firstt, (double) curchunk->begin, unlerp(firstt, (double) curchunkbackup->begin, (double) bup->begin)));
            cur->end=cur->next->begin;

            for (int i=0;i<cur->pitchcontour.size();i++)
                cur->pitchcontour[i].t=lerp(firstt, (double) curchunk->begin, unlerp(firstt, (double) curchunkbackup->begin, bup->pitchcontour[i].t));
        }

        for (Track::Chunk *cur=curchunk->next, *bup=curchunkbackup->next; cur && bup && cur!=bup; cur=cur->next, bup=bup->next) {
            cur->begin=cur->prev->end;
            cur->end=lrint(lerp((double) curchunk->end, lastt, unlerp((double) curchunkbackup->end, lastt, (double) bup->end)));

            for (int i=0;i<cur->pitchcontour.size();i++)
                cur->pitchcontour[i].t=lerp((double) curchunk->end, lastt, unlerp((double) curchunkbackup->end, lastt, bup->pitchcontour[i].t));
        }
    }

    curchunk->pitch=lrintf(y);

    for (int i=0;i<chunk->pitchcontour.size();i++)
        curchunk->pitchcontour[i].y=curchunkbackup->pitchcontour[i].y + (move_pitch_contour ? curchunk->pitch-curchunkbackup->pitch : 0);

    for (int i=0;i<chunk->pitchcontour.size();i++) {
        Track::PitchContourIterator pci(curchunk, i);
        Track::update_akima_slope(pci-2, pci-1, pci, pci+1, pci+2);
    }
}


void Controller::finish_move_chunk(Track::Chunk* chunk, double t, float y)
{
    audioprovider->terminate();
    audioprovider=nullptr;

    curchunk=curchunkbackup=nullptr;

    track.compute_synth_frames();
}


bool Controller::split_chunk(Track::Chunk* chunk, double t)
{
    backup(chunk, chunk);

    double s=(t-chunk->begin) / (chunk->end-chunk->begin);
    int atframe=lrint(chunk->beginframe*(1.0-s) + chunk->endframe*s);
    if (atframe<=chunk->beginframe || atframe>=chunk->endframe)
        return false;

    Track::Chunk* newchunk=new Track::Chunk(*chunk);

    chunk->end=newchunk->begin=lrint(t);
    chunk->endframe=newchunk->beginframe=atframe;

    chunk->next=newchunk;
    newchunk->prev=chunk;

    if (newchunk->next)
        newchunk->next->prev=newchunk;
    else
        track.lastchunk=newchunk;

    chunk->pitchcontour.erase(
        std::remove_if(
            chunk->pitchcontour.begin(),
            chunk->pitchcontour.end(),
            [t] (const Track::HermiteSplinePoint& hsp) { return hsp.t>=t; }
        ),
        chunk->pitchcontour.end()
    );

    newchunk->pitchcontour.erase(
        std::remove_if(
            newchunk->pitchcontour.begin(),
            newchunk->pitchcontour.end(),
            [t] (const Track::HermiteSplinePoint& hsp) { return hsp.t<t; }
        ),
        newchunk->pitchcontour.end()
    );

    return true;
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
    track.compute_synth_frames();
}


bool Controller::insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y)
{
    auto& pc=after.get_chunk()->pitchcontour;

    pc.insert(pc.begin()+after.get_index()+1, Track::HermiteSplinePoint { t, y, 0.0f });

    Track::update_akima_slope(after-1, after, after+1, after+2, after+3);

    track.compute_synth_frames();

    return true;
}


bool Controller::delete_pitch_contour_control_point(Track::PitchContourIterator cp)
{
    if (cp-1 && cp+1) {
        cp.get_chunk()->pitchcontour.erase(cp.get_chunk()->pitchcontour.begin() + cp.get_index());

        track.compute_synth_frames();
        return true;
    }
    else
        return false;
}


bool Controller::set_elastic(Track::Chunk* chunk, bool elastic)
{
    backup(chunk, chunk);

    chunk->elastic=elastic;

    return true;
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

    track.compute_synth_frames();
}
