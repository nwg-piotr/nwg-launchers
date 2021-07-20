/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <filesystem>
#include <string_view>
#include <variant>

#include "nwg_tools.h"
#include "grid.h"

CacheEntry::CacheEntry(std::string desktop_id, int clicks): desktop_id(std::move(desktop_id)), clicks(clicks) { }

/*
 * Returns locations of .desktop files
 * */
std::vector<std::filesystem::path> get_app_dirs() {
    std::vector<std::filesystem::path> result;
    result.reserve(8);

    std::filesystem::path home;
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
        std::filesystem::path{"/var/lib"} / suffix
    };
    for (auto&& fp_dir : flatpak_data_dirs) {
        if (std::find(result.begin(), result.end(), fp_dir) == result.end()) {
            result.emplace_back(fp_dir);
        }
    }
    
    return result;
}

/*
 * Parses .desktop file to DesktopEntry struct
 * */
std::optional<DesktopEntry> desktop_entry(const fs::path& path, std::string_view lang, std::string_view term) {
    using namespace std::literals::string_view_literals;

    DesktopEntry entry;
    entry.terminal = false;

    std::ifstream file{ path };
    std::string str;

    std::string name_ln {};         // localized: Name[ln]=
    std::string loc_name = concat("Name[", lang, "]=");

    std::string comment_ln {};      // localized: Comment[ln]=
    std::string loc_comment = concat("Comment[", lang, "]=");

    struct nop_t { } nop;
    struct cut_t { } cut;
    struct Match {
        std::string_view           prefix;
        std::string*               dest;
        std::variant<nop_t, cut_t> tag;
    };
    struct Result {
        bool   ok;
        size_t pos;
    };
    Match matches[] = {
        { "Name="sv,     &entry.name,      nop },
        { loc_name,      &name_ln,         nop },
        { "Exec="sv,     &entry.exec,      cut },
        { "Icon="sv,     &entry.icon,      nop },
        { "Comment="sv,  &entry.comment,   nop },
        { loc_comment,   &comment_ln,      nop },
        { "MimeType="sv, &entry.mime_type, nop },
    };

    // Skip everything not related
    constexpr auto header = "[Desktop Entry]"sv;
    while (std::getline(file, str)) {
        str.resize(header.size());
        if (str == header) {
            break;
        }
    }
    // Repeat until the next section
    constexpr auto nodisplay = "NoDisplay=true"sv;
    constexpr auto terminal = "Terminal=true"sv;
    while (std::getline(file, str)) {
        if (str[0] == '[') { // new section begins, break
            break;
        }
        auto view = std::string_view{str};
        auto view_len = std::size(view);
        if (view == nodisplay) {
            return std::nullopt;
        }
        if (view == terminal) {
            entry.terminal = true;
        }
        auto try_strip_prefix = [&view, view_len](auto& prefix) {
            auto len = std::min(view_len, std::size(prefix));
            return Result {
                prefix == view.substr(0, len),
                len
            };
        };
        for (auto& [prefix, dest, tag] : matches) {
            if (auto [ok, pos] = try_strip_prefix(prefix); ok) {
                std::visit(Overloaded {
                    [dest=dest, pos=pos, &view](nop_t) { *dest = view.substr(pos); },
                    [dest=dest, pos=pos, &view](cut_t) {
                        auto idx = view.find(" %", pos);
                        if (idx == std::string_view::npos) {
                            idx = std::size(view);
                        }
                        *dest = view.substr(pos, idx - pos);
                    }
                },
                tag);
                break;
            }
        }
    }

    if (!name_ln.empty()) {
        entry.name = std::move(name_ln);
    }
    if (!comment_ln.empty()) {
        entry.comment = std::move(comment_ln);
    }
    if (entry.name.empty() || entry.exec.empty()) {
        return std::nullopt;
    }
    if (entry.terminal) {
        entry.exec = concat(term, " ", entry.exec);
    }
    return entry;
}

/*
 * Returns vector of strings out of the pinned cache file content
 * */
std::vector<std::string> get_pinned(const std::filesystem::path& pinned_file) {
    std::vector<std::string> lines;
    std::ifstream in{ pinned_file };
    if(!in) {
        Log::info("Could not find ", pinned_file, ", creating!");
        save_string_to_file("", pinned_file);
        return lines;
    }
    for (std::string str; std::getline(in, str);) {
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
