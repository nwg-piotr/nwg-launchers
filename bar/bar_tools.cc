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

void on_button_clicked(std::string cmd) {
    cmd = cmd + " &";
    const char *command = cmd.c_str();
    std::system(command);

    Gtk::Main::quit();
}
