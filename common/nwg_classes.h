/*
 * Classes for nwg-launchers
 * Copyright (c) 2020 Érico Nogueira
 * e-mail: ericonr@disroot.org
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#pragma once

#include <string>
#include <vector>

#include <gtkmm.h>
#include <glibmm/ustring.h>

/*
 * Argument parser
 * Credits for this cool class go to iain at https://stackoverflow.com/a/868894
 * */
class InputParser{
    public:
        InputParser (int &, char **);
        /// @author iain
        const std::string& getCmdOption(const std::string &) const;
        /// @author iain
        bool cmdOptionExists(const std::string &) const;

        std::string empty_string{};
    private:
        std::vector <std::string> tokens;
};

extern double opacity;
class CommonWindow : public Gtk::Window {
    public:
        CommonWindow(const Glib::ustring&, const Glib::ustring&);
        virtual ~CommonWindow();

        void check_screen();

    protected:
        bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& cr) override;
        void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) override;
    private:
        bool _SUPPORTS_ALPHA;
};

class AppBox : public Gtk::Button {
    public:
        AppBox();
        AppBox(Glib::ustring, std::string, Glib::ustring);
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
};
