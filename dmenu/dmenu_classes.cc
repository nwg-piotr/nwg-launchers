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

Anchor::Anchor() {
    //~ set_size_request(1, 1);
}

Anchor::~Anchor() {
}

bool Anchor::on_focus_in_event(GdkEventFocus* focus_event) {
    Gdk::Gravity gravity_widget {Gdk::GRAVITY_CENTER};
    Gdk::Gravity gravity_menu {Gdk::GRAVITY_CENTER};
    if (wm == "sway" || wm == "i3") {
        if (v_align == "t") {
            gravity_widget = Gdk::GRAVITY_NORTH;
            gravity_menu = Gdk::GRAVITY_NORTH;
        } else if (v_align == "b") {
            gravity_widget = Gdk::GRAVITY_SOUTH;
            gravity_menu = Gdk::GRAVITY_SOUTH;
        }
    } else {
        if (v_align == "t") {
            gravity_widget = Gdk::GRAVITY_SOUTH;
            gravity_menu = Gdk::GRAVITY_SOUTH;
        } else if (v_align == "b") {
            gravity_widget = Gdk::GRAVITY_NORTH;
            gravity_menu = Gdk::GRAVITY_NORTH;
        }
    }
    main_menu->popup_at_widget(this, gravity_widget, gravity_menu, nullptr);

    return true;
}

DMenu::DMenu() {
    searchbox.set_text("Type to search");
    searchbox.set_sensitive(false);
    searchbox.set_name("searchbox");
    search_phrase = "";
}

DMenu::~DMenu() {
}

bool DMenu::on_key_press_event(GdkEventKey* key_event) {
    if (show_searchbox) {
        if (key_event -> keyval == GDK_KEY_Escape) {
            Gtk::Main::quit();
            return Gtk::Menu::on_key_press_event(key_event);
        } else if (((key_event -> keyval >= GDK_KEY_A && key_event -> keyval <= GDK_KEY_Z)
            || (key_event -> keyval >= GDK_KEY_a && key_event -> keyval <= GDK_KEY_z)
            || (key_event -> keyval >= GDK_KEY_0 && key_event -> keyval <= GDK_KEY_9)
            || key_event -> keyval == GDK_KEY_plus
            || key_event -> keyval == GDK_KEY_minus
            || key_event -> keyval == GDK_KEY_underscore
            || key_event -> keyval == GDK_KEY_hyphen)
            && key_event->type == GDK_KEY_PRESS) {

            char character = key_event -> keyval;
            this -> search_phrase += character;

            this -> searchbox.set_text(this -> search_phrase);
            this -> filter_view();
            return true;

        } else if (key_event -> keyval == GDK_KEY_BackSpace && this -> search_phrase.size() > 0) {
            this -> search_phrase = this -> search_phrase.substr(0, this -> search_phrase.size() - 1);
            this -> searchbox.set_text(this -> search_phrase);
            this -> filter_view();
            return true;
        } else if (key_event -> keyval == GDK_KEY_Delete) {
			this -> search_phrase = "";
			this -> searchbox.set_text(this -> search_phrase);
			this -> filter_view();
			return true;
        }
    }
    //if the event has not been handled, call the base class
    return Gtk::Menu::on_key_press_event(key_event);
}

void on_item_clicked(std::string cmd) {
    if (dmenu_run) {
        cmd = cmd + " &";
        const char *command = cmd.c_str();
        std::system(command);
    } else {
        std::cout << cmd;
    }
    Gtk::Main::quit();
}

void DMenu::on_item_clicked(Glib::ustring cmd) {
    if (dmenu_run) {
        cmd = cmd + " &";
        const char *command = cmd.c_str();
        std::system(command);
    } else {
        std::cout << cmd;
    }
    Gtk::Main::quit();
}

