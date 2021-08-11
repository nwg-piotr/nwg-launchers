/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#pragma once

#include <fstream>
#include <optional>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#include <nlohmann/json.hpp>

#include "nwgconfig.h"
#include "filesystem-compat.h"
#include "nwg_classes.h"

namespace ns = nlohmann;

/* Primitive version of C++20's std::span */
template <typename T>
struct Span {
    Span(): _ref{ nullptr }, _n{ 0 } {}
    Span(const Span& s) = default;
    Span(std::vector<T>& t): _ref{ t.data() }, _n{ t.size() } { }
    T& operator [](std::size_t n) { return _ref[n]; }
    T*          _ref;
    std::size_t _n;

    T* begin() { return _ref; }
    T* end() { return _ref + _n; }
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
    int    clicks{ 0 };
    int    position{ 0 };
    FavTag favorite{ Common };
    PinTag pinned{ Unpinned };
    Stats(int c, int i, FavTag f, PinTag p)
      : clicks(c), position(i), favorite(f), pinned(p) { }
    Stats() = default;
};

struct Entry {
    std::string_view desktop_id;
    std::string      exec;
    Stats            stats;

    // TODO: should we store it separately?
    DesktopEntry desktop_entry;
};

class GridBox : public Gtk::Button {
public:
    /* name, comment, desktop-id, index */
    GridBox(Glib::ustring, Glib::ustring, const std::shared_ptr<Entry>&);
    ~GridBox() = default;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_focus_in_event(GdkEventFocus*) override;
    void on_enter() override;
    void on_activate() override;

    Glib::ustring    name;
    Glib::ustring    comment;

    std::shared_ptr<Entry> entry;
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

class AbstractBoxes {
protected:
    std::vector<GridBox*> boxes;
public:
    virtual ~AbstractBoxes() = default;

    decltype(auto) begin() { return boxes.begin(); }
    decltype(auto) end() { return boxes.end(); }
    auto & front() { return boxes.front(); }
    auto size() const { return boxes.size(); }
    auto empty() const { return boxes.empty(); }

    virtual void add(GridBox& box) = 0;
    virtual void erase(GridBox& box) = 0;
};

class BoxesModel: public AbstractBoxes, public Gio::ListModel, public Glib::Object {
public:
    virtual ~BoxesModel() = default;
    virtual void erase(GridBox& box) override {
        if (auto to_erase = std::remove(boxes.begin(), boxes.end(), &box); to_erase != boxes.end()) {
            box.reference();
            auto pos = std::distance(boxes.begin(), to_erase);
            boxes.erase(to_erase, boxes.end());
            items_changed(pos, 1, 0);
        }
    }
    virtual void mark_updated(GridBox& box) {
        if (auto iter = std::find(boxes.begin(), boxes.end(), &box); iter != boxes.end()) {
            auto pos = std::distance(boxes.begin(), iter);
            items_changed(pos, 1, 1);
        }
    }
protected:
    BoxesModel(): Glib::ObjectBase(typeid(BoxesModel)), Gio::ListModel() {}
    GType get_item_type_vfunc() override {
        return GridBox::get_type();
    }
    guint get_n_items_vfunc() override {
        return boxes.size();
    }
    gpointer get_item_vfunc(guint position) override {
        if (position < boxes.size()) {
            return boxes[position]->gobj();
        }
        return nullptr;
    }
};

/* CRTP class providing T::create() -> RefPtr<T> */
template <typename T> struct Create {
    static auto create() {
        auto* ptr = new T{};
        // refptr(ptr) constructor does not increase reference count, but ~refptr does decrease
        // resulting in refcount < 0
        ptr->reference();
        return Glib::RefPtr<T>{ ptr };
    }
};

inline auto container_add_sorted = [](auto && container, auto && elem, auto && cmp_less) {
    auto iter = std::find_if(container.begin(), container.end(), [&](auto & other) {
        return !cmp_less(elem, other);
    });
    auto pos = std::distance(container.begin(), iter);
    container.insert(iter, elem);
    return pos;
};

class PinnedBoxes: public BoxesModel, public Create<PinnedBoxes> {
    friend struct Create<PinnedBoxes>; // permit Create to access a protected constructor
protected:
    int monotonic_index = 0;
    PinnedBoxes(): Glib::ObjectBase(typeid(PinnedBoxes)) {}
public:
    void add(GridBox& box) override {
        // TODO: compare by monotonic_index
        auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
            return a->name.compare(b->name) > 0;
        });
        // monotonic index increases each time an entry is pinned
        // ensuring it will appear last
        ++monotonic_index;
        items_changed(pos, 0, 1); // pos - 1 maybe? insert inserts before the iterator
    }
};

class FavBoxes: public BoxesModel, public Create<FavBoxes> {
    friend struct Create<FavBoxes>; // permit Create to access a protected constructor
protected:
    FavBoxes(): Glib::ObjectBase(typeid(FavBoxes)) {}
public:
    void add(GridBox& box) override {
        auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
            return a->name.compare(b->name) > 0;
        });
        items_changed(pos, 0, 1); // pos - 1 maybe? insert inserts before the iterator
    }
};

