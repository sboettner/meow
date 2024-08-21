#include <fstream>
#include "controller.h"
#include "mainwindow.h"
#include "asyncoperationwindow.h"


MainWindow::MainWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, std::unique_ptr<Project>&& in_project):
    Gtk::ApplicationWindow(obj),
    project(std::move(in_project))
    
{
    add_action("undo", sigc::mem_fun(*this, &MainWindow::on_undo));
    add_action("saveproject", sigc::mem_fun(*this, &MainWindow::on_save_project));
    add_action("exporttrack", sigc::mem_fun(*this, &MainWindow::on_export_track));
    
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

    bpm              =Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("bpmadjustment"));
    beat_subdivisions=Glib::RefPtr<Gtk::Adjustment>::cast_dynamic(builder->get_object("subdivadjustment"));

    bpm              ->set_value(project->bpm);
    beat_subdivisions->set_value(project->beat_subdivisions);

    bpm              ->signal_value_changed().connect(sigc::mem_fun(*this, &MainWindow::on_bpm_changed));
    beat_subdivisions->signal_value_changed().connect(sigc::mem_fun(*this, &MainWindow::on_bpm_changed));
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


void MainWindow::on_save_project()
{
    Gtk::FileChooserDialog dlg(*this, "Save Project", Gtk::FILE_CHOOSER_ACTION_SAVE);

    dlg.add_button(Gtk::StockID("gtk-ok"), Gtk::RESPONSE_OK);
    dlg.add_button(Gtk::StockID("gtk-cancel"), Gtk::RESPONSE_CANCEL);

    auto filter_proj=Gtk::FileFilter::create();
    filter_proj->set_name("Project Files");
    filter_proj->add_pattern("*.meow");
    dlg.add_filter(filter_proj);

    if (dlg.run()==Gtk::RESPONSE_OK) {
        std::string filename=dlg.get_filename();
        if (filename.find('.')==std::string::npos)
            filename.append(".meow");

        std::ofstream ofs(filename, std::ios::binary);
        project->write(ofs);
    }
}


void MainWindow::on_export_track()
{
    Gtk::FileChooserDialog dlg(*this, "Save Project", Gtk::FILE_CHOOSER_ACTION_SAVE);

    dlg.add_button(Gtk::StockID("gtk-ok"), Gtk::RESPONSE_OK);
    dlg.add_button(Gtk::StockID("gtk-cancel"), Gtk::RESPONSE_CANCEL);

    auto filter_proj=Gtk::FileFilter::create();
    filter_proj->set_name("Wave Files");
    filter_proj->add_mime_type("audio/wav");
    dlg.add_filter(filter_proj);

    if (dlg.run()==Gtk::RESPONSE_OK) {
        class ExportTrackOperationWindow:public AsyncOperationWindow {
            std::string                 filename;
            const Track&                track;

        public:
            ExportTrackOperationWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, const std::string& filename, const Track& track):
                AsyncOperationWindow(obj, builder),
                filename(filename),
                track(track)
            {
            }

            void on_run() override
            {
                track.export_to_wave_file(filename.c_str(), *this);
            }

            void on_finished() override
            {
            }
        };

        auto builder=Gtk::Builder::create_from_resource("/opt/meow/asyncoperationwindow.ui");

        ExportTrackOperationWindow* asyncopwnd;
        builder->get_widget_derived("asyncopwnd", asyncopwnd, dlg.get_filename(), *project->tracks[0]);

        asyncopwnd->run();
    }
}


void MainWindow::on_bpm_changed()
{
    project->bpm=bpm->get_value();
    project->beat_subdivisions=beat_subdivisions->get_value();
}
