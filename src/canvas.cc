#include "canvas.h"


Canvas::Canvas()
{
	set_can_focus(true);

    add_events(Gdk::LEAVE_NOTIFY_MASK | Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	add_events(Gdk::KEY_PRESS_MASK | Gdk::SCROLL_MASK);
}


void Canvas::set_hadjustment(const Glib::RefPtr<Gtk::Adjustment>& adj)
{
    hadjustment=adj;
    hadjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Canvas::on_scrolled));
}


void Canvas::set_vadjustment(const Glib::RefPtr<Gtk::Adjustment>& adj)
{
    vadjustment=adj;
    vadjustment->signal_value_changed().connect(sigc::mem_fun(*this, &Canvas::on_scrolled));
}


void Canvas::on_scrolled()
{
    queue_draw();
}


void Canvas::on_size_allocate(Gtk::Allocation& allocation)
{
    DrawingArea::on_size_allocate(allocation);
}


bool Canvas::on_leave_notify_event(GdkEventCrossing* crossing_event)
{
    if (focuseditem) {
        focuseditem->hasfocus=false;
        focuseditem=nullptr;

        queue_draw();
    }

    return true;
}


bool Canvas::on_motion_notify_event(GdkEventMotion* event)
{
    GdkEventMotion tmpevent=*event;

    tmpevent.x/=hscale;
    tmpevent.y/=vscale;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    if (focuseditem && focuseditem->isdragging)
        focuseditem->on_motion_notify_event(&tmpevent);
    else {
        bool focuschanged=false;

        if (focuseditem) {
            if (focuseditem->contains_point(tmpevent.x, tmpevent.y)) return true;

            focuseditem->hasfocus=false;
            focuseditem=nullptr;
            focuschanged=true;
        }

        for (auto* ci: canvasitems) {
            if (ci->contains_point(tmpevent.x, tmpevent.y)) {
                focuseditem=ci;
                focuseditem->hasfocus=true;
                focuschanged=true;
                break;
            }
        }

        if (focuschanged)
            queue_draw();
    }

    return true;
}


bool Canvas::on_button_press_event(GdkEventButton* event)
{
    if (!focuseditem)
        return true;

    GdkEventButton tmpevent=*event;

    tmpevent.x/=hscale;
    tmpevent.y/=vscale;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    focuseditem->on_button_press_event(&tmpevent);

    return true;
}


bool Canvas::on_button_release_event(GdkEventButton* event)
{
    if (!focuseditem)
        return true;

    GdkEventButton tmpevent=*event;

    tmpevent.x/=hscale;
    tmpevent.y/=vscale;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    focuseditem->on_button_release_event(&tmpevent);

    return true;
}


bool Canvas::on_key_press_event(GdkEventKey* event)
{
    return true;
}


bool Canvas::on_scroll_event(GdkEventScroll* event)
{
    switch (event->direction) {
    case GDK_SCROLL_LEFT:
        if (hadjustment)
            hadjustment->set_value(hadjustment->get_value() - hadjustment->get_step_increment());
        break;
    case GDK_SCROLL_RIGHT:
        if (hadjustment)
            hadjustment->set_value(hadjustment->get_value() + hadjustment->get_step_increment());
        break;
    case GDK_SCROLL_UP:
        if (vadjustment)
            vadjustment->set_value(vadjustment->get_value() - vadjustment->get_step_increment());
        break;
    case GDK_SCROLL_DOWN:
        if (vadjustment)
            vadjustment->set_value(vadjustment->get_value() + vadjustment->get_step_increment());
        break;
    }

    return true;
}


bool Canvas::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	const Gtk::Allocation allocation = get_allocation();
	auto refStyleContext = get_style_context();
    
	// paint the background
	refStyleContext->render_background(cr, allocation.get_x(), allocation.get_y(), allocation.get_width(), allocation.get_height());

    cr->save();

    if (hadjustment)
        cr->translate(round(-hadjustment->get_value()*hscale), 0.0);

    if (vadjustment)
        cr->translate(0.0, round(-vadjustment->get_value()*vscale));

    draw_background_layer(cr);

    for (auto* ci: canvasitems)
        ci->on_draw(cr);

    draw_foreground_layer(cr);

    cr->restore();

    return true;
}


void Canvas::draw_background_layer(const Cairo::RefPtr<Cairo::Context>& cr)
{
}


void Canvas::draw_foreground_layer(const Cairo::RefPtr<Cairo::Context>& cr)
{
}


Canvas::CanvasItem::~CanvasItem()
{
}


bool Canvas::CanvasItem::contains_point(double x, double y)
{
    return x>=extents.x && y>=extents.y && x<extents.x+extents.width && y<extents.y+extents.height;
}


void Canvas::CanvasItem::on_motion_notify_event(GdkEventMotion* event)
{
}


void Canvas::CanvasItem::on_button_press_event(GdkEventButton* event)
{
}


void Canvas::CanvasItem::on_button_release_event(GdkEventButton* event)
{
}
