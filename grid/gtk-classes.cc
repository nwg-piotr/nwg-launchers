/* GTK-based application grid
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

MainWindow::MainWindow() {
    set_title("~nwggrid");
    set_role("~nwggrid");
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
    
    on_screen_changed(get_screen());
}

MainWindow::~MainWindow() {
}

gboolean on_window_clicked(GdkEventButton *event) {
	Gtk::Main::quit();
	return true;
}

AppBox::AppBox() {
	//~ this -> set_always_show_image(true);
	//~ this -> set_label(this -> name);
}

AppBox::~AppBox() {
}

void on_button_clicked(std::string cmd) {
	int clicks = 0;
	try {
		clicks = cache[cmd];
		clicks++;
	} catch (...) {
		clicks = 1;
	}
	cache[cmd] = clicks;
	save_json(cache, cache_file);

	cmd = cmd + " &";
	const char *command = cmd.c_str();
	std::system(command);
	
	Gtk::Main::quit();
}

bool on_button_entered(GdkEventCrossing* event, Glib::ustring comment) {
	description -> set_text(comment);
	return true;
}

bool on_button_focused(GdkEventFocus* event, Glib::ustring comment) {
	description -> set_text(comment);
	return true;
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
		this -> filtered_boxes = {};
		
		for (AppBox* box : this -> all_boxes) {	
			if (box -> name.uppercase().find(this -> search_phrase.uppercase()) != std::string::npos
				|| box -> exec.uppercase().find(this -> search_phrase.uppercase()) != std::string::npos) {

				this -> filtered_boxes.push_back(box);
			}
		}
		this -> rebuild_grid(true);
		this -> favs_grid.hide();
		this -> separator.hide();
	} else {
		this -> rebuild_grid(false);
		this -> favs_grid.show();
		this -> separator.show();
	}
}

void MainWindow::rebuild_grid(bool filtered) {
	int column = 0;
	int row = 0;
	
	for (Gtk::Widget *widget : this -> apps_grid.get_children()) {
		this -> apps_grid.remove(*widget);
		widget -> unset_state_flags(Gtk::STATE_FLAG_PRELIGHT);
	} 
	int cnt = 0;
	if (filtered) {
		for(AppBox* box : this -> filtered_boxes) {
			this -> apps_grid.attach(*box, column, row, 1, 1);
			if (column < num_col - 1) {
				column++;
			} else {
				column = 0;
				row++;
			}
			cnt++;
		}
	} else {
		for(AppBox* box : this -> all_boxes) {
			this -> apps_grid.attach(*box, column, row, 1, 1);
			if (column < num_col - 1) {
				column++;
			} else {
				column = 0;
				row++;
			}
			cnt++;
		}
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
