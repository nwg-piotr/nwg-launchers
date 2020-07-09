/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "nwg_tools.h"
#include "grid.h"

/*
 * Returns cache file path
 * */
std::string get_cache_path() {
    std::string s = "";
    char* val = getenv("XDG_CACHE_HOME");
    if (val) {
        s = val;
    } else {
        char* val = getenv("HOME");
        s = val;
        s += "/.cache";
    }
    fs::path dir (s);
    fs::path file ("nwg-fav-cache");
    fs::path full_path = dir / file;

    return full_path;
}

/*
 * Returns pinned cache file path
 * */
std::string get_pinned_path() {
    std::string s = "";
    char* val = getenv("XDG_CACHE_HOME");
    if (val) {
        s = val;
    } else {
        char* val = getenv("HOME");
        s = val;
        s += "/.cache";
    }
    fs::path dir (s);
    fs::path file ("nwg-pin-cache");
    fs::path full_path = dir / file;

    return full_path;
}

/*
 * Adds pinned entry and saves pinned cache file
 * */
void add_and_save_pinned(std::string command) {
    // Add if not yet pinned
    if (std::find(pinned.begin(), pinned.end(), command) == pinned.end()) {
        std::cout << "Pinning " << command << std::endl;
        pinned.push_back(command);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) out_file << e << "\n";
    }
}

/*
 * Removes pinned entry and saves pinned cache file
 * */
void remove_and_save_pinned(std::string command) {
    // Find the item index
    bool found = false;
    int idx (-1);
    for (int i = 0; i < (int)pinned.size(); i++) {
        if (pinned[i] == command) {
            found = true;
            idx = i;
            break;
        }
    }

    if (found && idx > 0) {
        std::cout << "Unpinning " << command << std::endl;
        pinned.erase(pinned.begin() + idx);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) out_file << e << "\n";
    }
}

/*
 * Splits string into vector of strings by delimiter
 * */
std::vector<std::string> split_string(std::string str, std::string delimiter) {
    std::vector<std::string> result;
    std::size_t current, previous = 0;
    current = str.find_first_of(delimiter);
    while (current != std::string::npos) {
        result.push_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delimiter, previous);
    }
    result.push_back(str.substr(previous, current - previous));
    return result;
}

/*
 * Returns locations of .desktop files
 * */
std::vector<std::string> get_app_dirs() {
    std::string homedir = getenv("HOME");
    std::vector<std::string> result = {homedir + "/.local/share/applications", "/usr/share/applications",
        "/usr/local/share/applications"};

    auto xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (xdg_data_dirs != NULL) {
        std::vector<std::string> dirs = split_string(xdg_data_dirs, ":");
        for (std::string dir : dirs) {
            result.push_back(dir);
        }
    }
    return result;
}

/*
 * Returns all .desktop files paths
 * */
std::vector<std::string> list_entries(std::vector<std::string> paths) {
    std::vector<std::string> desktop_paths;
    for (std::string dir : paths) {
        struct stat st;
        char* c = const_cast<char*>(dir.c_str());
        // if directory exists
        if (stat(c, &st) == 0) {
            for (const auto & entry : fs::directory_iterator(dir)) {
                desktop_paths.push_back(entry.path());
            }
        }
    }
    return desktop_paths;
}

/*
 * Parses .desktop file to vector<string> {'name', 'exec', 'icon', 'comment'}
 * */
