/*
 * GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <iostream>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "dmenu.h"

#define STR_EXPAND(x) #x
#define STR(x) STR_EXPAND(x)

const char* const HELP_MESSAGE =
"GTK dynamic menu: nwgdmenu " VERSION_STR " (c) Piotr Miller & Contributors 2020\n\n\
<input> | nwgdmenu - displays newline-separated stdin input as a GTK menu\n\
nwgdmenu - creates a GTK menu out of commands found in $PATH\n\n\
Options:\n\
-h               show this help message and exit\n\
-n               no search box\n\
-ha <l>|<r>      horizontal alignment left/right (default: center)\n\
-va <t>|<b>      vertical alignment top/bottom (default: middle)\n\
-r <rows>        number of rows (default: " STR(ROWS_DEFAULT) ")\n\
-c <name>        css file name (default: style.css)\n\
-o <opacity>     background opacity (0.0 - 1.0, default 0.3)\n\
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)\n\
-wm <wmname>     window manager name (if can not be detected)\n\
-run             ignore stdin, always build from commands in $PATH\n\n\
[requires layer-shell]:\n\
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},        default: OVERLAY\n\
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto\n\n\
Hotkeys:\n\
Delete        clear search box\n\
Insert        switch case sensitivity\n";

int main(int argc, char *argv[]) {
    std::filesystem::path custom_css_file {"style.css"};

    create_pid_file_or_kill_pid("nwgdmenu");

    InputParser input(argc, argv);
    if (input.cmdOptionExists("-h")){
        std::cout << HELP_MESSAGE;
        std::exit(0);
    }

    auto halign = input.getCmdOption("-ha");
    if (halign == "l" || halign == "left") {
        halign = "l";
    } else if (halign == "r" || halign == "right") {
        halign = "r";
    }

    auto valign = input.getCmdOption("-va");
    if (valign == "t" || valign == "top") {
        valign = "t";
    } else if (valign == "b" || valign == "bottom") {
        valign = "b";
    }

    if (auto css_name = input.getCmdOption("-c"); !css_name.empty()) {
        custom_css_file = css_name;
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
    if (!provider || !display || !screen) {
        Log::error("Failed to initialize GTK");
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    {
        auto css_file = setup_css_file("nwgdmenu", config_dir, custom_css_file);
        Log::info("Using css file \'", css_file, "\'");
        provider->load_from_path(css_file);
    }

    Config config {
        input,
        "~nwgdmenu",
        "~nwgdmenu",
        screen
    };
    DmenuConfig dmenu_config { config };

    auto all_commands = get_commands_list(dmenu_config);
    MainWindow window{ dmenu_config, all_commands };
    window.set_background_color(background_color);
    window.show_all_children();
    switch (halign.empty() + valign.empty() * 2) {
        case 0:
            window.show(hint::Sides{ { halign == "r", 50 }, { valign == "b", 50 } }); break;
        case 1:
            window.show(hint::Side<hint::Vertical>{ valign == "b", 50 }); break;
        case 2:
            window.show(hint::Side<hint::Horizontal>{ halign == "r", 50 }); break;
        case 3:
            window.show(hint::Center); break;
    }
    return app->run(window);
}
