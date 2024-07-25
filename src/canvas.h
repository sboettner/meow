#pragma once

#include <any>
#include <gtkmm.h>

class Canvas:public Gtk::DrawingArea {
public:
    Canvas();

    void set_hadjustment(const Glib::RefPtr<Gtk::Adjustment>&);
    void set_vadjustment(const Glib::RefPtr<Gtk::Adjustment>&);

protected:
    class CanvasLayer {
    public:
        CanvasLayer(Canvas&);
        virtual ~CanvasLayer();

        template<typename T>
        bool has_focus(const T& item) const
        {
            return canvas.has_focus(this, item);
        }

        virtual std::any get_focused_item(double x, double y);
        virtual bool is_focused_item(const std::any&, double x, double y);
        
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) = 0;
        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event);
        virtual void on_button_press_event(const std::any&, GdkEventButton* event);
        virtual void on_button_release_event(const std::any&, GdkEventButton* event);

    protected:
        Canvas&     canvas;
    };

    class CanvasItem {
    public:
        CanvasItem(CanvasLayer&);
        virtual ~CanvasItem();

        bool contains_point(double x, double y);

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) = 0;
        virtual void on_motion_notify_event(GdkEventMotion* event);
        virtual void on_button_press_event(GdkEventButton* event);
        virtual void on_button_release_event(GdkEventButton* event);

        bool is_focused() const
        {
            return layer.has_focus(this);
        }

    protected:
        CanvasLayer&        layer;
        Cairo::Rectangle    extents;
    };

    class ItemsLayer:public CanvasLayer {
    public:
        ItemsLayer(Canvas&);
        virtual ~ItemsLayer();

        virtual std::any get_focused_item(double x, double y);
        virtual bool is_focused_item(const std::any&, double x, double y);

        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&) override;
        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event) override;
        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;
        virtual void on_button_release_event(const std::any&, GdkEventButton* event) override;

        void add_item(CanvasItem*);

    private:
        std::vector<CanvasItem*>    canvasitems;
    };

    void on_size_allocate(Gtk::Allocation&) override;

    bool on_leave_notify_event(GdkEventCrossing* crossing_event) override;
    bool on_motion_notify_event(GdkEventMotion* event) override;
    bool on_button_press_event(GdkEventButton* event) override;
    bool on_button_release_event(GdkEventButton* event) override;
	bool on_key_press_event(GdkEventKey* event) override;
    bool on_scroll_event(GdkEventScroll* event) override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    double                          hscale=1.0;
    double                          vscale=1.0;

    Glib::RefPtr<Gtk::Adjustment>   hadjustment;
    Glib::RefPtr<Gtk::Adjustment>   vadjustment;

    template<typename T>
    bool has_focus(const CanvasLayer* layer, const T* item) const
    {
        return focusedlayer==layer && focuseditem.has_value() && std::any_cast<T*>(focuseditem)==item;
    }

private:
    std::vector<CanvasLayer*>       canvaslayers;

    CanvasLayer*                    focusedlayer=nullptr;
    std::any                        focuseditem;

    void on_scrolled();
};

