/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <filesystem>
#include <string_view>

#include "nwg_tools.h"
#include "grid.h"

CacheEntry::CacheEntry(std::string exec, int clicks): exec(std::move(exec)), clicks(clicks) { }

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
        val = getenv("HOME");
        s = val;
        s += "/.cache";
    }
    fs::path dir (s);
    fs::path file ("nwg-pin-cache");
    fs::path full_path = dir / file;

    return full_path;
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
        auto dirs = split_string(xdg_data_dirs, ":");
        for (auto& dir : dirs) {
            result.emplace_back(dir);
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
 * Parses .desktop file to DesktopEntry struct
 * */
DesktopEntry desktop_entry(std::string&& path, const std::string& lang) {
    using namespace std::literals::string_view_literals;

    DesktopEntry entry;

    std::ifstream file(path);
    std::string str;

    std::string name {};            // Name=
    std::string name_ln {};         // localized: Name[ln]=
    std::string loc_name = "Name[" + lang + "]=";

    std::string comment {};         // Comment=
    std::string comment_ln {};      // localized: Comment[ln]=
    std::string loc_comment = "Comment[" + lang + "]=";

    auto header = "[Desktop Entry]"sv;
    auto name_ = "Name="sv;
    auto exec_ = "Exec="sv;
    auto icon_ = "Icon="sv;
    auto comment_ = "Comment="sv;

    // Skip everything not related
    while (std::getline(file, str)) {
        str.resize(header.size());
        if (str == header) {
            break;
        }
    }
    // Repeat until the next section
    while (std::getline(file, str)) {
        if (str.empty()) {
            continue;
        }
        if (str[0] == '[') {
            break;
        }
        auto view = std::string_view{str};
        auto view_len = std::size(view);
        struct Result {
            bool   found;
            size_t index;
        };
        auto strip_prefix = [&view, view_len](auto& prefix) {
            auto len = std::min(view_len, std::size(prefix));
            return Result {
                prefix == view.substr(0, len),
                len
            };
        };
        if (auto [r, i] = strip_prefix(loc_name); r) {
            name_ln = view.substr(i);
            continue;
        }
        if (auto [r, i] = strip_prefix(name_); r) {
            name = view.substr(i);
            continue;
        }
        if (auto [r, i] = strip_prefix(exec_); r) {
            auto val = view.substr(i);
            if (auto idx = val.find_first_of("%"); idx != std::string_view::npos) {
                val = val.substr(0, idx - 1);
            }
            entry.exec = val;
            continue;
        }
        if (auto [r, i] = strip_prefix(icon_); r) {
            entry.icon = view.substr(i);
            continue;
        }
        if (auto [r, i] = strip_prefix(comment_); r) {
            comment = view.substr(i);
            continue;
        }
        if (auto [r, i] = strip_prefix(loc_comment); r) {
            comment_ln = view.substr(i);
            continue;
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
    std::ifstream in(pinned_file);
    if(!in) {
        std::cerr << "Could not find " << pinned_file << ", creating!" << std::endl;
        save_string_to_file("", pinned_file);
        return lines;
    }
    std::string str;

    while (std::getline(in, str)) {
        // add non-empty lines to the vector
        if (!str.empty()) {
            lines.emplace_back(std::move(str));
        }
    }
    in.close();
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
    sort(sorted_cache.begin(), sorted_cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs) {
        return lhs.clicks > rhs.clicks;
    });
    // Trim to the number of columns, as we need just 1 row of favourites
    auto from = sorted_cache.begin() + number;
    auto to = sorted_cache.end();
    sorted_cache.erase(from, to);
    return sorted_cache;
}
