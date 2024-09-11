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


Controller::Controller(Project& project):project(project)
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

    curchunk=backup(first, last, chunk);

    moving_time_offset =chunk->begin - t;

    audioprovider=create_render_audio_provider(get_track(), curchunk, curchunk->next->next);
    audiodev->play(audioprovider);
}


void Controller::do_move_chunk(double t, float y, bool move_pitch_contour, bool move_time)
{
    if (!curchunk->elastic) {
        double firstt=undo_stack.top().first->begin;
        double lastt =undo_stack.top().last ->end;

        // FIXME: do some more sensible clamping here -- this is just sufficient to avoid chunk lengths to become negative and cause a crash subsequently
        const double len=curchunk->backup->end - curchunk->backup->begin;
        curchunk->begin=move_time ? std::clamp(moving_time_offset + t, firstt+1.0, lastt-len-1.0) : curchunk->backup->begin;
        curchunk->end=curchunk->begin + len;

        for (Track::Chunk *cur=curchunk->prev, *bup=curchunk->backup->prev; cur && bup && cur!=bup; cur=cur->prev, bup=bup->prev) {
            cur->begin=lerp(firstt, curchunk->begin, unlerp(firstt, curchunk->backup->begin, bup->begin));
            cur->end=cur->next->begin;

            for (int i=0;i<cur->pitchcontour.size();i++)
                cur->pitchcontour[i].t=lerp(firstt, curchunk->begin, unlerp(firstt, curchunk->backup->begin, bup->pitchcontour[i].t));
        }

        for (Track::Chunk *cur=curchunk->next, *bup=curchunk->backup->next; cur && bup && cur!=bup; cur=cur->next, bup=bup->next) {
            cur->begin=cur->prev->end;
            cur->end=lerp(curchunk->end, lastt, unlerp(curchunk->backup->end, lastt, bup->end));

            for (int i=0;i<cur->pitchcontour.size();i++)
                cur->pitchcontour[i].t=lerp(curchunk->end, lastt, unlerp(curchunk->backup->end, lastt, bup->pitchcontour[i].t));
        }
    }

    curchunk->pitch=lrintf(y);

    for (int i=0;i<curchunk->pitchcontour.size();i++)
        curchunk->pitchcontour[i].y=curchunk->backup->pitchcontour[i].y + (move_pitch_contour ? curchunk->pitch-curchunk->backup->pitch : 0);

    for (int i=0;i<curchunk->pitchcontour.size();i++) {
        Track::PitchContourIterator pci(curchunk, i);
        Track::update_akima_slope(pci-2, pci-1, pci, pci+1, pci+2);
    }
}


void Controller::finish_move_chunk(double t, float y)
{
    audioprovider->terminate();
    audioprovider=nullptr;

    curchunk=nullptr;

    get_track().compute_synth_frames();
}


void Controller::begin_move_edge(Track::Chunk* chunk, double t)
{
    curchunk=backup(chunk->prev, chunk, chunk);

    moving_time_offset =chunk->begin - t;
}


void Controller::do_move_edge(double t)
{
    curchunk->prev->end=curchunk->begin=std::clamp(moving_time_offset+t, curchunk->prev->begin+1.0, curchunk->end-1.0);

    for (int i=0;i<curchunk->prev->pitchcontour.size();i++) {
        curchunk->prev->pitchcontour[i].t=lerp(
            curchunk->prev->begin,
            curchunk->prev->end,
            unlerp(
                curchunk->backup->prev->begin,
                curchunk->backup->prev->end,
                curchunk->backup->prev->pitchcontour[i].t
            )
        );
    }

    for (int i=0;i<curchunk->pitchcontour.size();i++) {
        curchunk->pitchcontour[i].t=lerp(
            curchunk->begin,
            curchunk->end,
            unlerp(
                curchunk->backup->begin,
                curchunk->backup->end,
                curchunk->backup->pitchcontour[i].t
            )
        );
    }
}


void Controller::finish_move_edge(double t)
{
    curchunk=nullptr;

    get_track().compute_synth_frames();
}