/* Rebuild menu to match the search phrase */
void DMenu::filter_view() {
    if (this -> search_phrase.size() > 0) {
        // remove all items except searchbox
        for (auto item : this -> get_children()) {
            if (item -> get_name() != "search_item") {
                delete item;
            }
        }
        int cnt = 0;
        bool limit_exhausted = false;
        for (Glib::ustring command : all_commands) {
            if (command.find(this -> search_phrase) == 0) {
                Gtk::MenuItem *item = new Gtk::MenuItem();
                item -> set_label(command);
                item -> signal_activate().connect(sigc::bind<Glib::ustring>(sigc::mem_fun
                    (*this, &DMenu::on_item_clicked), command));
                this -> append(*item);
                cnt++;
                if (cnt > rows - 1) {
                    limit_exhausted = true;
                    break;
                }
            }
        }
        if (!limit_exhausted) {
            for (Glib::ustring command : all_commands) {
                if (command.find(this -> search_phrase) != std::string::npos && command.find(this -> search_phrase) != 0) {
                    Gtk::MenuItem *item = new Gtk::MenuItem();
                    item -> set_label(command);
                    item -> signal_activate().connect(sigc::bind<Glib::ustring>(sigc::mem_fun
                        (*this, &DMenu::on_item_clicked), command));
                    this -> append(*item);
                    cnt++;
                    if (cnt > rows - 1) {
                        break;
                    }
                }
            }
        }
        this -> show_all();

    } else {
        this -> searchbox.set_text("Type to search");
        // remove all items except searchbox
        for (auto item : this -> get_children()) {
            if (item -> get_name() != "search_item") {
                delete item;
            }
        }
        int cnt = 0;
        for (Glib::ustring command : all_commands) {
            Gtk::MenuItem *item = new Gtk::MenuItem();
            item -> set_label(command);
            item -> signal_activate().connect(sigc::bind<Glib::ustring>(sigc::mem_fun(*this, &DMenu::on_item_clicked), command));
            this -> append(*item);
            cnt++;
            if (cnt > rows - 1) {
                break;
            }
        }
        this -> show_all();
    }
}

MainWindow::MainWindow() : menu(nullptr) {
    set_title("~nwgdmenu");
    set_role("~nwgdmenu");
    set_skip_pager_hint(true);
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    set_app_paintable(true);

    signal_draw().connect(sigc::mem_fun(*this, &MainWindow::on_draw));
    signal_screen_changed().connect(sigc::mem_fun(*this, &MainWindow::on_screen_changed));

    on_screen_changed(get_screen());

    if (wm == "dwm" || wm == "bspwm" || wm == "qtile" || wm == "bspwm" || wm == "tiling") {
        fullscreen();
    } else if (wm == "sway" || wm == "i3") {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    }
    set_decorated(false);
}

MainWindow::~MainWindow() {
}

gboolean on_window_clicked(GdkEventButton *event) {
    Gtk::Main::quit();
    return true;
}

bool MainWindow::on_button_press_event(GdkEventButton* button_event) {
    if((button_event->type == GDK_BUTTON_PRESS) && (button_event->button == 3)) {
        if(!menu->get_attach_widget()) {
          menu->attach_to_widget(*this);
        }
        if(menu) {
            menu -> popup_at_widget(this->anchor, Gdk::GRAVITY_CENTER, Gdk::GRAVITY_CENTER, nullptr);
        }
        return true; //It has been handled.
    } else {
        return false;
    }
}

bool MainWindow::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    cr->save();
    if (_SUPPORTS_ALPHA) {
        cr->set_source_rgba(0.0, 0.0, 0.0, opacity);    // transparent
    } else {
        cr->set_source_rgb(0.0, 0.0, 0.0);          // opaque
    }
    cr->set_operator(Cairo::OPERATOR_SOURCE);
    cr->paint();
    cr->restore();

    return Gtk::Window::on_draw(cr);
}

void MainWindow::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    auto screen = get_screen();
    auto visual = screen->get_rgba_visual();

    if (!visual) {
        std::cout << "Your screen does not support alpha channels!" << std::endl;
    } else {
        _SUPPORTS_ALPHA = TRUE;
    }
    set_visual(visual);
}

void MainWindow::set_visual(Glib::RefPtr<Gdk::Visual> visual) {
    gtk_widget_set_visual(GTK_WIDGET(gobj()), visual->gobj());
}
