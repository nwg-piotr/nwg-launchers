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

#pragma once

#include <string>
#include <vector>
#include <variant>

#include <gtkmm.h>
#include <glibmm/ustring.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell.h>
#endif

#include "filesystem-compat.h"

template <typename ... Os>
struct Overloaded: Os... { using Os::operator()...; };
template <typename ... Os> Overloaded(Os ...) -> Overloaded<Os...>;

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

#ifdef HAVE_GTK_LAYER_SHELL
struct LayerShellArgs {
    GtkLayerShellLayer layer                  = GTK_LAYER_SHELL_LAYER_OVERLAY;
    int                exclusive_zone         = -1;
    bool               exclusive_zone_is_auto = true;
    
    LayerShellArgs(const InputParser& parser);
};
#endif

enum class HAlign: unsigned int { NotSpecified = 0, Left, Right };
enum class VAlign: unsigned int { NotSpecified = 0, Top, Bottom };

/* 
 * Stores configuration data
 */
struct Config {
    const InputParser& parser;
    std::string        wm;
    std::string_view   title;
    std::string_view   role;
    HAlign             halign{ HAlign::NotSpecified };
    VAlign             valign{ VAlign::NotSpecified };
    fs::path           css_filename{ "style.css" };   // filename relative to config dir
    
#ifdef HAVE_GTK_LAYER_SHELL
    LayerShellArgs layer_shell_args;
#endif

    Config(const InputParser&, std::string_view, std::string_view, const Glib::RefPtr<Gdk::Screen>&);
};

class CommonWindow : public Gtk::Window {
    public:
        CommonWindow(Config&);
        virtual ~CommonWindow() = default;

        void check_screen();
        void set_background_color(RGBA color);
        
        virtual int get_height(); // we need to override get_height for dmenu to work
        
        std::string_view title_view();
    protected:
        bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& cr) override;
        void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) override;
    private:
        std::string_view title;
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

        virtual ~AppBox() = default;
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

struct Instance {
    Gtk::Application& app;
    fs::path pid_file;
    int      pid_lock_fd;

    Instance(Gtk::Application& app, std::string_view name);
    virtual ~Instance();
    // note: the provided implementation of on_{sigterm,sigint} handlers
    // calls Gtk::Application::quit, which does NOT call any destructors
    virtual void on_sigterm();
    virtual void on_sigusr1();
    virtual void on_sighup();
    virtual void on_sigint();
};

struct IconProvider {
    Glib::RefPtr<Gtk::IconTheme> icon_theme;
    Glib::RefPtr<Gdk::Pixbuf>    fallback;
    int                          icon_size;

    IconProvider(const Glib::RefPtr<Gtk::IconTheme>& theme, int icon_size);
    // Returns Gtk::Image out of the icon name of file path
    // the returned image is scaled to icon_size x icon_size
    Gtk::Image load_icon(const std::string& icon) const;
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
        // should we send it as one message? 1 write() is better than N
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

/*
 * This namespace defines types that can be passed to PlatformWindow::show
 * The rationale behind this design is as follows:
 * 1) Logic implementing all the positioning is collected in one place,
 *      not scattered across classes / spagetti functions
 * 2) It is resolved at compile time, allowing compiler to optimize it better
 * 3) It can be tweaked and expanded with ease and safety (everything is checked at compile-time)
 */
namespace hint {
    constexpr struct Fullscreen_ {} Fullscreen;
    constexpr struct Center_     {} Center;
    struct Horizontal{};
    struct Vertical{};
    template <typename S>
    struct Side { bool side; int margin; constexpr static S side_type{}; };
    struct Sides { Side<Horizontal> h; Side<Vertical> v; };
}

/*
 * Each shell defined by which means window is positioned. They do not share common base class,
 *  but instead wrapped in a variant to avoid unecessary dynamic allocations
 * Shell::show method receives a window reference and a templated parameter,
 *   it's job is to position window according to the parameter type
 * GenericShell only uses common Gtk functions and is best used on X11 or as a fallback
 * SwayShell uses IPC connection to Sway/i3
 * LayerShell uses wlr-layer-shell (or rather gtk-layer-shell library built on top of it)
 */
struct GenericShell {
    GenericShell(Config& config);
    Geometry geometry(CommonWindow& window);
    template <typename S> void show(CommonWindow&, S);
    // some window managers (openbox, notably) do not open window in fullscreen
    // when requested
    bool respects_fullscreen = true;
};

struct SwayShell: GenericShell {
    SwayShell(CommonWindow& window, Config& config);
    // use GenericShell::show unless called with Fullscreen
    using GenericShell::show;
    void show(CommonWindow& window, hint::Fullscreen_);

