#include "canvas.h"


Canvas::Canvas()
{
	set_can_focus(true);

    add_events(Gdk::LEAVE_NOTIFY_MASK | Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	add_events(Gdk::KEY_PRESS_MASK);
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

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

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

    return true;
}


bool Canvas::on_button_press_event(GdkEventButton* event)
{
    GdkEventButton tmpevent=*event;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    for (auto* ci: canvasitems)
        ci->on_button_press_event(&tmpevent);

    return true;
}


bool Canvas::on_key_press_event(GdkEventKey* event)
{
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
        cr->translate(-hadjustment->get_value(), 0.0);

    if (vadjustment)
        cr->translate(0.0, -vadjustment->get_value());

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


bool Canvas::CanvasItem::contains_point(int x, int y)
{
    return x>=extents.get_x() && y>=extents.get_y() && x<extents.get_x()+extents.get_width() && y<extents.get_y()+extents.get_height();
}

