/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "nwg_tools.h"
#include "bar.h"

/*
 * Returns json object out of a template file
 * */
ns::json get_bar_json(std::string custom_bar) {
    std::string bar_string = read_file_to_string(custom_bar);

    return string_to_json(bar_string);
}

/*
 * Returns a vector of BarEntry data structs
 * */
std::vector<BarEntry> get_bar_entries(ns::json bar_json) {
    // read from json object
    std::vector<BarEntry> entries {};
    for (ns::json::iterator it = bar_json.begin(); it != bar_json.end(); ++it) {
        int index = std::distance(bar_json.begin(), it);
        struct BarEntry entry = {bar_json[index].at("name"), bar_json[index].at("exec"), bar_json[index].at("icon")};
        entries.push_back(entry);
    }
    return entries;
}

/*
 * Returns x, y, width, hight of focused display
 * */
std::vector<int> display_geometry(std::string wm, MainWindow &window) {
    std::vector<int> geo = {0, 0, 0, 0};
    if (wm == "sway") {
        std::string jsonString = get_output("swaymsg -t get_outputs");
        ns::json jsonObj = string_to_json(jsonString);
        for (ns::json::iterator it = jsonObj.begin(); it != jsonObj.end(); ++it) {
            int index = std::distance(jsonObj.begin(), it);
            if (jsonObj[index].at("focused") == true) {
                geo[0] = jsonObj[index].at("rect").at("x");
                geo[1] = jsonObj[index].at("rect").at("y");
                geo[2] = jsonObj[index].at("rect").at("width");
                geo[3] = jsonObj[index].at("rect").at("height");
            }
        }
    } else {
        // it's going to fail until the window is actually open
        int retry = 0;
        while (geo[2] == 0 || geo[3] == 0) {
            Glib::RefPtr<Gdk::Screen> screen = window.get_screen();
            int display_number = screen -> get_monitor_at_window(screen -> get_active_window());
            Gdk::Rectangle rect;
            screen -> get_monitor_geometry(display_number, rect);
            geo[0] = rect.get_x();
            geo[1] = rect.get_y();
            geo[2] = rect.get_width();
            geo[3] = rect.get_height();
            retry++;
            if (retry > 100) {
                std::cout << "\nERROR: Failed checking display geometry\n\n";
                break;
            }
        }
    }
    return geo;
}

/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(std::string icon) {
    Glib::RefPtr<Gtk::IconTheme> icon_theme;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    icon_theme = Gtk::IconTheme::get_default();

    if (icon.find_first_of("/") != 0) {
        try {
            pixbuf = icon_theme->load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg", image_size, image_size, true);
        }
    } else {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg", image_size, image_size, true);
        }
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

void on_button_clicked(std::string cmd) {
    cmd = cmd + " &";
    const char *command = cmd.c_str();
    std::system(command);

    Gtk::Main::quit();
}
