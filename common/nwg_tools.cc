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
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "nwgconfig.h"
#include "nwg_tools.h"

// extern variables from nwg_tools.h
int image_size = 72;

// stores the name of the pid_file, for use in atexit
static std::string pid_file{};

/*
 * Returns config dir
 * */
std::filesystem::path get_config_dir(std::string_view app) {
    std::filesystem::path path;
    char* val = getenv("XDG_CONFIG_HOME");
    if (!val) {
        val = getenv("HOME");
        if (!val) {
            std::cerr << "ERROR: Couldn't find config directory, $HOME not set!";
            std::exit(EXIT_FAILURE);
        }
        path = val;
        path /= ".config";
    } else {
        path = val;
    }
    path /= "nwg-launchers";
    path /= app;
    return path;
}

/*
 * Returns path to cache directory
 * */
std::filesystem::path get_cache_home() {
    char* home_ = getenv("XDG_CACHE_HOME");
    std::filesystem::path home;
    if (home_) {
        home = home_;
    } else {
        home_ = getenv("HOME");
        if (!home_) {
            std::cerr << "ERROR: Neither XDG_CACHE_HOME nor HOME are not defined\n";
            std::exit(EXIT_FAILURE);
        }
        home = home_;
        home /= ".cache";
    }
    return home;
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
        if (auto env_val = getenv(env)) {
            std::string_view s = env_val;
            if (s.find("sway") != s.npos) {
                wm_name = "sway";
                break;
            } else if (s.find("i3") != s.npos) {
                wm_name = "i3";
                break;
            } else {
                // is the value a full path or just a name?
                if (s.find('/') == s.npos) {
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
 * Detect installed terminal emulator, save the command to txt file for further use.
 * */
std::string get_term(std::string_view config_dir) {
    using namespace std::string_view_literals;
    auto concat = [](auto&& ... xs) { std::string r; ((r += xs), ...); return r; };
    
    auto term_file = concat(config_dir, "/term"sv);
    auto terminal_file = concat(config_dir, "/terminal"sv);

    std::error_code ec;
    auto term_file_exists = std::filesystem::is_regular_file(term_file, ec) && !ec;
    ec.clear();
    auto terminal_file_exists = std::filesystem::is_regular_file(terminal_file, ec) && !ec;
    
    if (term_file_exists) {
        if (!terminal_file_exists) {
            std::filesystem::rename(term_file, terminal_file);
            terminal_file_exists = true;
        } else {
            std::filesystem::remove(term_file);
        }
    }
    
    std::string term;
    auto check_env_vars = [&]() {
        // TODO: TERMINAL is usually just term name, we don't know if it supports '-e'
        constexpr std::array known_term_vars { /*"TERMINAL",*/ "TERMCMD"};
        for (auto var: known_term_vars) {
            if (auto e = getenv(var)) {
                term = e;
                return 0;
            }
        }
        return -1;
    };
    auto check_terms = [&]() {
        constexpr std::array term_flags { " -e"sv, ""sv };
        constexpr std::array terms {
            std::pair{ "alacritty"sv, 0 },
            std::pair{ "kitty"sv, 0 },
            std::pair{ "urxvt"sv, 0 },
            std::pair{ "lxterminal"sv, 0 },
            std::pair{ "sakura"sv, 0 },
            std::pair{ "st"sv, 0 },
            std::pair{ "termite"sv, 0 },
            std::pair{ "terminator"sv, 0 },
            std::pair{ "xfce4-terminal"sv, 0 },
            std::pair{ "gnome-terminal"sv, 0 },
            std::pair{ "foot"sv, 1 }
        };
        for (auto&& [term_, flag_]: terms) {
            // TODO: use exec/similar from glib to avoid spawning shells
            auto command = concat("command -v "sv, term, " > /dev/null 2>&1"sv);
            if (std::system(command.data()) == 0) {
                term = concat(term_, term_flags[flag_]);
                return 0;
            }
        }
        return -1;
    };
    
    bool needs_save = true;
    if (check_env_vars()) {
        if (terminal_file_exists) {
            term = read_file_to_string(terminal_file);
            // do NOT append ' -e' as it breaks non-standard terminals
            term.erase(remove(term.begin(), term.end(), '\n'), term.end());
            needs_save = false;
        } else if (check_terms()) {
            // nothing worked, fallback to xterm
            term = "xterm -e"sv;
        }
    }
    if (needs_save) {
        save_string_to_file(term, terminal_file);
    }
    return term;
}
 
/*
 * Connects to Sway socket, loading socket info from $SWAYSOCK or $I3SOCK
 * Throws SwayError
 * - sway --get-socketpath is not supported (yet?)
 * */
SwaySock::SwaySock() {
    auto path = getenv("SWAYSOCK");
    if (!path) {
        path = getenv("I3SOCK");
        if (!path) {
            throw SwayError::EnvNotSet;
        }
    }
    path = strdup(path);

    sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_ == -1) {
        free(path);
        throw SwayError::OpenFailed;
    }
    
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
    if (connect(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
        free(path);
        throw SwayError::ConnectFailed;
    }
    free(path);
}

SwaySock::~SwaySock() {
    if (close(sock_)) {
        std::cerr << "ERROR: Unable to close socket\n";
    }
}

/*
 * Returns `swaymsg -t get_outputs`
 * Throws `SwayError`
 * */
std::string SwaySock::get_outputs() {
    send_header_(0, Commands::GetOutputs);
    return recv_response_();
}

/*
 * Returns output of previously issued command
 * Throws `SwayError::Recv{Header,Body}Failed`
 */
std::string SwaySock::recv_response_() {
    std::size_t total = 0;
    while (total < HEADER_SIZE) {
        auto received = recv(sock_, header.data(), HEADER_SIZE - total, 0);
        if (received < 0) {
            throw SwayError::RecvHeaderFailed;
        }
        total += received;
    }
    auto payload_size = *reinterpret_cast<std::uint32_t*>(header.data() + MAGIC_SIZE);
    std::string buffer(payload_size + 1, '\0');
    auto payload = buffer.data();
    total = 0;
    while (total < payload_size) {
        auto received = recv(sock_, payload + total, payload_size - total, 0);
        if (received < 0) {
            throw SwayError::RecvBodyFailed;
        }
        total += received;
    }
    return buffer;   
}

/*
 * Asks Sway to run `cmd`
 * Throws `SwayError::Send{Header,Body}Failed`
 * */
void SwaySock::run(std::string_view cmd) {
    send_header_(cmd.size(), Commands::Run);
    send_body_(cmd);
    // should we recv the response?
    // suppress warning
    (void)recv_response_();
}

void SwaySock::send_header_(std::uint32_t message_len, Commands command) {
    memcpy(header.data(), MAGIC.data(), MAGIC_SIZE);
    memcpy(header.data() + MAGIC_SIZE, &message_len, sizeof(message_len));
    memcpy(header.data() + MAGIC_SIZE + sizeof(message_len), &command, sizeof(command));
    if (write(sock_, header.data(), HEADER_SIZE) == -1) {
        throw SwayError::SendHeaderFailed;
    }
}
void SwaySock::send_body_(std::string_view cmd) {
    if (write(sock_, cmd.data(), cmd.size()) == -1) {
        throw SwayError::SendBodyFailed;
    }
}

/*
 * Returns x, y, width, hight of focused display
 * */
Geometry display_geometry(const std::string& wm, Glib::RefPtr<Gdk::Display> display, Glib::RefPtr<Gdk::Window> window) {
    Geometry geo = {0, 0, 0, 0};
    if (wm == "sway") {
        try {
            SwaySock sock;
            auto jsonString = sock.get_outputs();
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
    } else if (wm == "i3") { // TODO: shouldn't sway also work with i3?
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
    std::string locale = "en";
    // avoid crash when LANG unset at all (regressed by #83 in v0.3.3) #114
    if (auto env = getenv("LANG")) {
        std::string_view loc = env;
        if (!loc.empty()) {
            auto idx = loc.find_first_of('_');
            if (idx != loc.npos) {
                loc.remove_suffix(loc.size() - idx);
            }
            locale = loc;
        }
    }
    return locale;
}

/*
 * Returns file content as a string
 * */
std::string read_file_to_string(const std::filesystem::path& filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(std::string_view s, const std::filesystem::path& filename) {
    std::ofstream file(filename);
    file << s;
}

/*
 * Splits string into vector of strings by delimiter
 * */
std::vector<std::string_view> split_string(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> result;
    std::size_t current, previous = 0;
    current = str.find_first_of(delimiter);
    while (current != str.npos) {
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
    if (pos != str.npos) {
        return str.substr(pos + 1);
    }
    return str;
}

/*
 * Reads json from file
 * */
ns::json json_from_file(const std::filesystem::path& path) {
    ns::json json;
    std::ifstream{path} >> json;
    return json;
}

/*
 * Converts json string into a json object
 * */
ns::json string_to_json(std::string_view jsonString) {
    return ns::json::parse(jsonString.begin(), jsonString.end());
}

/*
 * Saves json into file
 * */
void save_json(const ns::json& json_obj, const std::filesystem::path& filename) {
    std::ofstream o(filename);
    o << std::setw(2) << json_obj << std::endl;
}

/*
 * Sets RGBA background according to hex strings
 * Note: if `string` is #RRGGBB, alpha will not be changed
 * */
void decode_color(std::string_view string, RGBA& color) {
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
            color.red = ((rgba >> 16) & 0xff) / 255.0;
            color.green = ((rgba >> 8) & 0xff) / 255.0;
            color.blue = ((rgba) & 0xff) / 255.0;
        } else if (hex_string.size() == 10) {
            color.red = ((rgba >> 24) & 0xff) / 255.0;
            color.green = ((rgba >> 16) & 0xff) / 255.0;
            color.blue = ((rgba >> 8) & 0xff) / 255.0;
            color.alpha = ((rgba) & 0xff) / 255.0;
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
static void clean_pid_file() {
    unlink(pid_file.c_str());
}

/*
 * Signal handler to exit normally with SIGTERM
 * */
// Put string in global scope to be sure no dynamic allocation will happen
static const std::string exit_sigterm_msg = "Received SIGTERM, exiting...\n";
static void exit_normal(int sig) {
    // We have to use only async-signal-safe functions here
    if (sig == SIGTERM) {
        write(STDERR_FILENO, exit_sigterm_msg.c_str(), exit_sigterm_msg.length());
    }

    std::quick_exit(1);
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
    char *runtime_dir_tmp = getenv("XDG_RUNTIME_DIR");
    std::string runtime_dir;
    if (runtime_dir_tmp) {
        runtime_dir = runtime_dir_tmp;
    } else {
        runtime_dir = "/var/run/user/" + std::to_string(getuid());
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

    auto mypid = std::to_string(getpid());
    save_string_to_file(mypid, pid_file);

    // register function to clean pid file - will be used if it exits normally
    std::atexit(clean_pid_file);
    // also register for quick_exit, since it isn't safe to call std::exit
    // inside a signal handler
    std::at_quick_exit(clean_pid_file);
    // register signal handler for SIGTERM
    struct sigaction act {};
    act.sa_handler = exit_normal;
    sigaction(SIGTERM, &act, nullptr);
}
