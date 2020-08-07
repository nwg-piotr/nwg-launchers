/* GTK-based application grid
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

#include "nwg_tools.h"
#include "grid.h"

MainWindow::MainWindow(): CommonWindow("~nwggrid", "~nwggrid") {
    searchbox
        .signal_search_changed()
        .connect(sigc::mem_fun(*this, &MainWindow::filter_view));
    apps_grid.set_column_spacing(5);
    apps_grid.set_row_spacing(5);
    apps_grid.set_column_homogeneous(true);
    favs_grid.set_column_spacing(5);
    favs_grid.set_row_spacing(5);
    favs_grid.set_column_homogeneous(true);
    pinned_grid.set_column_spacing(5);
    pinned_grid.set_row_spacing(5);
    pinned_grid.set_column_homogeneous(true);
    label_desc.set_text("");
    label_desc.set_name("description");
    description = &label_desc;
    separator.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator.set_name("separator");
    separator1.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator1.set_name("separator");
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    // We can not go fullscreen() here:
    // On sway the window would become opaque - we don't want it
    // On i3 all windows below will be hidden - we don't want it as well
    if (wm != "sway" && wm != "i3") {
        fullscreen();
    } else {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
        set_decorated(false);
    }
}

bool MainWindow::on_button_press_event(GdkEventButton *event) {
    (void) event; // suppress warning

    this->quit();
    return true;
}

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
    auto key_val = key_event -> keyval;
    switch (key_val) {
        case GDK_KEY_Escape:
            this->quit();
            break;
        case GDK_KEY_Delete:
            this -> searchbox.set_text("");
            break;
        case GDK_KEY_Return:
        case GDK_KEY_Left:
        case GDK_KEY_Right:
        case GDK_KEY_Up:
        case GDK_KEY_Down:
            break;
        default:
            // Focus the searchbox:
            // because searchbox is now focused,
            // Gtk::Window will delegate the event there
            if (!this -> searchbox.is_focus()) {
                this -> searchbox.grab_focus();
                // when searchbox receives focus,
                // its contents become selected
                // and the incoming character will overwrite them
                // so we make sure to drop the selection
                this -> searchbox.prepare_to_insertion();
            }
    }
    // Delegate handling to the base class
    return Gtk::Window::on_key_press_event(key_event);
}

void MainWindow::filter_view() {
    auto search_phrase = searchbox.get_text();
    if (search_phrase.size() > 0) {
        this -> filtered_boxes.clear();

        auto phrase = search_phrase.casefold();
        for (auto& box : this -> all_boxes) {
            if (box.name.casefold().find(phrase) != Glib::ustring::npos ||
                box.exec.casefold().find(phrase) != Glib::ustring::npos ||
                box.comment.casefold().find(phrase) != Glib::ustring::npos)
            {
                this -> filtered_boxes.emplace_back(&box);
            }
        }
        this -> favs_grid.hide();
        this -> separator.hide();
        this -> rebuild_grid(true);
    } else {
        this -> favs_grid.show();
        this -> separator.show();
        this -> rebuild_grid(false);
    }
    // set focus to the first icon search results
    auto* first = this -> apps_grid.get_child_at(0, 0);
    if (first) {
        first -> grab_focus();
    }
}

void MainWindow::rebuild_grid(bool filtered) {
    int column = 0;
    int row = 0;

    this -> apps_grid.freeze_child_notify();
    for (Gtk::Widget *widget : this -> apps_grid.get_children()) {
        this -> apps_grid.remove(*widget);
        widget -> unset_state_flags(Gtk::STATE_FLAG_PRELIGHT);
    }
    int cnt = 0;
    if (filtered) {
        for (auto& box : this -> filtered_boxes) {
            this -> apps_grid.attach(*box, column, row, 1, 1);
            if (column < num_col - 1) {
                column++;
            } else {
                column = 0;
                row++;
            }
            cnt++;
        }
        // Set keyboard focus to the first visible button
        if (this -> fav_boxes.size() > 0) {
            fav_boxes.front().grab_focus();
        }
    } else {
        for (auto& box : this -> all_boxes) {
            this -> apps_grid.attach(box, column, row, 1, 1);
            if (column < num_col - 1) {
                column++;
            } else {
                column = 0;
                row++;
            }
            cnt++;
        }
        // Set keyboard focus to the first visible button
        if (this -> all_boxes.size() > 0) {
            all_boxes.front().grab_focus();
        }
    }
    this -> apps_grid.thaw_child_notify();
}

GridBox::GridBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment, bool pinned)
 : AppBox(std::move(name), std::move(exec), std::move(comment)), pinned(pinned) {}

bool GridBox::on_button_press_event(GdkEventButton* event) {
    int clicks = 0;
    try {
        clicks = cache[exec];
        clicks++;
    } catch(...) {
        clicks = 1;
    }
    cache[exec] = clicks;
    save_json(cache, cache_file);

    if (pins && event->button == 3) {
        if (pinned) {
            remove_and_save_pinned(exec);
        } else {
            add_and_save_pinned(exec);
        }
    }
    this -> activate();
    return false;
}

bool GridBox::on_focus_in_event(GdkEventFocus* event) {
    (void) event; // suppress warning

    description -> set_text(comment);
    return true;
}

void GridBox::on_enter() {
    description -> set_text(comment);
    return AppBox::on_enter();
}

void GridBox::on_activate() {
    exec.append(" &");
    std::system(exec.data());
    auto toplevel = dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel->quit();
}

GridSearch::GridSearch() {
    set_placeholder_text("Type to search");
    set_sensitive(true);
    set_name("searchbox");
}

void GridSearch::prepare_to_insertion() {
    select_region(0, 0);
    set_position(-1);
}
