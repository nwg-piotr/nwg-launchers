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

int by_name(Gtk::FlowBoxChild* a_, Gtk::FlowBoxChild* b_) {
    auto a = dynamic_cast<GridBox*>(a_->get_child());
    auto b = dynamic_cast<GridBox*>(b_->get_child());
    return a->name.compare(b->name);
}

MainWindow::MainWindow()
 : CommonWindow("~nwggrid", "~nwggrid")
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
    apps_grid.set_sort_func(&by_name);

    description.set_text("");
    description.set_name("description");
    separator.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator.set_name("separator");
    separator1.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
    separator1.set_name("separator");
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    // We can not go fullscreen() here:
    // On sway the window would become opaque - we don't want it
    // On i3 all windows below will be hidden - we don't want it as well
    if (wm != "sway" && wm != "i3") {
        fullscreen();
    } else {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
        set_decorated(false);
    }
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
    auto key_val = key_event -> keyval;
    switch (key_val) {
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
    if (auto min = std::min(cols, size)) {
        flowbox.set_min_children_per_line(std::min(size, cols));
        flowbox.set_max_children_per_line(std::min(size, cols));
    }
};
/*
 * Populate grid with widgets from container
 * */
inline auto build_grid = [](auto& grid, auto& container) {
    for (auto child : container) {
        grid.add(*child);
        child->get_parent()->set_can_focus(false); // FlowBoxChild shouldn't consume focus
    }
    refresh_max_children_per_line(grid, container);
};

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
            if (matches(box->name) || matches(box->exec) || matches(box->comment)) {
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
    std::array containers{ &filtered_boxes, &pinned_boxes, &fav_boxes, &apps_boxes };
    for (auto container : containers) {
        if (container->size() > 0) {
            container->front()->grab_focus();
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

    auto is_pinned = box.pinned == GridBox::Pinned;
    box.pinned = GridBox::PinTag{ !is_pinned };

    auto* from_grid = &this->apps_grid;
    auto* from = &this->apps_boxes;

    if (box.favorite) {
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
    this->refresh_separators();
}

/*
 * Saves pinned cache file
 * */
bool MainWindow::on_delete_event(GdkEventAny* event) {
    if (this->pins_changed) {
        std::ofstream out_file(pinned_file);
        for (auto pin : this->pinned_boxes) {
            out_file << pin->exec << '\n';
        }
    }
    return CommonWindow::on_delete_event(event);
}

bool MainWindow::has_fav_with_exec(const std::string& exec) const {
    auto pred = [&exec](auto* b) { return exec == b->exec; };
    return std::find_if(fav_boxes.begin(), fav_boxes.end(), pred) != fav_boxes.end();
}

// This constructor is not needed since C++20
GridBox::GridBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment, GridBox::FavTag fav, GridBox::PinTag pinned)
 : AppBox(std::move(name), std::move(exec), std::move(comment)), favorite(fav), pinned(pinned) {}

bool GridBox::on_button_press_event(GdkEventButton* event) {
    if (pins && event->button == 3) {
        auto toplevel = dynamic_cast<MainWindow*>(this -> get_toplevel());
        toplevel->toggle_pinned(*this);
    } else {
        int clicks = 0;
        try {
            clicks = cache[exec];
            clicks++;
        } catch(...) {
            clicks = 1;
        }
        cache[exec] = clicks;
        save_json(cache, cache_file);    

        this -> activate();
    }
    return AppBox::on_button_press_event(event);
}

bool GridBox::on_focus_in_event(GdkEventFocus* event) {
    (void) event; // suppress warning

    auto toplevel = dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel->set_description(comment);
    return true;
}

void GridBox::on_enter() {
    auto toplevel = dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel->set_description(comment);
    return AppBox::on_enter();
}

void GridBox::on_activate() {
    auto cmd = exec + " &";
    std::system(cmd.data());
    auto toplevel = dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel->close();
}
