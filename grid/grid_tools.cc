/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <string_view>
#include <variant>
#include <fstream>

#include "filesystem-compat.h"
#include "nwg_tools.h"
#include "grid.h"

CacheEntry::CacheEntry(std::string desktop_id, int clicks): desktop_id(std::move(desktop_id)), clicks(clicks) { }

/*
 * Returns locations of .desktop files
 * */
std::vector<fs::path> get_app_dirs() {
    std::vector<fs::path> result;
    result.reserve(8);

    fs::path home;
    if (auto home_ = getenv("HOME")) {
        home = home_;
    }

    if (auto env_ = getenv("XDG_DATA_HOME")) {
        std::string xdg_data_home = env_; // getenv result is not guaranteed to live long enough
        for (auto&& dir : split_string(xdg_data_home, ":")) {
            result.emplace_back(dir) /= "applications";
        }
    } else {
        if (!home.empty()) {
            result.emplace_back(home) /= ".local/share/applications";
        }
    }
    std::string xdg_data_dirs;
    if (auto env_ = getenv("XDG_DATA_DIRS")) {
        xdg_data_dirs = env_;
    } else {
        xdg_data_dirs = "/usr/local/share/:/usr/share/";
    }
    for (auto&& dir: split_string(xdg_data_dirs, ":")) {
        result.emplace_back(dir) /= "applications";
    }

    // Add flatpak dirs if not found in XDG_DATA_DIRS
    auto suffix = "flatpak/exports/share/applications";
    std::array flatpak_data_dirs {
        home / suffix,
        fs::path{"/var/lib"} / suffix
    };
    for (auto&& fp_dir : flatpak_data_dirs) {
        if (std::find(result.begin(), result.end(), fp_dir) == result.end()) {
            result.emplace_back(fp_dir);
        }
    }
    
    return result;
}

/*
 * Returns vector of strings out of the pinned cache file content
 * */
std::vector<std::string> get_pinned(const fs::path& pinned_file) {
    std::vector<std::string> lines;
    if (std::ifstream in{ pinned_file }) {
        for (std::string str; std::getline(in, str);) {
            // add non-empty lines to the vector
            if (!str.empty()) {
                lines.emplace_back(std::move(str));
            }
        }
    } else {
        Log::info("Could not find ", pinned_file, ", creating!");
        save_string_to_file("", pinned_file);
    }
    return lines;
}

/*
 * Returns n cache items sorted by clicks; n should be the number of grid columns
 * */
std::vector<CacheEntry> get_favourites(ns::json&& cache, int number) {
    // read from json object
    std::vector<CacheEntry> sorted_cache {}; // not yet sorted
    for (auto it : cache.items()) {
        sorted_cache.emplace_back(it.key(), it.value());
    }
    // actually sort by the number of clicks
    std::sort(sorted_cache.begin(), sorted_cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs) {
        return lhs.clicks > rhs.clicks;
    });
    // Trim to the number of columns, as we need just 1 row of favourites
    auto from = sorted_cache.begin() + number;
    auto to = sorted_cache.end();
    sorted_cache.erase(from, to);
    return sorted_cache;
}
