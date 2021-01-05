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

#include "dmenu.h"

Anchor::Anchor(DMenu *menu) : menu{menu} {}

bool Anchor::on_focus_in_event(GdkEventFocus* focus_event) {
    (void) focus_event; // suppress warning

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
    menu->popup_at_widget(this, gravity_widget, gravity_menu, nullptr);

    return true;
}

DMenu::DMenu() {
    if (case_sensitive) {
        searchbox.set_text("Type To Search");
    } else {
        searchbox.set_text("TYPE TO SEARCH");
    }
    searchbox.set_sensitive(false);
    searchbox.set_name("searchbox");
    search_phrase = "";
}

void switch_case_sensitive(std::string filename, bool is_case_sensitive) {
    if (is_case_sensitive) {
        if (std::ifstream(filename)) {
            int status = remove(filename.c_str());
            std::cout << "status = " << status << std::endl;
        }
    } else {
        std::ofstream file(filename);
        file << "case_sensitive";
    }
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
            if (!case_sensitive) {
                character = toupper(character);
            }
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
        } else if (this -> get_children().size() == 2 && key_event -> keyval == GDK_KEY_Return) {
            // Workaround to launch the single item which has been selected programmatically
            this -> get_children()[1] -> activate();
            return true;
        } else if (key_event -> keyval == GDK_KEY_Insert) {
            this -> search_phrase = "";
            case_sensitive = !case_sensitive;
            switch_case_sensitive(settings_file, case_sensitive);
            this -> filter_view();
            return true;
        }
    }
    //if the event has not been handled, call the base class
    return Gtk::Menu::on_key_press_event(key_event);
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
            std::string sf = this -> search_phrase;
            std::string cm = command;
            if (!case_sensitive) {
                for(unsigned int l = 0; l < sf.length(); l++) {
                   sf[l] = toupper(sf[l]);
                }
                for(unsigned int l = 0; l < cm.length(); l++) {
                   cm[l] = toupper(cm[l]);
                }
            }
            if (cm.find(sf) == 0) {
                Gtk::MenuItem *item = new Gtk::MenuItem();
                item -> set_label(command);
                item -> signal_activate().connect(sigc::bind<Glib::ustring>(sigc::mem_fun
                    (*this, &DMenu::on_item_clicked), command));
                this -> append(*item);
                // This will highlight 1st menu item, still it won't start on Enter.
                // See workaround in on_key_press_event.
                if (cnt == 0) {
                    item -> select();
                }
                cnt++;
                if (cnt > rows - 1) {
                    limit_exhausted = true;
                    break;
                }
            }
        }
        if (!limit_exhausted) {
            for (Glib::ustring command : all_commands) {
                std::string sf = this -> search_phrase;
                std::string cm = command;
                if (!case_sensitive) {
                    for(unsigned int l = 0; l < sf.length(); l++) {
                       sf[l] = toupper(sf[l]);
                    }
                    for(unsigned int l = 0; l < cm.length(); l++) {
                       cm[l] = toupper(cm[l]);
                    }
                }
                if (cm.find(sf) != std::string::npos && cm.find(sf) != 0) {
                    Gtk::MenuItem *item = new Gtk::MenuItem();
                    item -> set_label(command);
                    item -> signal_activate().connect(sigc::bind<Glib::ustring>(sigc::mem_fun
                        (*this, &DMenu::on_item_clicked), cm));
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
        if (case_sensitive) {
            this -> searchbox.set_text("Type To Search");
        } else {
            this -> searchbox.set_text("TYPE TO SEARCH");
        }
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

MainWindow::MainWindow() : CommonWindow("~nwgdmenu", "~nwgdmenu"), menu(nullptr) {
    if (wm == "dwm" || wm == "bspwm" || wm == "qtile" || wm == "bspwm" || wm == "tiling") {
        fullscreen();
    } else if (wm == "sway" || wm == "i3") {
        set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    }
    set_decorated(false);
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
