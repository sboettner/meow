#pragma once

#include <gtkmm.h>

class Canvas:public Gtk::DrawingArea {
public:
    void set_hadjustment(const Glib::RefPtr<Gtk::Adjustment>&);
    void set_vadjustment(const Glib::RefPtr<Gtk::Adjustment>&);

protected:
    class CanvasItem {
    public:
        virtual ~CanvasItem();

        bool contains_point(int x, int y);

        virtual void update_extents() = 0;

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) = 0;
        virtual void on_button_press_event(GdkEventButton* event) = 0;

    protected:
        Gdk::Rectangle  extents;
    };

    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
	bool on_key_press_event(GdkEventKey* event) override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    virtual void draw_background_layer(const Cairo::RefPtr<Cairo::Context>& cr);
    virtual void draw_foreground_layer(const Cairo::RefPtr<Cairo::Context>& cr);

    Glib::RefPtr<Gtk::Adjustment>   hadjustment;
    Glib::RefPtr<Gtk::Adjustment>   vadjustment;

    std::vector<CanvasItem*>        canvasitems;

private:
    void on_scrolled();
};

