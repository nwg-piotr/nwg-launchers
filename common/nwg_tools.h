/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

#include <gtkmm.h>

#include <nlohmann/json.hpp>

namespace ns = nlohmann;

std::string get_config_dir(std::string);

std::string detect_wm(void);

std::string get_locale(void);

std::string read_file_to_string(std::string);
void save_string_to_file(std::string, std::string);
std::vector<std::string> split_string(std::string, std::string);

ns::json string_to_json(std::string);
void save_json(ns::json, std::string);

std::string get_output(std::string);

/*
 * Returns x, y, width, hight of focused display
 * */
template<class T> std::vector<int> display_geometry(std::string wm, T &window) {
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
                std::cerr << "\nERROR: Failed checking display geometry\n\n";
                break;
            }
        }
    }
    return geo;
}
