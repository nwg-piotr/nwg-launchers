/*
 * Tools for nwg-launchers
 * Copyright (c) 2021 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "charconv-compat.h"
#include "filesystem-compat.h"
#include "nwgconfig.h"
#include "nwg_exceptions.h"
#include "nwg_tools.h"
#include "log.h"


#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

// stores the name of the pid_file, for use in atexit
static fs::path pid_file{};

std::string_view get_home_dir() {
    static fs::path home_dir;
    if (home_dir.empty()) {
        const char* val = getenv("HOME");
        if (!val) {
            Log::error("$HOME not set");
            throw std::runtime_error{ "get_home_dir: $HOME not set" };
        }
        home_dir = val;
    }
    return { home_dir.native() };
}

/*
 * Returns config dir
 * */
fs::path get_config_dir(std::string_view app) {
    fs::path path;
    char* val = getenv("XDG_CONFIG_HOME");
    if (!val) {
        path = get_home_dir();
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
fs::path get_cache_home() {
    char* home_ = getenv("XDG_CACHE_HOME");
    fs::path home;
    if (home_) {
        home = home_;
    } else {
        home = get_home_dir();
        home /= ".cache";
    }
    return home;
}

/*
 * Return runtime dir
 * */
fs::path get_runtime_dir() {
    if (auto* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR")) {
        return xdg_runtime_dir;
    }
    std::error_code ec;
    {
        std::array<char, 64> myuid;
        auto myuid_ = getuid();
#ifdef HAVE_TO_CHARS
        if (auto [p, e] = std::to_chars(myuid.data(), myuid.data() + myuid.size(), myuid_); e == std::errc()) {
            std::string_view myuid_view{ myuid.data(), std::size_t(p - myuid.data()) };
#else
        if (auto n = std::snprintf(myuid.data(), 64, "%u", myuid_); n > 0) {
            std::string_view myuid_view{ myuid.data(), static_cast<std::string_view::size_type>(n) };
#endif
            if (fs::path path{ "/run/user/" }; fs::exists(path, ec) && !ec) {
                // let's try /run/user/ first
                path /= myuid_view;
                return path;
            }
        } else {
            throw std::runtime_error{ "Failed to convert UID to chars" };
        }
        ec.clear();
    }
    if (fs::path tmp{ Glib::get_tmp_dir() }; fs::exists(tmp, ec) && !ec) {
        return tmp;
    }
    throw std::runtime_error{ "Failed to determine user runtime directory" };
}

fs::path get_pid_file(std::string_view name) {
    auto dir = get_runtime_dir();
    dir /= name;
    return dir;
}

int parse_icon_size(std::string_view arg) {
    int i_s;
    if (parse_number(arg, i_s)) {
        switch (2 * (i_s > 2048) + (i_s < 16)) {
            case 0: return i_s;
            case 1: Log::error("Icon size is too small (<16), setting to 16");
                    return 16;
            case 2: 
                    Log::error("Icon size is too large (>2048), setting to 2048");
                    return 2048;
            default: throw std::logic_error{
                "parse_icon_size: unexpected switch variant triggered"
            };
        }
    } else {
        // -s argument couldn't be parsed, therefore something's really wrong
        throw std::runtime_error{ "Image size should be valid integer in range 16 - 2048\n" };
    }
}

/* RAII wrappers to reduce the amount of bookkeeping */
struct FdGuard {
    int fd;
    ~FdGuard() { close(fd); }
};
struct LockfGuard {
    int   fd;
    off_t len;
    LockfGuard(int fd, int cmd, off_t len): fd{ fd }, len{ len } {
        if (lockf(fd, cmd, len)) {
            int err = errno;
            throw ErrnoException{ "Failed to lock file: ", err };
        }
    }
    ~LockfGuard() {
        if (lockf(fd, F_ULOCK, len)) {
            int err = errno;
            Log::error("Failed to unlock file: ", error_description(err));
        }
    }
};
/* private helpers handling partial reads/writes (see {read,write}(2))  */
static inline size_t read_buf(int fd, char* buf, size_t n) {
    size_t total{ 0 };
    while (total < n) {
        auto bytes = read(fd, buf + total, n - total);
        if (bytes == 0) {
            break;
        }
        if (bytes < 0) {
            int err = errno;
            throw ErrnoException{ "read(2) failed: ", err };
        }
        total += bytes;
    }
    return total;
}
static inline void write_buf(int fd, const char* buf, size_t n) {
    size_t total{ 0 };
    while (total < n) {
        auto bytes = write(fd, buf + total, n - total);
        if (bytes == 0) {
            break;
        }
        if (bytes < 0) {
            int err = errno;
            throw ErrnoException{ "write(2) failed: ", err };
        }
        total += bytes;
    }
}

std::optional<pid_t> get_instance_pid(const char* path) {
    // we need write capability to be able to lockf(3) file
    auto fd = open(path, O_RDWR | O_CLOEXEC, 0);
    if (fd == -1) {
        int err = errno;
        if (err == ENOENT) {
            return std::nullopt;
        }
        throw ErrnoException{ "failed to open pid file: ", err };
    }
    FdGuard fd_guard{ fd };
    LockfGuard guard{ fd, F_LOCK, 0 };

    // we read at most 64 bytes which is more than enough for pid
    char buf[64]{};
    pid_t pid{ -1 };
    auto bytes = read_buf(fd, buf, 64);
    if (bytes == 0) {
        Log::warn("the pid file is empty");
        return std::nullopt;
    }
    std::string_view view{ buf, size_t(bytes) };
    if (!parse_number(view, pid)) {
        throw std::runtime_error{ "Failed to read pid from file" };
    }
    if (pid < 0) {
        Log::warn("the saved pid is negative");
        return std::nullopt;
    }
    if (kill(pid, 0) != 0) {
        Log::warn("the saved pid is stale");
        return std::nullopt;
    }
    return pid;
}

void write_instance_pid(const char* path, pid_t pid) {
    auto fd = open(path, O_WRONLY | O_CLOEXEC | O_CREAT, S_IWUSR | S_IRUSR);
    if (fd == -1) {
        int err = errno;
        throw ErrnoException{ "failed to open the pid file: ", err };
    }
    FdGuard fd_guard{ fd };
    LockfGuard guard{ fd, F_LOCK, 0 };
    auto str = std::to_string(pid);
    write_buf(fd, str.data(), str.size());
}

/*
 * Returns window manager name
 * */
std::string detect_wm(const Glib::RefPtr<Gdk::Display>& display, const Glib::RefPtr<Gdk::Screen>& screen) {
    /* Actually we only need to check if we're on sway, i3 or other WM,
     * but let's try to find a WM name if possible. If not, let it be just "other" */
    std::string wm_name{"other"};

#ifdef GDK_WINDOWING_X11
    {
        auto* g_display = display->gobj();
        auto* g_screen  = screen->gobj();
        if (GDK_IS_X11_DISPLAY (g_display)) {
            auto* str_ = gdk_x11_screen_get_window_manager_name (g_screen);
            if (str_) {
                Glib::ustring str = str_;
                wm_name = str.lowercase();
                return wm_name;
            }
        }
    }
#else
    (void)display;
    (void)screen;
#endif

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
    
    auto term_file = concat(config_dir, "/term"sv);
    auto terminal_file = concat(config_dir, "/terminal"sv);

    std::error_code ec;
    auto term_file_exists = fs::is_regular_file(term_file, ec) && !ec;
    ec.clear();
    auto terminal_file_exists = fs::is_regular_file(terminal_file, ec) && !ec;
    
    if (term_file_exists) {
        if (!terminal_file_exists) {
            fs::rename(term_file, terminal_file);
            terminal_file_exists = true;
        } else {
            fs::remove(term_file);
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
        Log::error("Unable to close socket");
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
 * Returns `swaymsg -t get_workspaces`
 * Throws `SwayError`
 * */
std::string SwaySock::get_workspaces() {
    send_header_(0, Commands::GetWorkspaces);
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
Geometry display_geometry(std::string_view wm, Glib::RefPtr<Gdk::Display> display, Glib::RefPtr<Gdk::Window> window) {
    Geometry geo = {0, 0, 0, 0};
    if (wm == "sway" || wm == "i3") {
        try {
            // TODO: Maybe there is a way to make it more uniform?
            SwaySock sock;
            auto jsonString = wm == "sway" ? sock.get_outputs() : sock.get_workspaces();
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
            Log::error("Failed checking display geometry, tries: ", retry);
            break;
        }
    }
    return geo;
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


namespace category {

using namespace std::string_view_literals;

std::string_view localize(const ns::json& source, std::string_view category) {
    auto& map = json_at(source, "categories"sv);
    if (category == "All"sv) {
        if (auto iter = map.find(category); iter != map.end()) {
            return iter.value().get<std::string_view>();
        }
        return "All"sv;
    }
    auto item = map.find(category);
    if (item == map.end()) {
        throw std::logic_error{ "Trying to localize unknown category" };
    }
    if (item.value()) {
        return item.value().get<std::string_view>();
    }
    return item.key();
}

} // namespace category


/*
 * Returns file content as a string
 * */
std::string read_file_to_string(const fs::path& filename) {
    std::ifstream input(filename);
    std::stringstream sstr;

    while(input >> sstr.rdbuf());

    return sstr.str();
}

/*
 * Saves a string to a file
 * */
void save_string_to_file(std::string_view s, const fs::path& filename) {
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


ns::json::reference json_at(ns::json& j, std::string_view key) {
#ifdef HAVE_MODERN_NLOHMANN_JSON
    auto& k = key;
#else
    // pray to SSO
    std::string k{ key };
#endif // HAVE_MODERN_NLOHMANN_JSON

    return j[k];
}

ns::json::const_reference json_at(const ns::json& j, std::string_view key) {
#ifdef HAVE_MODERN_NLOHMANN_JSON
    auto& k = key;
#else
    // pray to SSO
    std::string k{ key };
#endif // HAVE_MODERN_NLOHMANN_JSON

    return j[k];
}


/*
 * Reads json from file
 * */
ns::json json_from_file(const fs::path& path) {
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
void save_json(const ns::json& json_obj, const fs::path& filename) {
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
            Log::error("invalid color value. Should be RRGGBB or RRGGBBAA");
        }
    }
    catch (...) {
        Log::error("Unable to parse RGB(A) value");
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

/* Prepares CSS file for app `name` using config directory `config_dir`
 * if default css file (config_dir / style.css) does not exist, it is copied from DATA directory to config_dir
 * TODO: explain better
 */
fs::path setup_css_file(std::string_view name, const fs::path& config_dir, const fs::path& custom_css_file) {
    // default and custom style sheet
    auto default_css_file = config_dir / "style.css";
    // css file to be used
    auto css_file = config_dir / custom_css_file;
    // copy default file if not found
    if (!fs::exists(default_css_file)) {
        try {
            fs::path sample_css_file { DATA_DIR_STR };
            sample_css_file /= name;
            sample_css_file /= "style.css";
            fs::copy_file(sample_css_file, default_css_file, fs::copy_options::overwrite_existing);
        } catch (const fs::filesystem_error& error) {
            Log::error("Failed copying default style.css: \'", error.what(), "\'");
        }
    }

    if (!fs::is_regular_file(css_file)) {
        Log::error("Unable to open user-specified css file '", css_file, "', using default");
        css_file = default_css_file;
    }
    return css_file;
}

int instance_on_sigterm(void* userdata) {
    static_cast<Instance*>(userdata)->on_sigterm();
    return G_SOURCE_CONTINUE;
}
int instance_on_sigusr1(void* userdata) {
    static_cast<Instance*>(userdata)->on_sigusr1();
    return G_SOURCE_CONTINUE;
}
int instance_on_sighup(void* userdata) {
    static_cast<Instance*>(userdata)->on_sighup();
    return G_SOURCE_CONTINUE;
}
int instance_on_sigint(void* userdata) {
    static_cast<Instance*>(userdata)->on_sigint();
    return G_SOURCE_CONTINUE;
}
