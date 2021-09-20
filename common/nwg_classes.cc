/*
 * Classes for nwg-launchers
 * Copyright (c) 2021 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Copyright (c) 2021 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <unistd.h>
#include <glib-unix.h>

#include <algorithm>
#include <array>
#include <fstream>

#include "charconv-compat.h"
#include "nwgconfig.h"
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
            Log::error("Opacity must be in range 0.0 to 1.0");
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
    Log::info("wm: ", this->wm);

    auto halign_ = parser.getCmdOption("-ha");
    if (halign_ == "l" || halign_ == "left") { halign = HAlign::Left; }
    if (halign_ == "r" || halign_ == "right") { halign = HAlign::Right; }
    auto valign_ = parser.getCmdOption("-va");
    if (valign_ == "t" || valign_ == "top") { valign = VAlign::Top; }
    if (valign_ == "b" || valign_ == "bottom") { valign = VAlign::Bottom; }

    if (auto css_name = parser.getCmdOption("-c"); !css_name.empty()) {
        css_filename = css_name;
    }
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
        Log::warn("Your screen does not support alpha channels!");
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
    if (this->name.length() > 25) {
        this->name.resize(22);
        this->name.append("...");
    }
    this -> set_always_show_image(true);
}

Instance::Instance(Gtk::Application& app, std::string_view name): app{ app } {
    // TODO: maybe use dbus if it is present?
    pid_file = get_pid_file(name);
    pid_file += ".pid";
    auto lock_file = pid_file;
    lock_file += ".lock";

    // we'll need this lock file to synchronize us & running instance
    // note: it doesn't get unlinked when the program exits
    //       so the other instance can safely wait on this file
    pid_lock_fd = open(lock_file.c_str(), O_CLOEXEC | O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
    if (pid_lock_fd < 0) {
        int err = errno;
        throw std::runtime_error{ concat("failed to open pid lock: ", error_description(err)) };
    }

    // let's try to read pid file
    if (auto pid = get_instance_pid(pid_file.c_str())) {
        Log::info("Another instance is running, trying to terminate it...");
        if (kill(*pid, SIGTERM) != 0) {
            throw std::runtime_error{ "failed to send SIGTERM to pid" };
        }
        Log::plain("Success");
    }

    // acquire lock
    // we'll hold this lock until the very exit
    if (lockf(pid_lock_fd, F_LOCK, 0)) {
        int err = errno;
        throw std::runtime_error{ concat("failed to lock the pid lock: ", error_description(err)) };
    }

    // write instance pid
    write_instance_pid(pid_file.c_str(), getpid());

    // using glib unix extensions instead of plain signals allows for arbitrary functions to be used
    // when handling signals
    g_unix_signal_add(SIGHUP, instance_on_sighup, this);
    g_unix_signal_add(SIGINT, instance_on_sigint, this);
    g_unix_signal_add(SIGUSR1, instance_on_sigusr1, this);
    g_unix_signal_add(SIGTERM, instance_on_sigterm, this);
}

void Instance::on_sighup(){}
void Instance::on_sigint(){ app.quit(); }
void Instance::on_sigusr1() {}
void Instance::on_sigterm(){ app.quit(); }

Instance::~Instance() {
    // it is important to delete pid file BEFORE releasing the lock
    // otherwise other instance may overwrite it just before we delete it
    if (std::error_code err; !fs::remove(pid_file, err) && err) {
        Log::error("Failed to remove pid file '", pid_file, "': ", err.message());
    }
    if (lockf(pid_lock_fd, F_ULOCK, 0)) {
        int err = errno;
        Log::error("Failed to unlock pid lock: ", error_description(err));
    }
    close(pid_lock_fd);
}

IconProvider::IconProvider(const Glib::RefPtr<Gtk::IconTheme>& theme, int icon_size):
    icon_theme{ theme },
    icon_size{ icon_size }
{
    constexpr std::array fallback_icons {
        DATA_DIR_STR "/icon-missing.svg",
        DATA_DIR_STR "/icon-missing.png"
    };
    for (auto && icon: fallback_icons) {
        try {
            fallback = Gdk::Pixbuf::create_from_file(
                icon,
                icon_size,
                icon_size,
                true
            );
            break;
        } catch (const Glib::Error& e) {
            Log::error("Failed to load fallback icon '", icon, "'");
        }
    }
    if (!fallback) {
        throw std::runtime_error{ "No fallback icon available" };
    }
}

Gtk::Image IconProvider::load_icon(const std::string& icon) const {
    if (icon.empty()) {
        return Gtk::Image{ fallback };
    }
    try {
        if (icon.find_first_of("/") == icon.npos) {
            return Gtk::Image{ icon_theme->load_icon(icon, icon_size, Gtk::ICON_LOOKUP_FORCE_SIZE) };
        } else {
            return Gtk::Image{ Gdk::Pixbuf::create_from_file(icon, icon_size, icon_size, true) };
        }
    } catch (const Glib::Error& error) {
        Log::error("Failed to load icon '", icon, "': ", error.what());
    }
    try {
        return Gtk::Image{ Gdk::Pixbuf::create_from_file("/usr/share/pixmaps/" + icon, icon_size, icon_size, true) };
    } catch (const Glib::Error& error) {
        Log::error("Failed to load icon '", icon, "': ", error.what());
        Log::plain("falling back to placeholder");
    }
    return Gtk::Image{ fallback };
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
    auto get_geo = [&](auto && monitor) {
        Gdk::Rectangle rect;
        monitor->get_geometry(rect);
        geo.x = rect.get_x();
        geo.y = rect.get_y();
        geo.width = rect.get_width();
        geo.height = rect.get_height();
    };
    auto display = window.get_display();

#ifdef GDK_WINDOWING_X11
    // only works on X11, reports 0,0 on wayland
    auto device_mgr = display->get_device_manager();
    auto device = device_mgr->get_client_pointer();
    int x, y;
    device->get_position(x, y);
    if (auto monitor = display->get_monitor_at_point(x, y)) {
        get_geo(monitor);
    } else
#endif
    if (auto monitor = display->get_monitor_at_window(window.get_window())) {
        get_geo(monitor);
    } else {
        throw std::logic_error{ "No monitor at window" };
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
            Log::error("Incorrect layer-shell-layer value");
            std::exit(EXIT_FAILURE);
        }
    }
    if (auto zone = parser.getCmdOption("-layer-shell-exclusive-zone"); !zone.empty()) {
        this->exclusive_zone_is_auto = zone == "auto"sv;
        if (!this->exclusive_zone_is_auto) {
            if (!parse_number(zone, this->exclusive_zone)) {
                Log::error("Unable to decode layer-shell-exclusive-zone value");
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
