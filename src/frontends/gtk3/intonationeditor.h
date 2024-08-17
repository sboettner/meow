#pragma once

#include "track.h"
#include "canvas.h"

class Controller;

class IntonationEditor:public Canvas {
public:
    IntonationEditor(Controller&);
    IntonationEditor(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, Controller& controller);

protected:
    class BackgroundLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        BackgroundLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

    class ChunksLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        ChunksLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event) override;
        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;
        virtual void on_button_release_event(const std::any&, GdkEventButton* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);

    private:
        Cairo::RefPtr<Cairo::ImageSurface> create_chunk_thumbnail(const Track::Chunk* chunk);

        void on_toggle_elastic(Track::Chunk*);
        void on_split_chunk(Track::Chunk*, double);
    };

    class PitchContoursLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        PitchContoursLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

    class PitchControlPointsLayer:public CanvasLayer {
        IntonationEditor&   ie;

    public:
        PitchControlPointsLayer(IntonationEditor& ie):CanvasLayer(ie), ie(ie) {}

        virtual std::any get_focused_item(double x, double y) override;
        virtual bool is_focused_item(const std::any&, double x, double y) override;

        virtual void on_motion_notify_event(const std::any&, GdkEventMotion* event) override;
        virtual void on_button_press_event(const std::any&, GdkEventButton* event) override;
        virtual void on_button_release_event(const std::any&, GdkEventButton* event) override;
    	virtual void on_key_press_event(const std::any&, GdkEventKey* event) override;

    protected:
        virtual void on_draw(const Cairo::RefPtr<Cairo::Context>&);
    };

	bool on_key_press_event(GdkEventKey* event) override;

private:
    Controller&             controller;
    Track&                  track;

    BackgroundLayer         backgroundlayer;
    ChunksLayer             chunkslayer;
    PitchContoursLayer      pitchcontourslayer;
    PitchControlPointsLayer pitchcontrolpointslayer;

    Glib::RefPtr<Gtk::Adjustment>   bpm;
    Glib::RefPtr<Gtk::Adjustment>   beat_subdivisions;
};

