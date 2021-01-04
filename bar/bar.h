/* GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <filesystem>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>    // nlohmann-json package

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

extern std::string wm;

class BarBox : public AppBox {
public:
    BarBox(Glib::ustring, Glib::ustring, Glib::ustring);
    bool on_button_press_event(GdkEventButton*) override;
    void on_activate() override;
};

class MainWindow : public CommonWindow {
    public:
        MainWindow();

        Gtk::Grid favs_grid;                    // Favourites grid above
        Gtk::Separator separator;               // between favs and all apps
        std::vector<BarBox> all_boxes {};        // attached to apps_grid unfiltered view
        std::vector<BarBox> filtered_boxes {};   // attached to apps_grid filtered view
        std::vector<BarBox> boxes {};            // attached to favs_grid

    private:
        //Override default signal handler:
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
