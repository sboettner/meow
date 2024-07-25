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
    if (focusedlayer) {
        focusedlayer=nullptr;
        focuseditem.reset();

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

    bool focuschanged=false;

    if (!focusedlayer || (!(event->state&Gdk::BUTTON1_MASK) &&  !focusedlayer->is_focused_item(focuseditem, tmpevent.x, tmpevent.y))) {
        if (focusedlayer) {
            focuschanged=true;
            focusedlayer=nullptr;
            focuseditem.reset();
        }

        for (auto* cl: canvaslayers) {
            if (std::any fi=cl->get_focused_item(tmpevent.x, tmpevent.y); fi.has_value()) {
                focuschanged=true;
                focusedlayer=cl;
                focuseditem=fi;
            }
        }
    }

    if (focuschanged)
        queue_draw();

    if (focusedlayer)
        focusedlayer->on_motion_notify_event(focuseditem, &tmpevent);

    return true;
}


bool Canvas::on_button_press_event(GdkEventButton* event)
{
    if (focusedlayer) {
        GdkEventButton tmpevent=*event;

        tmpevent.x/=hscale;
        tmpevent.y/=vscale;

        if (hadjustment)
            tmpevent.x+=hadjustment->get_value();

        if (vadjustment)
            tmpevent.y+=vadjustment->get_value();

        focusedlayer->on_button_press_event(focuseditem, &tmpevent);
    }

    return true;
}


bool Canvas::on_button_release_event(GdkEventButton* event)
{
    if (focusedlayer) {
        GdkEventButton tmpevent=*event;

        tmpevent.x/=hscale;
        tmpevent.y/=vscale;

        if (hadjustment)
            tmpevent.x+=hadjustment->get_value();

        if (vadjustment)
            tmpevent.y+=vadjustment->get_value();

        focusedlayer->on_button_release_event(focuseditem, &tmpevent);
    }

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


std::any Canvas::CanvasLayer::get_focused_item(double x, double y)
{
    return {};
}


bool Canvas::CanvasLayer::is_focused_item(const std::any&, double x, double y)
{
    return false;
}


void Canvas::CanvasLayer::on_motion_notify_event(const std::any&, GdkEventMotion* event)
{
}


void Canvas::CanvasLayer::on_button_press_event(const std::any&, GdkEventButton* event)
{
}


void Canvas::CanvasLayer::on_button_release_event(const std::any&, GdkEventButton* event)
{
}


Canvas::ItemsLayer::ItemsLayer(Canvas& canvas):CanvasLayer(canvas)
{
}


Canvas::ItemsLayer::~ItemsLayer()
{
}


std::any Canvas::ItemsLayer::get_focused_item(double x, double y)
{
    for (auto* ci: canvasitems)
        if (ci->contains_point(x, y))
            return ci;
    
    return {};
}


bool Canvas::ItemsLayer::is_focused_item(const std::any& item, double x, double y)
{
    CanvasItem* ci=std::any_cast<CanvasItem*>(item);
    return ci && ci->contains_point(x, y);
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


void Canvas::ItemsLayer::on_motion_notify_event(const std::any& item, GdkEventMotion* event)
{
    if (CanvasItem* ci=std::any_cast<CanvasItem*>(item); ci)
        ci->on_motion_notify_event(event);
}


void Canvas::ItemsLayer::on_button_press_event(const std::any& item, GdkEventButton* event)
{
    if (CanvasItem* ci=std::any_cast<CanvasItem*>(item); ci)
        ci->on_button_press_event(event);
}


void Canvas::ItemsLayer::on_button_release_event(const std::any& item, GdkEventButton* event)
{
    if (CanvasItem* ci=std::any_cast<CanvasItem*>(item); ci)
        ci->on_button_release_event(event);
}


Canvas::CanvasItem::CanvasItem(CanvasLayer& layer):layer(layer)
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
