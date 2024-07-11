#include <memory>
#include <stdio.h>
#include <math.h>
#include <gtkmm.h>
#include "track.h"
#include "audio.h"

class ChunkChainEditor:public Gtk::Widget {
public:
    ChunkChainEditor(Track&);

protected:
    void get_preferred_width_vfunc(int& minimum_width, int& natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width, int& minimum_height, int& natural_height) const  override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;
    void get_preferred_width_for_height_vfunc(int height, int& minimum_width, int& natural_width) const override;
  
    void on_size_allocate(Gtk::Allocation& allocation) override;
    void on_realize() override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
	bool on_key_press_event(GdkEventKey* event) override; 	

private:
    Glib::RefPtr<Gdk::Window> m_refGdkWindow;

    Track&  track;

    Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Track::Chunk&);
};


ChunkChainEditor::ChunkChainEditor(Track& track):track(track)
{
    set_has_window(true);
	set_can_focus(true);

    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	add_events(Gdk::KEY_PRESS_MASK);
}


//Discover the total amount of minimum space and natural space needed by
//this widget.
//Let's make this simple example widget always need minimum 60 by 50 and
//natural 100 by 70.
void ChunkChainEditor::get_preferred_width_vfunc(int& minimum_width, int& natural_width) const
{
    minimum_width = 60;
    natural_width = 100;
}

void ChunkChainEditor::get_preferred_height_for_width_vfunc(int /* width */, int& minimum_height, int& natural_height) const
{
    minimum_height = 50;
    natural_height = 70;
}

void ChunkChainEditor::get_preferred_height_vfunc(int& minimum_height, int& natural_height) const
{
    minimum_height = 50;
    natural_height = 70;
}

void ChunkChainEditor::get_preferred_width_for_height_vfunc(int /* height */, int& minimum_width, int& natural_width) const
{
    minimum_width = 60;
    natural_width = 100;
}


void ChunkChainEditor::on_size_allocate(Gtk::Allocation& allocation)
{
    //Do something with the space that we have actually been given:
    //(We will not be given heights or widths less than we have requested, though
    //we might get more)

    //Use the offered allocation for this container:
    set_allocation(allocation);

    if(m_refGdkWindow) {
        m_refGdkWindow->move_resize( allocation.get_x(), allocation.get_y(),
            allocation.get_width(), allocation.get_height() );
    }
}


void ChunkChainEditor::on_realize()
{
    //Do not call base class Gtk::Widget::on_realize().
    //It's intended only for widgets that set_has_window(false).

    set_realized();

    if(!m_refGdkWindow) {
        //Create the GdkWindow:

        GdkWindowAttr attributes;
        memset(&attributes, 0, sizeof(attributes));

        Gtk::Allocation allocation = get_allocation();

        //Set initial position and size of the Gdk::Window:
        attributes.x = allocation.get_x();
        attributes.y = allocation.get_y();
        attributes.width = allocation.get_width();
        attributes.height = allocation.get_height();

        attributes.event_mask = get_events () | Gdk::EXPOSURE_MASK;
        attributes.window_type = GDK_WINDOW_CHILD;
        attributes.wclass = GDK_INPUT_OUTPUT;

        m_refGdkWindow = Gdk::Window::create(get_parent_window(), &attributes, GDK_WA_X | GDK_WA_Y);
        set_window(m_refGdkWindow);

        //make the widget receive expose events
        m_refGdkWindow->set_user_data(gobj());
    }
}


