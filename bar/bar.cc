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
-wm <wmname>     window manager name (if can not be detected)\n\n\
[requires layer-shell]:\n\
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},        default: OVERLAY\n\
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto\n";

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

    auto tname = input.getCmdOption("-t");
    if (!tname.empty()){
        definition_file = tname;
    }

    auto css_name = input.getCmdOption("-c");
    if (!css_name.empty()){
        custom_css_file = css_name;
    }

    auto background_color = input.get_background_color(0.9);

    int image_size = 72;
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
                Log::error("Size must be in range 16 - 256\n");
            }
        } else {
            Log::error("Image size should be valid integer in range 16 - 256\n");
        }
    }

    auto config_dir = get_config_dir("nwgbar");
    if (!fs::is_directory(config_dir)) {
        Log::info("Config dir not found, creating...");
        fs::create_directories(config_dir);
    }

    // default or custom template
    auto default_bar_file = config_dir / "bar.json";
    auto custom_bar_file = config_dir / definition_file;
    // copy default anyway if not found
    if (!fs::exists(default_bar_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwgbar/bar.json", default_bar_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            Log::error("Failed copying default template");
        }
    }

    ns::json bar_json;
    try {
        bar_json = json_from_file(custom_bar_file);
    }  catch (...) {
        Log::error("Template file not found, using default");
        bar_json = json_from_file(default_bar_file);
    }
    Log::info(bar_json.size(), " bar entries loaded");

    std::vector<BarEntry> bar_entries {};
    if (bar_json.size() > 0) {
        bar_entries = get_bar_entries(std::move(bar_json));
    }

    auto app = Gtk::Application::create();

    auto provider = Gtk::CssProvider::create();
    auto display = Gdk::Display::get_default();
    auto screen = display->get_default_screen();
    if (!provider || !display || !screen) {
        Log::error("Failed to initialize GTK");
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    {
        auto css_file = setup_css_file("nwgbar", config_dir, custom_css_file);
        provider->load_from_path(css_file);
        Log::info("Using css file \'", css_file, "\'");
    }
    auto icon_theme = Gtk::IconTheme::get_for_screen(screen);
    if (!icon_theme) {
        Log::error("Failed to load icon theme");
        return EXIT_FAILURE;
    }
    auto& icon_theme_ref = *icon_theme.get();
    auto icon_missing = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg");

    Config config {
        input,
        "~nwgbar",
        "~nwgbar",
        screen
    };
    BarWindow window{ config };
    window.set_background_color(background_color);

    Gtk::Box outer_box(Gtk::ORIENTATION_VERTICAL);
    outer_box.set_spacing(15);

    /* Create buttons */
    for (auto& entry : bar_entries) {
        Gtk::Image* image = app_image(icon_theme_ref, entry.icon, icon_missing, image_size);
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
    switch (config.halign) {
        case HAlign::Left:  favs_hbox.pack_start(window.favs_grid, false, false); break;
        case HAlign::Right: favs_hbox.pack_end(window.favs_grid, false, false); break;
        default: favs_hbox.pack_start(window.favs_grid, true, false);
    }
    inner_vbox.pack_start(favs_hbox, true, false);
    switch (config.valign) {
        case VAlign::Top:    outer_box.pack_start(inner_vbox, false, false); break;
        case VAlign::Bottom: outer_box.pack_end(inner_vbox, false, false); break;
        default: outer_box.pack_start(inner_vbox, Gtk::PACK_EXPAND_PADDING);
    }

    window.add(outer_box);
    window.show_all_children();
    window.show(hint::Fullscreen);

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    Log::info("Time: ", end_ms - start_ms, "ms");

    return app->run(window);
}
