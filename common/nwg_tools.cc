/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <stdlib.h>

#include <iostream>
#include <fstream>

#include "nwgconfig.h"
#include <nwg_tools.h>

/*
 * Returns config dir
 * */
std::string get_config_dir(std::string app) {
    std::string s;
    char *val = getenv("XDG_CONFIG_HOME");

    if (val) {
        s = val;
        s += "/nwg-launchers/";
    } else {
        val = getenv("HOME");
        if (!val) {
            std::cerr << "Couldn't find config directory, HOME not set!";
            std::exit(1);
        }
        s = val;
        s += "/.config/nwg-launchers/";
    }

    s += app;
    return s;
}

/*
 * Returns window manager name
 * */
std::string detect_wm() {
    /* Actually we only need to check if we're on sway or not,
     * but let's try to find a WM name if possible. If not, let it be just "other" */
    const char *env_var[2] = {"DESKTOP_SESSION", "SWAYSOCK"};
    char *env_val[2];
    std::string wm_name{"other"};

    for(int i=0; i<2; i++) {
        // get environment values
        env_val[i] = getenv(env_var[i]);
        if (env_val[i] != NULL) {
            std::string s(env_val[i]);
            if (s.find("sway") != std::string::npos) {
                wm_name = "sway";
                break;
            } else {
                // is the value a full path or just a name?
                if (s.find("/") == std::string::npos) {
                    // full value is the name
                    wm_name = s;
                    break;
                } else {
                    // path given
                    int idx = s.find_last_of("/") + 1;
                    wm_name = s.substr(idx);
                    break;
                }
            }
        }
    }
    return wm_name;
}

/*
 * Returns x, y, width, hight of focused display
 * */
Geometry display_geometry(const std::string& wm, Glib::RefPtr<Gdk::Display> display, Glib::RefPtr<Gdk::Window> window) {
    Geometry geo = {0, 0, 0, 0};
    if (wm == "sway") {
        try {
            auto jsonString = get_output("swaymsg -t get_outputs");
            auto jsonObj = string_to_json(jsonString);
            for (auto&& entry : jsonObj) {
                if (entry.at("focused")) {
                    auto&& rect = entry.at("rect");
                    geo.x = rect.at("x");
                    geo.y = rect.at("y");
                    geo.width = rect.at("width");
                    geo.height = rect.at("height");
                    break;
                }
            }
            return geo;
        }
        catch (...) { }
    }

    // it's going to fail until the window is actually open
    int retry = 0;
    while (geo.width == 0 || geo.height == 0) {
        Gdk::Rectangle rect;
        auto monitor = display->get_monitor_at_window(window);
        if (monitor) {
            monitor->get_geometry(rect);
            geo.x = rect.get_x();
            geo.y = rect.get_y();
            geo.width = rect.get_width();
            geo.height = rect.get_height();
        }
        retry++;
        if (retry > 100) {
            std::cerr << "\nERROR: Failed checking display geometry, tries: " << retry << "\n\n";
            break;
        }
    }
    return geo;
}

/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(const Gtk::IconTheme& icon_theme, const std::string& icon) {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    
    try {
        if (icon.find_first_of("/") == std::string::npos) {
            pixbuf = icon_theme.load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } else {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        }
    } catch (...) {
        pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg", image_size, image_size, true);
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

/*
 * Returns current locale
 * */
std::string get_locale() {
    std::string l {"en"};
    if (getenv("LANG") != NULL) {
        char* env_val = getenv("LANG");
        std::string loc(env_val);
        if (!loc.empty()) {
            if (loc.find("_") != std::string::npos) {
                int idx = loc.find_first_of("_");
                l = loc.substr(0, idx);
            } else {
                l = loc;
            }
        }
    }
    return l;
}

/*
 * Returns file content as a string
 * */
std::string read_file_to_string(const std::string& filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(const std::string& s, const std::string& filename) {
    std::ofstream file(filename);
    file << s;
}

/*
 * Splits string into vector of strings by delimiter
 * */
std::vector<std::string_view> split_string(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> result;
    std::size_t current, previous = 0;
    current = str.find_first_of(delimiter);
    while (current != std::string_view::npos) {
        result.emplace_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delimiter, previous);
    }
    result.emplace_back(str.substr(previous, current - previous));

    return result;
}

/*
 * Splits string by delimiter and takes the last piece
 * */
std::string_view take_last_by(std::string_view str, std::string_view delimiter) {
    auto pos = str.find_last_of(delimiter);
    if (pos != std::string_view::npos) {
        str.substr(pos);
    }
    return str;   
}

/*
 * Converts json string into a json object
 * */
ns::json string_to_json(const std::string& jsonString) {
    ns::json jsonObj;
    std::stringstream(jsonString) >> jsonObj;

    return jsonObj;
}

/*
 * Saves json into file
 * */
void save_json(const ns::json& json_obj, const std::string& filename) {
    std::ofstream o(filename);
    o << std::setw(2) << json_obj << std::endl;
}

/*
 * Returns output of a command as string
 * */
std::string get_output(const std::string& cmd) {
    const char *command = cmd.c_str();
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
