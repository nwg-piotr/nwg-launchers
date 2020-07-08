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
#include <unistd.h>

int main(int argc, char *argv[]) {

	pid_t pid = getpid();
    std::string mypid = std::to_string(pid);
    
    std::string pid_file = "/var/run/user/" + std::to_string(getuid()) + "/nwgdmenu.pid";
    
    int saved_pid {};
    if (std::ifstream(pid_file)) {
        try {
			saved_pid = std::stoi(read_file_to_string(pid_file));
			if (kill(saved_pid, 0) != -1) {  // found running instance!
				kill(saved_pid, 9);
				save_string_to_file(mypid, pid_file);
				std::exit(0);
			}
		} catch (...) {
            std::cout << "\nError reading pid file\n\n";
        }
    }
    save_string_to_file(mypid, pid_file);

    InputParser input(argc, argv);
    if(input.cmdOptionExists("-h")){
        std::cout << "GTK dynamic menu: nwgdmenu " VERSION_STR " (c) Piotr Miller & Contributors 2020\n\n";
        std::cout << "nwgdmenu - displays newline-separated stdin input as a GTK menu\n";
        std::cout << "nwgdmenu_run - creates a GTK menu out of commands found in $PATH\n\n";

        std::cout << "Options:\n";
        std::cout << "-h            show this help message and exit\n";
        std::cout << "-n            no search box\n";
        std::cout << "-ha <l>|<r>   horizontal alignment left/right (default: center)\n";
        std::cout << "-va <t>|<b>   vertical alignment top/bottom (default: middle)\n";
        std::cout << "-r <rows>     number of rows (default: " << rows <<")\n";
        std::cout << "-c <name>     css file name (default: style.css)\n";
        std::cout << "-o <opacity>  background opacity (0.0 - 1.0, default 0.3)\n";
        std::cout << "-wm <wmname>  window manager name (if can not be detected)\n";
        std::exit(0);
    }

    // We will build dmenu out of commands found in $PATH if nothing has been passed by stdin
    dmenu_run = isatty(fileno(stdin)) == 1;
    
    // Otherwise let's build from stdin input
    if (!dmenu_run) {
		all_commands = {};
		for (std::string line; std::getline(std::cin, line);) {
			Glib::ustring cmd = line;
			all_commands.push_back(cmd);
		}
	}

    if (input.cmdOptionExists("-n")){
        show_searchbox = false;
    }

    const std::string &halign = input.getCmdOption("-ha");
    if (!halign.empty()){
        if (halign == "l" || halign == "left") {
            h_align = "l";
        } else if (halign == "r" || halign == "right") {
            h_align = "r";
        }
    }

    const std::string &valign = input.getCmdOption("-va");
    if (!valign.empty()){
        if (valign == "t" || valign == "top") {
            v_align = "t";
        } else if (valign == "b" || valign == "bottom") {
            v_align = "b";
        }
    }

    const std::string &css_name = input.getCmdOption("-c");
    if (!css_name.empty()){
        custom_css_file = css_name;
    }

    const std::string &wm_name = input.getCmdOption("-wm");
    if (!wm_name.empty()){
        wm = wm_name;
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

    const std::string &rw = input.getCmdOption("-r");
    if (!rw.empty()){
        try {
            int r = std::stoi(rw);
            if (r > 0 && r <= 100) {
                rows = r;
            } else {
                std::cout << "\nERROR: Number of rows must be in range 1 - 100\n\n";
            }
        } catch (...) {
            std::cout << "\nERROR: Invalid rows number\n\n";
        }
    }

    std::string config_dir = get_config_dir();
    if (!fs::is_directory(config_dir)) {
        std::cout << "Config dir not found, creating...\n";
        fs::create_directories(config_dir);
    }

    // default and custom style sheet
    std::string default_css_file = config_dir + "/style.css";
    // css file to be used
    std::string css_file = config_dir + "/" + custom_css_file;
    // copy default file if not found
    if (!fs::exists(default_css_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwgdmenu/style.css", default_css_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cout << "Failed copying default style.css\n";
        }
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }

    if (dmenu_run) {
        /* get all applications dirs */
        std::vector<std::string> command_dirs = get_command_dirs();

        /* get a list of paths to all commands */
        std::vector<std::string> commands = list_commands(command_dirs);
        std::cout << commands.size() << " commands found\n";

        /* Create a vector of commands (w/o path) */
        all_commands = {};
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
    }

    /* turn off borders, enable floating on sway */
    if (wm == "sway") {
        std::string cmd = "swaymsg -q for_window [title=\"~nwgdmenu*\"] floating enable";
        const char *command = cmd.c_str();
        std::system(command);
        cmd = "swaymsg -q for_window [title=\"~nwgdmenu*\"] border none";
        command = cmd.c_str();
        std::system(command);
    }

    Gtk::Main kit(argc, argv);

    GtkCssProvider* provider = gtk_css_provider_new();
    GdkDisplay* display = gdk_display_get_default();
    GdkScreen* screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    if (std::ifstream(custom_css_file)) {
        gtk_css_provider_load_from_path(provider, custom_css_file.c_str(), NULL);
    } else {
        gtk_css_provider_load_from_path(provider, default_css_file.c_str(), NULL);
    }
    g_object_unref(provider);

    MainWindow window;
    // For openbox and similar we'll need the window x, y coordinates
    window.show();

    DMenu menu;
    main_menu = &menu;
    Anchor anchor;
    window.anchor = &anchor;

    window.signal_button_press_event().connect(sigc::ptr_fun(&on_window_clicked));

    /* Detect focused display geometry: {x, y, width, height} */
    std::vector<int> geometry = display_geometry(wm, window);

    int x = geometry[0];
    int y = geometry[1];
    int w = geometry[2];
    int h = geometry[3];

    if (wm == "sway" || wm == "i3") {
        window.resize(w, h);
        window.move(x, y);
        window.hide();
    } else {
        window.hide();
        int x_org;
        int y_org;
        window.resize(1, 1);
        if (!h_align.empty() || !v_align.empty()) {
            window.move(x, y);
            window.get_position(x_org, y_org);
        }
        // We assume that the window has been opened at mouse pointer coordinates
        window.get_position(x_org, y_org);

        if (h_align == "l") {
            window.move(x, y_org);
            window.get_position(x_org, y_org);
        }
        if (h_align == "r") {
            window.move(x + w - 50, y_org);
            window.get_position(x_org, y_org);
        }
        if (v_align == "t") {
            window.move(x_org, y);
            window.get_position(x_org, y_org);
        }
        if (v_align == "b") {
            window.move(x_org, y + h);
        }
        //~ window.hide();
    }

    if (show_searchbox) {
        Gtk::MenuItem *search_item = new Gtk::MenuItem();
        search_item -> add(menu.searchbox);
        search_item -> set_name("search_item");
        search_item -> set_sensitive(false);
        menu.append(*search_item);
    }

    menu.signal_deactivate().connect(sigc::ptr_fun(Gtk::Main::quit));

    int cnt = 0;
    for (Glib::ustring command : all_commands) {
        Gtk::MenuItem *item = new Gtk::MenuItem();
        item -> set_label(command);
        item -> signal_activate().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_item_clicked), command));
        menu.append(*item);
        cnt++;
        if (cnt > rows - 1) {
            break;
        }
    }
    
    menu.set_reserve_toggle_size(false);
    menu.set_property("width_request", (int)(w / 8));

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    Gtk::VBox inner_vbox;

    Gtk::HBox inner_hbox;

    if (h_align == "l") {
        inner_hbox.pack_start(anchor, false, false);
    } else if (h_align == "r") {
        inner_hbox.pack_end(anchor, false, false);
    } else {
        inner_hbox.pack_start(anchor, true, false);
    }

    if (v_align == "t") {
        inner_vbox.pack_start(inner_hbox, false, false);
    } else if (v_align == "b") {
        inner_vbox.pack_end(inner_hbox, false, false);
    } else {
        inner_vbox.pack_start(inner_hbox, true, false);
    }
    outer_box.pack_start(inner_vbox, Gtk::PACK_EXPAND_WIDGET);

    window.add(outer_box);
    window.show_all_children();

    menu.show_all();

    Gtk::Main::run(window);

    return 0;
}
