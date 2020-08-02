/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <filesystem>

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
void add_and_save_pinned(const std::string& command) {
    // Add if not yet pinned
    if (std::find(pinned.begin(), pinned.end(), command) == pinned.end()) {
        pinned.push_back(command);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) out_file << e << "\n";
    }
}

/*
 * Removes pinned entry and saves pinned cache file
 * */
void remove_and_save_pinned(const std::string& command) {
    // Find the item index
    bool found = false;
    std::size_t idx;
    for (std::size_t i = 0; i < pinned.size(); i++) {
        if (pinned[i] == command) {
            found = true;
            idx = i;
            break;
        }
    }

    if (found) {
        pinned.erase(pinned.begin() + idx);
        std::ofstream out_file(pinned_file);
        for (const auto &e : pinned) {
            out_file << e << "\n";
        }
    }
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
std::vector<std::string> list_entries(const std::vector<std::string>& paths) {
    std::vector<std::string> desktop_paths;
    std::error_code ec;
    for (auto& dir : paths) {
        // if directory exists
        if (std::filesystem::is_directory(dir, ec) && !ec) {
            for (const auto & entry : fs::directory_iterator(dir)) {
                desktop_paths.emplace_back(entry.path());
            }
        }
    }
    return desktop_paths;
}

/*
 * Parses .desktop file to vector<string> {'name', 'exec', 'icon', 'comment'}
 * */
DesktopEntry desktop_entry(std::string&& path, const std::string& lang) {
    DesktopEntry entry;

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
                    name_ln = str.substr(idx + 1);
                }
            }
            if (str.find("Name=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    name = str.substr(idx + 1);
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
                    entry.exec = std::move(val);
                }
            }
            if (str.find("Icon=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    entry.icon = str.substr(idx + 1);
                }
            }
            if (str.find("Comment=") == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    comment = str.substr(idx + 1);
                }
            }
            if (str.find(loc_comment) == 0) {
                if (str.find_first_of("=") != std::string::npos) {
                    int idx = str.find_first_of("=");
                    comment_ln = str.substr(idx + 1);
                }
            }
        }
    }
    if (name_ln.empty()) {
        entry.name = std::move(name);
    } else {
        entry.name = std::move(name_ln);
    }

    if (comment_ln.empty()) {
        entry.comment = std::move(comment);
    } else {
        entry.comment = std::move(comment_ln);
    }
    return entry;
}

/*
 * Returns json object out of the cache file
 * */
ns::json get_cache(const std::string& cache_file) {
    std::string cache_string = read_file_to_string(cache_file);

    return string_to_json(cache_string);
}

/*
 * Returns vector of strings out of the pinned cache file content
 * */
std::vector<std::string> get_pinned(const std::string& pinned_file) {
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

bool on_button_entered(GdkEventCrossing *event, const Glib::ustring& comment) {
    description -> set_text(comment);
    return true;
}

bool on_button_focused(GdkEventFocus *event, const Glib::ustring& comment) {
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

bool on_grid_button_press(GdkEventButton *event, const Glib::ustring& exec) {
    if (pins && event -> button == 3) {
        add_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}

bool on_pinned_button_press(GdkEventButton *event, const Glib::ustring& exec) {
    if (pins && event -> button == 3) {
        remove_and_save_pinned(exec);
        Gtk::Main::quit();
    }
    return true;
}
