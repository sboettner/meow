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
    for (auto* cl: canvaslayers)
        if (cl->on_leave_notify_event(crossing_event))
            break;

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

    for (auto* cl: canvaslayers)
        if (cl->on_motion_notify_event(&tmpevent))
            break;

    return true;
}


bool Canvas::on_button_press_event(GdkEventButton* event)
{
    GdkEventButton tmpevent=*event;

    tmpevent.x/=hscale;
    tmpevent.y/=vscale;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    for (auto* cl: canvaslayers)
        if (cl->on_button_press_event(&tmpevent))
            break;

    return true;
}


bool Canvas::on_button_release_event(GdkEventButton* event)
{
    GdkEventButton tmpevent=*event;

    tmpevent.x/=hscale;
    tmpevent.y/=vscale;

    if (hadjustment)
        tmpevent.x+=hadjustment->get_value();

    if (vadjustment)
        tmpevent.y+=vadjustment->get_value();

    for (auto* cl: canvaslayers)
        if (cl->on_button_release_event(&tmpevent))
            break;

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

    for (auto* cl: canvaslayers)
        cl->on_draw(cr);

    cr->restore();

    return true;
}


Canvas::CanvasLayer::CanvasLayer(Canvas& canvas):canvas(canvas)
{
    canvas.canvaslayers.push_back(this);
}


Canvas::CanvasLayer::~CanvasLayer()
{
}


bool Canvas::CanvasLayer::on_leave_notify_event(GdkEventCrossing* crossing_event)
{
    return false;
}


bool Canvas::CanvasLayer::on_motion_notify_event(GdkEventMotion* event)
{
    return false;
}


bool Canvas::CanvasLayer::on_button_press_event(GdkEventButton* event)
{
    return false;
}


bool Canvas::CanvasLayer::on_button_release_event(GdkEventButton* event)
{
    return false;
}


Canvas::ItemsLayer::ItemsLayer(Canvas& canvas):CanvasLayer(canvas)
{
}


Canvas::ItemsLayer::~ItemsLayer()
{
}


void Canvas::ItemsLayer::add_item(CanvasItem* item)
{
    canvasitems.push_back(item);
}


void Canvas::ItemsLayer::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    for (auto* ci: canvasitems)
        ci->on_draw(cr);
}


bool Canvas::ItemsLayer::on_leave_notify_event(GdkEventCrossing* crossing_event)
{
    if (focuseditem) {
        focuseditem->hasfocus=false;
        focuseditem=nullptr;

        canvas.queue_draw();
    }

    return false;
}


bool Canvas::ItemsLayer::on_motion_notify_event(GdkEventMotion* event)
{
    if (focuseditem && focuseditem->isdragging)
        focuseditem->on_motion_notify_event(event);
    else {
        bool focuschanged=false;

        if (focuseditem) {
            if (focuseditem->contains_point(event->x, event->y)) return true;

            focuseditem->hasfocus=false;
            focuseditem=nullptr;
            focuschanged=true;
        }

        for (auto* ci: canvasitems) {
            if (ci->contains_point(event->x, event->y)) {
                focuseditem=ci;
                focuseditem->hasfocus=true;
                focuschanged=true;
                break;
            }
        }

        if (focuschanged)
            canvas.queue_draw();
    }

    return false;
}


bool Canvas::ItemsLayer::on_button_press_event(GdkEventButton* event)
{
    return focuseditem && focuseditem->on_button_press_event(event);
}


bool Canvas::ItemsLayer::on_button_release_event(GdkEventButton* event)
{
    return focuseditem && focuseditem->on_button_release_event(event);
}


Canvas::CanvasItem::~CanvasItem()
{
}


bool Canvas::CanvasItem::contains_point(double x, double y)
{
    return x>=extents.x && y>=extents.y && x<extents.x+extents.width && y<extents.y+extents.height;
}


bool Canvas::CanvasItem::on_motion_notify_event(GdkEventMotion* event)
{
    return false;
}


bool Canvas::CanvasItem::on_button_press_event(GdkEventButton* event)
{
    return false;
}


bool Canvas::CanvasItem::on_button_release_event(GdkEventButton* event)
{
    return false;
}