bool ChunkChainEditor::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	const Gtk::Allocation allocation = get_allocation();
	auto refStyleContext = get_style_context();
    
	// paint the background
	refStyleContext->render_background(cr, allocation.get_x(), allocation.get_y(), allocation.get_width(), allocation.get_height());

    const int yfooter=allocation.get_height() - 128;

    cr->set_source_rgb(0.125, 0.125, 0.125);
    cr->set_line_width(1.0);

    // draw piano grid lines
    for (int i=0;i<=24;i+=12) {
        for (int j: { 1, 3, 6, 8, 10 }) {
            cr->rectangle(0.0, yfooter-(i+j+1)*16, allocation.get_width(), 16);
            cr->fill();
        }

        for (int j: { 4, 11 }) {
            cr->move_to(0.0, yfooter-(i+j+1)*16-0.5);
            cr->line_to(allocation.get_width(), yfooter-(i+j+1)*16-0.5);
            cr->stroke();
        }
    }

    for (Track::Chunk* chunk=track.get_first_chunk(); chunk; chunk=chunk->next) {
        Cairo::RefPtr<Cairo::LinearGradient> gradient=Cairo::LinearGradient::create(chunk->begin*0.01, 0.0, chunk->end*0.01, 0.0);

        if (chunk->voiced) {
            gradient->add_color_stop_rgb(0.0, 0.0, 0.25, 0.0625);
            gradient->add_color_stop_rgb(1.0, 0.0, 0.50, 0.1250);
        }
        else {
            gradient->add_color_stop_rgb(0.0, 0.25, 0.0, 0.125);
            gradient->add_color_stop_rgb(1.0, 0.50, 0.0, 0.250);
        }

        cr->set_source(gradient);

        cr->rectangle(chunk->begin*0.01, yfooter, chunk->end*0.01, yfooter+128);
        cr->fill();

        // TODO: cache this
        Cairo::RefPtr<Cairo::ImageSurface> thumb=create_chunk_thumbnail(*chunk);

        if (chunk->voiced)
            cr->set_source_rgb(0.25, 1.0, 0.5);
        else
            cr->set_source_rgb(1.0, 0.25, 0.5);

        cr->mask(thumb, chunk->begin*0.01, yfooter);
    }

    cr->save();
    cr->rectangle(0.0, 0.0, allocation.get_width(), yfooter);
    cr->clip();

    for (Track::Chunk* chunk=track.get_first_chunk(); chunk; chunk=chunk->next) {
        if (chunk->voiced) {
            auto from=track.find_nearest_frame(chunk->begin);
            auto to  =track.find_nearest_frame(chunk->end);

            double avgperiod=(to->position - from->position) / (to - from);
            double avgfreq=track.get_samplerate() / avgperiod;
            double avgpitch=log(avgfreq / 440.0) / M_LN2 * 12.0 + 69.0;

            cr->set_source_rgb(0.0, 0.5, 0.125);
            cr->rectangle(from->position*0.01, yfooter - (avgpitch-35)*16, (to->position-from->position)*0.01, 16);
            cr->fill();

            cr->set_source_rgb(0.25, 1.0, 0.5);
            cr->set_line_width(2.0);

            cr->move_to(from->position*0.01, yfooter - (from->pitch-36+0.5)*16);

            while (from<to) {
                from++;
                if (from->pitch>0)
                    cr->line_to(from->position*0.01, yfooter - (from->pitch-36+0.5)*16);
            }

            cr->stroke();
        }
    }

    cr->restore();

    return true;
}


bool ChunkChainEditor::on_motion_notify_event(GdkEventMotion* event)
{
    return true;
}


bool ChunkChainEditor::on_button_press_event(GdkEventButton* event)
{
    return true;
}


bool ChunkChainEditor::on_key_press_event(GdkEventKey* event)
{
    return true;
}


Cairo::RefPtr<Cairo::ImageSurface> ChunkChainEditor::create_chunk_thumbnail(const Track::Chunk& chunk)
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


class AppWindow:public Gtk::Window {
public:
    AppWindow(Track& track);

private:
    ChunkChainEditor    cce;
};


AppWindow::AppWindow(Track& track):cce(track)
{
    set_default_size(1024, 768);

    add(cce);

    show_all_children();
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

    AppWindow wnd(track);

    return app->run(wnd);
}
