
/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
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

#include "nwg_tools.h"
#include "grid.h"

// we only store GridBoxes inside of our FlowBoxes, so dynamic_cast won't fail
inline auto child_ = [](auto c) -> auto& { return *dynamic_cast<GridBox*>(c->get_child()); };
int by_name(Gtk::FlowBoxChild* a, Gtk::FlowBoxChild* b) {
    return child_(a).name.compare(child_(b).name);
}
// return -1 if a < b, 0 if a == b, 1 if a > b
inline auto cmp_ = [](auto a, auto b) { return int(a > b) - int(a < b); };
int by_position(Gtk::FlowBoxChild* a, Gtk::FlowBoxChild* b) {
    auto& toplevel = *dynamic_cast<MainWindow*>(a->get_toplevel());
    return cmp_(toplevel.stats_of(child_(a)).position, toplevel.stats_of(child_(b)).position);
}
int by_clicks(Gtk::FlowBoxChild* a, Gtk::FlowBoxChild* b) {
    auto& toplevel = *dynamic_cast<MainWindow*>(a->get_toplevel());
    return -cmp_(toplevel.stats_of(child_(a)).clicks, toplevel.stats_of(child_(b)).clicks);
}
MainWindow::MainWindow(Config& config, Span<std::string> es, Span<Stats> ss)
 : PlatformWindow(config), execs(es), stats(ss)
{
    searchbox
        .signal_search_changed()
        .connect(sigc::mem_fun(*this, &MainWindow::filter_view));
    searchbox.set_placeholder_text("Type to search");
    searchbox.set_sensitive(true);
    searchbox.set_name("searchbox");

    auto setup_grid = [](auto& grid) {
        grid.set_column_spacing(5);
        grid.set_row_spacing(5);
        grid.set_homogeneous(true);
        grid.set_halign(Gtk::ALIGN_CENTER);
        grid.set_selection_mode(Gtk::SELECTION_NONE);
    };
    setup_grid(apps_grid);
    setup_grid(favs_grid);
    setup_grid(pinned_grid);

    pinned_grid.set_sort_func(&by_position);
    apps_grid.set_sort_func(&by_name);
    favs_grid.set_sort_func(&by_clicks);

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

    favs_hbox.pack_start(favs_grid, true, false, 0);
    inner_vbox.pack_start(favs_hbox, false, false, 5);
    inner_vbox.pack_start(separator, false, true, 0);

    apps_hbox.pack_start(apps_grid, Gtk::PACK_EXPAND_PADDING);
    inner_vbox.pack_start(apps_hbox, Gtk::PACK_SHRINK);

    outer_vbox.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    scrolled_window.show_all_children();

    outer_vbox.pack_start(description, Gtk::PACK_SHRINK);

    this -> add(outer_vbox);
    this -> show_all_children();
}

