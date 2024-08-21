#pragma once

#include "intonationeditor.h"


class Project;
class Controller;


class MainWindow:public Gtk::ApplicationWindow {
public:
    MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, std::unique_ptr<Project>&&);

protected:
    void on_size_allocate(Gtk::Allocation& allocation) override;

private:
    void on_undo();
    void on_save_project();
    void on_export_track();
    void on_bpm_changed();

    std::unique_ptr<Project>        project;
    std::unique_ptr<Controller>     controller;
    
    IntonationEditor*               ie;

    Glib::RefPtr<Gtk::Adjustment>   hadjustment;
    Glib::RefPtr<Gtk::Adjustment>   vadjustment;

    Glib::RefPtr<Gtk::Adjustment>   bpm;
    Glib::RefPtr<Gtk::Adjustment>   beat_subdivisions;
};

