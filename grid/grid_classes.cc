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

MainWindow::MainWindow(size_t fav_size, size_t pinned_size)
 : CommonWindow("~nwggrid", "~nwggrid")
{
    searchbox
        .signal_search_changed()
        .connect(sigc::mem_fun(*this, &MainWindow::filter_view));
    apps_grid.set_column_spacing(5);
    apps_grid.set_row_spacing(5);
    apps_grid.set_column_homogeneous(true);
    favs_grid.set_column_spacing(5);
    favs_grid.set_row_spacing(5);
    favs_grid.set_column_homogeneous(true);
    pinned_grid.set_column_spacing(5);
    pinned_grid.set_row_spacing(5);
    pinned_grid.set_column_homogeneous(true);
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
    hbox_header.pack_start(searchbox, Gtk::PACK_EXPAND_PADDING);
    outer_vbox.pack_start(hbox_header, Gtk::PACK_SHRINK, Gtk::PACK_EXPAND_PADDING);

    scrolled_window.set_propagate_natural_height(true);
    scrolled_window.set_propagate_natural_width(true);
    scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
    scrolled_window.add(inner_vbox);

    pinned_hbox.pack_start(pinned_grid, true, false, 0);
    inner_vbox.pack_start(pinned_hbox, false, false, 5);
    if (pins && pinned_size > 0) {
        inner_vbox.pack_start(separator1, false, true, 0);
    }
    
    favs_hbox.pack_start(favs_grid, true, false, 0);
    inner_vbox.pack_start(favs_hbox, false, false, 5);
    // if favs disabled, fav_size == 0
    if (fav_size > 0) {
        inner_vbox.pack_start(separator, false, true, 0);
    }

    apps_hbox.pack_start(apps_grid, Gtk::PACK_EXPAND_PADDING);
    inner_vbox.pack_start(apps_hbox, true, true, 0);

    outer_vbox.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    scrolled_window.show_all_children();

    outer_vbox.pack_start(description, Gtk::PACK_SHRINK);
    
    this -> add(outer_vbox);
    this -> show_all_children();
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
                this -> searchbox.prepare_to_insertion();
            }
    }
    // Delegate handling to the base class
    return Gtk::Window::on_key_press_event(key_event);
}

inline auto advance = [](auto& x, auto& y) {
    x++;
    if (x == num_col) {
        x = 0;
        y++;
    }
};

// fills empty `grid` with elements from `container` respecting `num_col` global variable
inline auto build_grid = [](auto& grid, auto& container) {
    int x = 0;
    int y = 0;
    for (auto* child : container) {
        grid.attach(*child, x, y, 1, 1);
        advance(x, y);
    }
};

// shifts elements of `grid` from `x_`, `y_` to `x__`, `y__` respecting `num_col` global variable,
// effectively filling the hole created by removing an element
inline auto fill_hole = [](auto x_, auto y_, auto& grid, auto x__, auto y__) {
    auto x = x_;
    auto y = y_;

    advance(x, y);
    advance(x__, y__);

    while (x != x__ || y != y__) {
        auto child = grid.get_child_at(x, y);
        if (child) {
            grid.child_property_left_attach(*child).set_value(x_);
            grid.child_property_top_attach(*child).set_value(y_);
            advance(x_, y_);
        }
        advance(x, y);
    }
};

void MainWindow::filter_view() {
    auto& grid = apps_grid;
    auto* container = &apps_boxes;

    auto search_phrase = searchbox.get_text();
    auto filtered = search_phrase.size() > 0;
    if (filtered) {
        auto phrase = search_phrase.casefold();
        auto matches = [&phrase](auto& str) {
            return str.casefold().find(phrase) != Glib::ustring::npos;
        };

        this -> filtered_boxes.clear();
        for (auto& box : this -> apps_boxes) {
            if (matches(box->name) || matches(box->exec) || matches(box->comment)) {
                this -> filtered_boxes.emplace_back(box);
            }
        }
        this -> favs_grid.hide();
        this -> separator.hide();

        container = &this->filtered_boxes;
    } else {
        this -> favs_grid.show();
        this -> separator.show();
    }
    grid.freeze_child_notify();
    grid.foreach([&grid](auto& child) {
        grid.remove(child);
        child.unset_state_flags(Gtk::STATE_FLAG_PRELIGHT);
    });
    build_grid(grid, *container);
    this -> focus_first_box();
    grid.thaw_child_notify();
}

void MainWindow::build_grids() {
    this -> pinned_grid.freeze_child_notify();
    this -> favs_grid.freeze_child_notify();
    this -> apps_grid.freeze_child_notify();

    build_grid(this->pinned_grid, this->pinned_boxes);
    build_grid(this->favs_grid, this->fav_boxes);
    build_grid(this->apps_grid, this->apps_boxes);

    this -> focus_first_box();

    this -> pinned_grid.show_all_children();
    this -> favs_grid.show_all_children();
    this -> apps_grid.show_all_children();

    this -> pinned_grid.thaw_child_notify();
    this -> favs_grid.thaw_child_notify();
    this -> apps_grid.thaw_child_notify();

    this -> set_description(std::to_string(apps_boxes.size()));
}

void MainWindow::focus_first_box() {
    auto containers = { &filtered_boxes, &pinned_boxes, &fav_boxes, &apps_boxes };
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

    // init just to use type deduction
    auto* from_grid = &this->apps_grid;
    auto* from = &this->apps_boxes;

    // actual initialization here
    switch (box.favorite) {
        case GridBox::Common:
            from = &this->apps_boxes;
            from_grid = &this->apps_grid;
            break;
        case GridBox::Favorite:
            from = &this->fav_boxes;
            from_grid = &this->favs_grid;
            break;
    }
    auto* to = &this->pinned_boxes;
    auto* to_grid = &this->pinned_grid;
    if (is_pinned) {
        std::swap(from, to);
        std::swap(from_grid, to_grid);
    }

    // now `to` is the container we want to move `box` to from `from` :^)
    // and `from_grid` is where we move to `to_grid` from
    std::remove(from->begin(), from->end(), &box);

    auto x = from_grid->child_property_left_attach(box).get_value();
    auto y = from_grid->child_property_top_attach(box).get_value();

    // `from` is not empty, it contains atleast the widget we want to remove
    auto last = from->back();
    auto x_ = from_grid->child_property_left_attach(*last).get_value();
    auto y_ = from_grid->child_property_top_attach(*last).get_value();

    decltype(x_) xnew = 0;
    decltype(y_) ynew = 0;

    if (to->size() > 0) {
        auto last = to->back();
        xnew = to_grid->child_property_left_attach(*last).get_value();
        ynew = to_grid->child_property_top_attach(*last).get_value();
        advance(xnew, ynew);
    }

    if (x > x_ && y > y_) {
        std::cerr << "ERROR: Critical failure -- failed to retrieve box coordinates\n";
        std::abort();
    }
    from_grid->freeze_child_notify();    
    from_grid->remove(box);
    fill_hole(x, y, *from_grid, x_, y_);
    from_grid->thaw_child_notify();    

    to->push_back(&box);
    to_grid->attach(box, xnew, ynew, 1, 1);
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
    exec.append(" &");
    std::system(exec.data());
    auto toplevel = dynamic_cast<MainWindow*>(this->get_toplevel());
    toplevel->close();
}

GridSearch::GridSearch() {
    set_placeholder_text("Type to search");
    set_sensitive(true);
    set_name("searchbox");
}

void GridSearch::prepare_to_insertion() {
    select_region(0, 0);
    set_position(-1);
}
