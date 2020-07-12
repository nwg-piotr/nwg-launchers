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
 * Returns locations of command files
 * */
std::vector<std::string> get_command_dirs() {
    std::vector<std::string> result = {};
    auto command_dirs = getenv("PATH");
    if (command_dirs != NULL) {
        std::vector<std::string> dirs = split_string(command_dirs, ":");
        for (std::string dir : dirs) {
            result.push_back(dir);
        }
    }
    return result;
}

/*
 * Returns all command paths
 * */
std::vector<std::string> list_commands(std::vector<std::string> paths) {
    std::vector<std::string> command_paths;
    for (std::string dir : paths) {
        struct stat st;
        char* c = const_cast<char*>(dir.c_str());
        // if directory exists
        if (stat(c, &st) == 0) {
            for (const auto & entry : fs::directory_iterator(dir)) {
                command_paths.push_back(entry.path());
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
