/*
 * GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>
#include <iostream>
#include <fstream>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "grid.h"
#include "grid_entries.h"
#include "time_report.h"

const char* const HELP_MESSAGE =
"GTK application grid: nwggrid " VERSION_STR " (c) 2021 Piotr Miller, Sergey Smirnykh & Contributors \n\n\
\
Options:\n\
-h               show this help message and exit\n\
-f               display favourites (most used entries); does not work with -d\n\
-p               display pinned entries; does not work with -d \n\
-d               look for .desktop files in custom paths (-d '/my/path1:/my/another path:/third/path') \n\
-o <opacity>     default (black) background opacity (0.0 - 1.0, default 0.9)\n\
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)\n\
-n <col>         number of grid columns (default: 6)\n\
-s <size>        button image size (default: 72)\n\
-c <name>        css file name (default: style.css)\n\
-l <ln>          force use of <ln> language\n\
-g <theme>       GTK theme name\n\
-wm <wmname>     window manager name (if can not be detected)\n\
-oneshot         run in the foreground, exit when window is closed\n\
                 generally you should not use this option, use simply `nwggrid` instead\n\
[requires layer-shell]:\n\
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},         default: OVERLAY\n\
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto\n";

/* Base class for application drivers, simply calls Application::run */
struct ApplicationDriver {
    Glib::RefPtr<Gtk::Application> app;

    ApplicationDriver(const Glib::RefPtr<Gtk::Application>& app): app{ app } {
        // intentionally left blank
    }
    virtual ~ApplicationDriver() = default;
    virtual int run() { return app->run(); }
};

/* Keeps the application alive when the window is closed, registers & deregisters */
struct ServerDriver: public ApplicationDriver {
    GridInstance instance;

    ServerDriver(const Glib::RefPtr<Gtk::Application>& app, GridWindow& window):
        ApplicationDriver{ app },
        instance{ *app.get(), window, "nwggrid-server" }
    {
        app->hold();
    }
};

/* Does not register application instance, exits once the window is closed */
struct OneshotDriver: public ApplicationDriver {
    GridWindow&  window;
    GridInstance instance;

    OneshotDriver(const Glib::RefPtr<Gtk::Application>& app, GridWindow& window):
        ApplicationDriver{ app },
        window{ window },
        instance{ *app.get(), window, "nwggrid" }
    {
        app->hold();
    }
    int run() override {
        window.show(hint::Fullscreen);
        window.signal_hide().connect([this](){
            this->app->release();
        });
        return ApplicationDriver::run();
    }
};

int main(int argc, char *argv[]) {
    try {
        ntime::Time start{ "start" };

        InputParser input{ argc, argv };
        if (input.cmdOptionExists("-h")){
            std::cout << HELP_MESSAGE;
            std::exit(0);
        }

        auto config_dir = get_config_dir("nwggrid");
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

        GridConfig config {
            input,
            screen,
            config_dir
        };
        Log::info("Locale: ", config.lang);

	settings->property_gtk_theme_name() = config.theme;

        Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
        {
            auto css_file = setup_css_file("nwggrid", config_dir, config.css_filename);
            provider->load_from_path(css_file);
            Log::info("Using css file \'", css_file, "\'");
        }
        IconProvider icon_provider {
            Gtk::IconTheme::get_for_screen(screen),
            config.icon_size
        };

        // This will be read-only, to find n most clicked items (n = number of grid columns)
        std::vector<CacheEntry> favourites;
        if (config.favs) {
            try {
                auto cache = json_from_file(config.cached_file);
                if (cache.size() > 0) {
                    Log::info(cache.size(), " cache entries loaded");
                } else {
                    Log::info("No cache entries loaded");
                }
                auto n = std::min(config.num_col, cache.size());
                favourites = get_favourites(std::move(cache), n);
            }  catch (...) {
                // TODO: only save cache if favs were changed
                Log::error("Failed to read cache file '", config.cached_file, "'");
            }
        }

        std::vector<std::string> pinned;
        if (config.pins) {
            pinned = get_pinned(config.pinned_file);
            if (pinned.size() > 0) {
                Log::info(pinned.size(), " pinned entries loaded");
            } else {
                Log::info("No pinned entries found");
            }
        }

        std::vector<fs::path> dirs;
        if (auto special_dirs = input.getCmdOption("-d"); !special_dirs.empty()) {
            using namespace std::string_view_literals;
            // use special dirs specified with -d argument (feature request #122)
            auto dirs_ = split_string(special_dirs, ":");
            Log::info("Using custom .desktop files path(s):\n");
            std::array status { "' [INVALID]\n"sv, "' [OK]\n"sv };
            for (auto && dir: dirs_) {
                std::error_code ec;
                auto is_dir = fs::is_directory(dir, ec) && !ec;
                Log::plain('\'', dir, status[is_dir]);
                if (is_dir) {
                    dirs.emplace_back(dir);
                }
            }
        } else {
            // get all applications dirs
            dirs = get_app_dirs();
        }

        ntime::Time commons{ "common", start };

        GridWindow window{ config };

        ntime::Time window_time{ "window", commons };

        EntriesModel   table{ config, window, icon_provider, pinned, favourites };
        EntriesManager entries_provider{ dirs, table, config };

        ntime::Time model_time{ "models", window_time };
        ntime::report(start);

        std::unique_ptr<ApplicationDriver> driver;
        if (config.oneshot) {
            driver.reset(new OneshotDriver{ app, window });
        } else {
            driver.reset(new ServerDriver{ app, window });
        }
        return driver->run();
    } catch (const Glib::Error& err) {
        // Glib::ustring performs conversion with respect to locale settings
        // it might throw (and it does [on my machine])
        // so let's try our best
        auto ustr = err.what();
        try {
            Log::error(ustr);
        } catch (const Glib::ConvertError& err) {
            Log::plain("[message conversion failed]");
            Log::error(std::string_view{ ustr.data(), ustr.bytes() });
        } catch (...) {
            Log::error("Failed to print error message due to unknown error");
        }
    } catch (const std::exception& err) {
        Log::error(err.what());
    }
    return EXIT_FAILURE;
}
