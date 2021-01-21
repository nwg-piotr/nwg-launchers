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

#pragma once

#include <string>
#include <vector>
#include <variant>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

struct RGBA {
    double red;
    double green;
    double blue;
    double alpha;
};

/*
 * Argument parser
 * Credits for this cool class go to iain at https://stackoverflow.com/a/868894
 * */
class InputParser{
    public:
        InputParser (int, char **);
        /// @author iain
        std::string_view getCmdOption(std::string_view) const;
        /// @author iain
        bool cmdOptionExists(std::string_view) const;
        RGBA get_background_color(double default_opacity) const;
    private:
        std::vector <std::string_view> tokens;
};

class CommonWindow : public Gtk::Window {
    public:
        CommonWindow(const Glib::ustring&, const Glib::ustring&);
        virtual ~CommonWindow();

        void check_screen();
        void set_background_color(RGBA color);
    protected:
        bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& cr) override;
        void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) override;
    private:
        RGBA background_color;
        bool _SUPPORTS_ALPHA;
};

class AppBox : public Gtk::Button {
    public:
        AppBox();
        AppBox(Glib::ustring, Glib::ustring, Glib::ustring);
        AppBox(AppBox&&) = default;
        AppBox(const AppBox&) = delete;

        Glib::ustring name;
        Glib::ustring exec;
        Glib::ustring comment;

        virtual ~AppBox();
};

/*
 * Stores x, y, width, height
 * */
struct Geometry {
    int x;
    int y;
    int width;
    int height;
};

struct DesktopEntry {
    std::string name;
    std::string exec;
    std::string icon;
    std::string comment;
    std::string mime_type;
    bool terminal;
};

enum class SwayError {
    ConnectFailed,
    EnvNotSet,
    OpenFailed,
    RecvHeaderFailed,
    RecvBodyFailed,
    SendHeaderFailed,
    SendBodyFailed
};

struct SwaySock {
    SwaySock();
    SwaySock(const SwaySock&) = delete;
    ~SwaySock();
    // pass the command to sway via socket
    template <typename ... Ts>
    void run(Ts ... ts) {
        auto body_size = (ts.size() + ...);
        send_header_(body_size, Commands::Run);
        (send_body_(ts), ...);
        // should we recv the response?
        // suppress warning
        (void)recv_response_();
    }
    // swaymsg -t get_outputs
    std::string get_outputs();
    std::string get_workspaces();
    
    // see sway-ipc (7)
    enum class Commands: std::uint32_t {
        Run = 0,
        GetWorkspaces = 1,
        GetOutputs = 3
    };
    static constexpr std::array MAGIC { 'i', '3', '-', 'i', 'p', 'c' };
    static constexpr auto MAGIC_SIZE = MAGIC.size();
    // magic + body length (u32) + type (u32)
    static constexpr auto HEADER_SIZE = MAGIC_SIZE + 2 * sizeof(std::uint32_t);
    
    int                           sock_;
    std::array<char, HEADER_SIZE> header;

    void send_header_(std::uint32_t, Commands);
    void send_body_(std::string_view);
    std::string recv_response_();
};

struct GenericShell {
    Geometry geometry(Gtk::Window& window) {
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
    void show(Gtk::Window& window) {
        window.show();
        window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
        window.fullscreen();
    }
};

struct SwayShell: GenericShell {
    SwayShell(Gtk::Window& window): title_{window.get_title().c_str(), window.get_title().bytes()}
    {
        window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
        window.set_decorated(false);
    }
    SwaySock         sock_;
    std::string_view title_;
    void show(Gtk::Window& window) {
        // We can not go fullscreen() here:
        // On sway the window would become opaque - we don't want it
        // On i3 all windows below will be hidden - we don't want it as well
        using namespace std::string_view_literals;
        sock_.run("for_window [title="sv, title_, "*] floating enable"sv);
        sock_.run("for_window [title="sv, title_, "*] border none"sv);
        window.show();
        // works just fine on Sway/i3 as far as I could test
        // thus, no need to use ipc (I hope)
        auto [x, y, w, h] = geometry(window);
        window.resize(w, h);
        window.move(x, y);
    }
};

struct LayerShell: GenericShell {
#ifdef HAVE_GTK_LAYER_SHELL
    LayerShell(Gtk::Window& window) {
        // this has to be called before the window is realized
        gtk_layer_init_for_window(window.gobj());
    }
    void show(Gtk::Window& window) {
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
        gtk_layer_set_namespace(gtk_win, "nwggrid");
        gtk_layer_set_exclusive_zone(gtk_win, -1);        
    }
#endif
};

struct Platform {
    Platform(Gtk::Window& window_, std::string_view wm):
        shell{std::in_place_type_t<GenericShell>{}},
        window{window_}
    {
#ifdef HAVE_GTK_LAYER_SHELL
        if (gtk_layer_is_supported()) {
            shell.emplace<LayerShell>(window);
            return;
        }
#endif
        if (wm == "sway" || wm == "i3") {
            shell.emplace<SwayShell>(window);
        }
    }
    void show() {
        std::visit([&](auto& shell){ shell.show(window); }, shell);
    }
    std::variant<LayerShell, SwayShell, GenericShell> shell;
    Gtk::Window& window;
};

