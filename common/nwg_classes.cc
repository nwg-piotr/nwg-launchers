/*
 * Classes for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <algorithm>
#include <array>
#include <iostream>

#include "nwg_classes.h"
#include "nwg_tools.h"

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
    return {};
}

bool InputParser::cmdOptionExists(std::string_view option) const {
    return std::find(this->tokens.begin(), this->tokens.end(), option)
        != this->tokens.end();
}

RGBA InputParser::get_background_color(double default_opacity) const {
    RGBA color{ 0.0, 0.0, 0.0, default_opacity };
    if (auto opacity_str = getCmdOption("-o"); !opacity_str.empty()) {
        auto opacity = std::stod(std::string{opacity_str});
        if (opacity >= 0.0 && opacity <= 1.0) {
            color.alpha = opacity;
        } else {
            std::cerr << "ERROR: Opacity must be in range 0.0 to 1.0\n";
        }
    }
    if (auto color_str = getCmdOption("-b"); !color_str.empty()) {
        decode_color(color_str, color);
    }
    return color;
}

CommonWindow::CommonWindow(std::string_view title, std::string_view role): title{title} {
    set_title({title.data(), title.size()});
    set_role({role.data(), role.size()});
    set_skip_pager_hint(true);
    add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
    set_app_paintable(true);
    check_screen();
}

CommonWindow::~CommonWindow() { }

std::string_view CommonWindow::title_view() { return title; }

bool CommonWindow::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    cr->save();
    auto [r, g, b, a] = this->background_color;
    if (_SUPPORTS_ALPHA) {
        cr->set_source_rgba(r, g, b, a);
    } else {
        cr->set_source_rgb(r, g, b);
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
    gtk_widget_set_visual(GTK_WIDGET(gobj()), visual->gobj());
}

void CommonWindow::set_background_color(RGBA color) {
    this->background_color = color;
}

AppBox::AppBox() {
    this -> set_always_show_image(true);
}

AppBox::AppBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment) : Gtk::Button(name, true) {
    this -> name = name;
    if (name.length() > 25) {
        name = name.substr(0, 22) + "...";
    }
    this -> exec = std::move(exec);
    this -> comment = std::move(comment);
    this -> set_always_show_image(true);
}

AppBox::~AppBox() {
}
