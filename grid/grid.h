/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#pragma once

#include <unordered_set>

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
    std::size_t size() { return _n; }
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
    // no point making it string_view as Glib::spawn_command_async takes const string&
    // making it string& however breaks move ctors/assignments
    std::string*     exec;
    Stats            stats;

    // TODO: should we store it separately?
    std::unique_ptr<DesktopEntry> desktop_entry_;

    Entry(std::string_view id, Stats stats, std::unique_ptr<DesktopEntry> entry):
        desktop_id{ id }, exec{ &entry->exec }, stats{ stats }, desktop_entry_{ std::move(entry) }
    {
        // intentionally left blank
    }
    auto & desktop_entry() {
        return *desktop_entry_;
    }
};

class GridBox : public Gtk::Button {
public:
    /* name, comment, desktop-id, index */
    GridBox(Glib::ustring, Glib::ustring, Entry& entry);
    GridBox(GridBox&&) = default;
    ~GridBox() = default;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_focus_in_event(GdkEventFocus*) override;
    void on_enter() override;
    void on_activate() override;

    Glib::ustring    name;
    Glib::ustring    comment;

    Entry* entry;
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
    bool oneshot{ false };    // run in foreground, exit when window is closed
};


struct CategoryButton;

struct CategoriesSet {
    struct Category {
        std::string category;
        std::size_t refs;
        CategoryButton* button;
        Category(std::string_view category): category{ category }, refs{ 1 } {}
    };

    std::list<Category> categories_store;
    std::unordered_map<std::string_view, decltype(categories_store)::iterator> categories;
    std::unordered_set<std::string_view> active_categories;
    bool all_enabled{ true };

    using Index = typename decltype(categories_store)::iterator;

    CategoriesSet() = default;
    bool toggle(std::string_view category);
    std::pair<Index, bool> ref(std::string_view category);
    std::pair<Index, bool> unref(std::string_view category);
    bool enabled(std::string_view category) const;
    bool enabled(const GridBox& box) const;

    void delete_by_index(Index index);
};

struct CategoryButton: public Gtk::ToggleButton {
    Gdk::ModifierType modifiers;
    bool mod_pressed{ false };

    CategoriesSet& categories;
    CategoriesSet::Index index;

    CategoryButton(const std::string& name, CategoriesSet& set, CategoriesSet::Index index);
    ~CategoryButton();

    bool on_button_press_event(GdkEventButton* key) override {
        mod_pressed = (key->state & modifiers) == Gdk::CONTROL_MASK;
        // if Ctrl is not pressed, disable all other categories
        if (!mod_pressed) {
            auto& fboxchild = *dynamic_cast<Gtk::FlowBoxChild*>(get_parent());
            auto& fbox = dynamic_cast<Gtk::FlowBox&>(*fboxchild.get_parent());
            fbox.foreach([this](Gtk::Widget& widget) {
                auto& fboxchild = static_cast<Gtk::FlowBoxChild&>(widget);
                auto* button = dynamic_cast<CategoryButton*>(fboxchild.get_child());
                if (this != button) {
                    button->set_active(false);
                }
            });
        }
        return Gtk::ToggleButton::on_button_press_event(key);
    }
    bool on_button_release_event(GdkEventButton* key) override {
        return Gtk::ToggleButton::on_button_release_event(key);
    }
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
        if (auto iter = std::find(boxes.begin(), boxes.end(), &box); iter != boxes.end()) {
            auto pos = std::distance(boxes.begin(), iter);
            boxes.erase(iter);
            box.reference();
            items_changed(pos, 1, 0);
        }
    }
    virtual void update(GridBox& from, GridBox& to) {
        if (auto iter = std::find(boxes.begin(), boxes.end(), &from); iter != boxes.end()) {
            auto pos = std::distance(boxes.begin(), iter);
            // two references required, but why?
            to.reference();
            to.reference();
            *iter = &to;
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
    template <typename ... Ts>
    static auto create(Ts && ... ts) {
        auto* ptr = new T{ std::forward<Ts>(ts)... };
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
    int monotonic_index{ 0 };
    PinnedBoxes(): Glib::ObjectBase(typeid(PinnedBoxes)) {}
public:
    void add(GridBox& box) override {
        box.entry->stats.pinned = Stats::Pinned;
        // temporary fix for #176
        // initial indices are set to < 0 so they are not reordered
        // but we reset them to 0 when erasing, so that when the entry is unpinned & pinned again
        // it receives proper position
        if (box.entry->stats.position >= 0) {
            box.entry->stats.position = monotonic_index;
            ++monotonic_index;
        }
        auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
            return a->entry->stats.position > b->entry->stats.position;
        });

        // monotonic index increases each time an entry is pinned
        // ensuring it will appear last
        items_changed(pos, 0, 1);
    }
    void erase(GridBox& box) override {
        box.entry->stats.position = 0;
        BoxesModel::erase(box);
    }
};

