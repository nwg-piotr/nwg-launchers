/* GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

/* 
 * Returns cache file path 
 * */
std::string get_cache_path() {
	std::string s = "";
	char* val = getenv("XDG_CACHE_HOME");
	if (val) {
		s = val;
	} else {
		char* val = getenv("HOME");
		s = val;
		s += "/.cache";
	}
	fs::path dir (s);
	fs::path file ("nwg-fav-cache");
	fs::path full_path = dir / file;
	
	return full_path;
}

/* 
 * Returns config dir 
 * */
std::string get_config_dir() {
	std::string s = "";
	char* val = getenv("XDG_CONFIG_HOME");
	if (val) {
		s = val;
	} else {
		char* val = getenv("HOME");
		s = val;
		s += "/.config/nwggrid";
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

	for(int i=0; i<3; i++) {
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
 * Returns locations of .desktop files 
 * */
std::vector<std::string> get_app_dirs() {
	std::string homedir = getenv("HOME");
	std::vector<std::string> result = {homedir + "/.local/share/applications", "/usr/share/applications",
		"/usr/local/share/applications"};
	// TODO XDG_DATA_DIRS should be prepended

	return result;
}

/* 
 * Returns all .desktop files paths 
 * */
std::vector<std::string> list_entries(std::vector<std::string> paths) {
	std::vector<std::string> desktop_paths;
	for (std::string dir : paths) {
		struct stat st;
		char* c = const_cast<char*>(dir.c_str());
		// if directory exists
		if (stat(c, &st) == 0) {
			for (const auto & entry : fs::directory_iterator(dir)) {
				desktop_paths.push_back(entry.path());
			}
		}
	}
	return desktop_paths;
}

/* 
 * Parses .desktop file to vector<string> {'name', 'exec', 'icon', 'comment'} 
 * */
std::vector<std::string> desktop_entry(std::string path, std::string lang) {
	std::vector<std::string> fields = {"", "", "", ""};
	
	std::ifstream file(path);
	std::string str;
	while (std::getline(file, str)) {
		bool read_me = true;
		if (str.find("[") == 0) {
			read_me = (str.find("[Desktop Entry") != std::string::npos);
			if (!read_me) {
				break;
			} else {
				continue;
			}
		}
		if (read_me) {
			std::string loc_name = "Name[" + lang + "]=";
			
			if (str.find("Name=") == 0 || str.find(loc_name) == 0) {
				if (str.find_first_of("=") != std::string::npos) {
					int idx = str.find_first_of("=");
					std::string val = str.substr(idx + 1);
					fields[0] = val;
				}
			}
			if (str.find("Exec=") == 0) {
				if (str.find_first_of("=") != std::string::npos) {
					int idx = str.find_first_of("=");
					std::string val = str.substr(idx + 1);
					// strip ' %' and following
					if (val.find_first_of("%") != std::string::npos) {
						int idx = val.find_first_of("%");
						val = val.substr(0, idx - 1);
					}
					fields[1] = val;
				}
			}
			if (str.find("Icon=") == 0) {
				if (str.find_first_of("=") != std::string::npos) {
					int idx = str.find_first_of("=");
					std::string val = str.substr(idx + 1);
					fields[2] = val;
				}
			}
			std::string loc_comment = "Comment[" + lang + "]=";
			if (str.find("Comment=") == 0 || str.find(loc_comment) == 0) {
				if (str.find_first_of("=") != std::string::npos) {
					int idx = str.find_first_of("=");
					std::string val = str.substr(idx + 1);
					fields[3] = val;
				}
			}
		}
		
	}
	return fields;
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

void save_json(ns::json json_obj, std::string filename) {
	std::ofstream o(filename);
	o << std::setw(2) << json_obj << std::endl;
}

/* 
 * Returns json object out of the cache file 
 * */
ns::json get_cache(std::string cache_file) {
    std::string cache_string = read_file_to_string(cache_file);

    return string_to_json(cache_string);
}

/* 
 * Returns n cache items sorted by clicks; n should be the number of grid columns 
 * */
std::vector<CacheEntry> get_favourites(ns::json cache, int number) {
    // read from json object
    std::vector<CacheEntry> sorted_cache {}; // not yet sorted
    for (ns::json::iterator it = cache.begin(); it != cache.end(); ++it) {
		struct CacheEntry entry = {it.key(), it.value()};
		sorted_cache.push_back(entry);
	}
	// actually sort by the number of clicks
	sort(sorted_cache.begin(), sorted_cache.end(), [](const CacheEntry& lhs, const CacheEntry& rhs) {
		return lhs.clicks > rhs.clicks;
	});
	// Trim to the number of columns, as we need just 1 row of favourites
	std::vector<CacheEntry>::const_iterator first = sorted_cache.begin();
	std::vector<CacheEntry>::const_iterator last = sorted_cache.begin() + number;
	std::vector<CacheEntry> favourites(first, last);
	return favourites;
}

/* 
 * Returns x, y, width, hight of focused display 
 * */
std::vector<int> display_geometry(std::string wm) {
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
	} // todo add detection in not-sway environment
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
			pixbuf = icon_theme->load_icon(icon, image_size, Gtk::ICON_LOOKUP_USE_BUILTIN);
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
