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
        CommonWindow(std::string_view, std::string_view);
        virtual ~CommonWindow();

        void check_screen();
        void set_background_color(RGBA color);
        
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

struct GenericShell {
    Geometry geometry(CommonWindow& window);
    void show(CommonWindow& window);
};

struct SwayShell: GenericShell {
    SwayShell(CommonWindow& window);
    void show(CommonWindow& window);
    SwaySock sock_;
};

struct LayerShell: GenericShell {
#ifdef HAVE_GTK_LAYER_SHELL
    LayerShell(CommonWindow& window);
    void show(CommonWindow& window);
#endif
};

struct PlatformWindow: public CommonWindow {
public:
    PlatformWindow(std::string_view, std::string_view, std::string_view);
    void show();
private:
    std::variant<LayerShell, SwayShell, GenericShell> shell;
};

