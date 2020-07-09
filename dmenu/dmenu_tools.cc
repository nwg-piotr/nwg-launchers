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
 * Returns locations of .desktop files
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
 * Returns all .desktop files paths
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
