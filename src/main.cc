#include <memory>
#include <deque>
#include <stdio.h>
#include <math.h>
#include <gtkmm.h>
#include "track.h"
#include "controller.h"
#include "audio.h"
#include "canvas.h"

template<typename T>
T sqr(T x)
{
    return x*x;
}


class UnvoicedChunkAudioProvider:public IAudioProvider {
    const Track&        track;
    const Waveform&     wave;
    const Track::Chunk& chunk;

    int                 frame;
    int                 framecount;
    long                ptr;

public:
    UnvoicedChunkAudioProvider(const Track& track, const Track::Chunk& chunk):track(track), wave(track.get_waveform()), chunk(chunk)
    {
        frame=chunk.beginframe;
        framecount=track.get_frame_count();

        if (frame>0) frame--;   // start one frame earlier to fade in

        ptr=(long) ceil(track.get_frame(frame).position);
    }
    
    virtual unsigned long provide(float* buffer, unsigned long count) override;
};


unsigned long UnvoicedChunkAudioProvider::provide(float* buffer, unsigned long count)
{
    unsigned long done=0;

    while (done<count) {
        if (frame>=framecount-1 || frame>chunk.endframe) break;

        float sample=wave[ptr];

        if (frame<chunk.beginframe) {
            // fade in
            double t0=track.get_frame(frame  ).position;
            double t1=track.get_frame(frame+1).position;

            sample*=(1.0f - cosf(float((ptr-t0)/(t1-t0) * M_PI))) / 2;
        }

        if (frame==chunk.endframe) {
            // fade out
            double t0=track.get_frame(frame  ).position;
            double t1=track.get_frame(frame+1).position;

            sample*=(1.0f + cosf(float((ptr-t0)/(t1-t0) * M_PI))) / 2;
        }

        ptr++;

        buffer[done++]=sample;

        if (ptr>=track.get_frame(frame+1).position) frame++;
    }

    return done;
}


class ChunkSequenceEditor:public Canvas {
public:
    ChunkSequenceEditor(Controller&);

private:
    class ChunkItem:public CanvasItem {
        ChunkSequenceEditor&    cse;
        Track::Chunk&           chunk;

    public:
        ChunkItem(ChunkSequenceEditor&, Track::Chunk&);

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
        virtual void on_button_press_event(GdkEventButton* event) override;
    };

    Controller&     controller;
    Track&          track;

    ItemsLayer      items;

    Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Track::Chunk&);
};


ChunkSequenceEditor::ChunkSequenceEditor(Controller& controller):
    controller(controller),
    track(controller.get_track()),
    items(*this)
{
    set_size_request(-1, 128);
    set_hexpand(true);

    hscale=0.01;
    vscale=128.0;

    for (Track::Chunk* chunk=track.get_first_chunk(); chunk; chunk=chunk->next)
        items.add_item(new ChunkItem(*this, *chunk));
}


Cairo::RefPtr<Cairo::ImageSurface> ChunkSequenceEditor::create_chunk_thumbnail(const Track::Chunk& chunk)
{
    const int width=(chunk.end - chunk.begin) / 100;
    const int height=128;

    Cairo::RefPtr<Cairo::ImageSurface> img=Cairo::ImageSurface::create(Cairo::Format::FORMAT_A8, width, height);

    const int stride=img->get_stride();
    unsigned char* data=img->get_data();

    for (int x=0;x<width;x++) {
        int begin=chunk.begin + (chunk.end-chunk.begin)* x   /width;
        int end  =chunk.begin + (chunk.end-chunk.begin)*(x+1)/width;

        int vals[end-begin];
        int c=0;

        for (int i=begin;i<end;i++)
            vals[c++]=lrintf((1.0f - track.get_waveform()[i])*height/2);
        
        std::sort(vals, vals+c);

        for (int y=0, i=0; y<height; y++) {
            while (i<c && vals[i]<y) i++;
            data[x + y*stride]=1020*i*(c-i)/(c*c);
        }
    }

    img->mark_dirty();

    return img;
}