std::vector<std::string> desktop_entry(std::string path, std::string lang) {
    std::vector<std::string> fields = {"", "", "", ""};

    std::ifstream file(path);
    std::string str;
    
    std::string name {""};          // Name=
    std::string name_ln {""};       // localized: Name[ln]=
    std::string loc_name = "Name[" + lang + "]=";
    
    std::string comment {""};       // Comment=
    std::string comment_ln {""};    // localized: Comment[ln]=
    std::string loc_comment = "Comment[" + lang + "]=";
    
    while (std::getline(file, str)) {
        bool read_me = true;
        if (str.find("[") == 0) {
            read_me = (str.find("[Desktop Entry") != std::string::npos);
            if (!read_me) {
            break;
        } else {
            continue;
            }
        }
        if (read_me) {
            if (str.find(loc_name) == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    name_ln = val;
                }
            }
            if (str.find("Name=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    name = val;
                }
            }
            if (str.find("Exec=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    // strip ' %' and following
                    if (val.find_first_of("%") != std::string::npos) {
                        int idx = val.find_first_of("%");
                        val = val.substr(0, idx - 1);
                    }
                    fields[1] = val;
                }
            }
            if (str.find("Icon=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    fields[2] = val;
                }
            }
            if (str.find("Comment=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    comment = val;
                }
            }
            if (str.find(loc_comment) == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    std::string val = str.substr(idx + 1);
                    comment_ln = val;
                }
            }
        }
    }
    if (name_ln.empty()) {
        fields[0] = name;
    } else {
        fields[0] = name_ln;
    }

    if (comment_ln.empty()) {
        fields[3] = comment;
    } else {
        fields[3] = comment_ln;
    }
    return fields;
}

/* Converts json string into a json object;
 * Requires nlohmann-json package, https://github.com/nlohmann/json
 * */
ns::json string_to_json(std::string jsonString) {
    const char *s = jsonString.c_str();
    ns::json jsonObj;
    std::stringstream(s) >> jsonObj;

    return jsonObj;
}

void save_json(ns::json json_obj, std::string filename) {
    std::ofstream o(filename);
    o << std::setw(2) << json_obj << std::endl;
}

/*
 * Returns json object out of the cache file
 * */
ns::json get_cache(std::string cache_file) {
    std::string cache_string = read_file_to_string(cache_file);

    return string_to_json(cache_string);
}

/*
 * Returns vector of strings out of the pinned cache file content
 * */
std::vector<std::string> get_pinned(std::string pinned_file) {
    std::vector<std::string> lines;
    std::ifstream in(pinned_file.c_str());
    if(!in) {
        std::cerr << "Could not find " << pinned_file << ", creating!" << std::endl;
        save_string_to_file("", pinned_file);
        return lines;
    }
    std::string str;

    while (std::getline(in, str)) {
        // add non-empty lines to the vector
        if(str.size() > 0) {
            lines.push_back(str);
        }
    }
    in.close();
    return lines;
 }

/*
 * Returns n cache items sorted by clicks; n should be the number of grid columns
 * */
std::vector<CacheEntry> get_favourites(ns::json cache, int number) {
    // read from json object
    std::vector<CacheEntry> sorted_cache {}; // not yet sorted
    for (ns::json::iterator it = cache.begin(); it != cache.end(); ++it) {
        struct CacheEntry entry = {it.key(), it.value()};
        sorted_cache.push_back(entry);
    }
    // actually sort by the number of clicks
    sort(sorted_cache.begin(), sorted_cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs) {
        return lhs.clicks > rhs.clicks;
    });
    // Trim to the number of columns, as we need just 1 row of favourites
    std::vector<CacheEntry>::const_iterator first = sorted_cache.begin();
    std::vector<CacheEntry>::const_iterator last = sorted_cache.begin() + number;
    std::vector<CacheEntry> favourites(first, last);
    return favourites;
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
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    } else {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

bool on_button_entered(GdkEventCrossing *event, Glib::ustring comment) {
    description -> set_text(comment);
    return true;
}

bool on_button_focused(GdkEventFocus *event, Glib::ustring comment) {
    description -> set_text(comment);
    return true;
}

void on_button_clicked(std::string cmd) {
    int clicks = 0;
    try {
        clicks = cache[cmd];
        clicks++;
    } catch (...) {
        clicks = 1;
    }
    cache[cmd] = clicks;
    save_json(cache, cache_file);

    cmd = cmd + " &";
    const char *command = cmd.c_str();
    std::system(command);

    Gtk::Main::quit();
}

bool on_grid_button_press(GdkEventButton *event, Glib::ustring exec) {
    if (pins && event -> button == 3) {
        add_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}

bool on_pinned_button_press(GdkEventButton *event, Glib::ustring exec) {
    if (pins && event -> button == 3) {
        remove_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}
