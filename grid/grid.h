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

#include "nwg_classes.h"
#include "nwgconfig.h"

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

class MainWindow : public Gtk::Window {
    public:
        MainWindow();
        void set_visual(Glib::RefPtr<Gdk::Visual> visual);
        virtual ~MainWindow();

        Gtk::SearchEntry searchbox;             // This will stay insensitive, updated with search_phrase value only
        Gtk::Label label_desc;                  // To display .desktop entry Comment field at the bottom
        Glib::ustring search_phrase;            // updated on key_press_event
        Gtk::Grid apps_grid;                    // All application buttons grid
        Gtk::Grid favs_grid;                    // Favourites grid above
        Gtk::Grid pinned_grid;                  // Pinned entries grid above
        Gtk::Separator separator;               // between favs and all apps
        Gtk::Separator separator1;              // below pinned
        std::list<AppBox*> all_boxes {};        // attached to apps_grid unfiltered view
        std::list<AppBox*> filtered_boxes {};   // attached to apps_grid filtered view
        std::list<AppBox*> fav_boxes {};        // attached to favs_grid
        std::list<AppBox*> pinned_boxes {};     // attached to pinned_grid

    private:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey* event) override;
        void filter_view();
        void rebuild_grid(bool filtered);

    protected:
        virtual bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& cr);
        void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen);
        bool _SUPPORTS_ALPHA = false;
};

struct DesktopEntry {
    std::string name;
    std::string exec;
    std::string icon;
    std::string comment;
};

struct CacheEntry {
    std::string exec;
    int clicks;
};

/*
 * Function declarations
 * */
std::string get_cache_path(void);
std::string get_pinned_path(void);
void add_and_save_pinned(std::string);
void remove_and_save_pinned(std::string);
std::vector<std::string> get_app_dirs(void);
std::vector<std::string> list_entries(std::vector<std::string>);
std::vector<std::string> desktop_entry(std::string, std::string);
ns::json get_cache(std::string);
 std::vector<std::string> get_pinned(std::string);
std::vector<CacheEntry> get_favourites(ns::json, int);
bool on_button_entered(GdkEventCrossing *, Glib::ustring);
bool on_button_focused(GdkEventFocus *, Glib::ustring);
void on_button_clicked(std::string);
bool on_grid_button_press(GdkEventButton *, Glib::ustring);
bool on_pinned_button_press(GdkEventButton *, Glib::ustring);