ChunkSequenceEditor::ChunkItem::ChunkItem(ChunkSequenceEditor& cse, Track::Chunk& chunk):CanvasItem(cse.items), cse(cse), chunk(chunk)
{
    extents=Cairo::Rectangle { double(chunk.begin)*cse.hscale, 0.0, double(chunk.end-chunk.begin)*cse.hscale, cse.vscale };
}


void ChunkSequenceEditor::ChunkItem::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    // TODO: cache this
    Cairo::RefPtr<Cairo::LinearGradient> gradient=Cairo::LinearGradient::create(chunk.begin*0.01, 0.0, chunk.end*0.01, 0.0);

    double factor=is_focused() ? 1.5 : 1.0;

    if (chunk.voiced) {
        gradient->add_color_stop_rgb(0.0, 0.0, 0.25*factor, 0.0625*factor);
        gradient->add_color_stop_rgb(1.0, 0.0, 0.50*factor, 0.1250*factor);
    }
    else {
        gradient->add_color_stop_rgb(0.0, 0.25*factor, 0.0, 0.125*factor);
        gradient->add_color_stop_rgb(1.0, 0.50*factor, 0.0, 0.250*factor);
    }

    cr->set_source(gradient);

    cr->rectangle(chunk.begin*0.01, 0.0, chunk.end*0.01, 128.0);
    cr->fill();

    // TODO: cache this
    Cairo::RefPtr<Cairo::ImageSurface> thumb=cse.create_chunk_thumbnail(chunk);

    if (chunk.voiced)
        cr->set_source_rgb(0.25, 1.0, 0.5);
    else
        cr->set_source_rgb(1.0, 0.25, 0.5);

    cr->mask(thumb, chunk.begin*0.01, 0.0);
}


void ChunkSequenceEditor::ChunkItem::on_button_press_event(GdkEventButton* event)
{
    if (contains_point(event->x, event->y))
        cse.controller.get_audio_device().play(std::make_shared<UnvoicedChunkAudioProvider>(cse.track, chunk));
}


class VoicedChunkAudioProvider:public IAudioProvider {
    const Track&        track;
    const Waveform&     wave;
    const Track::Chunk& chunk;

    long                ptr;
    int                 nextframe;
    double              nextpeakpos;
    double              nextperiod;
    bool                repeatflag=false;

    struct ActiveFrame {
        double  t;
        double  tstep;
        double  tbegin; // start of window
        double  tmid;   // center/peak of window
        double  tend;   // end of window
    };

    std::deque<ActiveFrame> active;

public:
    double              pitchfactor=1.0;

    VoicedChunkAudioProvider(const Track& track, const Track::Chunk& chunk):track(track), wave(track.get_waveform()), chunk(chunk)
    {
        nextframe=chunk.beginframe;
        nextpeakpos=track.get_frame(nextframe).position;
        nextperiod =track.get_frame(nextframe).period;

        ptr=nextframe>0 ? lrint(track.get_frame(nextframe-1).position) : 0;
    }
    
    virtual unsigned long provide(float* buffer, unsigned long count) override;
};


