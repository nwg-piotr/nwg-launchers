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
#include <charconv>
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

Config::Config(const InputParser& parser, std::string_view title, std::string_view role, const Glib::RefPtr<Gdk::Screen>& screen):
    parser{parser},
    title{title},
    role{role}
#ifdef HAVE_GTK_LAYER_SHELL
    ,layer_shell_args{parser}
#endif
{
    if (auto wm_name = parser.getCmdOption("-wm"); !wm_name.empty()){
        this->wm = wm_name;
    } else {
        this->wm = detect_wm(screen->get_display(), screen);
    }
    std::cout << "wm: " << this->wm << '\n';
}

CommonWindow::CommonWindow(Config& config): title{config.title} {
    set_title({config.title.data(), config.title.size()});
    set_role({config.role.data(), config.role.size()});
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

int CommonWindow::get_height() { return Gtk::Window::get_height(); }

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

GenericShell::GenericShell(Config& config) {
    // respects_fullscreen is default initialized to true
    using namespace std::string_view_literals;
    constexpr std::array wms { "openbox"sv, "i3"sv, "sway"sv };
    for (auto && wm: wms) {
        if (config.wm == wm) {
            respects_fullscreen = false;
            break;
        }
    }
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

SwayShell::SwayShell(CommonWindow& window, Config& config):
    GenericShell{config}
{
    window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    window.set_decorated(false);
    using namespace std::string_view_literals;
    sock_.run("for_window [title="sv, window.title_view(), "*] floating enable"sv);
    sock_.run("for_window [title="sv, window.title_view(), "*] border none"sv);
}

void SwayShell::show(CommonWindow& window, hint::Fullscreen_) {
    // We can not go fullscreen() here:
    // On sway the window would become opaque - we don't want it
    // On i3 all windows below will be hidden - we don't want it as well
    window.show();
    // works just fine on Sway/i3 as far as I could test
    // thus, no need to use ipc (I hope)
    auto [x, y, w, h] = geometry(window);
    window.resize(w, h);
    window.move(x, y);
}

#ifdef HAVE_GTK_LAYER_SHELL
LayerShellArgs::LayerShellArgs(const InputParser& parser) {
    using namespace std::string_view_literals;
    if (auto layer = parser.getCmdOption("-layer-shell-layer"); !layer.empty()) {
        constexpr std::array map {
            std::pair{ "BACKGROUND"sv, GTK_LAYER_SHELL_LAYER_BACKGROUND },
            std::pair{ "BOTTOM"sv,     GTK_LAYER_SHELL_LAYER_BOTTOM },
            std::pair{ "TOP"sv,        GTK_LAYER_SHELL_LAYER_TOP },
            std::pair{ "OVERLAY"sv,    GTK_LAYER_SHELL_LAYER_OVERLAY }
        };
        bool found = false;
        for (auto && [s, l]: map) {
            if (layer == s) {
                this->layer = l;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "ERROR: Incorrect layer-shell-layer value\n";
            std::exit(EXIT_FAILURE);
        }
    }
    if (auto zone = parser.getCmdOption("-layer-shell-exclusive-zone"); !zone.empty()) {
        this->exclusive_zone_is_auto = zone == "auto"sv;
        if (!this->exclusive_zone_is_auto) {
            auto [p, ec] = std::from_chars(zone.data(), zone.data() + zone.size(), this->exclusive_zone);
            if (ec != std::errc()) {
                std::cerr << "ERROR: Unable to decode layer-shell-exclusive-zone value\n";
                std::exit(EXIT_FAILURE);
            }
        }
    }
}

LayerShell::LayerShell(CommonWindow& window, LayerShellArgs args): args{args} {
    // this has to be called before the window is realized
    gtk_layer_init_for_window(window.gobj());
}
#endif

PlatformWindow::PlatformWindow(Config& config):
    CommonWindow{config},
    shell{std::in_place_type<GenericShell>, config}
{
    #ifdef HAVE_GTK_LAYER_SHELL
    if (gtk_layer_is_supported()) {
        shell.emplace<LayerShell>(*this, config.layer_shell_args);
        return;
    }
    #endif
    if (config.wm == "sway" || config.wm == "i3") {
        shell.emplace<SwayShell>(*this, config);
    }
}
