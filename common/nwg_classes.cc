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
        cr->set_source_rgba(background.red, background.green, background.blue, background.alpha);
    } else {
        cr->set_source_rgb(background.red, background.green, background.blue);
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

void CommonWindow::quit() {
    auto toplevel = dynamic_cast<Gtk::Window*>(this->get_toplevel());
    if (!toplevel) {
        std::cerr << "ERROR: Toplevel widget is not a window\n";
        std::exit(EXIT_FAILURE);
    }
    toplevel->get_application()->quit();
}

AppBox::AppBox() {
    this -> set_always_show_image(true);
}

AppBox::AppBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment) {
    this -> name = name;
    if (name.length() > 25) {
        name = name.substr(0, 22) + "...";
    }
    this -> exec = std::move(exec);
    this -> comment = std::move(comment);
    this -> set_always_show_image(true);
    this -> set_label(name);
}

AppBox::~AppBox() {
}
