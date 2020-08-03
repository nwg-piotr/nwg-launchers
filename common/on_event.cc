/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "on_event.h"

gboolean on_window_clicked(GdkEventButton *event) {
    (void) event; // suppress warning

    Gtk::Main::quit();
    return true;
}
