#pragma once

#include <gtkmm.h>

class Canvas:public Gtk::DrawingArea {
public:
    Canvas();

    void set_hadjustment(const Glib::RefPtr<Gtk::Adjustment>&);
    void set_vadjustment(const Glib::RefPtr<Gtk::Adjustment>&);

protected:
    class CanvasItem {
    public:
        virtual ~CanvasItem();

        bool contains_point(double x, double y);

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) = 0;
        virtual void on_motion_notify_event(GdkEventMotion* event);
        virtual void on_button_press_event(GdkEventButton* event);
        virtual void on_button_release_event(GdkEventButton* event);

        bool            hasfocus=false;
        bool            isdragging=false;

    protected:
        Cairo::Rectangle    extents;
    };

    void on_size_allocate(Gtk::Allocation&) override;

    bool on_leave_notify_event(GdkEventCrossing* crossing_event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
	bool on_key_press_event(GdkEventKey* event) override;
    bool on_scroll_event(GdkEventScroll* event) override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    virtual void draw_background_layer(const Cairo::RefPtr<Cairo::Context>& cr);
    virtual void draw_foreground_layer(const Cairo::RefPtr<Cairo::Context>& cr);

    double                          hscale=1.0;
    double                          vscale=1.0;

    Glib::RefPtr<Gtk::Adjustment>   hadjustment;
    Glib::RefPtr<Gtk::Adjustment>   vadjustment;

    std::vector<CanvasItem*>        canvasitems;

    CanvasItem*                     focuseditem=nullptr;

private:
    void on_scrolled();
};

