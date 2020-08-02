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
 * Constructor is required for std::vector::emplace_back to work
 * It is not needed when compiling with C++20 and greater
 * */
BarEntry::BarEntry(std::string&& name, std::string&& exec, std::string&& icon) : name(std::move(name)), exec(std::move(exec)), icon(std::move(icon)) {}

/*
 * Returns json object out of a template file
 * */
ns::json get_bar_json(const std::string& custom_bar) {
    std::string bar_string = read_file_to_string(custom_bar);

    return string_to_json(bar_string);
}

/*
 * Returns a vector of BarEntry data structs
 * */
std::vector<BarEntry> get_bar_entries(ns::json&& bar_json) {
    // read from json object
    std::vector<BarEntry> entries {};
    for (auto&& json : bar_json) {
        entries.emplace_back(std::move(json.at("name")),
                             std::move(json.at("exec")),
                             std::move(json.at("icon")));
    }
    return entries;
}


/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(const std::string& icon) {
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
