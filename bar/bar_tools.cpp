/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

/* Try to lock the lock file
 * Thanks to chmike at https://stackoverflow.com/a/1643134
 * */
int tryGetLock(char const *lockName) {
    mode_t m = umask(0);
    int fd = open(lockName, O_RDWR|O_CREAT, 0666);
    umask( m );
    if( fd >= 0 && flock(fd, LOCK_EX | LOCK_NB ) < 0) {
        close(fd);
        fd = -1;
    }
    return fd;
}

/*
 * Returns config dir
 * */
std::string get_config_dir() {
    std::string s = "";
    char* val = getenv("XDG_CONFIG_HOME");
    if (val) {
        s = val;
        s += "/nwgbar";
    } else {
        char* val = getenv("HOME");
        s = val;
        s += "/.config/nwgbar";
    }
    fs::path dir (s);
    return s;
}

/*
 * Returns a file content as a string
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

/* Converts json string into a json object;
 * Requires nlohmann-json package, https://github.com/nlohmann/json
 * */
ns::json string_to_json(std::string jsonString) {
    const char *s = jsonString.c_str();
    ns::json jsonObj;
    std::stringstream(s) >> jsonObj;

    return jsonObj;
}

/*
 * Returns json object out of a template file
 * */
ns::json get_bar_json(std::string custom_bar) {
    std::string bar_string = read_file_to_string(custom_bar);

    return string_to_json(bar_string);
}

/*
 * Returns a vector of BarEntry data structs
 * */
std::vector<BarEntry> get_bar_entries(ns::json bar_json) {
    // read from json object
    std::vector<BarEntry> entries {};
    for (ns::json::iterator it = bar_json.begin(); it != bar_json.end(); ++it) {
        int index = std::distance(bar_json.begin(), it);
        struct BarEntry entry = {bar_json[index].at("name"), bar_json[index].at("exec"), bar_json[index].at("icon")};
        entries.push_back(entry);
    }
    return entries;
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

/*
 * Returns Gtk::Image out of the icon name of file path
 * */
Gtk::Image* app_image(std::string icon) {
    Glib::RefPtr<Gtk::IconTheme> icon_theme;
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;

    icon_theme = Gtk::IconTheme::get_default();

    if (icon.find_first_of("/") != 0) {
        try {
            pixbuf = icon_theme->load_icon(icon, image_size, Gtk::ICON_LOOKUP_FORCE_SIZE);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file("/usr/share/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    } else {
        try {
            pixbuf = Gdk::Pixbuf::create_from_file(icon, image_size, image_size, true);
        } catch (...) {
            pixbuf = Gdk::Pixbuf::create_from_file("/usr/share/nwggrid/icon-missing.svg", image_size, image_size, true);
        }
    }
    auto image = Gtk::manage(new Gtk::Image(pixbuf));

    return image;
}

/*
 * Argument parser
 * Credits for this cool class go to iain at https://stackoverflow.com/a/868894
 * */
class InputParser{
    public:
        InputParser (int &argc, char **argv){
            for (int i=1; i < argc; ++i)
                this->tokens.push_back(std::string(argv[i]));
        }
        /// @author iain
        const std::string& getCmdOption(const std::string &option) const{
            std::vector<std::string>::const_iterator itr;
            itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const std::string empty_string("");
            return empty_string;
        }
        /// @author iain
        bool cmdOptionExists(const std::string &option) const{
            return std::find(this->tokens.begin(), this->tokens.end(), option)
                   != this->tokens.end();
        }
    private:
        std::vector <std::string> tokens;
};
