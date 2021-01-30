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

class DMenu : public Gtk::Menu {
    public:
        DMenu(Gtk::Window&);
        ~DMenu();
        void emplace_back(const Glib::ustring&);
        void show_all() {
            Gtk::Menu::show_all();
            // required to have first item selected on launch
            fix_selection();
        }
        
    private:
        Gtk::SearchEntry searchbox;
        // parent window
        Gtk::Window&     main;
        // the first item in list
        Gtk::MenuItem*   first_item = nullptr;
        // whether case sensitivity was changed during run
        bool case_sensitivity_changed = false;
        
        bool on_key_press_event(GdkEventKey* event) override;
        void filter_view();
        void switch_case_sensitivity();
        void fix_selection();
        void on_item_clicked(Glib::ustring cmd);
};

class Anchor : public Gtk::Button {
    public:
        Anchor(DMenu&);
    private:
        bool on_focus_in_event(GdkEventFocus* focus_event) override;
        Gdk::Gravity gravity_widget, gravity_menu;
        DMenu& menu;
};

class MainWindow : public CommonWindow {
    public:
        MainWindow();

        Gtk::Menu* menu;
        Gtk::Button* anchor;

    private:
        bool on_button_press_event(GdkEventButton* button_event);
};

/*
 * Function declarations
 * */
std::vector<std::string> list_commands();
std::string get_settings_path();

void on_item_clicked(std::string);
