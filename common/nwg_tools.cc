/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <iostream>
#include <fstream>

#include "nwgconfig.h"
#include "nwg_tools.h"

// extern variables from nwg_tools.h
int image_size = 72;

// stores the name of the pid_file, for use in atexit
static std::string pid_file{};

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
        if (!val) {
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
    /* Actually we only need to check if we're on sway, i3 or other WM,
     * but let's try to find a WM name if possible. If not, let it be just "other" */
    std::string wm_name{"other"};

    for(auto env : {"DESKTOP_SESSION", "SWAYSOCK", "I3SOCK"}) {
        // get environment values
        char *env_val = getenv(env);
        if (env_val != NULL) {
            std::string s(env_val);
            if (s.find("sway") != std::string::npos) {
                wm_name = "sway";
                break;
            } else if (s.find("i3") != std::string::npos) {
                wm_name = "i3";
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
 * Detect installed terminal emulator, save the command to txt file.
 * If none found, 'xterm' is our fallback value.
 * */
 std::string get_term(std::string config_dir) {
    std::string t{"xterm"};
    std::string term_file = config_dir + "/term";
    if (std::ifstream(term_file)) {
        // File exists: read command from the file, skip checks
        t = read_file_to_string(term_file);
        t.erase(remove(t.begin(), t.end(), '\n'), t.end());
    } else {
        // We'll only look for terminals that return 0 exit code from the '<terminal> -v' command.
        // First one found will be saved to the ''~/.config/nwg-launchers/nwggrid/term' file.
        std::string checks [] = {
            "alacritty -V",
            "kitty -v",
            "lxterminal -v",
            "xfce4-terminal -V",
            "termite -v",
            "terminator -v"
        };
        for (auto&& check : checks) {
            check = check + " &>/dev/null";
            const char *command = check.c_str();
            int status = std::system(command);
            if (status == 0) {
                t = split_string(check, " ")[0];
                std::string cmd = "echo " + t + " > " + term_file;
                const char *c = cmd.c_str();
                std::system(c);
                std::cout << "Saving \'" << t << "\' to \'" << term_file << "\' - edit to use another terminal.\n";
                break;
            }
        }
        if (!std::ifstream(term_file)) {
            std::cout << "No known terminal found. Saving \'xterm\' to \'" << term_file << "\'.\n";
            std::string cmd = "echo " + t + " > " + term_file;
            const char *c = cmd.c_str();
            std::system(c);
        }
    }
    return t;
 }

/*
 * Returns x, y, width, hight of focused display
 * */
Geometry display_geometry(const std::string& wm, Glib::RefPtr<Gdk::Display> display, Glib::RefPtr<Gdk::Window> window) {
    Geometry geo = {0, 0, 0, 0};
    if (wm == "sway") {
        try {
            auto jsonString = get_output("swaymsg -t get_outputs");
            auto jsonObj = string_to_json(jsonString);
            for (auto&& entry : jsonObj) {
                if (entry.at("focused")) {
                    auto&& rect = entry.at("rect");
                    geo.x = rect.at("x");
                    geo.y = rect.at("y");
                    geo.width = rect.at("width");
                    geo.height = rect.at("height");
                    break;
                }
            }
            return geo;
        }
        catch (...) { }
    } else if (wm == "i3") {
        try {
            auto jsonString = get_output("i3-msg -t get_workspaces");
            auto jsonObj = string_to_json(jsonString);
            for (auto&& entry : jsonObj) {
                if (entry.at("focused")) {
                    auto&& rect = entry.at("rect");
                    geo.x = rect.at("x");
                    geo.y = rect.at("y");
                    geo.width = rect.at("width");
                    geo.height = rect.at("height");
                    break;
                }
            }
        }
        catch (...) { }
    }

    // it's going to fail until the window is actually open
    int retry = 0;
    while (geo.width == 0 || geo.height == 0) {
        Gdk::Rectangle rect;
        auto monitor = display->get_monitor_at_window(window);
        if (monitor) {
            monitor->get_geometry(rect);
            geo.x = rect.get_x();
            geo.y = rect.get_y();
            geo.width = rect.get_width();
            geo.height = rect.get_height();
        }
        retry++;
        if (retry > 100) {
            std::cerr << "\nERROR: Failed checking display geometry, tries: " << retry << "\n\n";
            break;
        }
    }
    return geo;
}

/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(
    const Gtk::IconTheme& icon_theme,
    const std::string& icon,
    const Glib::RefPtr<Gdk::Pixbuf>& fallback
) {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    try {
        if (icon.find_first_of("/") == std::string::npos) {
            pixbuf = icon_theme.load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } else {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        }
    } catch (...) {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file("/usr/share/pixmaps/" + icon, image_size, image_size, true);
        } catch (...) {
            pixbuf = fallback;
        }
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

/*
 * Returns current locale
 * */
std::string get_locale() {
    // avoid crash when LANG unset at all (regressed by #83 in v0.3.3) #114
    if (getenv("LANG") != NULL) {
        std::string loc = getenv("LANG");
        if (!loc.empty()) {
            auto idx = loc.find_first_of("_");
            if (idx != std::string::npos) {
                loc.resize(idx);
            }
            return loc;
        }
    }
    return "en";
}

/*
 * Returns file content as a string
 * */
std::string read_file_to_string(const std::string& filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(const std::string& s, const std::string& filename) {
    std::ofstream file(filename);
    file << s;
}

/*
 * Splits string into vector of strings by delimiter
 * */
std::vector<std::string> split_string(std::string_view str, std::string_view delimiter) {
    std::vector<std::string> result;
    std::size_t current, previous = 0;
    current = str.find_first_of(delimiter);
    while (current != std::string_view::npos) {
        result.emplace_back(str.substr(previous, current - previous));
        previous = current + 1;
        current = str.find_first_of(delimiter, previous);
    }
    result.emplace_back(str.substr(previous, current - previous));

    return result;
}

/*
 * Splits string by delimiter and takes the last piece
 * */
std::string_view take_last_by(std::string_view str, std::string_view delimiter) {
    auto pos = str.find_last_of(delimiter);
    if (pos != std::string_view::npos) {
        return str.substr(pos + 1);
    }
    return str;
}

/*
 * Converts json string into a json object
 * */
ns::json string_to_json(const std::string& jsonString) {
    ns::json jsonObj;
    std::stringstream(jsonString) >> jsonObj;

    return jsonObj;
}

/*
 * Saves json into file
 * */
void save_json(const ns::json& json_obj, const std::string& filename) {
    std::ofstream o(filename);
    o << std::setw(2) << json_obj << std::endl;
}

/*
 * Sets RGBA background according to hex strings
* */
void set_background(std::string_view string) {
    std::string hex_string {"0x"};
    unsigned long int rgba;
    std::stringstream ss;
    try {
        if (string.find("#") == 0) {
            hex_string += string.substr(1);
        } else {
            hex_string += string;
        }
        ss << std::hex << hex_string;
        ss >> rgba;
        if (hex_string.size() == 8) {
            background.red = ((rgba >> 16) & 0xff) / 255.0;
            background.green = ((rgba >> 8) & 0xff) / 255.0;
            background.blue = ((rgba) & 0xff) / 255.0;
        } else if (hex_string.size() == 10) {
            background.red = ((rgba >> 24) & 0xff) / 255.0;
            background.green = ((rgba >> 16) & 0xff) / 255.0;
            background.blue = ((rgba >> 8) & 0xff) / 255.0;
            background.alpha = ((rgba) & 0xff) / 255.0;
        } else {
            std::cerr << "ERROR: invalid color value. Should be RRGGBB or RRGGBBAA\n";
        }
    }
    catch (...) {
        std::cerr << "Error parsing RGB(A) value\n";
    }
}

/*
 * Returns output of a command as string
 * */
std::string get_output(const std::string& cmd) {
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

/*
 * Remove pid_file created by create_pid_file_or_kill_pid.
 * This function will be run before exiting.
 * */
static void clean_pid_file(void) {
    unlink(pid_file.c_str());
}

/*
 * Signal handler to exit normally with SIGTERM
 * */
static void exit_normal(int sig) {
    if (sig == SIGTERM) {
        std::cerr << "Received SIGTERM, exiting...\n";
    }

    std::exit(1);
}

/*
 * Creates PID file for the new instance,
 * or kills the other cmd instance.
 *
 * If it creates a PID file, it also sets up a signal handler to exit
 * normally if it receives SIGTERM; and registers an atexit() action
 * to run when exiting normally.
 *
 * This allows for behavior where using the shortcut to open one
 * of the launchers closes the currently running one.
 * */
void create_pid_file_or_kill_pid(std::string cmd) {
    std::string myuid = std::to_string(getuid());

    char *runtime_dir_tmp = getenv("XDG_RUNTIME_DIR");
    std::string runtime_dir;
    if (runtime_dir_tmp) {
        runtime_dir = runtime_dir_tmp;
    } else {
        runtime_dir = "/var/run/user/" + myuid;
    }

    pid_file = runtime_dir + "/" + cmd + ".pid";

    auto pid_read = std::ifstream(pid_file);
    // set to not throw exceptions
    pid_read.exceptions(std::ifstream::goodbit);
    if (pid_read.is_open()) {
        // opening file worked - file exists
        pid_t saved_pid;
        pid_read >> saved_pid;

        if (saved_pid > 0 && kill(saved_pid, 0) == 0) {
            // found running instance
            // PID file will be deleted by process's atexit routine
            int rv = kill(saved_pid, SIGTERM);

            // exit with status dependent on kill success
            std::exit(rv == 0 ? 0 : 1);
        }
    }

    std::string mypid = std::to_string(getpid());
    save_string_to_file(mypid, pid_file);

    // register function to clean pid file
    atexit(clean_pid_file);
    // register signal handler for SIGTERM
    struct sigaction act {};
    act.sa_handler = exit_normal;
    sigaction(SIGTERM, &act, nullptr);
}
