/*
 * GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>
#include <iostream>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "grid.h"

const char* const HELP_MESSAGE =
"GTK application grid: nwggrid " VERSION_STR " (c) 2020 Piotr Miller, Sergey Smirnykh & Contributors \n\n\
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
-wm <wmname>     window manager name (if can not be detected)\n\n\
[requires layer-shell]:\n\
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},        default: OVERLAY\n\
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto\n";

int main(int argc, char *argv[]) {
    try {
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

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

        auto app = Gtk::Application::create(argc, argv);

        auto provider = Gtk::CssProvider::create();
        auto display = Gdk::Display::get_default();
        auto screen = display->get_default_screen();
        if (!provider || !display || !screen) {
            Log::error("Failed to initialize GTK");
            return EXIT_FAILURE;
        }

        GridConfig config {
            input,
            screen,
            config_dir
        };
        Log::info("Locale: ", config.lang);

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

        gettimeofday(&tp, NULL);
        long int commons_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        gettimeofday(&tp, NULL);
        long int bs_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        GridWindow window{ config };

        gettimeofday(&tp, NULL);
        long int images_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        gettimeofday(&tp, NULL);
        long int boxes_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        struct EntriesTable {
            GridConfig&    config;
            // we will reference strings with string_views, so we don't want invalidations
            std::list<std::string> desktop_ids_store;
            // Maps desktop-ids to their table indices, empty stands for 'hidden'
            std::unordered_map<std::string_view, std::shared_ptr<Entry>> desktop_ids;

            // Table, only contains shown entries
            std::vector<DesktopEntry> desktop_entries;
            std::vector<Gtk::Image>   images;

            Span<std::string> pinned;
            Span<CacheEntry>  favourites;

            EntriesTable(GridConfig& config, Span<std::string> pins, Span<CacheEntry> favs):
                config{ config }, pinned{ pins }, favourites{ favs }
            {
                // intentionally left blank
            }
            void clear() {
                desktop_ids.clear();
                images.clear();
                desktop_entries.clear();
            }
            void add_entry(std::string id, const fs::path& path) {
                std::list<std::string> node;
                auto && id_ = node.emplace_front(std::move(id));
                if (auto [at, inserted] = desktop_ids.try_emplace(id_); inserted) {
                    if (auto entry = desktop_entry(path, config.lang, config.term)) {
                        // save the node to the store
                        desktop_ids_store.splice(desktop_ids_store.begin(), node);
                        at->second.reset(new Entry{
                            id_,
                            entry->exec, // TODO: destruct DesktopEntry to pieces so that we could safely move exec here
                            Stats{ 0, 0, Stats::Common, Stats::Unpinned },
                            std::move(*entry)
                        });
                    } else {
                        Log::warn("'", path, "' is not valid .desktop file");
                    }
                }
            }
            void mark_entries() {
                int pin_index = 0; // preserve pins order
                for (auto& pin : pinned) {
                    if (auto result = desktop_ids.find(pin); result != desktop_ids.end() && result->second) {
                        result->second->stats.pinned   = Stats::Pinned;
                        result->second->stats.position = pin_index;
                        ++pin_index;
                    }
                }
                for (auto& [fav, clicks] : favourites) {
                    if (auto result = desktop_ids.find(fav); result != desktop_ids.end() && result->second) {
                        result->second->stats.clicks   = clicks;
                        result->second->stats.favorite = Stats::Favorite;
                    }
                }
            }
        };

        struct EntriesProvider {
            GridWindow&    window;
            EntriesTable&  table;
            const IconProvider& icons;

            Span<fs::path> dirs;

            EntriesProvider(GridWindow& window, EntriesTable& table, const IconProvider& icons, Span<fs::path> dirs):
                window{ window },
                table{ table },
                icons{ icons },
                dirs{ dirs }
            {
                reload();
            }

            void reload() {
                auto desktop_id = [](auto& path) {
                    return path.string(); // actual desktop_id requires '/' to be replaced with '-'
                };

                table.clear();

                for (auto& dir : dirs) {
                    std::error_code ec;
                    fs::directory_iterator dir_iter{ dir, ec };
                    for (auto& entry : dir_iter) {
                        if (ec) {
                            Log::error(ec.message());
                            ec.clear();
                            continue;
                        }
                        if (!entry.is_regular_file()) {
                            continue;
                        }
                        auto& path = entry.path();
                        auto&& rel_path = path.lexically_relative(dir);
                        auto&& id = desktop_id(rel_path);
                        table.add_entry(id, path);
                    }
                }
                table.mark_entries();
                for (auto& [desktop_id, entry_] : table.desktop_ids) {
                    if (entry_) {
                        auto & entry = *entry_;
                        auto& ab = window.emplace_box(std::move(entry.desktop_entry.name),
                                                      std::move(entry.desktop_entry.comment),
                                                      entry_);
                        auto icon = icons.load_icon(entry.desktop_entry.icon);
                        ab.set_image_position(Gtk::POS_TOP);
                        ab.set_image(icon);
                    }
                }
                window.build_grids();
            }
        };

        struct DirectoriesWatcher {
            EntriesProvider& entries_provider;
            std::vector<Glib::RefPtr<Gio::FileMonitor>> monitors;

            DirectoriesWatcher(EntriesProvider& entries_provider, Span<fs::path> dirs): entries_provider{ entries_provider } {
                for (auto && dir: dirs) {
                    try {
                        auto file = Gio::File::create_for_path(dir);
                        auto & monitor = monitors.emplace_back(file->monitor_directory());
                        monitor->signal_changed().connect([this](auto && file1, auto && file2, auto && event) {
                            (void)file1;
                            (void)file2;
                            switch (event) {
                                case Gio::FILE_MONITOR_EVENT_CHANGED:
                            //    Gio::FILE_MONITOR_EVENT_MOVED_IN:
                            //    Gio::FILE_MONITOR_EVENT_MOVED_OUT:
                                case Gio::FILE_MONITOR_EVENT_DELETED:
                                    this->entries_provider.reload();
                                    break;
                                default:break;
                            }
                        });
                    } catch (const Gio::Error& error) {
                        Log::error("Failed to monitor directory '", dir, "': ", error.what());
                    }
                }
            }
        };

        EntriesTable    table{ config, pinned, favourites };
        EntriesProvider entries_provider{ window, table, icon_provider, dirs };
        DirectoriesWatcher directories_watcher{ entries_provider, dirs };

        gettimeofday(&tp, NULL);
        long int grids_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        gettimeofday(&tp, NULL);
        long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        auto format = [](auto&& title, auto from, auto to) {
            Log::plain(title, to - from, "ms");
        };
        format("Total: ", start_ms, end_ms);
        format("\tgrids:   ", grids_ms, end_ms);
        format("\tboxes:   ", boxes_ms, grids_ms);
        format("\timages:  ", images_ms, boxes_ms);
        format("\tbs:      ", bs_ms, images_ms);
        format("\tcommons: ", commons_ms, bs_ms);

        GridInstance instance{ *app.get(), window };
        return app->run();
    } catch (const Glib::FileError& error) {
        Log::error(error.what());
    } catch (const std::runtime_error& error) {
        Log::error(error.what());
    }
    return EXIT_FAILURE;
}
