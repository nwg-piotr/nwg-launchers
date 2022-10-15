/*
 * GTK-based button bar
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>

#include "nwg_classes.h"
#include "nwg_tools.h"
#include "bar.h"
#include "log.h"
#include "time_report.h"

#include <bar/help.h>

int main(int argc, char *argv[]) {
    try {
        ntime::Time start_time{ "start" };

        InputParser input(argc, argv);
        if(input.cmdOptionExists("-h")){
            std::cout << bar::HELP_MESSAGE;
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
	auto settings = Gtk::Settings::get_for_screen(screen);
        if (!provider || !display || !settings || !screen) {
            Log::error("Failed to initialize GTK");
            return EXIT_FAILURE;
        }

        BarConfig config {
            input,
            screen
        };

	settings->property_gtk_theme_name() = config.theme;

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
            auto image = Gtk::make_managed<Gtk::Image>(icon_provider.load_icon(entry.icon));
            auto& ab = window.boxes.emplace_back(std::move(entry.name),
                                                 std::move(entry.exec),
                                                 std::move(entry.icon));
            ab.set_image_position(Gtk::POS_TOP);
            ab.set_image(*image);
            if (!entry.css_class.empty()) {
                auto && style_context = ab.get_style_context();
                style_context->add_class(entry.css_class);
            }
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

        ntime::Time end_time{ "end", start_time };
        ntime::report(start_time);

        return app->run(window);
    } catch (const Glib::Error& e) {
        Log::error(e.what());
    } catch (const std::exception& error) {
        Log::error(error.what());
    }
    return EXIT_FAILURE;
}
