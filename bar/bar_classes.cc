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

MainWindow::MainWindow(): CommonWindow("~nwgbar", "~nwgbar") {
    favs_grid.set_column_spacing(5);
    favs_grid.set_row_spacing(5);
    favs_grid.set_column_homogeneous(true);

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

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
    if (key_event -> keyval == GDK_KEY_Escape) {
        Gtk::Main::quit();
        return true;
    }
    //if the event has not been handled, call the base class
    return Gtk::Window::on_key_press_event(key_event);
}

/*
 * Constructor is required for std::vector::emplace_back to work
 * It is not needed when compiling with C++20 and greater
 * */
BarEntry::BarEntry(std::string name, std::string exec, std::string icon)
 : name(std::move(name)), exec(std::move(exec)), icon(std::move(icon)) {}

BarBox::BarBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment)
 : AppBox(std::move(name), std::move(exec), std::move(comment)) {}

bool BarBox::on_button_press_event(GdkEventButton* event) {
    (void)event; // suppress warning
    
    this->activate();
    return false;
}

void BarBox::on_activate() {
    exec.append(" &");
    const char *command = exec.c_str();
    std::system(command);

    Gtk::Main::quit();
}
