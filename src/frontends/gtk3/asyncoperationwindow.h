#pragma once

#include <thread>
#include <atomic>
#include <gtkmm.h>
#include "iprogressmonitor.h"


class AsyncOperationWindow:public Gtk::Window, protected IProgressMonitor {
public:
    AsyncOperationWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder);

    void run();

    virtual void on_run() = 0;
    virtual void on_finished() = 0;

protected:
    virtual void report(double) override;

private:
    void update();
    void finish();

    std::atomic<double> progress;
    Gtk::ProgressBar*   progressbar;

    Glib::Dispatcher    dispatch_update;
    Glib::Dispatcher    dispatch_finish;

    std::thread         thread;
};
