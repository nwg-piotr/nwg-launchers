/*
 * GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>

#include <charconv>

#include "nwg_classes.h"
#include "nwg_tools.h"
#include "bar.h"

std::string wm {""};            // detected or forced window manager name
const char* const HELP_MESSAGE =
"GTK button bar: nwgbar " VERSION_STR " (c) Piotr Miller & Contributors 2020\n\n\
Options:\n\
-h               show this help message and exit\n\
-v               arrange buttons vertically\n\
-ha <l>|<r>      horizontal alignment left/right (default: center)\n\
-va <t>|<b>      vertical alignment top/bottom (default: middle)\n\
-t <name>        template file name (default: bar.json)\n\
-c <name>        css file name (default: style.css)\n\
-o <opacity>     background opacity (0.0 - 1.0, default 0.9)\n\
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)\n\
-s <size>        button image size (default: 72)\n\
-wm <wmname>     window manager name (if can not be detected)\n";

int main(int argc, char *argv[]) {
    std::string definition_file {"bar.json"};
    std::string custom_css_file {"style.css"};
    std::string orientation {"h"};
    std::string h_align {""};
    std::string v_align {""};

    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    create_pid_file_or_kill_pid("nwgbar");

    InputParser input(argc, argv);
    if(input.cmdOptionExists("-h")){
        std::cout << HELP_MESSAGE;
        std::exit(0);
    }

    if (input.cmdOptionExists("-v")){
        orientation = "v";
    }

    auto halign = input.getCmdOption("-ha");
    if (!halign.empty()){
        if (halign == "l" || halign == "left") {
            h_align = "l";
        } else if (halign == "r" || halign == "right") {
            h_align = "r";
        }
    }

    auto valign = input.getCmdOption("-va");
    if (!valign.empty()){
        if (valign == "t" || valign == "top") {
            v_align = "t";
        } else if (valign == "b" || valign == "bottom") {
            v_align = "b";
        }
    }

    auto tname = input.getCmdOption("-t");
    if (!tname.empty()){
        definition_file = tname;
    }

    auto css_name = input.getCmdOption("-c");
    if (!css_name.empty()){
        custom_css_file = css_name;
    }

    auto wm_name = input.getCmdOption("-wm");
    if (!wm_name.empty()){
        wm = wm_name;
    }

    auto background_color = input.get_background_color(0.9);

    auto i_size = input.getCmdOption("-s");
    if (!i_size.empty()) {
        int i_s;
        auto from = i_size.data();
        auto to = from + i_size.size();
        auto [p, ec] = std::from_chars(from, to, i_s);
        if (ec == std::errc()) {
            if (i_s >= 16 && i_s <= 256) {
                image_size = i_s;
            } else {
                std::cerr << "\nERROR: Size must be in range 16 - 256\n\n";
            }
        } else {
            std::cerr << "\nERROR: Image size should be valid integer in range 16 - 256\n\n";
        }
    }

    auto config_dir = get_config_dir("nwgbar");
    if (!fs::is_directory(config_dir)) {
        std::cout << "Config dir not found, creating...\n";
        fs::create_directories(config_dir);
    }

    // default and custom style sheet
    auto default_css_file = config_dir / "style.css";
    // css file to be used
    auto css_file = config_dir / custom_css_file;
    // copy default file if not found
    if (!fs::exists(default_css_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwgbar/style.css", default_css_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "Failed copying default style.css\n";
        }
    }

    // default or custom template
    auto default_bar_file = config_dir / "bar.json";
    auto custom_bar_file = config_dir / definition_file;
    // copy default anyway if not found
    if (!fs::exists(default_bar_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwgbar/bar.json", default_bar_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "Failed copying default template\n";
        }
    }

    ns::json bar_json;
    try {
        bar_json = json_from_file(custom_bar_file);
    }  catch (...) {
        std::cerr << "ERROR: Template file not found, using default\n";
        bar_json = json_from_file(default_bar_file);
    }
    std::cout << bar_json.size() << " bar entries loaded\n";

    std::vector<BarEntry> bar_entries {};
    if (bar_json.size() > 0) {
        bar_entries = get_bar_entries(std::move(bar_json));
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }

    std::cout << "WM: " << wm << "\n";

    auto app = Gtk::Application::create();

    auto provider = Gtk::CssProvider::create();
    auto display = Gdk::Display::get_default();
    auto screen = display->get_default_screen();
    if (!provider || !display || !screen) {
        std::cerr << "ERROR: Failed to initialize GTK\n";
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    auto icon_theme = Gtk::IconTheme::get_for_screen(screen);
    if (!icon_theme) {
        std::cerr << "ERROR: Failed to load icon theme\n";
        return EXIT_FAILURE;
    }
    auto& icon_theme_ref = *icon_theme.get();
    auto icon_missing = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg");

    if (std::filesystem::is_regular_file(css_file)) {
        provider->load_from_path(css_file);
        std::cout << "Using " << css_file << '\n';
    } else {
        provider->load_from_path(default_css_file);
        std::cout << "Using " << default_css_file << '\n';
    }

    MainWindow window;
    window.set_background_color(background_color);

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    /* Create buttons */
    for (auto& entry : bar_entries) {
        Gtk::Image* image = app_image(icon_theme_ref, entry.icon, icon_missing);
        auto& ab = window.boxes.emplace_back(std::move(entry.name),
                                             std::move(entry.exec),
                                             std::move(entry.icon));
        ab.set_image_position(Gtk::POS_TOP);
        ab.set_image(*image);
    }

    int column = 0;
    int row = 0;

    window.favs_grid.freeze_child_notify();
    for (auto& box : window.boxes) {
        window.favs_grid.attach(box, column, row, 1, 1);
        if (orientation == "v") {
            row++;
        } else {
            column++;
        }
    }
    window.favs_grid.thaw_child_notify();

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
    window.show(hint::Fullscreen);

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    std::cout << "Time: " << end_ms - start_ms << "ms\n";

    return app->run(window);
}
