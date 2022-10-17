/* GTK-based button bar
 * Copyright (c) 2021 Piotr Miller
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

#include "charconv-compat.h"
#include "nwg_tools.h"
#include "bar.h"
#include "log.h"

BarConfig::BarConfig(const InputParser& parser, const Glib::RefPtr<Gdk::Screen>& screen):
    Config{ parser, "~nwgbar", "~nwgbar", screen }
{
    if (parser.cmdOptionExists("-v")) {
        orientation = Orientation::Vertical;
    }
    if (auto tname = parser.getCmdOption("-t"); !tname.empty()) {
        definition_file = tname;
    }
    if (auto i_size = parser.getCmdOption("-s"); !i_size.empty()) {
        icon_size = parse_icon_size(i_size);
    }
}

BarWindow::BarWindow(Config& config): PlatformWindow(config) {
    // scrolled_window -> outer_box -> inner_hbox -> grid
    grid.set_column_spacing(5);
    grid.set_row_spacing(5);
    grid.set_column_homogeneous(true);
    outer_box.set_spacing(15);
    inner_hbox.set_name("bar");
    switch (config.halign) {
        case HAlign::Left:  inner_hbox.pack_start(grid, false, false); break;
        case HAlign::Right: inner_hbox.pack_end(grid, false, false); break;
        default: inner_hbox.pack_start(grid, true, false);
    }
    switch (config.valign) {
        case VAlign::Top:    outer_box.pack_start(inner_hbox, false, false); break;
        case VAlign::Bottom: outer_box.pack_end(inner_hbox, false, false); break;
        default: outer_box.pack_start(inner_hbox, Gtk::PACK_EXPAND_PADDING);
    }
    scrolled_window.add(outer_box);
    add(scrolled_window);
    show_all_children();
}

bool BarWindow::on_button_press_event(GdkEventButton* button) {
    (void)button;
    this->close();
    return true;
}

bool BarWindow::on_key_press_event(GdkEventKey* key_event) {
    if (key_event -> keyval == GDK_KEY_Escape) {
        this->close();
    }
    //if the event has not been handled, call the base class
    return CommonWindow::on_key_press_event(key_event);
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
    try {
        Glib::spawn_command_line_async(exec);
    } catch (const Glib::SpawnError& error) {
        Log::error("Failed to run command: ", error.what());
    } catch (const Glib::ShellError& error) {
        Log::error("Failed to run command: ", error.what());
    }
    dynamic_cast<BarWindow*>(this->get_toplevel())->close();
}
