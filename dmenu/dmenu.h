/* GTK-based dmenu
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
#include <regex>
#include <algorithm>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

extern int rows;
extern std::filesystem::path settings_file;

extern bool dmenu_run;
extern bool show_searchbox;
extern bool case_sensitive;

class MainWindow : public PlatformWindow {
    public:
        MainWindow(std::string_view, std::vector<Glib::ustring>&);
        ~MainWindow();
        void emplace_back(const Glib::ustring&);
    private:
        void filter_view();
        void select_first_item();
        void switch_case_sensitivity();
        
        bool on_key_press_event(GdkEventKey*) override;
        
        Gtk::SearchEntry  searchbox;
        Gtk::ListViewText commands;
        Gtk::VBox         vbox;
        std::vector<Glib::ustring>& commands_source;
        bool case_sensitivity_changed = false;
        bool never_focused = true;
};

/*
 * Function declarations
 * */
std::vector<Glib::ustring> list_commands();
std::filesystem::path get_settings_path();
