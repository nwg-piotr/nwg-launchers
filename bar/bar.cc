/*
 * GTK-based button bar
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>

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
    try {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        InputParser input(argc, argv);
        if(input.cmdOptionExists("-h")){
            std::cout << HELP_MESSAGE;
            std::exit(0);
        }

        auto background_color = input.get_background_color(0.9);

        auto config_dir = get_config_dir("nwgbar");
        if (!fs::is_directory(config_dir)) {
            Log::info("Config dir not found, creating...");
            fs::create_directories(config_dir);
        }

        auto app = Gtk::Application::create();

        auto provider = Gtk::CssProvider::create();
        auto display = Gdk::Display::get_default();
        auto screen = display->get_default_screen();
        if (!provider || !display || !screen) {
            Log::error("Failed to initialize GTK");
            return EXIT_FAILURE;
        }

        BarConfig config {
            input,
            screen
        };

        // default or custom template
        auto default_bar_file = config_dir / "bar.json";
        auto custom_bar_file = config_dir / config.definition_file;
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

        Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
        {
            auto css_file = setup_css_file("nwgbar", config_dir, config.css_filename);
            provider->load_from_path(css_file);
            Log::info("Using css file \'", css_file, "\'");
        }
        IconProvider icon_provider {
            Gtk::IconTheme::get_for_screen(screen),
            config.icon_size
        };

        BarWindow window{ config };
        window.set_background_color(background_color);

        /* Create buttons */
        for (auto& entry : bar_entries) {
            auto image = icon_provider.load_icon(entry.icon);
            auto& ab = window.boxes.emplace_back(std::move(entry.name),
                                                 std::move(entry.exec),
                                                 std::move(entry.icon));
            ab.set_image_position(Gtk::POS_TOP);
            ab.set_image(image);
        }

        int column = 0;
        int row = 0;

        window.grid.freeze_child_notify();
        for (auto& box : window.boxes) {
            window.grid.attach(box, column, row, 1, 1);
            if (config.orientation == Orientation::Vertical) {
                row++;
            } else {
                column++;
            }
        }
        window.grid.thaw_child_notify();
        Instance instance{ *app.get(), "nwgbar" };

        window.show_all_children();
        window.show(hint::Fullscreen);

        gettimeofday(&tp, NULL);
        long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        Log::info("Time: ", end_ms - start_ms, "ms");

        return app->run(window);
    } catch (const Glib::FileError& error) {
        Log::error(error.what());
    } catch (const std::runtime_error& error) {
        Log::error(error.what());
    }
    return EXIT_FAILURE;
}
