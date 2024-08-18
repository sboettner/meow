#include "controller.h"
#include "mainwindow.h"


MainWindow::MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, std::unique_ptr<Project>&& in_project):
    Gtk::ApplicationWindow(obj),
    project(std::move(in_project))
    
{
    add_action("undo", sigc::mem_fun(*this, &MainWindow::on_undo));
    
    controller=std::make_unique<Controller>(*project);

    builder->get_widget_derived<IntonationEditor>("intonation_editor", ie, *controller);

    show_all_children();


    const Waveform& waveform=controller->get_track().get_waveform();

    hadjustment=Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("hadjustment"));
    vadjustment=Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("vadjustment"));

    hadjustment->set_upper(waveform.get_length());
    hadjustment->set_step_increment(waveform.get_samplerate()*0.1);
    hadjustment->set_page_increment(waveform.get_samplerate()*1.0);

    ie->set_hadjustment(hadjustment);
    ie->set_vadjustment(vadjustment);
}


void MainWindow::on_size_allocate(Gtk::Allocation& allocation)
{
    Gtk::Window::on_size_allocate(allocation);

    Gtk::Allocation iealloc=ie->get_allocation();
    
    hadjustment->set_page_size(iealloc.get_width() / 0.01);
    vadjustment->set_page_size(iealloc.get_height() / 16.0);
}


void MainWindow::on_undo()
{
    controller->undo();

    ie->queue_draw();
}


