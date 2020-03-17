/*
 * GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
 
#include "dmenu.h"
#include "dmenu_tools.cpp"
#include "dmenu_classes.cc"
#include <sys/time.h>

int main(int argc, char *argv[]) {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	
	/* Try to lock /tmp/nwgdmenu.lock file. This will return -1 if the command is already running.
	 * Thanks to chmike at https://stackoverflow.com/a/1643134 */
	
	// Create pid file if not yet exists
	if (!std::ifstream("/tmp/nwgdmenu.lock")) {
		save_string_to_file("nwgdmenu lock file", "/tmp/nwgdmenu.lock");
	}
	
	if (tryGetLock("/tmp/nwgdmenu.lock") == -1) {
		// kill if already running
		std::remove("/tmp/nwgdmenu.lock");
		std::string cmd = "pkill -f nwgdmenu";
		const char *command = cmd.c_str();
		std::system(command);
		std::exit(0);
	}

	std::string lang ("");

	InputParser input(argc, argv);
    if(input.cmdOptionExists("-h")){
        std::cout << "GTK application grid: nwgdmenu 0.0.1 (c) Piotr Miller 2020\n\n";
        std::cout << "nwgdmenu [-h] [-f] [-o <opacity>] [-c <col>] [-s <size>] [-l <ln>]\n\n";
        std::cout << "Options:\n";
        std::cout << "-h            show this help message and exit\n";
        std::cout << "-f            display favourites\n";
        std::cout << "-o <opacity>  background opacity (0.0 - 1.0, default 0.9)\n";
        std::cout << "-c <col>      number of grid columns (default: 6)\n";
        std::cout << "-s <size>     button image size (default: 72)\n";
        std::cout << "-l <ln>       force use of <ln> language\n";
        std::exit(0);
    }
    if (input.cmdOptionExists("-f")){
        favs = true;
    }
    const std::string &forced_lang = input.getCmdOption("-l");
    if (!forced_lang.empty()){
        lang = forced_lang;
    }
    const std::string &cols = input.getCmdOption("-c");
    if (!cols.empty()){
        try {
			int n_c = std::stoi(cols);
			if (n_c > 0 && n_c < 100) {
				num_col = n_c;
			} else {
				std::cout << "\nERROR: Columns must be in range 1 - 99\n\n";
			}
		} catch (...) {
			std::cout << "\nERROR: Invalid number of columns\n\n";
		}
    }
    
    const std::string &opa = input.getCmdOption("-o");
    if (!opa.empty()){
        try {
			double o = std::stod(opa);
			if (o >= 0.0d && o <= 1.0d) {
				opacity = o;
			} else {
				std::cout << "\nERROR: Opacity must be in range 0.0 to 1.0\n\n";
			}
		} catch (...) {
			std::cout << "\nERROR: Invalid opacity value\n\n";
		}
    }
    
    const std::string &i_size = input.getCmdOption("-s");
    if (!i_size.empty()){
        try {
			int i_s = std::stoi(i_size);
			if (i_s >= 16 && i_s <= 256) {
				image_size = i_s;
			} else {
				std::cout << "\nERROR: Size must be in range 16 - 256\n\n";
			}
		} catch (...) {
			std::cout << "\nERROR: Invalid image size\n\n";
		}
    }

    std::string config_dir = get_config_dir();
    if (!fs::is_directory(config_dir)) {
		std::cout << "Config dir not found, creating...\n";
		fs::create_directory(config_dir);
	}

    std::string css_file = config_dir + "/style.css";
    const char *custom_css = css_file.c_str();
    if (!fs::exists(css_file)) {
		fs::path source_file = "/usr/share/nwgdmenu/style.css";
		fs::path target = css_file;
		try {
			fs::copy_file("/usr/share/nwgdmenu/style.css", target, fs::copy_options::overwrite_existing);
		} catch (...) {
			std::cout << "Failed copying default style.css\n";
		}
	}

    /* get current WM name */
    wm = detect_wm();
    std::cout << "WM: " << wm << "\n";
    
    /* get lang (2 chars long string) if not yet forced */
    if (lang.length() != 2) {
		lang = get_locale();
	}
    std::cout << "Locale: " << lang << "\n";
    
    /* get all applications dirs */
    std::vector<std::string> command_dirs = get_command_dirs();
	
    /* get a list of paths to all commands */
    std::vector<std::string> commands = list_commands(command_dirs);
    std::cout << commands.size() << " commands found\n";
	
	/* Create a vector of commands (w/o path) */
	std::vector<Glib::ustring> all_commands {};
	for (std::string command : commands) {
		std::vector<std::string> parts = split_string(command, "/");
		std::string cmd = parts[parts.size() - 1];
		if (!cmd.find(".") == 0 && cmd.size() != 1) {
			all_commands.push_back(cmd);
		}
	}
	
	/* Sort case insensitive */
	std::sort(all_commands.begin(), all_commands.end(), [](const std::string& a, const std::string& b) -> bool {
        for (size_t c = 0; c < a.size() and c < b.size(); c++) {
            if (std::tolower(a[c]) != std::tolower(b[c])) {
				return (std::tolower(a[c]) < std::tolower(b[c]));
			}
        }
        return a.size() < b.size();
    });
    
	/* turn off borders, enable floating on sway */
	if (wm == "sway") {
		std::string cmd = "swaymsg for_window [title=\"~nwgdmenu*\"] floating enable";
		const char *command = cmd.c_str();
		std::system(command);
		cmd = "swaymsg for_window [title=\"~nwgdmenu*\"] border none";
		command = cmd.c_str();
		std::system(command);
	}
    
    Gtk::Main kit(argc, argv);

    GtkCssProvider* provider = gtk_css_provider_new();
	GdkDisplay* display = gdk_display_get_default();
	GdkScreen* screen = gdk_display_get_default_screen(display);
	gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
	if (std::ifstream(custom_css)) {
		gtk_css_provider_load_from_path(provider, custom_css, NULL);
		std::cout << "Using " << custom_css << std::endl;
	} else {
		gtk_css_provider_load_from_path(provider, "/usr/share/nwgdmenu/style.css", NULL);
		std::cout << "Using /usr/share/nwgdmenu/style.css\n";
	}
	g_object_unref(provider);
    
    MainWindow window;
    DMenu menu;
    main_menu = &menu;
    Anchor anchor;
    window.anchor = &anchor;
    
    window.signal_button_press_event().connect(sigc::ptr_fun(&on_window_clicked));
    
    // This prevents openbox window from receiving keyboard enents
    if (wm != "openbox") {
		window.set_skip_taskbar_hint(true);
	}

    /* Detect focused display geometry: {x, y, width, height} */
    std::vector<int> geometry = display_geometry(wm, window);
    std::cout << "Focused display: " << geometry[0] << ", " << geometry[1] << ", " << geometry[2] << ", " 
		<< geometry[3] << '\n';

	int x = geometry[0];
	int y = geometry[1];
	int w = geometry[2];
	int h = geometry[3];

	if (wm == "sway") {
		window.resize(w, h);
	} else {
		std::cout << "x: " << x << " y: " << y << std::endl;
		window.move(x, y); 	// needed in FVWM, otherwise grid always appears on screen 0
	}

	Gtk::MenuItem *search_item = new Gtk::MenuItem();
	search_item->add(menu.searchbox);
	menu.append(*search_item);

	menu.signal_deactivate().connect(sigc::ptr_fun(Gtk::Main::quit));

	int cnt = 0;
	for (Glib::ustring command : all_commands) {
		Gtk::MenuItem *item = new Gtk::MenuItem();
		item -> set_label(command);
		item -> signal_activate().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_clicked), command));
		menu.append(*item);
		cnt++;
		if (cnt > entries_limit - 1) {
			break;
		}
	}
	menu.set_reserve_toggle_size(false);
	
	Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
	outer_box.set_spacing(15);

	Gtk::VBox inner_vbox;

	Gtk::HBox inner_hbox;

	//~ window.anchor.set_label("ANCHOR");

	inner_hbox.pack_start(anchor, true, false);

	inner_vbox.pack_start(inner_hbox, true, false);

	outer_box.pack_start(inner_vbox, Gtk::PACK_EXPAND_WIDGET);
    
    window.add(outer_box);
	window.show_all_children();
	
	menu.show_all();
	//~ window.menu->popup_at_widget(&anchor, Gdk::GRAVITY_SOUTH, Gdk::GRAVITY_NORTH, nullptr);
	std::cout << menu.get_children().size() << std::endl;

	gettimeofday(&tp, NULL);
	long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	std::cout << "Time: " << end_ms - start_ms << std::endl;
	
    Gtk::Main::run(window);
    
    return 0;
}