bool Controller::split_chunk(Track::Chunk* chunk, double t)
{
    chunk=backup(chunk, chunk, chunk);

    double s=(t-chunk->begin) / (chunk->end-chunk->begin);
    int atframe=lrint(chunk->beginframe*(1.0-s) + chunk->endframe*s);
    if (atframe<=chunk->beginframe || atframe>=chunk->endframe)
        return false;

    Track::Chunk* newchunk=new Track::Chunk(*chunk);

    chunk->end=newchunk->begin=t;
    chunk->endframe=newchunk->beginframe=atframe;

    chunk->next=newchunk;
    newchunk->prev=chunk;

    if (newchunk->next)
        newchunk->next->prev=newchunk;
    else
        get_track().lastchunk=newchunk;

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


bool Controller::merge_chunks(Track::Chunk* chunk)
{
    chunk=backup(chunk->prev, chunk, chunk);

    Track::Chunk* removed=chunk;
    chunk=chunk->prev;

    if (removed->next)
        removed->next->prev=chunk;
    else
        get_track().lastchunk=chunk;
    
    chunk->next=removed->next;
    chunk->end=removed->end;
    chunk->endframe=removed->endframe;

    for (auto& pc: removed->pitchcontour)
        chunk->pitchcontour.push_back(pc);
    
    delete removed;

    return true;
}


void Controller::begin_move_pitch_contour_control_point(Track::PitchContourIterator cp, double t, float y)
{
    curpci=Track::PitchContourIterator(backup(cp.get_chunk(), cp.get_chunk(), cp.get_chunk()), cp.get_index());
}


void Controller::do_move_pitch_contour_control_point(double t, float y)
{
    if (curpci-1 && curpci+1)
        curpci->t=std::clamp(t, (curpci-1)->t+48.0, (curpci+1)->t-48.0);

    curpci->y=y;

    Track::update_akima_slope(curpci-4, curpci-3, curpci-2, curpci-1, curpci);
    Track::update_akima_slope(curpci-3, curpci-2, curpci-1, curpci, curpci+1);
    Track::update_akima_slope(curpci-2, curpci-1, curpci, curpci+1, curpci+2);
    Track::update_akima_slope(curpci-1, curpci, curpci+1, curpci+2, curpci+3);
    Track::update_akima_slope(curpci, curpci+1, curpci+2, curpci+3, curpci+4);
}


void Controller::finish_move_pitch_contour_control_point(double t, float y)
{
    get_track().compute_synth_frames();
}


bool Controller::insert_pitch_contour_control_point(Track::PitchContourIterator after, double t, float y)
{
    auto& pc=after.get_chunk()->pitchcontour;

    pc.insert(pc.begin()+after.get_index()+1, Track::HermiteSplinePoint { t, y, 0.0f });

    Track::update_akima_slope(after-1, after, after+1, after+2, after+3);

    get_track().compute_synth_frames();

    return true;
}


bool Controller::delete_pitch_contour_control_point(Track::PitchContourIterator cp)
{
    if (cp-1 && cp+1) {
        cp.get_chunk()->pitchcontour.erase(cp.get_chunk()->pitchcontour.begin() + cp.get_index());

        get_track().compute_synth_frames();
        return true;
    }
    else
        return false;
}


bool Controller::set_elastic(Track::Chunk* chunk, bool elastic)
{
    chunk=backup(chunk, chunk, chunk);

    chunk->elastic=elastic;

    return true;
}


Track::Chunk* Controller::backup(Track::Chunk* first, Track::Chunk* last, Track::Chunk* mid)
{
    undo_stack.push({ first, last });

    Track::Chunk* midcopy=nullptr;

    Track::Chunk* firstcopy=new Track::Chunk(*first);
    firstcopy->backup=first;

    if (firstcopy->prev)
        firstcopy->prev->next=firstcopy;
    else
        get_track().firstchunk=firstcopy;

    if (first==mid)
        midcopy=firstcopy;
    
    while (first!=last) {
        first=first->next;

        Track::Chunk* copy=new Track::Chunk(*first);
        copy->backup=first;

        firstcopy->next=copy;
        copy->prev=firstcopy;

        firstcopy=copy;

        if (first==mid)
            midcopy=firstcopy;
    }

    if (firstcopy->next)
        firstcopy->next->prev=firstcopy;
    else
        get_track().lastchunk=firstcopy;

    return midcopy;
}


void Controller::undo()
{
    if (undo_stack.empty()) return;

    BackupState& bs=undo_stack.top();

    if (bs.first->prev)
        bs.first->prev->next=bs.first;
    else
        get_track().firstchunk=bs.first;
    
    if (bs.last->next)
        bs.last->next->prev=bs.last;
    else
        get_track().lastchunk=bs.last;

    undo_stack.pop();

    get_track().compute_synth_frames();
}
