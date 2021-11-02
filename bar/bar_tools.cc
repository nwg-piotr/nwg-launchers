/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "bar.h"

/*
 * Returns a vector of BarEntry data structs
 * */
std::vector<BarEntry> get_bar_entries(ns::json&& bar_json) {
    // read from json object
    std::vector<BarEntry> entries {};
    for (auto&& json : bar_json) {
        auto && entry = entries.emplace_back(
            std::move(json.at("name")),
            std::move(json.at("exec")),
            std::move(json.at("icon"))
        );
        if (auto iter = json.find("class"); iter != json.end()) {
            entry.css_class = *iter;
        }
    }
    return entries;
}