class FavBoxes: public BoxesModel, public Create<FavBoxes> {
    friend struct Create<FavBoxes>; // permit Create to access a protected constructor
protected:
    FavBoxes(): Glib::ObjectBase(typeid(FavBoxes)) {}
public:
    void add(GridBox& box) override {
        box.entry->stats.favorite = Stats::Favorite;
        box.entry->stats.clicks = 1;
        auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
            return a->entry->stats.clicks < b->entry->stats.clicks;
        });
        items_changed(pos, 0, 1);
    }
};

class AppBoxes: public BoxesModel, public Create<AppBoxes> {
    friend struct Create<AppBoxes>; // permit Create to access a protected constructor
private:
    std::vector<GridBox*> all_boxes; // unsorted & unfiltered boxes
    Glib::ustring         search_criteria;
    CategoriesSet&        categories;
protected:
    AppBoxes(CategoriesSet& set): Glib::ObjectBase(typeid(AppBoxes)), categories{ set } {}
    void add_if_matches(GridBox* box) {
        auto enabled = categories.enabled(*box);
        if (enabled && (box->name.casefold().find(search_criteria) != Glib::ustring::npos)) {
            container_add_sorted(boxes, box, [](auto* a, auto* b) {
                return a->name.compare(b->name) > 0;
            });
        }
    }
    void filter_impl(bool restore) {
        // TODO: only update actually removed/inserted entries
        auto old_size = boxes.size();
        if (!restore) {
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
public:
    void add(GridBox& box) override {
        // TODO: ensure the box does not exist before insertion for all *Boxes classes
        all_boxes.push_back(&box);
        auto ok = categories.enabled(box) && (
            search_criteria.length() == 0
            ||
            box.name.casefold().find(search_criteria) != Glib::ustring::npos
        );
        if (ok) {
            auto pos = container_add_sorted(boxes, &box, [](auto* a, auto* b) {
                return a->name.compare(b->name) > 0;
            });
            items_changed(pos, 0, 1);
        }
    }
    void erase(GridBox& box) override {
        // erasing from filtered boxes will decrease reference count by 1, destroying object
        // but we want it alive to remove it from all_boxes and then to destroy it ourselves
        box.reference();
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
            filter_impl(search_criteria.length() == 0);
        }
    }
    void on_category_toggled() {
        filter_impl(false);
    }
    bool is_filtered() {
        return search_criteria.length() > 0;
    }
};

class GridWindow : public PlatformWindow {
    public:
        GridWindow(GridConfig& config);
        GridWindow(const GridWindow&) = delete;
        ~GridWindow();

        Gtk::SearchEntry  searchbox;              // Search apps
        Gtk::FlowBox      categories_box;
        Gtk::ToggleButton categories_all;
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
        Gtk::HBox categories_hbox;
        Gtk::ScrolledWindow scrolled_window;
        GridConfig&           config;

        template <typename ... Args>
        GridBox& emplace_box(Args&& ... args);      // emplace box
        void update_box_by_id(std::string_view desktop_id, GridBox&&);
        void remove_box_by_desktop_id(std::string_view desktop_id);

        void build_grids();
        void toggle_pinned(GridBox& box);
        void set_description(const Glib::ustring&);
        void save_cache();
        void run_box(GridBox& box);

        std::string& exec_of(const GridBox& box) {
            return *box.entry->exec;
        }
        Stats& stats_of(const GridBox& box) {
            return box.entry->stats;
        }
    protected:
        //Override default signal handler:
        bool on_key_press_event(GdkEventKey*) override;
        void on_show() override;
        bool on_delete_event(GdkEventAny*) override;
        bool on_button_press_event(GdkEventButton*) override;
    private:
        void ref_categories(const GridBox& box);
        void unref_categories(GridBox& box);
        
        std::list<GridBox>  all_boxes {}; // stores all applications buttons
        Glib::RefPtr<AppBoxes> apps_boxes;   // common boxes (possibly filtered)
        Glib::RefPtr<FavBoxes> fav_boxes;    // favourites (most clicked)
        Glib::RefPtr<PinnedBoxes> pinned_boxes; // boxes pinned by user

        CategoriesSet categories;

        bool pins_changed = false;
        bool favs_changed = false;

        void focus_first_box();
        void filter_view();
        void refresh_separators();
};

template <typename ... Args>
GridBox& GridWindow::emplace_box(Args&& ... args) {
    auto& ab = this -> all_boxes.emplace_back(std::forward<Args>(args)...);
    ref_categories(ab);
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

    GridInstance(Gtk::Application& app, GridWindow& window, std::string_view name):
        Instance{ app, name }, window{ window }
    {
        // intentionally left blank
    }
    /* Instance on_* handlers call Application::quit
     * which internally calls _exit, destructors are not called
     * To handle this problem GridInstance overrides handlers
     * to call Application::release
     */
    void on_sighup() override;  // reload
    void on_sigint() override;  // save & exit
    void on_sigterm() override;  // save & exit
    void on_sigusr1() override; // show
    ~GridInstance() {
        window.save_cache();
    }
};

/*
 * Function declarations
 * */
std::vector<fs::path>       get_app_dirs(void);
std::vector<std::string>    get_pinned(const fs::path& pinned_file);
std::vector<CacheEntry>     get_favourites(ns::json&&, int);
