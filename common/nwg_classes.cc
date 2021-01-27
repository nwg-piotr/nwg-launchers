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

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

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

AppBox::AppBox(Glib::ustring name, Glib::ustring exec, Glib::ustring comment):
    Gtk::Button(name, true),
    name{std::move(name)},
    exec{std::move(exec)},
    comment{std::move(comment)}
{
    if (name.length() > 25) {
        name.resize(22);
        name.append("...");
    }
    this -> set_always_show_image(true);
}

Geometry GenericShell::geometry(CommonWindow& window) {
    Geometry geo;
    auto display = window.get_display();
    if (auto monitor = display->get_monitor_at_window(window.get_window())) {
        Gdk::Rectangle rect;
        monitor->get_geometry(rect);
        geo.x = rect.get_x();
        geo.y = rect.get_y();
        geo.width = rect.get_width();
        geo.height = rect.get_height();
    }
    return geo;
}

void GenericShell::show(CommonWindow& window) {
    window.show();
    window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    window.fullscreen();
}

SwayShell::SwayShell(CommonWindow& window) {
    window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    window.set_decorated(false);
}

void SwayShell::show(CommonWindow& window) {
    // We can not go fullscreen() here:
    // On sway the window would become opaque - we don't want it
    // On i3 all windows below will be hidden - we don't want it as well
    using namespace std::string_view_literals;
    sock_.run("for_window [title="sv, window.title_view(), "*] floating enable"sv);
    sock_.run("for_window [title="sv, window.title_view(), "*] border none"sv);
    window.show();
    // works just fine on Sway/i3 as far as I could test
    // thus, no need to use ipc (I hope)
    auto [x, y, w, h] = geometry(window);
    window.resize(w, h);
    window.move(x, y);    
}

#ifdef HAVE_GTK_LAYER_SHELL
LayerShell::LayerShell(CommonWindow& window) {
    // this has to be called before the window is realized
    gtk_layer_init_for_window(window.gobj());
}

void LayerShell::show(CommonWindow& window) {
    window.show();
    auto gtk_win = window.gobj();
    std::array edges {
        GTK_LAYER_SHELL_EDGE_LEFT,
        GTK_LAYER_SHELL_EDGE_RIGHT,
        GTK_LAYER_SHELL_EDGE_TOP,
        GTK_LAYER_SHELL_EDGE_BOTTOM
    };
    for (auto edge: edges) {
        gtk_layer_set_anchor(gtk_win, edge, true);
        gtk_layer_set_margin(gtk_win, edge, 0);
    }
    gtk_layer_set_layer(gtk_win, GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_keyboard_interactivity(gtk_win, true);
    gtk_layer_set_namespace(gtk_win, window.title_view().data());
    gtk_layer_set_exclusive_zone(gtk_win, -1);        
}
#endif

PlatformWindow::PlatformWindow(std::string_view wm, std::string_view title, std::string_view role):
    CommonWindow{title, role},
    shell{std::in_place_type_t<GenericShell>{}}
{
    #ifdef HAVE_GTK_LAYER_SHELL
    if (gtk_layer_is_supported()) {
        shell.emplace<LayerShell>(*this);
        return;
    }
    #endif
    if (wm == "sway" || wm == "i3") {
        shell.emplace<SwayShell>(*this);
    }
}

void PlatformWindow::show() {
    std::visit([&](auto& shell){ shell.show(*this); }, shell);
}
