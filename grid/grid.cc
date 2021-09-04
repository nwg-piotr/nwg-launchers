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
#include <fstream>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "grid.h"
#include "on_desktop_entry.h"

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
-wm <wmname>     window manager name (if can not be detected)\n\
-oneshot         run in the foreground, exit when window is closed\n\
                 generally you should not use this option, use simply `nwggrid` instead\n\
[requires layer-shell]:\n\
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},         default: OVERLAY\n\
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto\n";

inline bool looks_like_desktop_file(const Glib::RefPtr<Gio::File>& file) {
    fs::path path{ file->get_path() };
    return path.extension() == ".desktop";
}
inline bool looks_like_desktop_file(const fs::directory_entry& entry) {
    auto && path = entry.path();
    return path.extension() == ".desktop";
}
inline bool can_be_loaded(const Glib::RefPtr<Gio::File>& file) {
    auto file_type = file->query_file_type();
    return file_type == Gio::FILE_TYPE_REGULAR;
}
inline bool can_be_loaded(const fs::directory_entry& entry) {
    return entry.is_regular_file();
}
inline auto desktop_id(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& dir) {
    return dir->get_relative_path(file);
}
inline auto desktop_id(const fs::path& file, const fs::path& dir) {
    return file.lexically_relative(dir);
}

// Table containing entries
// internally is a thin wrapper over list<entry>
struct EntriesModel {
    GridConfig& config;
    GridWindow& window;

    // TODO: think of saner way to load icons
    IconProvider& icons;

    Span<std::string> pins;
    Span<CacheEntry>  favs;

    // list because entries should not get invalidated when inserting/erasing
    std::list<Entry> entries;
    using Index = typename decltype(entries)::iterator;

    EntriesModel(GridConfig& config, GridWindow& window, IconProvider& icons, Span<std::string> pins, Span<CacheEntry> favs):
        config{ config }, window{ window }, icons{ icons }, pins{ pins }, favs{ favs }
    {
        // intentionally left blank
    }

