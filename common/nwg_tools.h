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

#include <nlohmann/json.hpp>

#include "nwg_classes.h"

namespace ns = nlohmann;

extern int image_size; // button image size in pixels

std::filesystem::path get_cache_home();
std::filesystem::path get_config_dir(std::string_view);

std::string detect_wm(const Glib::RefPtr<Gdk::Display>&, const Glib::RefPtr<Gdk::Screen>&);

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
Geometry display_geometry(std::string_view, Glib::RefPtr<Gdk::Display>, Glib::RefPtr<Gdk::Window>);

void create_pid_file_or_kill_pid(std::string);
