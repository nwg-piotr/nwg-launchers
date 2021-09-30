/*
 * Tools for nwg-launchers
 * Copyright (c) 2021 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "filesystem-compat.h"
#include "nwg_classes.h"

namespace ns = nlohmann;

fs::path get_cache_home();
fs::path get_config_dir(std::string_view);
fs::path get_runtime_dir();
// returns path to pid file <name>
fs::path get_pid_file(std::string_view name);
// parses icon size from `arg`, saturating to [16, 2048] if needed
int parse_icon_size(std::string_view arg);
// returns saved instance pid or nullopt if the file does not exist
// throws std::runtime_error
std::optional<pid_t> get_instance_pid(const char* pid_file_path);
void write_instance_pid(const char* path, pid_t pid);

std::string detect_wm(const Glib::RefPtr<Gdk::Display>&, const Glib::RefPtr<Gdk::Screen>&);

std::string get_term(std::string_view);
std::string get_locale(void);

std::string read_file_to_string(const fs::path&);
void save_string_to_file(std::string_view, const fs::path&);
std::vector<std::string_view> split_string(std::string_view, std::string_view);
std::string_view take_last_by(std::string_view, std::string_view);

ns::json json_from_file(const fs::path&);
ns::json string_to_json(std::string_view);
void save_json(const ns::json&, const fs::path&);
void decode_color(std::string_view, RGBA& color);

std::string get_output(const std::string&);
fs::path setup_css_file(std::string_view name, const fs::path& config_dir, const fs::path& custom_css_file);
Geometry display_geometry(std::string_view, Glib::RefPtr<Gdk::Display>, Glib::RefPtr<Gdk::Window>);

// Glibmm does not provide C++ wrappers over glibmm-unix extensions
// so, to handle a signal, we define following plain functions
// taking pointer to Instance as userdata and calling respective methods
// on the said pointer
// These functions always return G_SOURCE_CONTINUE
int instance_on_sigterm(void*);
int instance_on_sigusr1(void*);
int instance_on_sighup(void*);
int instance_on_sigint(void*);

namespace Log {
    template <typename ... Ts>
    void write(std::ostream& out, Ts && ... ts) {
        ((out << ts), ...);
        out << '\n';
    }

    template <typename ... Ts>
    void info(Ts && ... ts) { write(std::cerr, "INFO: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void warn(Ts && ... ts) { write(std::cerr, "WARN: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void error(Ts && ... ts) { write(std::cerr, "ERROR: ", std::forward<Ts>(ts)...); }
    template <typename ... Ts>
    void plain(Ts && ... ts) { write(std::cerr, std::forward<Ts>(ts)...); }
}

constexpr auto concat = [](auto&& ... xs) { std::string r; ((r += xs), ...); return r; };
