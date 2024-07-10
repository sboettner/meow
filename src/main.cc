#include <vector>
#include <memory>
#include <stdio.h>
#include <math.h>
#include <gtkmm.h>
#include "correlation.h"
#include "waveform.h"
#include "audio.h"

template<typename T>
T sqr(T x)
{
    return x*x;
}


template<typename T>
class Array2D {
    T*  data;
    int ni;
    int nj;

public:
    Array2D(int ni, int nj):ni(ni), nj(nj)
    {
        data=new T[ni*nj];
    }

    ~Array2D()
    {
        delete[] data;
    }

    T& operator()(int i, int j)
    {
        assert(0<=i && i<ni);
        assert(0<=j && j<nj);
        return data[i*nj + j];
    }
};


// analysis frame
struct Frame {
    double  position;
    float   period;     // zero if unvoiced
    float   pitch;
};


struct Chunk {
    Chunk*  prev;
    Chunk*  next;

    int     begin;
    int     end;
    
    bool    voiced;
};


std::vector<Frame> compute_frame_decomposition(const Waveform& wave, int blocksize, int overlap)
{
    std::unique_ptr<ICorrelationService> corrsvc(ICorrelationService::create(blocksize));

    std::vector<Frame> frames;

    frames.push_back({ 0.0, 0.0f, 0.0f });

    double position=blocksize - overlap;

    for (;;) {
        const long offs=lrint(position);
        if (offs+blocksize>=wave.get_length()) break;

        float correlation[blocksize];
        float normalized[blocksize];    // normalized correlation, same as Pearson correlation coefficient

        corrsvc->run(wave+offs-overlap, wave+offs-blocksize+overlap, correlation);

        float y0=0.0f;
        for (int i=-overlap;i<overlap;i++)
            y0+=sqr(wave[offs+i]);
        // FIXME: should be same as correlation[2*overlap]

        float y1=y0;

        normalized[0]=1.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            y0+=sqr(wave[offs+overlap+i-1]);
            y1+=sqr(wave[offs-overlap-i]);

            normalized[i]=correlation[2*overlap+i-1] / sqrt(y0*y1);
        }

        float dtmp=0.0f;
        int zerocrossings=0;

        for (int i=0;i<blocksize/4;i++) {
            // 4th order 1st derivative finite difference approximation
            float d=normalized[i] - 8*normalized[i+1] + 8*normalized[i+3] - normalized[i+4];
            if (d*dtmp<0)
                zerocrossings++;

            dtmp=d;
        }

        if (zerocrossings>blocksize/32) {
            // many zerocrossing of the 1st derivative indicate an unvoiced frame
            printf("\e[35;1m%d zerocrossings\n", zerocrossings);
            frames.push_back({ position, 0.0f, 0.0f });
            position=offs + blocksize/4;
            continue;
        }

        bool pastnegative=false;
        float bestpeakval=0.0f;
        float bestperiod=0.0f;

        for (int i=1;i<blocksize-2*overlap;i++) {
            pastnegative|=normalized[i] < 0;

            if (pastnegative && normalized[i]>normalized[i-1] && normalized[i]>normalized[i+1]) {
                // local maximum, determine exact location by quadratic interpolation
                float a=(normalized[i-1]+normalized[i+1])/2 - normalized[i];
                float b=(normalized[i+1]-normalized[i-1])/2;

                float peakval=normalized[i] - b*b/a/4;
                if (peakval>bestpeakval + 0.01f) {
                    bestperiod=i - b/a/2;
                    bestpeakval=peakval;
                }
            }
        }

        if (bestperiod==0.0f) {
            printf("\e[31;1m%d zerocrossings\n", zerocrossings);
            frames.push_back({ position, 0.0f, 0.0f });
            position=offs + blocksize/4;
        }
        else {
            float freq=wave.get_samplerate() / bestperiod;
            float pitch=logf(freq / 440.0f) / M_LN2 * 12.0f + 69.0f;

            printf("\e[32;1mperiod=%.1f  freq=%.1f  val=%.4f  pitch=%.2f\n", bestperiod, wave.get_samplerate()/bestperiod, bestpeakval, pitch);

            frames.push_back({ position, bestperiod, pitch });
            position+=bestperiod;
        }
    }

    frames.push_back({ (double) wave.get_length(), 0.0f, 0.0f });

    return frames;
}


