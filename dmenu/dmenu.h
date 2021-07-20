/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */


#include <filesystem>
#include <vector>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;

#ifndef ROWS_DEFAULT
#define ROWS_DEFAULT 20 // used in dmenu.cc/HELP_MESSAGE, don't turn into variable
#endif

struct DmenuConfig: public Config {
    DmenuConfig(const InputParser& parser, const Glib::RefPtr<Gdk::Screen>& screen);

    fs::path settings_file;
    int rows{ ROWS_DEFAULT };            // number of menu items to display
    bool dmenu_run{ true };
    bool show_searchbox{ true };
    bool case_sensitive{ true };
};

class DmenuWindow : public PlatformWindow {
    public:
        DmenuWindow(DmenuConfig&, std::vector<Glib::ustring>&);
        ~DmenuWindow();
        void emplace_back(const Glib::ustring&);

        int get_height() override;
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
        DmenuConfig&       config;
};

/*
 * Function declarations
 * */
std::vector<Glib::ustring> get_commands_list(const DmenuConfig& config);
fs::path get_settings_path();