unsigned long VoicedChunkAudioProvider::provide(float* buffer, unsigned long count)
{
    if (terminating && active.empty())
        return 0;
    
    unsigned long done=0;

    double t0=track.get_frame(chunk.beginframe).position;
    double t1=track.get_frame(chunk.  endframe).position;

    const double scale=1.0;

    while (count--) {
        double t=t0 + (t1-t0)*(ptr-chunk.begin)/(chunk.end-chunk.begin);
        if (repeatflag)
            t-=t1-t0;

        if (!terminating) {
            while (t+nextperiod/scale>=nextpeakpos) {
                active.push_back({
                    (t-nextpeakpos)*scale + track.get_frame(nextframe).position,
                    scale,
                    track.get_frame(nextframe-1).position,
                    track.get_frame(nextframe).position,
                    track.get_frame(nextframe+1).position
                });

                nextpeakpos+=nextperiod * pitchfactor;

                while (nextpeakpos>=track.get_frame(nextframe+1).position) {
                    nextframe++;
                    if (nextframe==chunk.endframe) {
                        nextframe=chunk.beginframe;
                        
                        nextpeakpos-=t1-t0;
                        t-=t1-t0;
                        repeatflag=true;
                    }
                }

                nextperiod=track.get_frame(nextframe).period;
            }
        }

        float out=0.0f;

        for (auto& af: active) {
            if (af.t<=af.tbegin)
                ;   // silence before frame
            else if (af.t<=af.tmid) {
                float s=float((af.t-af.tbegin) / (af.tmid-af.tbegin));
                out+=wave(af.t) * (1.0f-cosf(M_PI*s)) * 0.5f;
            }
            else if (af.t<af.tend) {
                float s=float((af.t-af.tmid) / (af.tend-af.tmid));
                out+=wave(af.t) * (1.0f+cosf(M_PI*s)) * 0.5f;
            }

            af.t+=af.tstep;
        }

        buffer[done++]=out;

        while (!active.empty() && active[0].t>=active[0].tend)
            active.pop_front();

        if (++ptr==chunk.end) {
            ptr=chunk.begin;
            repeatflag=false;
        }
    }

    return done;
}


class IntonationEditor:public Canvas {
public:
    IntonationEditor(Controller&);

protected:
    class BackgroundLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        BackgroundLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

    class ChunksLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        ChunksLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event) override;
        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;
        virtual void on_button_release_event(const std::any&, GdkEventButton* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);

    private:
        Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Track::Chunk* chunk);
    };

    class PitchContoursLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        PitchContoursLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

    class PitchControlPointsLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        PitchControlPointsLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event) override;
        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;
        virtual void on_button_release_event(const std::any&, GdkEventButton* event) override;
    	virtual void on_key_press_event(const std::any&, GdkEventKey* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

	bool on_key_press_event(GdkEventKey* event) override;

private:
    Controller&             controller;
    Track&                  track;

    BackgroundLayer         backgroundlayer;
    ChunksLayer             chunkslayer;
    PitchContoursLayer      pitchcontourslayer;
    PitchControlPointsLayer pitchcontrolpointslayer;
};


IntonationEditor::IntonationEditor(Controller& controller):
    controller(controller),
    track(controller.get_track()),
    backgroundlayer(*this),
    chunkslayer(*this),
    pitchcontourslayer(*this),
    pitchcontrolpointslayer(*this)
{
    set_hexpand(true);
    set_vexpand(true);

    hscale=0.01;
    vscale=16.0;
}


bool IntonationEditor::on_key_press_event(GdkEventKey* event)
{
    if (event->keyval==GDK_KEY_z && (event->state&GDK_CONTROL_MASK)) {
        controller.undo();
        queue_draw();
        return true;
    }

    return Canvas::on_key_press_event(event);
}


void IntonationEditor::BackgroundLayer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const double width=ie.track.get_waveform().get_length();

    cr->set_source_rgb(0.125, 0.125, 0.125);
    cr->set_line_width(1.0);

    // draw piano grid lines
    for (int i=0;i<120;i+=12) {
        for (int j: { 1, 3, 6, 8, 10 }) {
            cr->rectangle(0.0, (119-i-j)*ie.vscale, width*ie.hscale, ie.vscale);
            cr->fill();
        }

        for (int j: { 4, 11 }) {
            cr->move_to(0.0, (119-i-j)*ie.vscale-0.5);
            cr->line_to(width*ie.hscale, (119-i-j)*ie.vscale-0.5);
            cr->stroke();
        }
    }
}


std::any IntonationEditor::ChunksLayer::get_focused_item(double x, double y)
{
    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next)
        if (x>=chunk->begin*ie.hscale && x<chunk->end*ie.hscale && y>=(119-chunk->pitch)*ie.vscale && y<=(120-chunk->pitch)*ie.vscale)
            return chunk;

    return {};
}


