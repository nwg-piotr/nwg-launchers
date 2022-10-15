/*
 * GTK-based dmenu
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <iostream>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "dmenu.h"
#include "log.h"

#include <dmenu/help.h>

int main(int argc, char *argv[]) {
    try {

        InputParser input(argc, argv);
        if (input.cmdOptionExists("-h")){
            std::cout << dmenu::HELP_MESSAGE;
            std::exit(0);
        }

        auto background_color = input.get_background_color(0.3);

        auto config_dir = get_config_dir("nwgdmenu");
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
        DmenuConfig config {
            input,
            screen
        };

	settings->property_gtk_theme_name() = config.theme;

        Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
        {
            auto css_file = setup_css_file("nwgdmenu", config_dir, config.css_filename);
            Log::info("Using css file \'", css_file, "\'");
            provider->load_from_path(css_file);
        }

        auto all_commands = get_commands_list(config);
        DmenuWindow window{ config, all_commands };
        window.set_background_color(background_color);
        window.show_all_children();
        switch (2 * (config.valign == VAlign::NotSpecified) + (config.halign == HAlign::NotSpecified )) {
            case 0:
                window.show(hint::Sides{ { config.halign == HAlign::Right, 50 }, { config.valign == VAlign::Bottom, 50 } }); break;
            case 1:
                window.show(hint::Side<hint::Vertical>{ config.valign == VAlign::Bottom, 50 }); break;
            case 2:
                window.show(hint::Side<hint::Horizontal>{ config.halign == HAlign::Right, 50 }); break;
            case 3:
                window.show(hint::Center); break;
        }
        return app->run(window);
    } catch (const Glib::FileError& error) {
        Log::error(error.what());
    } catch (const std::runtime_error& error) {
        Log::error(error.what());
    }
    return EXIT_FAILURE;
}
