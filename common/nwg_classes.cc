/*
 * Classes for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <algorithm>
#include <array>
#include <iostream>

#include "nwg_classes.h"

InputParser::InputParser (int argc, char **argv) {
    tokens.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        tokens.emplace_back(argv[i]);
    }
}

std::string_view InputParser::getCmdOption(std::string_view option) const {
    auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
    if (itr != this->tokens.end() && ++itr != this->tokens.end()){
        return *itr;
    }
    return empty_string;
}

bool InputParser::cmdOptionExists(std::string_view option) const {
    return std::find(this->tokens.begin(), this->tokens.end(), option)
        != this->tokens.end();
}

CommonWindow::CommonWindow(const Glib::ustring& title, const Glib::ustring& role) {
    set_title(title);
    set_role(role);
    set_skip_pager_hint(true);
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    set_app_paintable(true);
    check_screen();
}

CommonWindow::~CommonWindow() { }

bool CommonWindow::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    cr->save();
    if (_SUPPORTS_ALPHA) {
        cr->set_source_rgba(0.0, 0.0, 0.0, opacity);
    } else {
        cr->set_source_rgb(0.0, 0.0, 0.0);
    }
    cr->set_operator(Cairo::OPERATOR_SOURCE);
    cr->paint();
    cr->restore();
    return Gtk::Window::on_draw(cr);
}

void CommonWindow::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    (void) previous_screen; // suppress warning
    this->check_screen();
}

void CommonWindow::check_screen() {
    auto screen = get_screen();
    auto visual = screen -> get_rgba_visual();

    if (!visual) {
        std::cerr << "Your screen does not support alpha channels!\n";
    }
    _SUPPORTS_ALPHA = (bool)visual;
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
