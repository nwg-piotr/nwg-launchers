/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <filesystem>
#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>

#include <gtkmm.h>

#include <nlohmann/json.hpp>

#include "nwg_classes.h"

namespace ns = nlohmann;

extern int image_size; // button image size in pixels

std::filesystem::path get_cache_home();
std::filesystem::path get_config_dir(std::string_view);

std::string detect_wm(void);

std::string get_term(std::string_view);
std::string get_locale(void);

std::string read_file_to_string(const std::filesystem::path&);
void save_string_to_file(std::string_view, const std::filesystem::path&);
std::vector<std::string_view> split_string(std::string_view, std::string_view);
std::string_view take_last_by(std::string_view, std::string_view);

ns::json json_from_file(const std::filesystem::path&);
ns::json string_to_json(std::string_view);
void save_json(const ns::json&, const std::filesystem::path&);
void decode_color(std::string_view, RGBA& color);

std::string get_output(const std::string&);

Gtk::Image* app_image(const Gtk::IconTheme&, const std::string&, const Glib::RefPtr<Gdk::Pixbuf>&);
Geometry display_geometry(const std::string&, Glib::RefPtr<Gdk::Display>, Glib::RefPtr<Gdk::Window>);

void create_pid_file_or_kill_pid(std::string);

enum class SwayError {
    ConnectFailed,
    EnvNotSet,
    OpenFailed,
    RecvHeaderFailed,
    RecvBodyFailed,
    SendHeaderFailed,
    SendBodyFailed
};

struct SwaySock {
    SwaySock();
    SwaySock(const SwaySock&) = delete;
    ~SwaySock();
    // pass the command to sway via socket
    void run(std::string_view);
    // swaymsg -t get_outputs
    std::string get_outputs();
    std::string get_workspaces();
    
    // see sway-ipc (7)
    enum class Commands: std::uint32_t {
        Run = 0,
        GetWorkspaces = 1,
        GetOutputs = 3
    };
    static constexpr std::array MAGIC { 'i', '3', '-', 'i', 'p', 'c' };
    static constexpr auto MAGIC_SIZE = MAGIC.size();
    // magic + body length (u32) + type (u32)
    static constexpr auto HEADER_SIZE = MAGIC_SIZE + 2 * sizeof(std::uint32_t);
    
    int                           sock_;
    std::array<char, HEADER_SIZE> header;

    void send_header_(std::uint32_t, Commands);
    void send_body_(std::string_view);
    std::string recv_response_();
};
