/* GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#pragma once

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>    // nlohmann-json package

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace ns = nlohmann;

enum class Orientation: unsigned int { Horizontal = 0, Vertical };

struct BarConfig: public Config {
    int icon_size{ 72 };
    Orientation orientation{ Orientation::Horizontal };
    fs::path definition_file{ "bar.json" };
    BarConfig(const InputParser& parser, const Glib::RefPtr<Gdk::Screen>& screen);
};

class BarBox : public AppBox {
public:
    BarBox(Glib::ustring, Glib::ustring, Glib::ustring);
    bool on_button_press_event(GdkEventButton*) override;
    void on_activate() override;
};

class BarWindow : public PlatformWindow {
    public:
        BarWindow(Config&);

        Gtk::VBox outer_box;
        Gtk::HBox inner_hbox;
        Gtk::Grid grid;                         // Buttons grid
        Gtk::Separator separator;               // between favs and all apps
        std::vector<BarBox> boxes {};           // attached to favs_grid

    private:
        //Override default signal handler:
        bool on_button_press_event(GdkEventButton* button) override;
        bool on_key_press_event(GdkEventKey* event) override;
};

struct BarEntry {
    std::string name;
    std::string exec;
    std::string icon;
    BarEntry(std::string, std::string, std::string);
};

/*
 * Function declarations
 * */
std::vector<BarEntry> get_bar_entries(ns::json&&);
