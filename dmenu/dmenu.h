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

extern std::string h_align;
extern std::string v_align;
extern std::string wm;
extern std::string settings_file;

extern int rows;
extern std::vector<Glib::ustring> all_commands;

extern bool dmenu_run;
extern bool show_searchbox;
extern bool case_sensitive;

class MainWindow : public CommonWindow {
    public:
        MainWindow();
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
        bool case_sensitivity_changed = false;
};

/*
 * Function declarations
 * */
std::vector<std::string> list_commands();
std::string get_settings_path();