    template <typename ... Ts>
    Index emplace_entry(Ts && ... args) {
        auto & entry = entries.emplace_front(std::forward<Ts>(args)...);
        set_entry_stats(entry);
        auto && box = window.emplace_box(
            entry.desktop_entry.name,
            entry.desktop_entry.comment,
            entry
        );
        auto image = Gtk::manage(new Gtk::Image{ icons.load_icon(entry.desktop_entry.icon) });
        box.set_image(*image);
        window.build_grids();

        return entries.begin();
    }
    template <typename ... Ts>
    void update_entry(Index index, Ts && ... args) {
        *index = Entry{ std::forward<Ts>(args)... };
        auto && entry = *index;
        set_entry_stats(entry);
        GridBox new_box {
            entry.desktop_entry.name,
            entry.desktop_entry.comment,
            entry
        };
        auto image = Gtk::manage(new Gtk::Image{ icons.load_icon(entry.desktop_entry.icon) });
        new_box.set_image(*image);
        window.update_box_by_id(entry.desktop_id, std::move(new_box));
    }
    void erase_entry(Index index) {
        auto && entry = *index;
        window.remove_box_by_desktop_id(entry.desktop_id);
        entries.erase(index);
        window.build_grids();
    }
    auto & row(Index index) {
        return *index;
    }
private:
    void set_entry_stats(Entry& entry) {
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

/* EntriesManager handles loading/updating entries.
 * For each directory in `dirs` it sets a monitor and loads all .desktop files in it.
 * It also supports "overwriting" files: if two files have the same desktop id,
 * it will work with the file stored in the directory listed first, i.e. having more precedence.
 * The "desktop id" mechanism it uses is a bit different than the mechanism described in
 * the Freedesktop standard, but it works roughly the same; if two files have conflicting desktop ids,
 * the "desktop id"s will conflict too, and vice versa. */
struct EntriesManager {
    struct Metadata {
        using Index = EntriesModel::Index;
        enum FileState: unsigned short {
            Ok = 0,
            Invalid,
            Hidden
        };
        Index     index;    // index in table; index is invalid if state is not Ok
        FileState state;
        int       priority; // the lower the value, the bigger the priority
                            // i.e. if file1.priority > file2.priority, the file2 wins

        Metadata(Index index, FileState state, int priority):
            index{ index }, state{ state }, priority{ priority }
        {
            // intentionally left blank
        }
    };

    // stores "desktop id"s
    // list because insertions/removals should not invalidate the store
    std::list<std::string>                         desktop_ids_store;
    // maps "desktop id" to Metadata
    std::unordered_map<std::string_view, Metadata> desktop_ids_info;
    // stored monitors
    // just to keep them alive
    std::vector<Glib::RefPtr<Gio::FileMonitor>>    monitors;

    EntriesModel& table;
    GridConfig&   config;

    DesktopEntryConfig desktop_entry_config;

    EntriesManager(Span<fs::path> dirs, EntriesModel& table, GridConfig& config):
        table{ table }, config{ config }, desktop_entry_config{ config.lang, config.term }
    {
        // set monitors
        monitors.reserve(dirs.size());
        for (auto && dir: dirs) {
            auto dir_index = monitors.size();
            auto monitored_dir = Gio::File::create_for_path(dir);
            auto && monitor = monitors.emplace_back(monitored_dir->monitor_directory());
            // dir_index and monitored_dir are captured by value
            // TODO: should I disconnect on exit to make sure there is no dangling reference to `this`?
            monitor->signal_changed().connect([this,monitored_dir,dir_index](auto && file1, auto && file2, auto event) {
                (void)file2; // silence warning
                if (looks_like_desktop_file(file1)) {
                    auto && id = desktop_id(file1, monitored_dir);
                    switch (event) {
                        // ignored in favor of CHANGES_DONE_HINT
                        case Gio::FILE_MONITOR_EVENT_CHANGED: break;
                        case Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
                            if (can_be_loaded(file1)) {
                                on_file_changed(id, file1, dir_index);
                            }
                            break;
                        case Gio::FILE_MONITOR_EVENT_DELETED:
                            on_file_deleted(id, dir_index);
                            break;
                        // ignore because CREATED is emitted when the file is created but not written to
                        // copying/moving emit two signals: CREATED and then CHANGED
                        case Gio::FILE_MONITOR_EVENT_CREATED:
                        // TODO: it seems we can safely ignored but I guess we should doublecheck
                        case Gio::FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: break;
                        // TODO: should we set WATCH_MOVES?
                        // we don't set WATCH_MOVES so these three should not be emitted
                        case Gio::FILE_MONITOR_EVENT_RENAMED:
                        case Gio::FILE_MONITOR_EVENT_MOVED_IN:
                        case Gio::FILE_MONITOR_EVENT_MOVED_OUT: Log::warn("WATCH_MOVES flag is set but not handled"); break;
                        // we don't set SEND_MOVED (deprecated)
                        case Gio::FILE_MONITOR_EVENT_MOVED: Log::warn("SEND_MOVED flag is deprecated and thus shouldn't be used"); break;
                        // TODO: handle unmounting, e.g. for all files in directory when pre-unmounting erase their entries
                        case Gio::FILE_MONITOR_EVENT_PRE_UNMOUNT:
                        case Gio::FILE_MONITOR_EVENT_UNMOUNTED: Log::warn("Unmounting is not supported yet"); break;
                        // no default statement so we could see a compiler warning if new flag is added in the future
                    };
                }
            });
        }
        // dir_index is used as priority
        std::size_t dir_index{ 0 };
        for (auto && dir: dirs) {
            std::error_code ec;
            // TODO: shouldn't it be recursive_directory_iterator?
            fs::directory_iterator dir_iter{ dir, ec };
            for (auto& entry : dir_iter) {
                if (ec) {
                    Log::error(ec.message());
                    ec.clear();
                    continue;
                }
                if (looks_like_desktop_file(entry) && can_be_loaded(entry)) {
                    auto && path = entry.path();
                    auto && id = desktop_id(path, dir);
                    try_load_entry_(id, path, dir_index);
                }
            }
            ++dir_index;
        }
    }
    void on_file_changed(std::string id, const Glib::RefPtr<Gio::File>& file, int priority) {
        auto && path = file->get_path();
        if (auto result = desktop_ids_info.find(id); result != desktop_ids_info.end()) {
            auto && meta = result->second;
            if (meta.priority < priority) {
                // changed file is overridden, no need to do anything
                return;
            }
            meta.priority = priority;
            on_desktop_entry(path, desktop_entry_config, Overloaded {
                // successfully reloaded the new entry
                [&meta=meta,this,&result](DesktopEntry && desktop_entry) {
                    if (meta.state == Metadata::Ok) {
                        // entry was ok, now ok -> update contents
                        table.update_entry(
                            result->second.index,
                            result->first,
                            Stats{},
                            std::move(desktop_entry)
                        );
                    } else {
                        // entry wasn't ok, but now ok -> add it to table it
                        meta.index = table.emplace_entry(
                            result->first,
                            Stats{},
                            std::move(desktop_entry)
                        );
                        meta.state = Metadata::Ok;
                    }
                },
                // reloaded entry is hidden
                [&meta=meta,this](OnDesktopEntry::Hidden){
                    if (meta.state == Metadata::Ok) {
                        table.erase_entry(meta.index);
                    }
                    meta.state = Metadata::Hidden;
                },
                // failed to reload entry
                [&meta=meta,&path=path,this](OnDesktopEntry::Error){
                    Log::error("Failed to load desktop file'", path, "'");
                    if (meta.state == Metadata::Ok) {
                        table.erase_entry(meta.index);
                    }
                    meta.state = Metadata::Invalid;
                }
            });
        } else {
            // there was not such entry, add it
            try_load_entry_(std::move(id), path, priority);
        }
    }
    void on_file_deleted(std::string id, int priority) {
        if (auto result = desktop_ids_info.find(id); result != desktop_ids_info.end()) {
            if (result->second.priority < priority) {
                return;
            }
            if (result->second.state == Metadata::Ok) {
                table.erase_entry(result->second.index);
            }
            desktop_ids_info.erase(result);
            auto iter = std::find(desktop_ids_store.begin(), desktop_ids_store.end(), id);
            desktop_ids_store.erase(iter);
        } else {
            Log::error("on_file_deleted: no entry with id '", id, "'");
        }
    }
private:
    // tries to load & insert entry with `id` from `file`
    void try_load_entry_(std::string id, const fs::path& file, int priority) {
        // node with id
        std::list<std::string> id_node;
        // desktop_ids_store stores string_views.
        // If we just insert id, there will be dangling reference when id is freed.
        // To avoid this, we store id in the node and then take a view of it.
        auto && id_ = id_node.emplace_front(std::move(id));

        auto [iter, inserted] = desktop_ids_info.try_emplace(
            id_,
            EntriesModel::Index{},
            Metadata::Hidden,
            priority
        );
        if (inserted) {
            // the entry was inserted, therefore we need to add the node to the store
            // to keep the view valid
            desktop_ids_store.splice(desktop_ids_store.begin(), id_node);
            // load it
            on_desktop_entry(file, desktop_entry_config, Overloaded {
                [&,this,iter=iter](DesktopEntry && desktop_entry){
                    auto && meta = iter->second;
                    meta.state = Metadata::Ok;
                    meta.index = table.emplace_entry(
                        id_,
                        Stats{},
                        std::move(desktop_entry)
                    );
                },
                [&](OnDesktopEntry::Error){ Log::error("Failed to load desktop file '", file, "'"); },
                [](OnDesktopEntry::Hidden){ }
            });
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

        auto app = Gtk::Application::create();

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

        GridWindow window{ config };

        gettimeofday(&tp, NULL);
        long int window_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        EntriesModel   table{ config, window, icon_provider, pinned, favourites };
        EntriesManager entries_provider{ dirs, table, config };

        gettimeofday(&tp, NULL);
        long int model_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        auto format = [](auto&& title, auto from, auto to) {
            Log::plain(title, to - from, "ms");
        };
        format("Total: ", start_ms, model_ms);
        format("\tcommon: ", start_ms, commons_ms);
        format("\twindow: ", commons_ms, window_ms);
        format("\tmodels: ", window_ms, model_ms);

        GridInstance instance{ *app.get(), window };
        return app->run();
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