class AppBoxes: public BoxesModel, public Create<AppBoxes> {
    friend struct Create<AppBoxes>; // permit Create to access a protected constructor
private:
    std::vector<GridBox*> all_boxes; // unsorted & unfiltered boxes
    Glib::ustring search_criteria;
protected:
    AppBoxes(): Glib::ObjectBase(typeid(AppBoxes)) {}
    void add_if_matches(GridBox* box) {
        if (box->name.casefold().find(search_criteria) != Glib::ustring::npos) {
            container_add_sorted(boxes, box, [](auto* a, auto* b) {
                return a->name.compare(b->name) > 0;
            });
        }
    }
public:
    void add(GridBox& box) override {
        // TODO: ensure the box does not exist before insertion for all *Boxes classes
        all_boxes.push_back(&box);
        if (search_criteria.length() == 0 || (box.name.casefold().find(search_criteria) != Glib::ustring::npos)) {
            auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
                return a->name.compare(b->name) > 0;
            });
            items_changed(pos, 0, 1);
        }
    }
    void erase(GridBox& box) override {
        box.reference(); // TODO: is it really needed?
        // erase from filtered boxes
        BoxesModel::erase(box);
        // erase from all boxes
        if (auto to_erase_2 = std::remove(all_boxes.begin(), all_boxes.end(), &box); to_erase_2 != all_boxes.end()) {
            all_boxes.erase(to_erase_2);
        }
    }
    void filter(const Glib::ustring& criteria) {
        auto criteria_ = criteria.casefold();
        if (search_criteria != criteria_) {
            search_criteria = criteria_;
            // TODO: only update actually removed/inserted entries
            auto old_size = boxes.size();
            if (search_criteria.length() > 0) {
                for (auto && box: boxes) {
                    box->reference();
                }
                boxes.clear();
                for (auto && box: all_boxes) {
                    box->reference();
                    add_if_matches(box);
                }
            } else {
                boxes = all_boxes;
                std::sort(boxes.begin(), boxes.end(), [](auto* a, auto* b) {
                    return a->name.compare(b->name) < 0;
                });
                for (auto && box: all_boxes) {
                    box->reference();
                    box->reference();
                }
            }
            items_changed(0, old_size, boxes.size());
        }
    }
    bool is_filtered() {
        return search_criteria.length() > 0;
    }
};


class GridWindow : public PlatformWindow {
    public:
        GridWindow(GridConfig& config);
        GridWindow(const GridWindow&) = delete;

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
        void remove_box_by_desktop_id(std::string_view desktop_id);
        void update_box_by_id(std::string_view desktop_id);

        void build_grids();
        void toggle_pinned(GridBox& box);
        void set_description(const Glib::ustring&);
        void save_cache();
        void run_box(GridBox& box);

        std::string& exec_of(const GridBox& box) {
            return box.entry->exec;
        }
        Stats& stats_of(const GridBox& box) {
            return box.entry->stats;
        }
    protected:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey*) override;
        bool on_delete_event(GdkEventAny*) override;
        bool on_button_press_event(GdkEventButton*) override;
    private:
        std::list<GridBox>  all_boxes {}; // stores all applications buttons
        Glib::RefPtr<AppBoxes> apps_boxes;   // common boxes (possibly filtered)
        Glib::RefPtr<FavBoxes> fav_boxes;    // favourites (most clicked)
        Glib::RefPtr<PinnedBoxes> pinned_boxes; // boxes pinned by user

        bool pins_changed = false;
        bool favs_changed = false;

        void focus_first_box();
        void filter_view();
        void refresh_separators();
};

template <typename ... Args>
GridBox& GridWindow::emplace_box(Args&& ... args) {
    auto& ab = this -> all_boxes.emplace_back(std::forward<Args>(args)...);
    ab.reference();
    ab.reference();
    AbstractBoxes* boxes = apps_boxes.get();
    auto& stats = this -> stats_of(ab);
    if (stats.pinned) {
        boxes = pinned_boxes.get();
    } else if (stats.favorite) {
        boxes = fav_boxes.get();
    }
    boxes->add(ab);
    return ab;
}

struct CacheEntry {
    std::string desktop_id;
    int clicks;
    CacheEntry(std::string, int);
};

struct GridInstance: public Instance {
    GridWindow& window;

    GridInstance(Gtk::Application& app, GridWindow& window): Instance{ app, "nwggrid" }, window{ window } {
        app.hold();
    }
    /* Instance::on_sig{int,term} call Application::quit, which in turn emit shutdown signal
     * However, window.save_cache bound to said event doesn't get called for some reason
     * So we call it ourselves before calling Application::quit */
    void on_sighup() override;  // reload
    void on_sigint() override;  // save & exit
    void on_sigterm() override;  // save & exit
    void on_sigusr1() override; // show
};

/*
 * Function declarations
 * */
std::vector<fs::path>       get_app_dirs(void);
std::vector<std::string>    get_pinned(const fs::path& pinned_file);
std::vector<CacheEntry>     get_favourites(ns::json&&, int);
std::optional<DesktopEntry> desktop_entry(const fs::path& path, std::string_view lang, std::string_view term);
