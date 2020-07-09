/* GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * *
 * Credits for window transparency go to AthanasiusOfAlex at https://stackoverflow.com/a/21460337
 * transparent.cpp
 * Code adapted from 'alphademo.c' by Mike
 * (http://plan99.net/~mike/blog--now a dead link--unable to find it.)
 * as modified by karlphillip for StackExchange:
 * (https://stackoverflow.com/questions/3908565/how-to-make-gtk-window-background-transparent)
 * Re-worked for Gtkmm 3.0 by Louis Melahn, L.C. January 31, 2014.
 * */

#include "bar.h"

MainWindow::MainWindow() {
    set_title("~nwgbar");
    set_role("~nwgbar");
    set_skip_pager_hint(true);
    favs_grid.set_column_spacing(5);
    favs_grid.set_row_spacing(5);
    favs_grid.set_column_homogeneous(true);
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);

    set_app_paintable(true);

    signal_draw().connect(sigc::mem_fun(*this, &MainWindow::on_draw));
    signal_screen_changed().connect(sigc::mem_fun(*this, &MainWindow::on_screen_changed));

    on_screen_changed(get_screen());

    // We can not go fullscreen() here:
    // On sway the window would become opaque - we don't wat it
    // On i3 all windows below will be hidden - we don't want it too
    if (wm != "sway" && wm != "i3") {
        fullscreen();
    } else {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
        set_decorated(false);
    }
}

MainWindow::~MainWindow() {
}

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
    if (key_event -> keyval == GDK_KEY_Escape) {
        Gtk::Main::quit();
        return true;
    }
    //if the event has not been handled, call the base class
    return Gtk::Window::on_key_press_event(key_event);
}

bool MainWindow::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    cr->save();
    if (_SUPPORTS_ALPHA) {
        cr->set_source_rgba(0.0, 0.0, 0.0, opacity);    // transparent
    } else {
        cr->set_source_rgb(0.0, 0.0, 0.0);          	// opaque
    }
    cr->set_operator(Cairo::OPERATOR_SOURCE);
    cr->paint();
    cr->restore();

    return Gtk::Window::on_draw(cr);
}

void MainWindow::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    auto screen = get_screen();
    auto visual = screen->get_rgba_visual();

    if (!visual) {
        std::cout << "Your screen does not support alpha channels!" << std::endl;
    } else {
        _SUPPORTS_ALPHA = TRUE;
    }
    set_visual(visual);
}

void MainWindow::set_visual(Glib::RefPtr<Gdk::Visual> visual) {
    gtk_widget_set_visual(GTK_WIDGET(gobj()), visual->gobj());
}
