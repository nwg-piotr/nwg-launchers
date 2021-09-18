/* GTK-based application grid
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <list>
#include <vector>

#include "nwg_classes.h"
#include "filesystem-compat.h"
#include "on_desktop_entry.h"
#include "grid.h"

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
            entry.desktop_entry().name,
            entry.desktop_entry().comment,
            entry
        );
        // boxing is necessary
        // for some reason the icons are not shown if the images are not boxed
        auto image = Gtk::make_managed<Gtk::Image>(icons.load_icon(entry.desktop_entry().icon));
        box.set_image(*image);
        box.set_always_show_image(true);
        window.build_grids();

        return entries.begin();
    }
    template <typename ... Ts>
    void update_entry(Index index, Ts && ... args) {
        // TODO: merge entries
        *index = Entry{ std::forward<Ts>(args)... };
        auto && entry = *index;
        set_entry_stats(entry);
        GridBox new_box {
            entry.desktop_entry().name,
            entry.desktop_entry().comment,
            entry
        };
        // boxing is necessary
        // for some reason the icons are not shown if the images are not boxed
        auto image = Gtk::make_managed<Gtk::Image>(icons.load_icon(entry.desktop_entry().icon));
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
            // temporary fix for #176
            // see comments to PinnedBoxes class
            entry.stats.position = (pins.end() - result) - pins.size() - 1;
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

    EntriesManager(Span<fs::path> dirs, EntriesModel& table, GridConfig& config);
    void on_file_changed(std::string id, const Glib::RefPtr<Gio::File>& file, int priority);
    void on_file_deleted(std::string id, int priority);
private:
    // tries to load & insert entry with `id` from `file`
    void try_load_entry_(std::string id, const fs::path& file, int priority);
};
