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

namespace ns = nlohmann;

std::string get_config_dir(std::string);

std::string detect_wm(void);

std::string get_locale(void);

std::string read_file_to_string(std::string);
void save_string_to_file(std::string, std::string);
std::vector<std::string> split_string(std::string, std::string);

ns::json string_to_json(std::string);
void save_json(ns::json, std::string);

std::string get_output(std::string);
