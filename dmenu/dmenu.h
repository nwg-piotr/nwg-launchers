/* GTK-based dmenu
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */
 
#include <iostream>
#include <fstream>
#include <filesystem>
#include <gtkmm.h>
#include "nlohmann/json.hpp"	// nlohmann-json package
#include <glibmm/ustring.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

namespace fs = std::filesystem;
namespace ns = nlohmann;

static Gtk::Label *description;
int num_col (6);				// number of grid columns
int image_size (72);			// button image size in pixels
bool favs (false);				// whether to display favourites
double opacity (0.3);			// overlay window opacity
std::string wm;					// detected window manager name
ns::json cache;
std::string cache_file {};
int entries_limit (25);

#ifndef NWG_DMENU_APP_BOX_H
#define NWG_DMENU_APP_BOX_H
	class AppBox : public Gtk::Button {
		public: 
			AppBox();
			Glib::ustring name;
			Glib::ustring exec;
			Glib::ustring comment;
			
			AppBox(Glib::ustring name, std::string exec, Glib::ustring comment) {
				this -> name = name;
				Glib::ustring n = this -> name;
				if (n.length() > 25) {
					n = this -> name.substr(0, 22) + "...";
				}
				this -> exec = exec;
				this -> comment = comment;
				this -> set_always_show_image(true);
				this -> set_label(n);
			}
			
			virtual ~AppBox();
		
	};
#endif // NWG_DMENU_APP_BOX_H

#ifndef NWG_DMENU_MAIN_WINDOW_H
#define NWG_DMENU_MAIN_WINDOW_H
	class MainWindow : public Gtk::Window {
		public:
			MainWindow();
			void set_visual(Glib::RefPtr<Gdk::Visual> visual);
			virtual ~MainWindow();

			Gtk::SearchEntry searchbox;				// This will stay insensitive, updated with search_phrase value only
			Gtk::Label label_desc;					// To display .desktop entry Comment field at the bottom
			Glib::ustring search_phrase;			// updated on key_press_event
			Gtk::Grid apps_grid;					// All application buttons grid
			Gtk::Grid favs_grid;					// Favourites grid above
			Gtk::Separator separator;				// between favs and all apps
			std::list<AppBox*> all_boxes {};		// attached to apps_grid unfiltered view
			std::list<AppBox*> filtered_boxes {};	// attached to apps_grid filtered view
			std::list<AppBox*> fav_boxes {};		// attached to favs_grid

		private:
			//Override default signal handler:
			bool on_key_press_event(GdkEventKey* event) override;	
			void filter_view();
			void rebuild_grid(bool filtered);
		
		protected:
			virtual bool on_draw(const ::Cairo::RefPtr< ::Cairo::Context>& cr);
			void on_screen_changed(const Glib::RefPtr<Gdk::Screen>& previous_screen);
			bool _SUPPORTS_ALPHA = false;
	};
#endif // NWG_DMENU_MAIN_WINDOW_H
	
#ifndef NWG_DMENU_ENTRY_H
#define NWG_DMENU_ENTRY_H
	struct Entry {
		std::string name;
		std::string exec;
	};
#endif // NWG_DMENU_ENTRY_H
