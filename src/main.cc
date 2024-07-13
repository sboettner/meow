#include <memory>
#include <stdio.h>
#include <math.h>
#include <gtkmm.h>
#include "track.h"
#include "audio.h"
#include "canvas.h"

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
    ChunkSequenceEditor(Track&, IAudioDevice&);

private:
    class ChunkItem:public CanvasItem {
        ChunkSequenceEditor&    cse;
        Track::Chunk&           chunk;

    public:
        ChunkItem(ChunkSequenceEditor&, Track::Chunk&);

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
        virtual void on_button_press_event(GdkEventButton* event);
    };

    Track&  track;
    IAudioDevice&   audiodev;

    Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Track::Chunk&);
};


ChunkSequenceEditor::ChunkSequenceEditor(Track& track, IAudioDevice& audiodev):track(track), audiodev(audiodev)
{
    set_size_request(-1, 128);
    set_hexpand(true);

    hscale=0.01;
    vscale=128.0;

    for (Track::Chunk* chunk=track.get_first_chunk(); chunk; chunk=chunk->next)
        canvasitems.push_back(new ChunkItem(*this, *chunk));
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


ChunkSequenceEditor::ChunkItem::ChunkItem(ChunkSequenceEditor& cse, Track::Chunk& chunk):cse(cse), chunk(chunk)
{
    extents=Cairo::Rectangle { double(chunk.begin), 0.0, double(chunk.end-chunk.begin), 1.0 };
}


void ChunkSequenceEditor::ChunkItem::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    // TODO: cache this
    Cairo::RefPtr<Cairo::LinearGradient> gradient=Cairo::LinearGradient::create(chunk.begin*0.01, 0.0, chunk.end*0.01, 0.0);

    double factor=hasfocus ? 1.5 : 1.0;

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
        cse.audiodev.play(std::make_shared<UnvoicedChunkAudioProvider>(cse.track, chunk));
}


class IntonationEditor:public Canvas {
public:
    IntonationEditor(Track&, IAudioDevice&);

protected:
    class ChunkItem:public CanvasItem {
        IntonationEditor&   ie;
        Track::Chunk&       chunk;

    public:
        ChunkItem(IntonationEditor&, Track::Chunk&);

        void on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
        void on_motion_notify_event(GdkEventMotion* event) override;
        void on_button_press_event(GdkEventButton* event) override;
        void on_button_release_event(GdkEventButton* event) override;
    };

    void draw_background_layer(const Cairo::RefPtr<Cairo::Context>& cr) override;
    void draw_foreground_layer(const Cairo::RefPtr<Cairo::Context>& cr) override;

private:
    Track&  track;
    IAudioDevice&   audiodev;
};


IntonationEditor::ChunkItem::ChunkItem(IntonationEditor& ie, Track::Chunk& chunk):ie(ie), chunk(chunk)
{
    const auto* from=&ie.track.get_frame(chunk.beginframe);
    const auto* to  =&ie.track.get_frame(chunk.endframe);

    extents=Cairo::Rectangle { from->position, 119.0-chunk.avgpitch, to->position-from->position, 1.0 };
}


void IntonationEditor::ChunkItem::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const auto* from=&ie.track.get_frame(chunk.beginframe);
    const auto* to  =&ie.track.get_frame(chunk.endframe);

    if (hasfocus)
        cr->set_source_rgb(0.0, 0.75, 0.25);
    else
        cr->set_source_rgb(0.0, 0.5, 0.125);

    cr->rectangle(from->position*ie.hscale, (119-chunk.newpitch)*ie.vscale, (to->position-from->position)*ie.hscale, ie.vscale);
    cr->fill();

    const double pitchdelta=chunk.newpitch-chunk.avgpitch;

    cr->set_source_rgb(0.25, 1.0, 0.5);
    cr->set_line_width(2.0);

    cr->move_to(from->position*ie.hscale, (119.5-from->pitch-pitchdelta)*ie.vscale);

    while (from<to) {
        from++;
        if (from->pitch>0)
            cr->line_to(from->position*ie.hscale, (119.5-from->pitch-pitchdelta)*ie.vscale);
    }

    cr->stroke();
}


void IntonationEditor::ChunkItem::on_motion_notify_event(GdkEventMotion* event)
{
    if (isdragging) {
        chunk.newpitch=119.5-event->y;

        extents.y=119.0-chunk.newpitch;

        ie.queue_draw();
    }
}


void IntonationEditor::ChunkItem::on_button_press_event(GdkEventButton* event)
{
    isdragging=true;
}


void IntonationEditor::ChunkItem::on_button_release_event(GdkEventButton* event)
{
    isdragging=false;
}


IntonationEditor::IntonationEditor(Track& track, IAudioDevice& audiodev):track(track), audiodev(audiodev)
{
    set_hexpand(true);
    set_vexpand(true);

    hscale=0.01;
    vscale=16.0;

    for (Track::Chunk* chunk=track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (chunk->voiced) {
            canvasitems.push_back(new ChunkItem(*this, *chunk));
        }
    }
}


void IntonationEditor::draw_background_layer(const Cairo::RefPtr<Cairo::Context>& cr)
{
    const double width=track.get_waveform().get_length();

    cr->set_source_rgb(0.125, 0.125, 0.125);
    cr->set_line_width(1.0);

    // draw piano grid lines
    for (int i=0;i<120;i+=12) {
        for (int j: { 1, 3, 6, 8, 10 }) {
            cr->rectangle(0.0, (119-i-j)*vscale, width*hscale, vscale);
            cr->fill();
        }

        for (int j: { 4, 11 }) {
            cr->move_to(0.0, (119-i-j)*vscale-0.5);
            cr->line_to(width*hscale, (119-i-j)*vscale-0.5);
            cr->stroke();
        }
    }
}


void IntonationEditor::draw_foreground_layer(const Cairo::RefPtr<Cairo::Context>& cr)
{
}


class AppWindow:public Gtk::Window {
public:
    AppWindow(Track& track, IAudioDevice& audiodev);

protected:
    void on_size_allocate(Gtk::Allocation& allocation) override;

private:
    Gtk::Grid           grid;

    IntonationEditor    ie;
    ChunkSequenceEditor cse;

    Gtk::Scrollbar      hscrollbar;
    Gtk::Scrollbar      vscrollbar;
};


AppWindow::AppWindow(Track& track, IAudioDevice& audiodev):ie(track, audiodev), cse(track, audiodev)
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

    Glib::RefPtr<Gtk::Adjustment> hadjustment=Gtk::Adjustment::create(0.0, 0.0, track.get_waveform().get_length(), track.get_samplerate()*0.1, track.get_samplerate(), 0.0);
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
    Track track(Waveform::load("testdata/example2.wav"));

    track.compute_frame_decomposition(1024, 24);
    track.detect_chunks();


    std::unique_ptr<IAudioDevice> audiodev(IAudioDevice::create());
    

    auto app=Gtk::Application::create(argc, argv);

    auto settings=Gtk::Settings::get_default();
    settings->property_gtk_application_prefer_dark_theme()=true;

    AppWindow wnd(track, *audiodev);

    return app->run(wnd);
}