bool IntonationEditor::ChunksLayer::is_focused_item(const std::any& item, double x, double y)
{
    auto* chunk=std::any_cast<Track::Chunk*>(item);
    return chunk && x>=chunk->begin*ie.hscale && x<chunk->end*ie.hscale && y>=(119-chunk->pitch)*ie.vscale && y<=(120-chunk->pitch)*ie.vscale;
}


void IntonationEditor::ChunksLayer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next) {
        double r, g, b;

        if (chunk->voiced)
            r=0.0, g=0.75, b=0.25;
        else
            r=0.5, g=0.0, b=0.25;
        
        if (!has_focus(chunk)) {
            r*=0.75;
            g*=0.75;
            b*=0.75;
        }

        Cairo::RefPtr<Cairo::LinearGradient> gradient=Cairo::LinearGradient::create(chunk->begin*ie.hscale, 0.0, chunk->end*ie.hscale, 0.0);
        gradient->add_color_stop_rgb(0.0, r*0.5, g*0.5, b*0.5);
        gradient->add_color_stop_rgb(1.0, r, g, b);
        cr->set_source(gradient);

        cr->rectangle(chunk->begin*ie.hscale, (119-chunk->pitch)*ie.vscale, (chunk->end-chunk->begin)*ie.hscale, ie.vscale);
        cr->fill();

        Cairo::RefPtr<Cairo::ImageSurface> thumb=create_chunk_thumbnail(chunk);

        Cairo::RefPtr<Cairo::LinearGradient> gradient2=Cairo::LinearGradient::create(chunk->begin*ie.hscale, 0.0, chunk->end*ie.hscale, 0.0);
        gradient2->add_color_stop_rgb(0.0, r+0.25, g+0.25, b+0.25);
        gradient2->add_color_stop_rgb(1.0, r*1.5+0.5, g*1.5+0.5, b*1.5+0.5);
        cr->set_source(gradient2);

        cr->mask(thumb, chunk->begin*ie.hscale, (119-chunk->pitch)*ie.vscale);
    }
}


void IntonationEditor::ChunksLayer::on_motion_notify_event(const std::any& item, GdkEventMotion* event)
{
    if (event->state & Gdk::BUTTON1_MASK) {
        ie.controller.do_move_chunk(std::any_cast<Track::Chunk*>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);

        ie.queue_draw();
    }
}


void IntonationEditor::ChunksLayer::on_button_press_event(const std::any& item, GdkEventButton* event)
{
    ie.controller.begin_move_chunk(std::any_cast<Track::Chunk*>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);
}


void IntonationEditor::ChunksLayer::on_button_release_event(const std::any& item, GdkEventButton* event)
{
    ie.controller.finish_move_chunk(std::any_cast<Track::Chunk*>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);

    ie.queue_draw();
}


Cairo::RefPtr<Cairo::ImageSurface> IntonationEditor::ChunksLayer::create_chunk_thumbnail(const Track::Chunk* chunk)
{
    const int width=lrint((chunk->end - chunk->begin) * ie.hscale);
    const int height=lrint(ie.vscale);

    Cairo::RefPtr<Cairo::ImageSurface> img=Cairo::ImageSurface::create(Cairo::Format::FORMAT_A8, width, height);

    const int stride=img->get_stride();
    unsigned char* data=img->get_data();

    const double t0=ie.track.get_frame(chunk->beginframe).position;
    const double t1=ie.track.get_frame(chunk->  endframe).position;

    for (int x=0;x<width;x++) {
        int begin=lrint(t0 + (t1-t0)*x/width);
        int end  =lrint(t0 + (t1-t0)*(x+1)/width);

        float sum=0.0;
        for (int i=begin;i<end;i++)
            sum+=sqr(ie.track.get_waveform()[i]);

        sum/=end-begin;
        sum*=1e+7f;
        sum+=1.0f;

        float val=logf(sum);
        int val0=(int) floorf(val);
        
        for (int i=0; i<val0 && i<height; i++)
            data[x + (height-i-1)*stride]=255;

        if (val0<height)
            data[x + (height-val0-1)*stride]=lrintf((val-val0)*255.0f);

        for (int i=val0+1; i<height; i++)
            data[x + (height-i-1)*stride]=0;
    }

    img->mark_dirty();

    return img;
}

