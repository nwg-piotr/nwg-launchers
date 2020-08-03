/* GTK-based application grid
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

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

extern bool pins;
extern double opacity;
extern std::string wm;

extern int num_col;
extern Gtk::Label *description;

extern std::string pinned_file;
extern std::vector<std::string> pinned;
extern ns::json cache;
extern std::string cache_file;

class MainWindow : public CommonWindow {
    public:
        MainWindow();

        Gtk::SearchEntry searchbox;             // This will stay insensitive, updated with search_phrase value only
        Gtk::Label label_desc;                  // To display .desktop entry Comment field at the bottom
        Glib::ustring search_phrase;            // updated on key_press_event
        Gtk::Grid apps_grid;                    // All application buttons grid
        Gtk::Grid favs_grid;                    // Favourites grid above
        Gtk::Grid pinned_grid;                  // Pinned entries grid above
        Gtk::Separator separator;               // between favs and all apps
        Gtk::Separator separator1;              // below pinned
        std::list<AppBox> all_boxes {};         // attached to apps_grid unfiltered view
        std::list<AppBox*> filtered_boxes {};   // attached to apps_grid filtered view
        std::list<AppBox> fav_boxes {};         // attached to favs_grid
        std::list<AppBox> pinned_boxes {};      // attached to pinned_grid

    private:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey* event) override;
        void filter_view();
        void rebuild_grid(bool filtered);
};

struct CacheEntry {
    std::string exec;
    int clicks;
    CacheEntry(std::string, int);
};

/*
 * Function declarations
 * */
std::string get_cache_path(void);
std::string get_pinned_path(void);
void add_and_save_pinned(const std::string&);
void remove_and_save_pinned(const std::string&);
std::vector<std::string> get_app_dirs(void);
std::vector<std::string> list_entries(const std::vector<std::string>&);
DesktopEntry desktop_entry(std::string&&, const std::string&);
ns::json get_cache(const std::string&);
std::vector<std::string> get_pinned(const std::string&);
std::vector<CacheEntry> get_favourites(ns::json&&, int);
bool on_button_entered(GdkEventCrossing *, const Glib::ustring&);
bool on_button_focused(GdkEventFocus *, const Glib::ustring&);
void on_button_clicked(std::string);
bool on_grid_button_press(GdkEventButton *, const Glib::ustring&);
bool on_pinned_button_press(GdkEventButton *, const Glib::ustring&);
