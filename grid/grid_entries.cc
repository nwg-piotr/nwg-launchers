/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
#include "grid_entries.h"
#include "log.h"

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

EntriesManager::EntriesManager(Span<fs::path> dirs, EntriesModel& table, GridConfig& config):
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

// tries to load & insert entry with `id` from `file`
void EntriesManager::try_load_entry_(std::string id, const fs::path& file, int priority) {
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
            [&,this,iter=iter](std::unique_ptr<DesktopEntry> && desktop_entry){
                auto && meta = iter->second;
                meta.state = Metadata::Ok;
                meta.index = table.emplace_entry(
                    id_,
                    Stats{},
                    std::move(desktop_entry)
                );
            },
            [&](OnDesktopEntry::Error){
                Log::error("Failed to load desktop file '", file, "'");
            },
            [](OnDesktopEntry::Hidden){ }
        });
    } else {
        Log::info(".desktop file '", file, "' with id '", id_, "' overridden, ignored");
    }
}

void EntriesManager::on_file_deleted(std::string id, int priority) {
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

void EntriesManager::on_file_changed(std::string id, const Glib::RefPtr<Gio::File>& file, int priority) {
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
            [&meta=meta,this,&result](std::unique_ptr<DesktopEntry> && desktop_entry) {
                if (meta.state == Metadata::Ok) {
                    // entry was ok, now ok -> update contents
                    auto new_index = table.update_entry(
                        result->second.index,
                        result->first,
                        Stats{},
                        std::move(desktop_entry)
                    );
                    result->second.index = new_index;
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
