#include <memory>
#include "project.h"
#include "controller.h"
#include "audio.h"
#include "mainwindow.h"


class App:public Gtk::Application {
public:
    App(int argc, char* argv[]);

    void on_startup() override;
    void on_activate() override;
    void on_load_wave();

private:
    Gtk::ApplicationWindow* welcomedlg=nullptr;
};


App::App(int argc, char* argv[]):Gtk::Application(argc, argv)
{
    add_action("loadwave", sigc::mem_fun(*this, &App::on_load_wave));
}


void App::on_startup()
{
    Gtk::Application::on_startup();

    auto builder=Gtk::Builder::create_from_resource("/opt/meow/mainmenu.ui");

    set_menubar(Glib::RefPtr<Gio::MenuModel>::cast_dynamic(builder->get_object("mainmenu")));
}


void App::on_activate()
{
    Gtk::Application::on_activate();

    auto builder=Gtk::Builder::create_from_resource("/opt/meow/welcomedialog.ui");

    builder->get_widget("welcomedlg", welcomedlg);

    welcomedlg->show_all();

    add_window(*welcomedlg);
}


void App::on_load_wave()
{
    Gtk::FileChooserDialog dlg(*welcomedlg, "Load Waveform", Gtk::FILE_CHOOSER_ACTION_OPEN);

    dlg.add_button(Gtk::StockID("gtk-ok"), Gtk::RESPONSE_OK);
    dlg.add_button(Gtk::StockID("gtk-cancel"), Gtk::RESPONSE_CANCEL);

    auto filter_wave=Gtk::FileFilter::create();
    filter_wave->set_name("Wave Files");
    filter_wave->add_mime_type("audio/wav");
    dlg.add_filter(filter_wave);

    if (dlg.run()==Gtk::RESPONSE_OK) {
        auto project=std::make_unique<Project>();

        project->track=std::make_unique<Track>(Waveform::load(dlg.get_filename().c_str()));
        project->track->compute_frame_decomposition(1024, 24);
        project->track->refine_frame_decomposition();
        project->track->detect_chunks();
        project->track->compute_pitch_contour();
        project->track->compute_synth_frames();

        auto builder=Gtk::Builder::create_from_resource("/opt/meow/mainwindow.ui");
        
        MainWindow* wnd;
        builder->get_widget_derived("mainwnd", wnd, std::move(project));

        wnd->show_all();

        add_window(*wnd);

        // hack to show menu bar: https://gitlab.gnome.org/GNOME/gtk/-/issues/2834
        Gtk::Settings::get_default()->property_gtk_shell_shows_menubar()=(bool) Gtk::Settings::get_default()->property_gtk_shell_shows_menubar();

        delete welcomedlg;
        welcomedlg=nullptr;
    }
}


int main(int argc, char* argv[])
{
    try {
        App app(argc, argv);

        auto settings=Gtk::Settings::get_default();
        settings->property_gtk_application_prefer_dark_theme()=true;

        return app.run();
    }
    catch (const Gtk::BuilderError& err) {
        printf("Error: %s\n", err.what().c_str());
        return 1;
    }
}