bool MainWindow::on_button_press_event(GdkEventButton *event) {
    (void) event; // suppress warning

    this->close();
    return true;
}

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
    switch (key_event->keyval) {
        case GDK_KEY_Escape:
            this->close();
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
inline auto refresh_max_children_per_line = [](auto& flowbox, auto& container) {
    auto size = container.size();
    decltype(size) cols = num_col;
    if (auto min = std::min(cols, size)) { // suppress gtk warnings about size=0
        flowbox.set_min_children_per_line(std::min(size, cols));
        flowbox.set_max_children_per_line(std::min(size, cols));
    }
};

/* Populate grid with widgets from container */
inline auto build_grid = [](auto& grid, auto& container) {
    for (auto child : container) {
        grid.add(*child);
        child->get_parent()->set_can_focus(false); // FlowBoxChild shouldn't consume focus
    }
    refresh_max_children_per_line(grid, container);
};

/* Called each time `search_entry` changes, rebuilds `apps_grid` according to search criteria */
void MainWindow::filter_view() {
    auto clean_grid = [](auto& grid) {
        grid.foreach([&grid](auto& child) {
            grid.remove(child);
            dynamic_cast<Gtk::FlowBoxChild*>(&child)->remove();
        });
    };
    auto search_phrase = searchbox.get_text();
    is_filtered = search_phrase.size() > 0;
    filtered_boxes.clear();
    apps_grid.freeze_child_notify();
    if (is_filtered) {
        auto phrase = search_phrase.casefold();
        auto matches = [&phrase](auto& str) {
            return str.casefold().find(phrase) != Glib::ustring::npos;
        };
        for (auto* box : apps_boxes) {
            auto& exec = this->exec_of(*box);
            auto matches_exec = exec.find(phrase) != std::string::npos;
            if (matches(box->name) || matches_exec || matches(box->comment)) {
                filtered_boxes.push_back(box);
            }
        }
        clean_grid(apps_grid);
        build_grid(apps_grid, filtered_boxes);
    } else {
        clean_grid(apps_grid);
        build_grid(apps_grid, apps_boxes);
    }
    this -> refresh_separators();
    this -> focus_first_box();
    apps_grid.thaw_child_notify();
}

/* Sets separators' visibility according to grid status */
void MainWindow::refresh_separators() {
    auto set_shown = [](auto c, auto& s) { if (c) s.show(); else s.hide(); };
    auto p = !pinned_boxes.empty();
    auto f = !fav_boxes.empty();
    auto a1 = !filtered_boxes.empty() && is_filtered;
    auto a2 = !apps_boxes.empty() && !is_filtered;
    auto a = a1 || a2;
    set_shown(p && f, separator1);
    set_shown(f && a, separator);
    if (p && a) {
        separator.show();
    }
}

void MainWindow::build_grids() {
    this -> pinned_grid.freeze_child_notify();
    this -> favs_grid.freeze_child_notify();
    this -> apps_grid.freeze_child_notify();

    build_grid(this->pinned_grid, this->pinned_boxes);
    build_grid(this->favs_grid, this->fav_boxes);
    build_grid(this->apps_grid, this->apps_boxes);

    this -> monotonic_index = this->pinned_boxes.size();

    this -> pinned_grid.show_all_children();
    this -> favs_grid.show_all_children();
    this -> apps_grid.show_all_children();

    this -> pinned_grid.thaw_child_notify();
    this -> favs_grid.thaw_child_notify();
    this -> apps_grid.thaw_child_notify();

    this -> focus_first_box();
    this -> refresh_separators();
}

void MainWindow::focus_first_box() {
    // flowbox -> flowboxchild -> gridbox
    if (is_filtered) {
        if (auto child = apps_grid.get_child_at_index(0)) {
            child->get_child()->grab_focus();
            return;
        }
    }
    std::array grids { &pinned_grid,  &favs_grid, &apps_grid };
    for (auto grid : grids) {
        if (auto child = grid->get_child_at_index(0)) {
            child->get_child()->grab_focus();
            return;
        }
    }
}

void MainWindow::set_description(const Glib::ustring& text) {
    this->description.set_text(text);
}

void MainWindow::toggle_pinned(GridBox& box) {
    // pins changed, we'll need to update the cache
    this->pins_changed = true;

    // disable prelight
    box.unset_state_flags(Gtk::STATE_FLAG_PRELIGHT);

    auto& stats = this->stats_of(box);
    auto is_pinned = stats.pinned == Stats::Pinned;
    stats.pinned = Stats::PinTag{ !is_pinned };

    // monotonic index increases each time an entry is pinned
    // ensuring it will appear last
    stats.position = this->monotonic_index * !is_pinned;
    this->monotonic_index += !is_pinned;

    auto* from_grid = &this->apps_grid;
    auto* from = &this->apps_boxes;

    if (stats.favorite) {
        from_grid = &this->favs_grid;
        from = &this->fav_boxes;
    }

    auto* to = &this->pinned_boxes;
    auto* to_grid = &this->pinned_grid;
    if (is_pinned) {
        std::swap(from, to);
        std::swap(from_grid, to_grid);
    }
    auto to_remove = std::remove(from->begin(), from->end(), &box);;
    from->erase(to_remove);
    to->push_back(&box);
    refresh_max_children_per_line(*from_grid, *from);
    refresh_max_children_per_line(*to_grid, *to);

    // get_parent is important
    // FlowBox { ... FlowBoxChild { box } ... }
    // box is not child to FlowBox
    // but its parent, FlowBoxChild is
    // so we need to reparent FlowBoxChild, not the box itself
    box.get_parent()->reparent(*to_grid);
    // refresh filter if needed
    if (is_filtered) {
        this->filter_view();
    } else {
        this->refresh_separators();
    }
}


/*
 * Saves pinned cache file
 * */
void MainWindow::save_cache() {
    if (pins_changed) {
        std::sort(pinned_boxes.begin(), pinned_boxes.end(), [this](auto* a, auto* b) {
            return this->stats_of(*a).position < this->stats_of(*b).position;
        });
        std::ofstream out(pinned_file, std::ios::trunc);
        for (auto* pin : this->pinned_boxes) {
            out << *pin->desktop_id << '\n';
        }
    }
    if (favs) {
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
                favs_cache[*box.desktop_id] = clicks;
            }
        }
        save_json(favs_cache, cache_file);
    }
}

bool MainWindow::on_delete_event(GdkEventAny* event) {
    this -> save_cache();
    return CommonWindow::on_delete_event(event);
}

GridBox::GridBox(Glib::ustring name, Glib::ustring comment, const std::string& id, std::size_t index)
: name(std::move(name)), comment(std::move(comment)), desktop_id(&id), index(index) {
    // As we sort dynamically by actual names, we need to avoid shortening them, or long names will remain unsorted.
    // See the issue: https://github.com/nwg-piotr/nwg-launchers/issues/128
    auto display_name = this->name;
    if (display_name.length() > 25) {
       display_name.resize(22);
       display_name += "...";
    }
    this->set_always_show_image(true);
    this->set_label(display_name);
}

bool GridBox::on_button_press_event(GdkEventButton* event) {
    auto& toplevel = *dynamic_cast<MainWindow*>(this -> get_toplevel());
    if (pins && event->button == 3) { // right-clicked
        toplevel.toggle_pinned(*this);
    } else {
        this -> activate();
    }
    return Gtk::Button::on_button_press_event(event);
}

bool GridBox::on_focus_in_event(GdkEventFocus* event) {
    (void) event; // suppress warning

    auto& toplevel = *dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel.set_description(comment);
    return true;
}

void GridBox::on_enter() {
    auto& toplevel = *dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel.set_description(comment);
    return Gtk::Button::on_enter();
}

void GridBox::on_activate() {
    auto& toplevel = *dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel.stats_of(*this).clicks++;
    std::string cmd = toplevel.exec_of(*this);
    cmd += " &";
    if (cmd.find(term) == 0) {
        std::cout << "Running: \'" << cmd << "\'\n";
    }
    std::system(cmd.data());
    toplevel.close();
}