/******** Pitch Contours Layer ********/

std::any IntonationEditor::PitchContoursLayer::get_focused_item(double x, double y)
{
    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (!chunk->voiced) continue;

        for (int i=0;i<chunk->pitchcontour.size();i++) {
            Track::PitchContourIterator pci1(chunk, i);

            if (x<=pci1->t*ie.hscale) {
                auto pci0=pci1-1;
                if (!pci0 || x<pci0->t*ie.hscale) return {};

                Track::HermiteInterpolation hi(*pci0, *pci1);
                if (fabs((119.5 - hi(x/ie.hscale))*ie.vscale - y) < 2.5f)
                    return pci0;

                return {};
            }
        }
    }

    return {};
}


bool IntonationEditor::PitchContoursLayer::is_focused_item(const std::any& item, double x, double y)
{
    auto pci0=std::any_cast<Track::PitchContourIterator>(item);
    auto pci1=pci0 + 1;

    Track::HermiteInterpolation hi(*pci0, *pci1);
    return fabs((119.5 - hi(x/ie.hscale))*ie.vscale - y) < 2.5f;
}


void IntonationEditor::PitchContoursLayer::on_button_press_event(const std::any& item, GdkEventButton* event)
{
    if (event->type==GDK_DOUBLE_BUTTON_PRESS && ie.controller.insert_pitch_contour_control_point(std::any_cast<Track::PitchContourIterator>(item), event->x/ie.hscale, float(119.5-event->y/ie.vscale))) {
        canvas.update_focus(event->x, event->y);
        canvas.queue_draw();
    }
}


void IntonationEditor::PitchContoursLayer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->set_source_rgb(0.25, 0.25, 1.0);
    cr->set_line_width(3.0);

    Track::HermiteSplinePoint* lastpt=nullptr;

    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (!chunk->voiced) {
            lastpt=nullptr;
            continue;
        }

        for (int i=0;i<chunk->pitchcontour.size();i++) {
            Track::HermiteSplinePoint* curpt=&chunk->pitchcontour[i];

            if (!lastpt)
                cr->move_to(curpt->t*ie.hscale, (119.5-curpt->y)*ie.vscale);
            else {
                const double t0=lastpt->t;
                const double t1= curpt->t;
                const double dt= curpt->t - lastpt->t;

                cr->curve_to(
                    (t0 + dt/3) * ie.hscale,
                    (119.5 - lastpt->y - lastpt->dy*dt/3)*ie.vscale,
                    (t1 - dt/3) * ie.hscale,
                    (119.5 -  curpt->y +  curpt->dy*dt/3)*ie.vscale,
                    t1 * ie.hscale,
                    (119.5 -  curpt->y)*ie.vscale
                );
            }

            lastpt=curpt;
        }
    }

    cr->stroke();
}


/******** Pitch Control Points Layer ********/

std::any IntonationEditor::PitchControlPointsLayer::get_focused_item(double x, double y)
{
    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (!chunk->voiced) continue;

        for (int i=0;i<chunk->pitchcontour.size();i++) {
            if (sqr(chunk->pitchcontour[i].t*ie.hscale-x)+sqr((119.5-chunk->pitchcontour[i].y)*ie.vscale-y) < 16.0)
                return Track::PitchContourIterator(chunk, i);
        }
    }

    return {};
}


bool IntonationEditor::PitchControlPointsLayer::is_focused_item(const std::any& item, double x, double y)
{
    auto pci=std::any_cast<Track::PitchContourIterator>(item);

    return sqr(pci->t*ie.hscale-x)+sqr((119.5-pci->y)*ie.vscale)<25.0f;
}


void IntonationEditor::PitchControlPointsLayer::on_motion_notify_event(const std::any& item, GdkEventMotion* event)
{
    if (event->state & Gdk::BUTTON1_MASK) {
        ie.controller.do_move_pitch_contour_control_point(std::any_cast<Track::PitchContourIterator>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);
        ie.queue_draw();
    }
}