Chunk* detect_notes(const std::vector<Frame>& frames)
{
    const int n=frames.size();

    struct Node {
        int     pitch;
        int     back;
        float   cost;
    };

    Array2D<Node> nodes(n, 5);

    for (int j=0;j<5;j++) {
        nodes(0, j).pitch=-1;
        nodes(0, j).back=-1;
        nodes(0, j).cost=0.0f;
    }

    for (int i=1;i<n;i++) {
        if (frames[i].pitch>0) {
            int p=lrintf(frames[i].pitch) - 2;

            for (int j=0;j<5;j++, p++) {
                float bestcost=INFINITY;
                int bestback=0;

                for (int k=0;k<5;k++) {
                    float cost=nodes(i-1, k).cost;

                    if (nodes(i-1, k).pitch>=0 && nodes(i-1, k).pitch!=p)
                        cost+=10.0f / abs(nodes(i-1, k).pitch-p); // change penalty
                    
                    cost+=sqr(frames[i].pitch - p);

                    if (cost<bestcost) {
                        bestcost=cost;
                        bestback=k;
                    }
                }

                nodes(i, j).pitch=p;
                nodes(i, j).back=bestback;
                nodes(i, j).cost=bestcost;
            }
        }
        else {
            for (int j=0;j<5;j++) {
                nodes(i, j).pitch=-1;
                nodes(i, j).back=j;
                nodes(i, j).cost=nodes(i-1, j).cost;
            }
        }
    }

    int i=n-2, j=0;
    for (int k=1;k<5;k++)
        if (nodes(i, k).cost < nodes(i, j).cost)
            j=k;

    printf("total cost: %f\n", nodes(i, j).cost);
    
    static const char* notenames[]={ "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };

    Chunk* first=nullptr;

    while (i>=0) {
        int begin=i;
        int pitch=nodes(i, j).pitch;

        while (begin>=0 && nodes(begin, j).pitch==pitch)
            j=nodes(begin--, j).back;
        
        if (pitch>=0)
            printf("\e[32;1mchunk %.2f-%.2f: %s-%d\n", frames[begin+1].position, frames[i+1].position, notenames[pitch%12], pitch/12);
        else
            printf("\e[35;1mchunk %.2f-%.2f: unvoiced\n", frames[begin+1].position, frames[i+1].position);

        Chunk* tmp=new Chunk;
        tmp->prev  =tmp->next=nullptr;
        tmp->begin =lrint(frames[begin+1].position);
        tmp->end   =lrint(frames[i    +1].position);
        tmp->voiced=pitch>=0;

        if (first) {
            first->prev=tmp;
            tmp  ->next=first;
        }

        first=tmp;

        i=begin;
    }

    return first;
}


class ChunkChainEditor:public Gtk::Widget {
public:
    ChunkChainEditor(const Waveform& wave, const std::vector<Frame>& frames, Chunk* chunks);

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

    const Waveform&     wave;
    Chunk*              chunks;
    const std::vector<Frame>&   frames;

    Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Chunk&);

    std::vector<Frame>::const_iterator find_nearest_frame(double position);
};


ChunkChainEditor::ChunkChainEditor(const Waveform& wave, const std::vector<Frame>& frames, Chunk* chunks):wave(wave), frames(frames), chunks(chunks)
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

    for (Chunk* chunk=chunks; chunk; chunk=chunk->next) {
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

    for (Chunk* chunk=chunks; chunk; chunk=chunk->next) {
        if (chunk->voiced) {
            auto from=find_nearest_frame(chunk->begin);
            auto to  =find_nearest_frame(chunk->end);

            double avgperiod=(to->position - from->position) / (to - from);
            double avgfreq=wave.get_samplerate() / avgperiod;
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


Cairo::RefPtr<Cairo::ImageSurface> ChunkChainEditor::create_chunk_thumbnail(const Chunk& chunk)
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
            vals[c++]=lrintf((1.0f - wave[i])*height/2);
        
        std::sort(vals, vals+c);

        for (int y=0, i=0; y<height; y++) {
            while (i<c && vals[i]<y) i++;
            data[x + y*stride]=1020*i*(c-i)/(c*c);
        }
    }

    img->mark_dirty();

    return img;
}


std::vector<Frame>::const_iterator ChunkChainEditor::find_nearest_frame(double position)
{
    auto i=std::lower_bound(frames.begin(), frames.end(), position, [](const Frame& frame, double position) { return frame.position<position; });

    if (i==frames.begin())
        return i;
    
    if (i==frames.end())
        return --i;
    
    if (position-(i-1)->position < i->position-position)
        return --i;
    else
        return i;
}


class AppWindow:public Gtk::Window {
public:
    AppWindow(const Waveform& wave, const std::vector<Frame>& frames, Chunk* chunks);

private:
    ChunkChainEditor    cce;
};


AppWindow::AppWindow(const Waveform& wave, const std::vector<Frame>& frames, Chunk* chunks):cce(wave, frames, chunks)
{
    set_default_size(1024, 768);

    add(cce);

    show_all_children();
}


int main(int argc, char* argv[])
{
    Waveform* wave=Waveform::load("testdata/example2.wav");

    auto frames=compute_frame_decomposition(*wave, 1024, 24);
    
    auto* chunks=detect_notes(frames);


    std::unique_ptr<IAudioDevice> audiodev(IAudioDevice::create());
    

    auto app=Gtk::Application::create(argc, argv);

    auto settings=Gtk::Settings::get_default();
    settings->property_gtk_application_prefer_dark_theme()=true;

    AppWindow wnd(*wave, frames, chunks);

    return app->run(wnd);
}
