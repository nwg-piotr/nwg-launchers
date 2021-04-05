/* GTK-based dmenu
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

#include <filesystem>
#include <fstream>
#include "dmenu.h"

inline auto set_searchbox_placeholder = [](auto && searchbox, auto case_sensitive) {
    constexpr std::array placeholders { "TYPE TO SEARCH", "Type to Search" };
    searchbox.set_placeholder_text(placeholders[case_sensitive]);
};

inline auto build_commands_list = [](auto && dmenu, auto && commands, auto max) {
    decltype(max) count{ 0 };
    for (auto && command: commands) {
        dmenu.emplace_back(command);
        count++;
        if (count == max) {
            break;
        }
    }
};

MainWindow::MainWindow(Config& config, std::vector<Glib::ustring>& src):
    PlatformWindow(config),
    commands{ 1, false, Gtk::SELECTION_SINGLE },
    commands_source{ src }
{
    // different shells emit different events
    auto display_name = this->get_screen()->get_display()->get_name();
    auto is_wayland = display_name.find("wayland") == 0;
    // non-void lambdas are broken in gtkmm, thus the need for bind_return
    signal_leave_notify_event().connect(sigc::bind_return([this,is_wayland](auto* event) {
        constexpr std::array windowing {
            std::array { GDK_NOTIFY_ANCESTOR, GDK_NOTIFY_VIRTUAL }, // X11 (i3 and openbox atleast)
            std::array { GDK_NOTIFY_NONLINEAR, GDK_NOTIFY_NONLINEAR_VIRTUAL } // Wayland (wlr-layer-shell)
        };
        for (auto detail: windowing[is_wayland]) {
            if (event->detail == detail) {
                this->close();
            }
        }
    }, true));
    commands.set_name("commands");
    commands.set_reorderable(false);
    commands.set_headers_visible(false);
    commands.set_enable_search(false);
    commands.set_hover_selection(true);
    commands.set_activate_on_single_click(true);
    commands.signal_row_activated().connect([this](auto && path, auto*) {
        // this is ******
        auto model = this->commands.get_model();
        auto iter = model->get_iter(path);
        Glib::ustring item;
        iter->get_value(0, item);
        item += " &";
        system(item.c_str());
        this->close();
    });
    searchbox.set_name("searchbox");
    if (show_searchbox) {
        set_searchbox_placeholder(searchbox, case_sensitive);
        searchbox.signal_search_changed().connect(sigc::mem_fun(*this, &MainWindow::filter_view));
        vbox.pack_start(searchbox, false, false);
    }
    vbox.pack_start(commands);
    
    add(vbox);
    
    build_commands_list(*this, commands_source, rows);
}

MainWindow::~MainWindow() {
    using namespace std::string_view_literals;
    if (case_sensitivity_changed) {
        std::ofstream file{ settings_file, std::ios::trunc };
        constexpr std::array values { "case_insensitive"sv, "case_sensitive"sv };
        file << values[case_sensitive];
    }
}

void MainWindow::emplace_back(const Glib::ustring& command) {
    this->commands.append(command);
}

void MainWindow::filter_view() {
    auto model_refptr = commands.get_model();
    auto& model = dynamic_cast<Gtk::ListStore&>(*model_refptr.get());
    model.clear();
    auto search_phrase = searchbox.get_text();
    if (search_phrase.length() > 0) {
        // append at most `max` entries satisfying `matches` predicate, return count
        auto fill_matches = [this](auto && source, auto && matches, auto max) {
            decltype(max) count = 0;
            auto begin = source.begin(); auto end = source.end();
            while (begin < end && count < max) {
                auto && command = *begin++;
                if (matches(command)) {
                    this->emplace_back(command);
                    count++;
                }
            }
            return count;
        };
        // append entries matching `exact`, then entries matching `almost` (at most `max` entries)
        auto fill_all = [this,fill_matches,rows=rows](auto && exact, auto && almost) {
            auto count = fill_matches(this->commands_source, exact, rows);
            if (count < rows) {
                fill_matches(this->commands_source, almost, rows - count);
            }
        };
        if (case_sensitive) {
            fill_all([&a=search_phrase](auto && b){ return b.find(a) == 0; },
                     [&a=search_phrase](auto && b){ auto r = b.find(a); return r > 0 && r != a.npos; });
        } else {
            auto sf = search_phrase.casefold();
            fill_all([&a=sf](auto && b){ return b.casefold().find(a) == 0; },
                     [&a=sf](auto && b){ auto r = b.casefold().find(a); return r > 0 && r != a.npos; });
        }
    } else {
        // searchentry is clear, show all options
        build_commands_list(*this, commands_source, rows);
    }
    select_first_item();
}

void MainWindow::select_first_item() {
    Gtk::ListStore::Path path{1};
    commands.set_cursor(path);
    commands.grab_focus();
}

void MainWindow::switch_case_sensitivity() {
    case_sensitivity_changed = true;
    case_sensitive = !case_sensitive;
    searchbox.set_text("");
    set_searchbox_placeholder(searchbox, case_sensitive);
}

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
    switch (key_event->keyval) {
        case GDK_KEY_Escape:
            close();
            break;
        case GDK_KEY_Delete:
            if (show_searchbox) {
                searchbox.set_text("");
            }
            break;
        case GDK_KEY_Insert:
            if (show_searchbox) {
                switch_case_sensitivity();
                searchbox.set_text("");
            }
            break;
        case GDK_KEY_Left:
        case GDK_KEY_Right:
        case GDK_KEY_Up:
        case GDK_KEY_Down:
            break;
        case GDK_KEY_Return:
            // handling code assigned in constructor
            break;
        default:
            if (show_searchbox) {
                searchbox.grab_focus();
                searchbox.select_region(0, 0);
                searchbox.set_position(-1);
            }
    }

    //if the event has not been handled, call the base class
    return CommonWindow::on_key_press_event(key_event);
}

// TreeView will have height of 1 until it's actually shown
// in this hack we do our best to calculate actual window size
// we assume all cells have same height (which is true for ListViewText) and all cells are filled
int MainWindow::get_height() {
    auto model = commands.get_model();
    // Gtk::TreeModel::iter_n_root_children is protected, so ...
    auto rows = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model->gobj()), nullptr);
    auto base_height = CommonWindow::get_height();
    int off_x = -1, off_y = -1, cell_width = -1, cell_height = -1;
    Gdk::Rectangle rect;
    auto* column = commands.get_column(0);
    column->cell_get_size(rect, off_x, off_y, cell_width, cell_height);
    auto cell_spacing = column->get_spacing();
    return base_height + cell_height * (rows + 1) + cell_spacing * rows;
}
