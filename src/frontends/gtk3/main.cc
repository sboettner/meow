#include <memory>
#include <fstream>
#include "project.h"
#include "controller.h"
#include "audio.h"
#include "mainwindow.h"
#include "asyncoperationwindow.h"


class App:public Gtk::Application {
public:
    App(int argc, char* argv[]);

    void on_startup() override;
    void on_activate() override;

private:
    void on_load_project();
    void on_load_wave();

    void open_main_window_for_project(std::unique_ptr<Project>&&);

    Gtk::ApplicationWindow* welcomedlg=nullptr;
};


App::App(int argc, char* argv[]):Gtk::Application(argc, argv)
{
    add_action("loadproject", sigc::mem_fun(*this, &App::on_load_project));
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


void App::on_load_project()
{
    Gtk::FileChooserDialog dlg(*welcomedlg, "Load Project", Gtk::FILE_CHOOSER_ACTION_OPEN);

    dlg.add_button(Gtk::StockID("gtk-ok"), Gtk::RESPONSE_OK);
    dlg.add_button(Gtk::StockID("gtk-cancel"), Gtk::RESPONSE_CANCEL);

    auto filter_proj=Gtk::FileFilter::create();
    filter_proj->set_name("Project Files");
    filter_proj->add_pattern("*.meow");
    dlg.add_filter(filter_proj);

    if (dlg.run()==Gtk::RESPONSE_OK) {
        auto project=std::make_unique<Project>();

        std::ifstream ifs(dlg.get_filename(), std::ios::binary);
        project->read(ifs);

        open_main_window_for_project(std::move(project));
    }
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
        class LoadWaveformOperationWindow:public AsyncOperationWindow {
            App&                        app;
            std::string                 filename;

            std::unique_ptr<Project>    project;

        public:
            LoadWaveformOperationWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder, App& app, const std::string& filename):
                AsyncOperationWindow(obj, builder),
                app(app),
                filename(filename)
            {
                project=std::make_unique<Project>();
            }

            void on_run() override
            {
                auto waveform=Waveform::load(filename.c_str());
                waveform->compute_frame_decomposition(1024, 24, *this);

                auto track=std::make_unique<Track>(std::move(waveform));
                track->detect_chunks();
                track->compute_pitch_contour();
                track->compute_synth_frames();

                project->tracks.push_back(std::move(track));
            }

            void on_finished() override
            {
                app.open_main_window_for_project(std::move(project));
            }
        };

        auto builder=Gtk::Builder::create_from_resource("/opt/meow/asyncoperationwindow.ui");

        LoadWaveformOperationWindow* asyncopwnd;
        builder->get_widget_derived("asyncopwnd", asyncopwnd, *this, dlg.get_filename());

        asyncopwnd->run();
    }
}


void App::open_main_window_for_project(std::unique_ptr<Project>&& project)
{
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
