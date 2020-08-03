/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "nwg_tools.h"
#include "dmenu.h"

/*
 * Returns settings cache file path
 * */
std::string get_settings_path() {
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
    fs::path file ("nwg-dmenu-case");
    fs::path full_path = dir / file;

    return full_path;
}

/*
 * Returns all command paths
 * */
std::vector<std::string> list_commands() {
    std::vector<std::string> command_paths;

    if (std::string command_dirs = getenv("PATH"); !command_dirs.empty()) {
        auto paths = split_string(command_dirs, ":");
        std::error_code ec;
        for (auto& dir : paths) {
            // if directory exists
            if (fs::is_directory(dir, ec) && !ec) {
                for (const auto & entry : fs::directory_iterator(dir)) {
                    command_paths.emplace_back(entry.path());
                }
            }
        }
    }
    return command_paths;
}

void on_item_clicked(std::string cmd) {
    if (dmenu_run) {
        cmd = cmd + " &";
        const char *command = cmd.c_str();
        std::system(command);
    } else {
        std::cout << cmd;
    }
    Gtk::Main::quit();
}
