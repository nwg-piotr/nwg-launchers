/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#pragma once

#include <fstream>
#include <filesystem>
#include <optional>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "nwg_classes.h"

namespace fs = std::filesystem;
namespace ns = nlohmann;

/* Primitive version of C++20's std::span */
template <typename T>
struct Span {
    Span(const Span& s) = default;
    Span(std::vector<T>& t): _ref(t.data()) { }
    T& operator [](std::size_t n) { return _ref[n]; }
    T* _ref;
};

struct Stats {
    enum FavTag: bool {
        Common = 0,
        Favorite = 1,
    };
    enum PinTag: bool {
        Unpinned = 0,
        Pinned = 1,
    };
    int    clicks;
    int    position;
    FavTag favorite;
    PinTag pinned;
    Stats(int c, int i, FavTag f, PinTag p)
      : clicks(c), position(i), favorite(f), pinned(p) { }
};

class GridBox : public Gtk::Button {
public:
    /* name, comment, desktop-id, index */
    GridBox(Glib::ustring, Glib::ustring, const std::string& id, std::size_t);
    ~GridBox() = default;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_focus_in_event(GdkEventFocus*) override;
    void on_enter() override;
    void on_activate() override;

    Glib::ustring      name;
    Glib::ustring      comment;
    const std::string* desktop_id; // ptr to desktop-id, never null
    std::size_t        index;      // row index
};

struct GridConfig: public Config {
    GridConfig(const InputParser& parser, const Glib::RefPtr<Gdk::Screen>& screen, const fs::path& config_dir);

    bool pins;                // whether to display pinned
    bool favs;                // whether to display favorites
    std::string term;         // user-preferred terminal
    std::string lang;         // user-preferred language
    std::size_t num_col{ 6 }; // number of grid columns
    fs::path pinned_file;     // file with pins
    fs::path cached_file;     // file with favs
    int icon_size{ 72 };
    RGBA background_color;
};

class MainWindow : public PlatformWindow {
    public:
        MainWindow(GridConfig& config, Span<std::string> entries, Span<Stats> stats);
        MainWindow(const MainWindow&) = delete;

        Gtk::SearchEntry searchbox;              // Search apps
        Gtk::Label description;                  // To display .desktop entry Comment field at the bottom
        Gtk::FlowBox apps_grid;                  // All application buttons grid
        Gtk::FlowBox favs_grid;                  // Favourites grid above
        Gtk::FlowBox pinned_grid;                // Pinned entries grid above
        Gtk::Separator separator;                // between favs and all apps
        Gtk::Separator separator1;               // below pinned
        Gtk::VBox outer_vbox;
        Gtk::VBox inner_vbox;
        Gtk::HBox hbox_header;
        Gtk::HBox pinned_hbox;
        Gtk::HBox favs_hbox;
        Gtk::HBox apps_hbox;
        Gtk::ScrolledWindow scrolled_window;
        GridConfig&           config;

        template <typename ... Args>
        GridBox& emplace_box(Args&& ... args);      // emplace box

        void build_grids();
        void toggle_pinned(GridBox& box);
        void set_description(const Glib::ustring&);
        void save_cache();

        std::string& exec_of(const GridBox& box) {
            return execs[box.index];
        }
        Stats& stats_of(const GridBox& box) {
            return stats[box.index];
        }
    protected:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey*) override;
        bool on_delete_event(GdkEventAny*) override;
        bool on_button_press_event(GdkEventButton*) override;
    private:
        std::list<GridBox>    all_boxes {};      // stores all applications buttons
        std::vector<GridBox*> apps_boxes {};     // all common boxes
        std::vector<GridBox*> filtered_boxes {}; // common boxes meeting search criteria
        std::vector<GridBox*> fav_boxes {};      // attached to favs_grid
        std::vector<GridBox*> pinned_boxes {};   // attached to pinned_grid


        Span<std::string> execs;
        Span<Stats>       stats;

        int  monotonic_index;       // to keep pins in order, see grid_classes.cc comment
        bool pins_changed = false;
        bool is_filtered = false;

        void focus_first_box();
        void filter_view();
        void refresh_separators();
};

template <typename ... Args>
GridBox& MainWindow::emplace_box(Args&& ... args) {
    auto& ab = this -> all_boxes.emplace_back(std::forward<Args>(args)...);
    auto* boxes = &apps_boxes;
    auto& stats = this -> stats_of(ab);
    if (stats.pinned) {
        boxes = &pinned_boxes;
    } else if (stats.favorite) {
        boxes = &fav_boxes;
    }
    boxes->push_back(&ab);
    return ab;
}

struct CacheEntry {
    std::string desktop_id;
    int clicks;
    CacheEntry(std::string, int);
};

/*
 * Function declarations
 * */
std::vector<std::filesystem::path> get_app_dirs(void);
std::vector<std::string>           get_pinned(const std::filesystem::path& pinned_file);
std::vector<CacheEntry>            get_favourites(ns::json&&, int);
std::optional<DesktopEntry> desktop_entry(const fs::path& path, std::string_view lang, std::string_view term);
