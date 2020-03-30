/*
 * GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include "bar.h"
#include "bar_tools.cpp"
#include "bar_classes.cc"
#include <sys/time.h>

int main(int argc, char *argv[]) {
    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    /* Try to lock /tmp/nwgbar.lock file. This will return -1 if the command is already running.
     * Thanks to chmike at https://stackoverflow.com/a/1643134 */
    // Create pid file if not yet exists
    if (!std::ifstream("/tmp/nwgbar.lock")) {
        save_string_to_file("nwgbar lock file", "/tmp/nwgbar.lock");
    }

    if (tryGetLock("/tmp/nwgbar.lock") == -1) {
        // kill if already running
        std::remove("/tmp/nwgbar.lock");
        std::string cmd = "pkill -f nwgbar";
        const char *command = cmd.c_str();
        std::system(command);
        std::exit(0);
    }

    std::string lang ("");

    InputParser input(argc, argv);
    if(input.cmdOptionExists("-h")){
        std::cout << "GTK button bar: nwgbar " << version << " (c) Piotr Miller 2020\n\n";
        std::cout << "nwgbar [-h] [-v] [-ha <l>|<r>] [-va <t>|<b>] [-t <name>] [-o <opacity>] [-s <size>]\n\n";
        std::cout << "Options:\n";
        std::cout << "-h            show this help message and exit\n";
        std::cout << "-v            arrange buttons vertically\n";
        std::cout << "-ha <l>|<r>   horizontal alignment left/right (default: center)\n";
        std::cout << "-va <t>|<b>   vertical alignment top/bottom (default: middle)\n";
        std::cout << "-t <name>     template file name (default: bar.json)\n";
        std::cout << "-c <name>     css file name (default: style.css)\n";
        std::cout << "-o <opacity>  background opacity (0.0 - 1.0, default 0.9)\n";
        std::cout << "-s <size>     button image size (default: 72)\n";
        std::cout << "-wm <wmname>  window manager name (if can not be detected)\n";
        std::exit(0);
    }

    if(input.cmdOptionExists("-v")){
        orientation = "v";
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

    const std::string &tname = input.getCmdOption("-t");
    if (!tname.empty()){
        definition_file = tname;
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

    // default and custom style sheet
    std::string default_css_file = config_dir + "/style.css";
    // css file to be used
    std::string css_file = config_dir + "/" + custom_css_file;
    std::cout << css_file << std::endl;
    // copy default file if not found
    const char *custom_css = css_file.c_str();
    if (!fs::exists(default_css_file)) {
        fs::path source_file = "/usr/share/nwgbar/style.css";
        fs::path target = default_css_file;
        try {
            fs::copy_file("/usr/share/nwgbar/style.css", target, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cout << "Failed copying default style.css\n";
        }
    }

    // default or custom template
    std::string bar_file = config_dir + "/" + definition_file;
    // copy default anyway if not found
    const char *custom_bar = bar_file.c_str();
    if (!fs::exists(config_dir + "/bar.json")) {
        fs::path source_file = "/usr/share/nwgbar/bar.json";
        fs::path target = bar_file;
        try {
            fs::copy_file("/usr/share/nwgbar/bar.json", target, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cout << "Failed copying default template\n";
        }
    }

    ns::json bar_json {};
    try {
        bar_json = get_bar_json(custom_bar);
    }  catch (...) {
        std::cout << "\nERROR: Template file not found, using default\n";
        bar_json = get_bar_json("/usr/share/nwgbar/bar.json");
    }
    std::cout << bar_json.size() << " bar entries loaded\n";

    std::vector<BarEntry> bar_entries {};
    if (bar_json.size() > 0) {
        bar_entries = get_bar_entries(bar_json);
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }
    
    std::cout << "WM: " << wm << "\n";

    /* turn off borders, enable floating on sway */
    if (wm == "sway") {
        std::string cmd = "swaymsg for_window [title=\"~nwgbar*\"] floating enable";
        const char *command = cmd.c_str();
        std::system(command);
        cmd = "swaymsg for_window [title=\"~nwgbar*\"] border none";
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
        gtk_css_provider_load_from_path(provider, "/usr/share/nwgbar/style.css", NULL);
        std::cout << "Using /usr/share/nwgbar/style.css\n";
    }
    g_object_unref(provider);

    MainWindow window;
    window.show();

    window.signal_button_press_event().connect(sigc::ptr_fun(&on_window_clicked));

    /* Detect focused display geometry: {x, y, width, height} */
    std::vector<int> geometry = display_geometry(wm, window);
    std::cout << "Focused display: " << geometry[0] << ", " << geometry[1] << ", " << geometry[2] << ", "
    << geometry[3] << '\n';

    int x = geometry[0];
    int y = geometry[1];
    int w = geometry[2];
    int h = geometry[3];

    if (wm == "sway" || wm == "i3" || wm == "openbox") {
        window.resize(w, h);
        window.move(x, y);
    }

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    /* Create buttons */
    if (bar_entries.size() > 0) {
        for (BarEntry entry : bar_entries) {
            Gtk::Image* image = app_image(entry.icon);
            AppBox *ab = new AppBox(entry.name, entry.exec, entry.icon);
            ab -> set_image_position(Gtk::POS_TOP);
            ab -> set_image(*image);
            ab -> signal_clicked().connect(sigc::bind<std::string>(sigc::ptr_fun(&on_button_clicked), entry.exec));

            window.boxes.push_back(ab);
        }
    }

    int column = 0;
    int row = 0;

    if (bar_entries.size() > 0) {
        for (AppBox *box : window.boxes) {
            window.favs_grid.attach(*box, column, row, 1, 1);
            if (orientation == "v") {
                row++;
            } else {
                column++;
            }
        }
    }

    Gtk::VBox inner_vbox;

    Gtk::HBox favs_hbox;
    favs_hbox.set_name("bar");
    if (h_align == "l") {
        favs_hbox.pack_start(window.favs_grid, false, false);
    } else if (h_align == "r") {
        favs_hbox.pack_end(window.favs_grid, false, false);
    } else {
        favs_hbox.pack_start(window.favs_grid, true, false);
    }

    inner_vbox.pack_start(favs_hbox, true, false);

    if (v_align == "t") {
        outer_box.pack_start(inner_vbox, false, false);
    } else if (v_align == "b") {
        outer_box.pack_end(inner_vbox, false, false);
    } else {
        outer_box.pack_start(inner_vbox, Gtk::PACK_EXPAND_PADDING);
    }

    window.add(outer_box);
    window.show_all_children();

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    std::cout << "Time: " << end_ms - start_ms << std::endl;

    Gtk::Main::run(window);

    return 0;
}
