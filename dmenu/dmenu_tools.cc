/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <algorithm>
#include <iostream>
#include <fstream>

#include "nwg_tools.h"
#include "dmenu.h"

/*
 * Returns settings cache file path
 * */
std::filesystem::path get_settings_path() {
    auto full_path = get_cache_home();
    full_path /= "nwg-dmenu-case";
    return full_path;
}

/*
 * Returns all commands paths
 * */
static std::vector<Glib::ustring> list_commands() {
    std::vector<Glib::ustring> commands;
    if (auto command_dirs_ = getenv("PATH")) {
        std::string command_dirs{ command_dirs_ };
        auto paths = split_string(command_dirs, ":");
        std::error_code ec;
        for (auto && dir: paths) {
            if (fs::is_directory(dir, ec) && !ec) {
                for (auto && entry: fs::directory_iterator(dir)) {
                    auto cmd = take_last_by(entry.path().native(), "/");
                    if (cmd.size() > 1 && cmd[0] != '.') {
                        commands.emplace_back(cmd.data(), cmd.size());
                    }
                }
            }
        }
    }
    return commands;
}

/*
 * Returns list of commands loaded according to config
 * */
std::vector<Glib::ustring> get_commands_list(const DmenuConfig& config) {
    std::vector<Glib::ustring> all_commands;
    if (config.dmenu_run) {
        /* get a list of paths to all commands from all application dirs */
        all_commands = list_commands();
        Log::info(all_commands.size(), " commands found");

        /* Sort case insensitive */
        std::sort(all_commands.begin(), all_commands.end(), [](auto& a, auto& b) {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](auto a, auto b) {
                return std::tolower(a) < std::tolower(b);
            });
        });
    } else {
        for (std::string line; std::getline(std::cin, line);) {
            all_commands.emplace_back(std::move(line));
        }
    }
    return all_commands;
}