void IntonationEditor::PitchControlPointsLayer::on_button_press_event(const std::any& item, GdkEventButton* event)
{
    ie.controller.begin_move_pitch_contour_control_point(std::any_cast<Track::PitchContourIterator>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);
}


void IntonationEditor::PitchControlPointsLayer::on_button_release_event(const std::any& item, GdkEventButton* event)
{
    ie.controller.finish_move_pitch_contour_control_point(std::any_cast<Track::PitchContourIterator>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);
}


void IntonationEditor::PitchControlPointsLayer::on_key_press_event(const std::any& item, GdkEventKey* event)
{
    if (event->keyval==GDK_KEY_Delete && ie.controller.delete_pitch_contour_control_point(std::any_cast<Track::PitchContourIterator>(item)))
        canvas.drop_focus();
}


void IntonationEditor::PitchControlPointsLayer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    for (Track::Chunk* chunk=ie.track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (!chunk->voiced) continue;

        for (int i=0;i<chunk->pitchcontour.size();i++) {
            double r;

            if (has_focus(Track::PitchContourIterator(chunk, i))) {
                cr->set_source_rgb(0.5, 0.75, 1.0);
                r=5.0;
            }
            else {
                cr->set_source_rgb(0.125, 0.5, 1.0);
                r=4.0;
            }

            cr->arc(chunk->pitchcontour[i].t*ie.hscale, (119.5-chunk->pitchcontour[i].y)*ie.vscale, r, 0, 2*M_PI);
            cr->fill();
        }
    }
}


class AppWindow:public Gtk::Window {
public:
    AppWindow(Controller& controller);

protected:
    void on_size_allocate(Gtk::Allocation& allocation) override;

private:
    Gtk::Grid           grid;

    IntonationEditor    ie;
    ChunkSequenceEditor cse;

    Gtk::Scrollbar      hscrollbar;
    Gtk::Scrollbar      vscrollbar;
};


AppWindow::AppWindow(Controller& controller):ie(controller), cse(controller)
{
    set_default_size(1024, 768);

    hscrollbar.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    vscrollbar.set_orientation(Gtk::ORIENTATION_VERTICAL);

    grid.attach(ie, 1, 0);
    grid.attach(cse, 1, 1);
    grid.attach(hscrollbar, 1, 2);
    grid.attach(vscrollbar, 0, 0);

    add(grid);

    show_all_children();

    const Waveform& waveform=controller.get_track().get_waveform();

    Glib::RefPtr<Gtk::Adjustment> hadjustment=Gtk::Adjustment::create(0.0, 0.0, waveform.get_length(), waveform.get_samplerate()*0.1, waveform.get_samplerate(), 0.0);
    hscrollbar.set_adjustment(hadjustment);
    ie.set_hadjustment(hadjustment);
    cse.set_hadjustment(hadjustment);

    Glib::RefPtr<Gtk::Adjustment> vadjustment=Gtk::Adjustment::create(0.0, 0.0, 120.0, 1.0, 10.0, 0.0);
    vscrollbar.set_adjustment(vadjustment);
    ie.set_vadjustment(vadjustment);
}


void AppWindow::on_size_allocate(Gtk::Allocation& allocation)
{
    Gtk::Window::on_size_allocate(allocation);

    Gtk::Allocation iealloc=ie.get_allocation();

    hscrollbar.get_adjustment()->set_page_size(iealloc.get_width() / 0.01);
    vscrollbar.get_adjustment()->set_page_size(iealloc.get_height() / 16.0);
}


int main(int argc, char* argv[])
{
    Track track(Waveform::load("testdata/example3.wav"));

    track.compute_frame_decomposition(1024, 24);
    track.refine_frame_decomposition();
    track.detect_chunks();
    track.compute_pitch_contour();


    Controller controller(track);


    auto app=Gtk::Application::create(argc, argv);

    auto settings=Gtk::Settings::get_default();
    settings->property_gtk_application_prefer_dark_theme()=true;

    AppWindow wnd(controller);

    return app->run(wnd);
}
