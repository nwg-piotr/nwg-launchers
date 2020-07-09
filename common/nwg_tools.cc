/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <stdlib.h>

#include <iostream>
#include <fstream>
#include <filesystem>

#include "nwg_tools.h"

/*
 * Returns config dir
 * */
std::string get_config_dir(std::string app) {
    std::string s;
    char *val = getenv("XDG_CONFIG_HOME");

    if (val) {
        s = val;
        s += "/nwg-launchers/";
    } else {
        val = getenv("HOME");
        if (val) {
            std::cerr << "Couldn't find config directory, HOME not set!";
            std::exit(1);
        }
        s = val;
        s += "/.config/nwg-launchers/";
    }

    s += app;
    return s;
}

/*
 * Returns window manager name
 * */
std::string detect_wm() {
    /* Actually we only need to check if we're on sway or not,
     * but let's try to find a WM name if possible. If not, let it be just "other" */
    const char *env_var[2] = {"DESKTOP_SESSION", "SWAYSOCK"};
    char *env_val[2];
    std::string wm_name{"other"};

    for(int i=0; i<2; i++) {
        // get environment values
        env_val[i] = getenv(env_var[i]);
        if (env_val[i] != NULL) {
            std::string s(env_val[i]);
            if (s.find("sway") != std::string::npos) {
                wm_name = "sway";
                break;
            } else {
                // is the value a full path or just a name?
                if (s.find("/") == std::string::npos) {
                    // full value is the name
                    wm_name = s;
                    break;
                } else {
                    // path given
                    int idx = s.find_last_of("/") + 1;
                    wm_name = s.substr(idx);
                    break;
                }
            }
        }
    }
    return wm_name;
}

/*
 * Returns first 2 chars of current locale string
 * */
std::string get_locale() {
    char* env_val = getenv("LANG");
    std::string loc(env_val);
    std::string l = "en";
    if (loc.find("_") != std::string::npos) {
        int idx = loc.find_first_of("_");
        l = loc.substr(0, idx);
    }
    return l;
}

/*
 * Returns file content as a string
 * */
std::string read_file_to_string(std::string filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(std::string s, std::string filename) {
    std::ofstream file(filename);
    file << s;
}

/*
 * Returns output of a command as string
 * */
std::string get_output(std::string cmd) {
    const char *command = cmd.c_str();
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
