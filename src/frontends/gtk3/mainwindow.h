#pragma once

#include "intonationeditor.h"


class Controller;


class MainWindow:public Gtk::ApplicationWindow {
public:
    MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, Controller& controller);

protected:
    void on_size_allocate(Gtk::Allocation& allocation) override;

private:
    void on_undo();

    Controller&         controller;
    IntonationEditor*   ie;

    Glib::RefPtr<Gtk::Adjustment>   hadjustment;
    Glib::RefPtr<Gtk::Adjustment>   vadjustment;
};