    SwaySock sock_;
};

#ifdef HAVE_GTK_LAYER_SHELL
struct LayerShell {
    LayerShell(CommonWindow& window, LayerShellArgs args);
    template <typename S> void show(CommonWindow& window, S);
    
    LayerShellArgs args;
};
#endif

struct PlatformWindow: public CommonWindow {
public:
    PlatformWindow(Config& config);
    void fullscreen();
    template <typename S> void show(S);
private:
    std::variant<
#ifdef HAVE_GTK_LAYER_SHELL
                 LayerShell,
#endif
                 SwayShell, GenericShell> shell;
};

template <typename Hint>
void GenericShell::show(CommonWindow& window, Hint hint) {
    window.show();
    window.set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);
    window.set_decorated(false);
    auto display = geometry(window);
    auto window_coord_at_side = [](auto d_size, auto w_size, auto side, auto margin) {
        std::array map { margin, d_size - w_size - margin };
        return map[side];
    };
    Overloaded place_window {
        [&](hint::Center_) {
            auto x = display.x + (display.width - window.get_width()) / 2;
            auto y = display.y + (display.height - window.get_height()) / 2;
            window.move(x, y);
        },
        [&](hint::Fullscreen_) {
            if (this->respects_fullscreen) {
                window.fullscreen();
            } else {
                window.resize(display.width, display.height);
                window.move(display.x, display.y);
            }
        },
        [&](hint::Side<hint::Horizontal> hint) {
            auto w_x = window_coord_at_side(display.width, window.get_width(), hint.side, hint.margin);
            window.move(display.x + w_x, display.y + (display.height - window.get_height()) / 2);
        },
        [&](hint::Side<hint::Vertical> hint) {
            auto w_y = window_coord_at_side(display.height, window.get_height(), hint.side, hint.margin);
            window.move(display.x + (display.width - window.get_width()) / 2, display.y + w_y);
        },
        [&,display](hint::Sides hint) {
            auto w_x = window_coord_at_side(display.width, window.get_width(), hint.h.side, hint.h.margin);
            auto w_y = window_coord_at_side(display.height, window.get_height(), hint.v.side, hint.v.margin);
            window.move(display.x + w_x, display.y + w_y);
        }
    };
    place_window(hint);
    window.present();   // grab focus
}

#ifdef HAVE_GTK_LAYER_SHELL
template <typename Hint>
void LayerShell::show(CommonWindow& window, Hint hint) {
    std::array<bool, 4> edges{ 0, 0, 0, 0 };
    std::array<int,  4> margins{ 0, 0, 0, 0 };
    constexpr Overloaded index { [](hint::Horizontal){ return 0; }, [](hint::Vertical) { return 2; } };
    auto account_side = [&](auto side) {
        constexpr auto i = index(side.side_type);
        edges[i + side.side] = true;
        margins[i + side.side] = side.margin;
    };
    Overloaded set_edges_margins {
        [&](hint::Center_) { /* nothing to do */ },
        [&](hint::Fullscreen_) { edges = { 1, 1, 1, 1 }; },
        [&](hint::Sides hint) { account_side(hint.h); account_side(hint.v); },
        account_side
    };
    set_edges_margins(hint);
    window.show();
    auto gtk_win = window.gobj();
    std::array edges_ {
        GTK_LAYER_SHELL_EDGE_LEFT,
        GTK_LAYER_SHELL_EDGE_RIGHT,
        GTK_LAYER_SHELL_EDGE_TOP,
        GTK_LAYER_SHELL_EDGE_BOTTOM
    };
    for (size_t i = 0; i < 4; i++) {
        gtk_layer_set_anchor(gtk_win, edges_[i], edges[i]);
        gtk_layer_set_margin(gtk_win, edges_[i], margins[i]);
    }
    gtk_layer_set_layer(gtk_win, args.layer);
    gtk_layer_set_keyboard_interactivity(gtk_win, true);
    gtk_layer_set_namespace(gtk_win, window.title_view().data());
    if (args.exclusive_zone_is_auto) {
        gtk_layer_auto_exclusive_zone_enable (gtk_win);
    } else {
        gtk_layer_set_exclusive_zone(gtk_win, args.exclusive_zone);
    }
}
#endif

template <typename Hint>
void PlatformWindow::show(Hint h) {
    std::visit([&](auto& shell){ shell.show(*this, h); }, shell);
}


