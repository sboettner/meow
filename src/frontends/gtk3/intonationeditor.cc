#include "intonationeditor.h"
#include "controller.h"


template<typename T>
T sqr(T x)
{
    return x*x;
}


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


IntonationEditor::IntonationEditor(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, Controller& controller):
    Canvas(obj, builder),
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

    bpm              =Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("bpmadjustment"));
    beat_subdivisions=Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("subdivadjustment"));

    bpm              ->signal_value_changed().connect(sigc::mem_fun(*this, &IntonationEditor::queue_draw));
    beat_subdivisions->signal_value_changed().connect(sigc::mem_fun(*this, &IntonationEditor::queue_draw));
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

    const double beatlength=ie.track.get_samplerate() * 60.0 / ie.bpm->get_value();

    cr->set_source_rgb(0.09375, 0.09375, 0.09375);
    cr->set_line_width(2.0);
    for (int i=0;i*beatlength<ie.track.get_waveform().get_length();i++) {
        double t=lrint(i*beatlength*ie.hscale);
        cr->move_to(t, 0.0);
        cr->line_to(t, 120.0*ie.vscale);
        cr->stroke();
    }

    const int subdiv=(int) ie.beat_subdivisions->get_value();
    const double subdivlength=beatlength / subdiv;

    cr->set_line_width(1.0);
    for (int i=0;i*subdivlength<ie.track.get_waveform().get_length();i++) {
        if (!(i%subdiv)) continue;

        double t=lrint(i*subdivlength*ie.hscale - 0.5) + 0.5;
        cr->move_to(t, 0.0);
        cr->line_to(t, 120.0*ie.vscale);
        cr->stroke();
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
        else if (chunk->elastic)
            r=0.5, g=0.25, b=0.75;
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
        ie.controller.do_move_chunk(
            std::any_cast<Track::Chunk*>(item),
            event->x/ie.hscale,
            119.5-event->y/ie.vscale,
            !!(event->state&Gdk::CONTROL_MASK),
            !!(event->state&Gdk::SHIFT_MASK)
        );

        ie.queue_draw();
    }
}


void IntonationEditor::ChunksLayer::on_button_press_event(const std::any& item, GdkEventButton* event)
{
    auto* chunk=std::any_cast<Track::Chunk*>(item);

    if (event->button==1)
        ie.controller.begin_move_chunk(chunk, event->x/ie.hscale, 119.5-event->y/ie.vscale);

    if (event->button==3) {
        // show context menu
        auto menu=Gtk::make_managed<Gtk::Menu>();

        auto menuitem_elastic=Gtk::make_managed<Gtk::CheckMenuItem>("Elastic");
        menuitem_elastic->set_active(chunk->elastic);
        menuitem_elastic->set_sensitive(!chunk->voiced);
        menuitem_elastic->signal_activate().connect(sigc::bind(sigc::mem_fun(*this, &ChunksLayer::on_toggle_elastic), chunk));
        menu->append(*menuitem_elastic);

        menu->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());

        auto menuitem_split=Gtk::make_managed<Gtk::MenuItem>("Split");
        menuitem_split->signal_activate().connect(sigc::bind(sigc::mem_fun(*this, &ChunksLayer::on_split_chunk), chunk, event->x/ie.hscale));
        menu->append(*menuitem_split);

        menu->show_all();

        menu->popup_at_pointer((GdkEvent*) event);
    }
}


void IntonationEditor::ChunksLayer::on_button_release_event(const std::any& item, GdkEventButton* event)
{
    if (event->button==1)
        ie.controller.finish_move_chunk(std::any_cast<Track::Chunk*>(item), event->x/ie.hscale, 119.5-event->y/ie.vscale);
}


void IntonationEditor::ChunksLayer::on_toggle_elastic(Track::Chunk* chunk)
{
    if (ie.controller.set_elastic(chunk, !chunk->elastic))
        ie.queue_draw();
}


void IntonationEditor::ChunksLayer::on_split_chunk(Track::Chunk* chunk, double t)
{
    if (ie.controller.split_chunk(chunk, t))
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

