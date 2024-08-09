#include <memory>
#include <deque>
#include <stdio.h>
#include <math.h>
#include <gtkmm.h>
#include "track.h"
#include "controller.h"
#include "audio.h"
#include "canvas.h"
#include "intonationeditor.h"

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
    track.compute_synth_frames();


    Controller controller(track);


    auto app=Gtk::Application::create(argc, argv);

    auto settings=Gtk::Settings::get_default();
    settings->property_gtk_application_prefer_dark_theme()=true;

    AppWindow wnd(controller);

    return app->run(wnd);
}
