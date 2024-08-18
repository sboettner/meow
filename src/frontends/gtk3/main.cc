#include <memory>
#include "track.h"
#include "controller.h"
#include "audio.h"
#include "mainwindow.h"


class App:public Gtk::Application {
public:
    App(int argc, char* argv[]);

    void on_activate() override;
    void on_load_wave();

private:
    Gtk::ApplicationWindow* welcomedlg=nullptr;
};


App::App(int argc, char* argv[]):Gtk::Application(argc, argv)
{
    add_action("loadwave", sigc::mem_fun(*this, &App::on_load_wave));
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
        // FIXME: use smart pointers
        Track* track=new Track(Waveform::load(dlg.get_filename().c_str()));

        track->compute_frame_decomposition(1024, 24);
        track->refine_frame_decomposition();
        track->detect_chunks();
        track->compute_pitch_contour();
        track->compute_synth_frames();

        Controller* controller=new Controller(*track);

        auto builder=Gtk::Builder::create_from_resource("/opt/meow/mainwindow.ui");
        
        MainWindow* wnd;
        builder->get_widget_derived("mainwnd", wnd, *controller);

        wnd->show_all();

        add_window(*wnd);

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
