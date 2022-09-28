
/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * *
 * Credits for window transparency go to AthanasiusOfAlex at https://stackoverflow.com/a/21460337
 * transparent.cpp
 * Code adapted from 'alphademo.c' by Mike
 * (http://plan99.net/~mike/blog--now a dead link--unable to find it.)
 * as modified by karlphillip for StackExchange:
 * (https://stackoverflow.com/questions/3908565/how-to-make-gtk-window-background-transparent)
 * Re-worked for Gtkmm 3.0 by Louis Melahn, L.C. January 31, 2014.
 * */

#include <fstream>

#include "charconv-compat.h"
#include "nwg_tools.h"
#include "grid.h"
#include "log.h"

GridConfig::GridConfig(const InputParser& parser, const Glib::RefPtr<Gdk::Screen>& screen, const fs::path& config_dir):
    Config{ parser, "~nwggrid", "~nwggrid", screen },
    term{ get_term(config_dir.native()) },
    background_color{ parser.get_background_color(0.9) }
{
    auto has_custom_paths = parser.cmdOptionExists("-d");
    auto has_favs = parser.cmdOptionExists("-f");
    auto has_pins = parser.cmdOptionExists("-p");
    if ((has_favs || has_pins) && has_custom_paths) {
        Log::error("'-f' and '-p' options are incompatible with '-d ...', ignoring '-p' and/or '-f'");
    }
    favs = has_favs && !has_custom_paths;
    pins = has_pins && !has_custom_paths;

    if (auto forced_lang = parser.getCmdOption("-l"); !forced_lang.empty()){
        lang = forced_lang;
    } else {
        lang = get_locale();
    }
    if (auto cols = parser.getCmdOption("-n"); !cols.empty()) {
        int n_c;
        if (parse_number(cols, n_c)) {
            if (n_c > 0 && n_c < 100) {
                num_col = n_c;
            } else {
                Log::error("Columns must be in range 1 - 99\n");
            }
        } else {
            Log::error("Invalid number of columns\n");
        }
    }

    if (pins || favs) {
        auto cache_home = get_cache_home();
        if (pins) {
            pinned_file = cache_home / "nwg-pin-cache";
        }
        if (favs) {
            cached_file = cache_home / "nwg-fav-cache";
        }
    }

    if (auto i_size = parser.getCmdOption("-s"); !i_size.empty()){
        icon_size = parse_icon_size(i_size);
    }
    oneshot = parser.cmdOptionExists("-oneshot");
}

static Gtk::Widget* make_widget(const Glib::RefPtr<Glib::Object>& object) {
    return dynamic_cast<GridBox*>(object.get());
}

static int sort_by_name(Gtk::FlowBoxChild* a, Gtk::FlowBoxChild* b) {
    return dynamic_cast<Gtk::ToggleButton*>(a->get_child())->get_label().compare(
        dynamic_cast<Gtk::ToggleButton*>(b->get_child())->get_label()
    );
}

GridWindow::GridWindow(GridConfig& config):
    PlatformWindow{ config }, config{ config }
{
    searchbox
        .signal_search_changed()
        .connect(sigc::mem_fun(*this, &GridWindow::filter_view));
    searchbox.set_placeholder_text("Type to search");
    searchbox.set_sensitive(true);
    searchbox.set_name("searchbox");

    auto setup_grid = [&](auto& grid) {
        grid.set_column_spacing(5);
        grid.set_row_spacing(5);
        grid.set_homogeneous(true);
        grid.set_halign(Gtk::ALIGN_CENTER);
        grid.set_selection_mode(Gtk::SELECTION_NONE);
    };
    setup_grid(apps_grid);
    setup_grid(favs_grid);
    setup_grid(pinned_grid);
    setup_grid(categories_box);

    categories_box.set_sort_func(&sort_by_name);

    apps_boxes = AppBoxes::create(categories);
    pinned_boxes = PinnedBoxes::create();
    fav_boxes = FavBoxes::create();

    categories_all.set_label("All");
    categories_all.signal_toggled().connect([this](){
        auto active = categories_all.get_active();
        categories.all_enabled = active;
        if (active) {
            // disable all other buttons
            categories_box.foreach([](auto& widget){
                auto& fboxchild = static_cast<Gtk::FlowBoxChild&>(widget);
                auto* button = dynamic_cast<Gtk::ToggleButton*>(fboxchild.get_child());
                if (!button) {
                    throw std::logic_error{ "Categories button is not a Gtk::ToggleButton" };
                }
                button->set_active(false);
            });
        }
        apps_boxes->on_category_toggled();
    });
    categories_all.set_name("categories_all");
    categories_all.set_active();

    // doesn't compile with lambda due to sigc bug, must use free function
    Gtk::FlowBox::SlotCreateWidget<Glib::Object> make_widget_{ &make_widget };
    pinned_grid.bind_model(pinned_boxes, make_widget_);
    favs_grid.bind_model(fav_boxes, make_widget_);
    apps_grid.bind_model(apps_boxes, make_widget_);

    description.set_ellipsize(Pango::ELLIPSIZE_END);
    description.set_text("");
    description.set_name("description");
    separator.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator.set_name("separator");
    separator1.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator1.set_name("separator");
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    outer_vbox.set_spacing(15);
    hbox_header.pack_start(searchbox, Gtk::PACK_EXPAND_PADDING, 0);
    outer_vbox.pack_start(hbox_header, Gtk::PACK_SHRINK, 0);

    scrolled_window.set_propagate_natural_height(true);
    scrolled_window.set_propagate_natural_width(true);
    scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolled_window.add(inner_vbox);

    pinned_hbox.pack_start(pinned_grid, Gtk::PACK_EXPAND_WIDGET, 0);
    inner_vbox.set_halign(Gtk::ALIGN_CENTER);
    inner_vbox.pack_start(pinned_hbox, Gtk::PACK_SHRINK, 5);
    inner_vbox.pack_start(separator1, false, true, 0);

    inner_vbox.pack_start(categories_hbox, Gtk::PACK_SHRINK, 0);
    categories_hbox.pack_start(categories_all, Gtk::PACK_EXPAND_PADDING, 0);
    categories_hbox.pack_start(categories_box, Gtk::PACK_EXPAND_PADDING, 0);

    favs_hbox.pack_start(favs_grid, true, false, 0);
    inner_vbox.pack_start(favs_hbox, false, false, 5);
    inner_vbox.pack_start(separator, false, true, 0);

    apps_hbox.pack_start(apps_grid, Gtk::PACK_EXPAND_PADDING);
    inner_vbox.pack_start(apps_hbox, Gtk::PACK_SHRINK);

    outer_vbox.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    scrolled_window.show_all_children();

    outer_vbox.pack_start(description, Gtk::PACK_SHRINK);

    this -> set_background_color(config.background_color);
    this -> add(outer_vbox);
    this -> show_all_children();
}

GridWindow::~GridWindow() {
    categories_box.foreach([this](auto & child){
        categories_box.remove(child);
    });
}

bool GridWindow::on_button_press_event(GdkEventButton *event) {
    (void) event; // suppress warning

    this->hide();
    return true;
}

bool GridWindow::on_key_press_event(GdkEventKey* key_event) {
    switch (key_event->keyval) {
        case GDK_KEY_Escape:
            this->hide();
            break;
        case GDK_KEY_Delete:
            this -> searchbox.set_text("");
            break;
        case GDK_KEY_Return:
        case GDK_KEY_Left:
        case GDK_KEY_Right:
        case GDK_KEY_Up:
        case GDK_KEY_Down:
            break;
        default:
            // Focus the searchbox:
            // because searchbox is now focused,
            // Gtk::Window will delegate the event there
            if (!this -> searchbox.is_focus()) {
                this -> searchbox.grab_focus();
                // when searchbox receives focus,
                // its contents become selected
                // and the incoming character will overwrite them
                // so we make sure to drop the selection
                this -> searchbox.select_region(0, 0);
                this -> searchbox.set_position(-1);

            }
    }
    // Delegate handling to the base class
    return Gtk::Window::on_key_press_event(key_event);
}

/*
 * In order to keep Gtk FlowBox content properly haligned, we have to maintain
 * `max_children_per_line` equal to the total number of children
 * */
inline auto refresh_max_children_per_line = [](auto& flowbox, auto& container, auto num_col) {
    auto size = container.size();
    decltype(size) cols = num_col;
    if (auto min = std::min(cols, size)) { // suppress gtk warnings about size=0
        //flowbox.set_min_children_per_line(std::min(size, cols));
        flowbox.set_max_children_per_line(std::min(size, cols));
    }
};

/* Prevent FlowBoxChild from being focused
 * explicit type to break compilation if we change container type
 * as other containers (well, most of them) don't have such quirk
 */
inline auto disable_flowbox_child_focus = [](Gtk::FlowBox& flowbox) {
    flowbox.foreach([](auto & child) {
        // child is not actually flowbox child: FlowBox{ ... FlowBoxChild{ child } ... }
        // hence the need for get_parent
        child.set_can_focus(false);
    });
};

/* Populate grid with widgets from container */
inline auto build_grid = [](auto& grid, auto& container, auto num_col) {
    refresh_max_children_per_line(grid, container, num_col);
    disable_flowbox_child_focus(grid);
};

/* Called each time `search_entry` changes, rebuilds `apps_grid` according to search criteria */
void GridWindow::filter_view() {
    apps_boxes->filter(searchbox.get_text());
    this -> refresh_separators();
    this -> focus_first_box();
    refresh_max_children_per_line(apps_grid, *apps_boxes.get(), config.num_col);
}

/* Sets separators' visibility according to grid status */
void GridWindow::refresh_separators() {
    auto set_shown = [](auto c, auto& s) { if (c) s.show(); else s.hide(); };
    auto p = !pinned_boxes->empty();
    auto f = !fav_boxes->empty();
    auto a = !apps_boxes->empty();
    // TODO: separator shrinks to dot if there is too few items in flowbox
    // can it be fixed?
    set_shown(p && f, separator1);
    set_shown((f || p) && a, separator);
}

void GridWindow::build_grids() {
    auto num_col = config.num_col;
    build_grid(this->pinned_grid, *pinned_boxes.get(), num_col);
    build_grid(this->favs_grid, *fav_boxes.get(), num_col);
    build_grid(this->apps_grid, *apps_boxes.get(), num_col);

    this -> pinned_grid.show_all_children();
    this -> favs_grid.show_all_children();
    this -> apps_grid.show_all_children();

    this -> focus_first_box();
    this -> refresh_separators();
}

void GridWindow::focus_first_box() {
    if (apps_boxes->is_filtered() && apps_boxes->size()) {
        apps_boxes->front()->grab_focus();
    } else {
        if (pinned_boxes->size()) {
            pinned_boxes->front()->grab_focus(); return;
        }
        if (fav_boxes->size()) {
            fav_boxes->front()->grab_focus(); return;
        }
        if (apps_boxes->size()) {
            apps_boxes->front()->grab_focus(); return;
        }
    }
}

void GridWindow::set_description(const Glib::ustring& text) {
    this->description.set_text(text);
}

void GridWindow::toggle_pinned(GridBox& box) {
    // pins changed, we'll need to update the cache
    this->pins_changed = true;

    // disable prelight
    box.unset_state_flags(Gtk::STATE_FLAG_PRELIGHT);

    auto& stats = this->stats_of(box);
    auto is_pinned = stats.pinned == Stats::Pinned;
    stats.pinned = Stats::PinTag{ !is_pinned };

    auto* from_grid = &this->apps_grid;
    AbstractBoxes* from = apps_boxes.get();

    if (stats.favorite) {
        from_grid = &this->favs_grid;
        from = this->fav_boxes.get();
    }

    AbstractBoxes* to = this->pinned_boxes.get();
    auto* to_grid = &this->pinned_grid;
    if (is_pinned) {
        std::swap(from, to);
        std::swap(from_grid, to_grid);
    }
    box.reference(); // reference count decreases when unparenting
    box.reference(); // TODO: this reference is required (errors otherwise), but why?
    box.reference();
    from->erase(box);
    // FlowBox { ... FlowBoxChild { box } ... }
    // it is necessary to remove box from FlowBoxChild
    // and then FlowBoxChild from FlowBox
    // as it doesn't get deleted for some reason
    if (auto parent = box.get_parent()) {
        //if (auto grid = parent->get_parent()) {
        //    grid->remove(*parent);
        //}
        from_grid->remove(*parent);
        parent->remove(box);
    }
    to->add(box);
    auto num_col = config.num_col;
    refresh_max_children_per_line(*from_grid, *from, num_col);
    refresh_max_children_per_line(*to_grid, *to, num_col);

    // refresh filters
    refresh_separators();
}


/*
 * Saves pinned cache file
 * */
void GridWindow::save_cache() {
    // if pins_changed can only be set to true if config.pins is true
    // but lets do double-check just in case
    if (config.pins && pins_changed) {
        if (std::ofstream out{ config.pinned_file, std::ios::trunc }) {
            for (auto* pin : *pinned_boxes.get()) {
                out << pin->entry->desktop_id << '\n';
            }
        } else {
            Log::error("failed to save pins to file '", config.pinned_file, "'");
        }
    }
    if (config.favs && favs_changed) {
        try {
            ns::json favs_cache;
            // find min positive clicks count
            decltype(Stats::clicks) min = 1000000; // avoid including <limits>
            for (auto& box : this->all_boxes) {
                if (auto clicks = stats_of(box).clicks; clicks > 0) {
                    min = std::min(min, clicks);
                }
            }
            // only save positives, substract min to keep clicks low, but preserve order
            for (auto& box : this->all_boxes) {
                if (auto clicks = stats_of(box).clicks - min + 1; clicks > 0) {
                    favs_cache.emplace(box.entry->desktop_id, clicks);
                }
            }
            save_json(favs_cache, config.cached_file);
        } catch (const ns::json::exception& e) {
            Log::error("unable to save favs: ", e.what());
        }
    }
}

void GridWindow::on_show() {
    // when running in server mode, the window is not scrolled back to top
    // each time it's shown
    // so we'll do it on our own
    auto hadjustment = scrolled_window.get_hadjustment();
    auto vadjustment = scrolled_window.get_vadjustment();
    hadjustment->set_value(hadjustment->get_lower());
    vadjustment->set_value(vadjustment->get_lower());
    focus_first_box();
    searchbox.set_text("");
    return PlatformWindow::on_show();
}

bool GridWindow::on_delete_event(GdkEventAny* event) {
    // no-op as on_delete_event doesn't get called when application exits w/
    this -> save_cache();
    return CommonWindow::on_delete_event(event);
}

void GridWindow::run_box(GridBox& box) {
    favs_changed = true;
    ++stats_of(box).clicks;
    auto& cmd = exec_of(box);
    // TODO: use special flag
    if (cmd.find(config.term) == 0) {
        Log::info("Running: \'", cmd, "\'");
    }
    try {
        Glib::spawn_command_line_async(cmd);
    } catch (const Glib::SpawnError& error) {
        Log::error("Failed to run command: ", error.what());
    } catch (const Glib::ShellError& error) {
        Log::error("Failed to run command: ", error.what());
    }
    hide();
}

inline auto with_box_by_id = [](auto && container, auto && desktop_id, auto && foo) {
    auto cmp = [&desktop_id](auto && box) { return box.entry->desktop_id == desktop_id; };
    if (auto iter = std::find_if(container.begin(), container.end(), cmp); iter != container.end()) {
        foo(iter);
    }
};

void GridWindow::remove_box_by_desktop_id(std::string_view desktop_id) {
    with_box_by_id(all_boxes, desktop_id, [this](auto && iter) {
        auto && box = *iter;
        unref_categories(box);
        // delete references to the widget from models
        pinned_boxes->erase(box);
        fav_boxes->erase(box);
        apps_boxes->erase(box);
        // remove flowboxchild
        box.reference();
        if (auto parent = dynamic_cast<Gtk::FlowBoxChild*>(box.get_parent())) {
            if (auto flowbox = parent->get_parent()) {
                flowbox->remove(*parent);
            }
        }
        // delete the actual widget
        all_boxes.erase(iter);
    });
}

void GridWindow::update_box_by_id(std::string_view desktop_id, GridBox && new_box) {
    with_box_by_id(all_boxes, desktop_id, [this,&new_box](auto && iter) {
        auto && box = *iter;
        auto && new_box_ref = all_boxes.emplace_front(std::move(new_box));

        ref_categories(new_box_ref);
        unref_categories(box);

        pinned_boxes->update(box, new_box_ref);
        fav_boxes->update(box, new_box_ref);
        apps_boxes->update(box, new_box_ref);
        all_boxes.erase(iter);
    });
}

void GridWindow::ref_categories(const GridBox& box) {
    for (auto && category: box.entry->desktop_entry_->categories) {
        if (auto [index, inserted] = categories.ref(category); inserted) {
            std::string_view category{ index->category };
            auto* button = Gtk::make_managed<CategoryButton>(index->category, categories, index);
            index->button = button;
            button->show();
            categories_box.insert(*button, -1);
            button->set_active(false);

            auto* fboxchild = dynamic_cast<Gtk::FlowBoxChild*>(button->get_parent());
            if (fboxchild) {
                fboxchild->set_can_focus(false);
            }

            // initial state: ALL active, others disabled
            // any: enable clicked, disable ALL & other active buttons
            // C+any: disable ALL, enable clicked

            button->signal_toggled().connect([this, category, button]() {
                auto active = button->get_active();
                if (active) {
                    categories_all.set_active(false);
                }
                categories.all_enabled = categories_all.get_active();
                categories.toggle(category);

                this->apps_boxes->on_category_toggled();
                this -> refresh_separators();
                this -> focus_first_box();
                refresh_max_children_per_line(apps_grid, *apps_boxes.get(), config.num_col);
            });
        }
    }
}

void GridWindow::unref_categories(GridBox& box) {
    for (auto && category: box.entry->desktop_entry_->categories) {
        if (auto [index, deleted] = categories.unref(category); deleted) {
            auto& button = *index->button;
            auto* parent = dynamic_cast<Gtk::Widget*>(button.get_parent());
            if (!parent) {
                throw std::logic_error{ "CategoryButton::get_parent returned non-widget" };
            }
            categories_box.remove(*parent);
        }
    }
}

CategoryButton::CategoryButton(const std::string& name, CategoriesSet& set, CategoriesSet::Index index):
    Gtk::ToggleButton{ name }, categories{ set }, index{ index }
{
    modifiers = Gtk::AccelGroup::get_default_mod_mask();
}

CategoryButton::~CategoryButton() {
    categories.delete_by_index(index);
}

bool CategoriesSet::toggle(std::string_view category) {
    if (auto iter = active_categories.find(category); iter != active_categories.end()) {
        active_categories.extract(iter);
        all_enabled = active_categories.empty();
        return false;
    } else {
        active_categories.emplace_hint(iter, category);
        return true;
    }
}

std::pair<CategoriesSet::Index, bool> CategoriesSet::ref(std::string_view category) {
    std::pair<Index, bool> ret{ {}, false };
    if (auto iter = categories.find(category); iter != categories.end()) {
        ++iter->second->refs;
        return ret;
    } else {
        auto& ref = categories_store.emplace_front(category);
        ret.first = categories_store.begin();
        categories.emplace_hint(iter, ref.category, ret.first);
        ret.second = true;
    }
    return ret;
}

std::pair<CategoriesSet::Index, bool> CategoriesSet::unref(std::string_view category) {
    std::pair<Index, bool> ret{ {}, false };
    if (auto iter = categories.find(category); iter != categories.end()) {
        --iter->second->refs;
        ret.second = !iter->second->refs;
        if (ret.second) {
            auto node = categories.extract(iter);
            active_categories.extract(category);
            ret.first = node.mapped();
        }
        all_enabled = active_categories.empty();
        return ret;
    }
    throw std::logic_error{ "Trying to unref non-existing category" };
}

void CategoriesSet::delete_by_index(Index index) {
    categories_store.erase(index);
}

inline auto enabled_impl = [](auto && cs) {
    return [&](auto c) { return cs.find(c) != cs.end(); };
};

bool CategoriesSet::enabled(std::string_view category) const {
    return all_enabled || enabled_impl(active_categories)(category);
}

bool CategoriesSet::enabled(const GridBox& box) const {
    auto && categories = box.entry->desktop_entry_->categories;
    return all_enabled || std::any_of(
        categories.begin(),
        categories.end(),
        enabled_impl(active_categories)
    );
}

GridBox::GridBox(Glib::ustring name, Glib::ustring comment, Entry& entry)
: name(std::move(name)), comment(std::move(comment)), entry{ &entry } {
    // As we sort dynamically by actual names, we need to avoid shortening them, or long names will remain unsorted.
    // See the issue: https://github.com/nwg-piotr/nwg-launchers/issues/128
    auto display_name = this->name;
    if (display_name.length() > 25) {
       display_name.resize(22);
       display_name += "...";
    }
    this->set_always_show_image(true);
    this->set_label(display_name);
    this->set_image_position(Gtk::POS_TOP);
}

bool GridBox::on_button_press_event(GdkEventButton* event) {
    auto& toplevel = *dynamic_cast<GridWindow*>(this -> get_toplevel());
    if (toplevel.config.pins && event->button == 3) { // right-clicked
        toplevel.toggle_pinned(*this);
    } else {
        this -> activate();
    }
    return Gtk::Button::on_button_press_event(event);
}

bool GridBox::on_focus_in_event(GdkEventFocus* event) {
    (void) event; // suppress warning

    auto& toplevel = *dynamic_cast<GridWindow*>(this->get_toplevel());
    toplevel.set_description(comment);
    return true;
}

void GridBox::on_enter() {
    auto& toplevel = *dynamic_cast<GridWindow*>(this->get_toplevel());
    toplevel.set_description(comment);
    return Gtk::Button::on_enter();
}

void GridBox::on_activate() {
    auto& toplevel = *dynamic_cast<GridWindow*>(this->get_toplevel());
    toplevel.run_box(*this);
}

void GridInstance::on_sighup() {
    Log::error("let's assume we reload something");
}

void GridInstance::on_sigusr1() {
    window.show(hint::Fullscreen);
}

void GridInstance::on_sigint() {
    app.release();
}

void GridInstance::on_sigterm() {
    app.release();
}
