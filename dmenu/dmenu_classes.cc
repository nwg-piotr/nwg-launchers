/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * *
 * Credits for window transparency go to AthanasiusOfAlex at https://stackoverflow.com/a/21460337 
 * transparent.cpp
 * Code adapted from 'alphademo.c' by Mike
 * (http://plan99.net/~mike/blog--now a dead link--unable to find it.)
 * as modified by karlphillip for StackExchange:
 * (https://stackoverflow.com/questions/3908565/how-to-make-gtk-window-background-transparent)
 * Re-worked for Gtkmm 3.0 by Louis Melahn, L.C. January 31, 2014.
 * */

MainWindow::MainWindow() : menu(nullptr) {
    set_title("~nwgdmenu");
    set_role("~nwgdmenu");
    //~ set_skip_taskbar_hint(true);
    set_skip_pager_hint(true);
    searchbox.set_text("Type to search");
    searchbox.set_sensitive(false);
    searchbox.set_name("searchbox");
    search_phrase = "";
    apps_grid.set_column_spacing(5);
	apps_grid.set_row_spacing(5);
	apps_grid.set_column_homogeneous(true);
	favs_grid.set_column_spacing(5);
	favs_grid.set_row_spacing(5);
	favs_grid.set_column_homogeneous(true);
	label_desc.set_text("");
	label_desc.set_name("description");
	description = &label_desc;
	separator.set_orientation(Gtk::ORIENTATION_HORIZONTAL);
	separator.set_name("separator");
	add_events(Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	
	set_app_paintable(true);
	
	signal_draw().connect(sigc::mem_fun(*this, &MainWindow::on_draw));
    signal_screen_changed().connect(sigc::mem_fun(*this, &MainWindow::on_screen_changed));
    //~ signal_show().connect(sigc::mem_fun(*this, &MainWindow::on_window_show));
    
    on_screen_changed(get_screen());

    if (wm != "sway") {
		fullscreen();
	}
}

MainWindow::~MainWindow() {
}

gboolean on_window_clicked(GdkEventButton *event) {
	Gtk::Main::quit();
	return true;
}

void on_button_clicked(std::string cmd) {
	cmd = cmd + " &";
	const char *command = cmd.c_str();
	std::system(command);
	
	Gtk::Main::quit();
}

bool MainWindow::on_button_press_event(GdkEventButton* button_event) {
	if((button_event->type == GDK_BUTTON_PRESS) && (button_event->button == 3)) {
		if(!menu->get_attach_widget()) {
		  menu->attach_to_widget(*this);
		}
		if(menu) {
			//~ menu->popup_at_pointer((GdkEvent*)button_event);
			menu -> popup_at_widget(&anchor, Gdk::GRAVITY_CENTER, Gdk::GRAVITY_CENTER, nullptr);
		}
		return true; //It has been handled.
	} else {
		return false;
	}
}

bool MainWindow::on_key_press_event(GdkEventKey* key_event) {
	if (key_event -> keyval == GDK_KEY_Escape) {
		Gtk::Main::quit();
		return true;

	} else if (((key_event -> keyval >= GDK_KEY_A && key_event -> keyval <= GDK_KEY_Z)
			|| (key_event -> keyval >= GDK_KEY_a && key_event -> keyval <= GDK_KEY_z)
			|| (key_event -> keyval >= GDK_KEY_0 && key_event -> keyval <= GDK_KEY_9))
			&& key_event->type == GDK_KEY_PRESS) {

		char character = key_event -> keyval;
		this -> search_phrase += character;

		this -> searchbox.set_text(this -> search_phrase);
		this -> filter_view();
		return true;
		
	} else if (key_event -> keyval == GDK_KEY_BackSpace && this -> search_phrase.size() > 0) {
		this -> search_phrase = this -> search_phrase.substr(0, this -> search_phrase.size() - 1);

		this -> searchbox.set_text(this -> search_phrase);
		this -> filter_view();
		return true;
	}
	//if the event has not been handled, call the base class
	return Gtk::Window::on_key_press_event(key_event);
}

void MainWindow::filter_view() {
	if (this -> search_phrase.size() > 0) {
		this -> separator.hide();
	} else {
		this -> separator.show();
	}
}

bool MainWindow::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    cr->save();
    if (_SUPPORTS_ALPHA) {
        cr->set_source_rgba(0.0, 0.0, 0.0, opacity);    // transparent
    } else {
        cr->set_source_rgb(0.0, 0.0, 0.0);          // opaque
    }
    cr->set_operator(Cairo::OPERATOR_SOURCE);
    cr->paint();
    cr->restore();

    return Gtk::Window::on_draw(cr);
}

void MainWindow::on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen) {
    auto screen = get_screen();
    auto visual = screen->get_rgba_visual();

    if (!visual) {
        std::cout << "Your screen does not support alpha channels!" << std::endl;
    } else {
        _SUPPORTS_ALPHA = TRUE;
    }

    set_visual(visual);
}

void MainWindow::set_visual(Glib::RefPtr<Gdk::Visual> visual) {
    gtk_widget_set_visual(GTK_WIDGET(gobj()), visual->gobj());
}
