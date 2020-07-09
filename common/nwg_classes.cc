/*
 * Classes for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <algorithm>

#include "nwg_classes.h"

InputParser::InputParser (int &argc, char **argv){
    for (int i=1; i < argc; ++i)
        this->tokens.push_back(std::string(argv[i]));
}

const std::string& InputParser::getCmdOption(const std::string &option) const {
    std::vector<std::string>::const_iterator itr;
    itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
    if (itr != this->tokens.end() && ++itr != this->tokens.end()){
        return *itr;
    }
    static const std::string empty_string("");
    return empty_string;
}

bool InputParser::cmdOptionExists(const std::string &option) const {
    return std::find(this->tokens.begin(), this->tokens.end(), option)
        != this->tokens.end();
}

AppBox::AppBox() {
    this -> set_always_show_image(true);
}

AppBox::AppBox(Glib::ustring name, std::string exec, Glib::ustring comment) {
    this -> name = name;
    Glib::ustring n = this -> name;
    if (n.length() > 25) {
        n = this -> name.substr(0, 22) + "...";
    }
    this -> exec = exec;
    this -> comment = comment;
    this -> set_always_show_image(true);
    this -> set_label(n);
}

AppBox::~AppBox() {
}
