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

struct EntriesModel {
    GridConfig& config;
    GridWindow& window;

    // TODO: think of saner way to load icons
    IconProvider& icons;

    Span<std::string> pins;
    Span<CacheEntry>  favs;

    std::list<std::shared_ptr<Entry>> entries;
    using Index = std::list<std::shared_ptr<Entry>>::iterator;

    EntriesModel(GridConfig& config, GridWindow& window, IconProvider& icons, Span<std::string> pins, Span<CacheEntry> favs):
        config{ config }, window{ window }, icons{ icons }, pins{ pins }, favs{ favs }
    {
        // intentionally left blank
    }

    template <typename ... Ts>
    Index emplace_entry(Ts && ... args) {
        entries.push_front(std::shared_ptr<Entry>{ new Entry{ std::forward<Ts>(args)... } });
        auto & entry = entries.front();
        mark_entry(*entry);
        auto && box = window.emplace_box(
            entry->desktop_entry.name,
            entry->desktop_entry.comment,
            entry
        );
        box.set_image_position(Gtk::POS_TOP);
        auto image = new Gtk::Image{ icons.load_icon(entry->desktop_entry.icon) };
        box.set_image(*image);
        window.build_grids();

        return entries.begin();
    }
    template <typename ... Ts>
    void update_entry(Index index, Ts && ... args) {
        index->reset(new Entry{ std::forward<Ts>(args)... });
        auto && entry = **index;
        mark_entry(entry);
        window.update_box_by_id(entry.desktop_id);
    }
    void erase_entry(Index index) {
        auto && entry = *index;
        window.remove_box_by_desktop_id(entry->desktop_id);
        entries.erase(index);
    }
    auto & row(Index index) {
        return *index;
    }
    void mark_entry(Entry& entry) {
        if (auto result = std::find(pins.begin(), pins.end(), entry.desktop_id); result != pins.end()) {
            entry.stats.pinned = Stats::Pinned;
        }
        auto cmp = [&entry](auto && fav){ return entry.desktop_id == fav.desktop_id; };
        if (auto result = std::find_if(favs.begin(), favs.end(), cmp); result != favs.end()) {
            entry.stats.favorite = Stats::Favorite;
            entry.stats.clicks = result->clicks;
        }
    }
};

struct EntriesManager {
    struct Metadata {
        using Index = EntriesModel::Index;
        enum FileState: unsigned short {
            Ok = 0,
            Invalid,
            Hidden
        };
        Index     index; // index is invalid if state is not Ok
        FileState state;
        int       priority;

        Metadata(Index index, FileState state, int priority):
            index{ index }, state{ state }, priority{ priority }
        {
            // intentionally left blank
        }
    };

    std::list<std::string>                         desktop_ids_store;
    std::unordered_map<std::string_view, Metadata> desktop_ids_info;
    std::vector<Glib::RefPtr<Gio::FileMonitor>>    monitors;

    EntriesModel& table;
    GridConfig&   config;

    EntriesManager(Span<fs::path> dirs, EntriesModel& table, GridConfig& config): table{ table }, config{ config } {
        // set monitors
        for (auto && dir: dirs) {
            auto dir_index = monitors.size();
            auto monitored_dir = Gio::File::create_for_path(dir);
            auto && monitor = monitors.emplace_back(monitored_dir->monitor_directory());
            // TODO: should I disconnect on exit to make sure there is no dangling reference?
            monitor->signal_changed().connect([this,monitored_dir,dir_index](auto && file1, auto && file2, auto event) {
                auto && id = monitored_dir->get_relative_path(file1);
                // TODO: only call if the file is not overwritten
                switch (event) {
                    case Gio::FILE_MONITOR_EVENT_CHANGED: on_file_changed(id, file1, dir_index); break;
                    case Gio::FILE_MONITOR_EVENT_DELETED: on_file_deleted(id, dir_index); break;
                    default:break;
                };
            });
        }
        std::size_t dir_index{ 0 };
        for (auto && dir: dirs) {
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
                auto&& id = rel_path.string();
                add_entry_unchecked(id, path, dir_index);
            }
        }
    }
    void on_file_changed(std::string id, const Glib::RefPtr<Gio::File>& file, int priority) {
        if (auto result = desktop_ids_info.find(id); result != desktop_ids_info.end()) {
            auto && meta = result->second;
            if (meta.priority < priority) {
                Log::info("file '", file->get_path(), "' with id '", id, "', priority ", priority, "changed but overriden, ignored");
                return;
            }
            Log::info("file '", file->get_path(), "' with id '", id, "', priority ", priority, " changed");
            meta.priority = priority;
            auto entry = desktop_entry(file->get_path(), config.lang, config.term);
            if (entry) {
                if (meta.state == Metadata::Ok) {
                    table.update_entry(
                        result->second.index,
                        result->first,
                        entry->exec,
                        Stats{},
                        std::move(*entry)
                    );
                } else {
                    add_entry_unchecked(std::move(id), file->get_path(), priority);
                }
            } else {
                // TODO: account for loading failed
                if (meta.state == Metadata::Ok) {
                    table.erase_entry(meta.index);
                }
                meta.state = Metadata::Hidden;
            }
        } else {
            Log::info("file '", file->get_path(), "' with id '", id, "', priority ", priority, " added");
            add_entry_unchecked(std::move(id), file->get_path(), priority);
        }
    }
    void on_file_deleted(std::string id, int priority) {
        if (auto result = desktop_ids_info.find(id); result != desktop_ids_info.end()) {
            if (result->second.priority < priority) {
                Log::info("deleting file with id '", id, "' ignored");
                return;
            }
            Log::info("deleting file with id '", id, "' and priotity ", priority);
            if (result->second.state == Metadata::Ok) {
                table.erase_entry(result->second.index);
            }
            desktop_ids_info.erase(result);
            auto iter = std::find(desktop_ids_store.begin(), desktop_ids_store.end(), id);
            desktop_ids_store.erase(iter);
        }
    }
    void add_entry_unchecked(std::string id, const fs::path& file, int priority) {
        std::list<std::string> id_node;
        auto && id_ = id_node.emplace_front(std::move(id));

        EntriesModel::Index index{};
        auto metadata = Metadata::Hidden;
        auto [iter, inserted] = desktop_ids_info.try_emplace(
            id_,
            index,
            metadata,
            priority
        );
        if (inserted) {
            desktop_ids_store.splice(desktop_ids_store.begin(), id_node);
            auto entry = desktop_entry(file, config.lang, config.term);
            // TODO: handle load errors
            if (entry) {
                Log::warn("Loaded .desktop file '", file, "' with id '", id_, "'");
                auto && meta = iter->second;
                meta.state = Metadata::Ok;
                meta.index = table.emplace_entry(
                    id_,
                    entry->exec,
                    Stats{},
                    std::move(*entry)
                );
            } else {
                Log::warn("Invalid .desktop file '", file, "'");
            }
        } else {
            Log::info(".desktop file '", file, "' with id '", id_, "' overridden, ignored");
        }
    }
};

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

        EntriesModel   table{ config, window, icon_provider, pinned, favourites };
        EntriesManager entries_provider{ dirs, table, config };

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
