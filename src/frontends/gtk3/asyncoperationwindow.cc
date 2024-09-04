#include "asyncoperationwindow.h"


AsyncOperationWindow::AsyncOperationWindow(BaseObjectType* obj, const Glib::RefPtr<Gtk::Builder>& builder):
    Gtk::Window(obj)
{
    builder->get_widget("progressbar", progressbar);

    dispatch_update.connect(sigc::mem_fun(*this, &AsyncOperationWindow::update));
    dispatch_finish.connect(sigc::mem_fun(*this, &AsyncOperationWindow::finish));
}


void AsyncOperationWindow::report(double in_progress)
{
    progress=in_progress;

    dispatch_update();
}


void AsyncOperationWindow::run()
{
    show_all();

    thread=std::thread([this]() {
        try {
            on_run();
        }
        catch (...) {
            exception=std::current_exception();
        }

        dispatch_finish();
    });
}


void AsyncOperationWindow::update()
{
    progressbar->set_fraction(progress);
}


void AsyncOperationWindow::finish()
{
    thread.join();

    on_finished();

    delete this;
}


void AsyncOperationWindow::rethrow_exception()
{
    if (exception)
        std::rethrow_exception(exception);
}
