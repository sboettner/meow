#include "canvas.h"

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


Canvas::CanvasItem::~CanvasItem()
{
}


bool Canvas::CanvasItem::contains_point(int x, int y)
{
    return x>=extents.get_x() && y>=extents.get_y() && x<extents.get_x()+extents.get_width() && y<extents.get_y()+extents.get_height();
}

