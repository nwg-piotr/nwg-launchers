/*
 * GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <charconv>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "dmenu.h"

#define ROWS_DEFAULT 20

#define STR_EXPAND(x) #x
#define STR(x) STR_EXPAND(x)

std::string wm {""};                        // detected or forced window manager name
std::filesystem::path settings_file {""};

int rows = ROWS_DEFAULT;                    // number of menu items to display

bool dmenu_run = false;
bool show_searchbox = true;
bool case_sensitive = true;

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
Hotkeys:\n\
Delete        clear search box\n\
Insert        switch case sensitivity\n";

int main(int argc, char *argv[]) {
    std::string custom_css_file {"style.css"};

    // For now the settings file only determines if case_sensitive was turned on.
    settings_file = get_settings_path();
    if (std::ifstream settings{ settings_file }) {
        std::string sensitivity;
        settings >> sensitivity;
        case_sensitive = sensitivity == "case_sensitive";
    }

    create_pid_file_or_kill_pid("nwgdmenu");

    InputParser input(argc, argv);
    if (input.cmdOptionExists("-h")){
        std::cout << HELP_MESSAGE;
        std::exit(0);
    }

    // We will build dmenu out of commands found in $PATH if nothing has been passed by stdin
    dmenu_run = isatty(STDIN_FILENO) == 1;

    if (input.cmdOptionExists("-run")){
        dmenu_run = true;
    }

    if (input.cmdOptionExists("-n")){
        show_searchbox = false;
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

    if (auto wm_name = input.getCmdOption("-wm"); !wm_name.empty()) {
        wm = wm_name;
    }

    auto background_color = input.get_background_color(0.3);

    if (auto rw = input.getCmdOption("-r"); !rw.empty()){
        int r;
        auto from = rw.data();
        auto to = from + rw.size();
        auto [p, ec] = std::from_chars(from, to, r);
        if (ec == std::errc()) {
            if (r > 0 && r <= 100) {
                rows = r;
            } else {
                std::cerr << "\nERROR: Number of rows must be in range 1 - 100\n\n";
            }
        } else {
            std::cerr << "\nERROR: Invalid rows number\n\n";
        }
    }

    auto config_dir = get_config_dir("nwgdmenu");
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
            fs::copy_file(DATA_DIR_STR "/nwgdmenu/style.css", default_css_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "Failed copying default style.css\n";
        }
    }

    /* get current WM name if not forced */
    if (wm.empty()) {
        wm = detect_wm();
    }

    std::vector<Glib::ustring> all_commands;
    if (dmenu_run) {
        /* get a list of paths to all commands from all application dirs */
        all_commands = list_commands();
        std::cout << all_commands.size() << " commands found\n";

        /* Sort case insensitive */
        std::sort(all_commands.begin(), all_commands.end(), [](auto& a, auto& b) {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](auto a, auto b) {
                return std::tolower(a) < std::tolower(b);
            });
        });
    } else {
        for (std::string line; std::getline(std::cin, line);) {
            all_commands.emplace_back(std::move(line));
        }
    }

    auto app = Gtk::Application::create();
    
    auto provider = Gtk::CssProvider::create();
    auto display = Gdk::Display::get_default();
    auto screen = display->get_default_screen();
    if (!provider || !display || !screen) {
        std::cerr << "ERROR: Failed to initialize GTK\n";
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);

    if (std::filesystem::is_regular_file(css_file)) {
        provider->load_from_path(css_file);
        std::cout << "Using " << css_file << '\n';
    } else {
        provider->load_from_path(default_css_file);
        std::cout << "Using " << default_css_file << '\n';
    }

    MainWindow window{ wm, all_commands };
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
            window.show(hint::Auto); break;
    }
    return app->run(window);
}
