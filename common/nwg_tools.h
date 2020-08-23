/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>

#include <gtkmm.h>

#include <nlohmann/json.hpp>

#include <nwg_classes.h>

namespace ns = nlohmann;

std::string get_config_dir(std::string);

std::string detect_wm(void);

std::string get_locale(void);

std::string read_file_to_string(const std::string&);
void save_string_to_file(const std::string&, const std::string&);
std::vector<std::string_view> split_string(std::string_view, std::string_view);
std::string_view take_last_by(std::string_view, std::string_view);

ns::json string_to_json(const std::string&);
void save_json(const ns::json&, const std::string&);
void set_background(const std::string_view);

std::string get_output(const std::string&);

extern int image_size;
Gtk::Image* app_image(const Gtk::IconTheme& theme, const std::string& icon);
Geometry display_geometry(const std::string&, Glib::RefPtr<Gdk::Display>, Glib::RefPtr<Gdk::Window>);
