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
    DesktopEntry entry;

    std::ifstream file(path);
    std::string str;

    std::string name {};            // Name=
    std::string name_ln {};         // localized: Name[ln]=
    std::string loc_name = "Name[" + lang + "]=";

    std::string comment {};         // Comment=
    std::string comment_ln {};      // localized: Comment[ln]=
    std::string loc_comment = "Comment[" + lang + "]=";

    while (std::getline(file, str)) {
        auto view = std::string_view(str.data(), str.size());
        bool read_me = true;
        if (view.find("[") == 0) {
            read_me = (view.find("[Desktop Entry") != std::string_view::npos);
            if (!read_me) {
                break;
            } else {
                continue;
            }
        }
        if (read_me) {
            // This is to resolve `Respect the NoDisplay setting in .desktop files #84`,
            // see https://wiki.archlinux.org/index.php/desktop_entries#Hide_desktop_entries.
            // The ~/.local/share/applications folder is going to be read first. Entries created from here won't be
            // overwritten from e.g. /usr/share/applications, as duplicates are being skipped.
            if (view.find("NoDisplay=true") == 0) {
                entry.no_display = true;
            }

            if (view.find(loc_name) == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    name_ln = view.substr(idx + 1);
                }
            }
            if (view.find("Name=") == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    name = view.substr(idx + 1);
                }
            }
            if (view.find("Exec=") == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    auto val = view.substr(idx + 1);
                    // strip ' %' and following
                    if (auto idx = val.find_first_of("%"); idx != std::string_view::npos) {
                        val = val.substr(0, idx - 1);
                    }
                    entry.exec = std::move(val);
                }
            }
            if (view.find("Icon=") == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    entry.icon = view.substr(idx + 1);
                }
            }
            if (view.find("Comment=") == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    comment = view.substr(idx + 1);
                }
            }
            if (view.find(loc_comment) == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    comment_ln = view.substr(idx + 1);
                }
            }
            if (view.find("MimeType=") == 0) {
                if (auto idx = view.find_first_of("="); idx != std::string_view::npos) {
                    entry.mime_type = view.substr(idx + 1);
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
