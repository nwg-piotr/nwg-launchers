/*
 * Tools for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>

#include <gtkmm.h>

#include <nlohmann/json.hpp>

#include <nwg_classes.h>

namespace ns = nlohmann;

std::string get_config_dir(std::string);

std::string detect_wm(void);

std::string get_locale(void);

std::string read_file_to_string(const std::string&);
void save_string_to_file(const std::string&, const std::string&);
std::vector<std::string> split_string(std::string, std::string);

ns::json string_to_json(const std::string&);
void save_json(const ns::json&, const std::string&);

std::string get_output(const std::string&);

/*
 * Returns x, y, width, height of focused display
 * */
Geometry display_geometry(const std::string&, Glib::RefPtr<Gdk::Display>, Glib::RefPtr<Gdk::Window>);