/*
 * GTK-based application grid
 * Copyright (c) 2020 Piotr Miller
 * e-mail: nwg.piotr@gmail.com
 * Website: http://nwg.pl
 * Project: https://github.com/nwg-piotr/nwg-launchers
 * License: GPL3
 * */

#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>

#include <charconv>

#include "nwg_tools.h"
#include "nwg_classes.h"
#include "on_event.h"
#include "grid.h"

bool pins = false;              // whether to display pinned
bool favs = false;              // whether to display favorites
std::string wm {""};            // detected or forced window manager name
std::string term {""};
std::size_t num_col = 6;        // number of grid columns

std::filesystem::path pinned_file;
std::filesystem::path cache_file;

const char* const HELP_MESSAGE =
"GTK application grid: nwggrid " VERSION_STR " (c) 2020 Piotr Miller, Sergey Smirnykh & Contributors \n\n\
\
Options:\n\
-h               show this help message and exit\n\
-f               display favourites (most used entries); does not work with -d\n\
-p               display pinned entries; does not work with -d \n\
-d               look for .desktop files in custom paths (-d '/my/path1:/my/another path:/third/path') \n\
-o <opacity>     default (black) background opacity (0.0 - 1.0, default 0.9)\n\
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)\n\
-n <col>         number of grid columns (default: 6)\n\
-s <size>        button image size (default: 72)\n\
-c <name>        css file name (default: style.css)\n\
-l <ln>          force use of <ln> language\n\
-wm <wmname>     window manager name (if can not be detected)\n";

int main(int argc, char *argv[]) {
    std::filesystem::path custom_css_file{ "style.css" };

    struct timeval tp;
    gettimeofday(&tp, NULL);
    long int start_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    create_pid_file_or_kill_pid("nwggrid");

    std::string lang ("");

    InputParser input(argc, argv);
    if (input.cmdOptionExists("-h")){
        std::cout << HELP_MESSAGE;
        std::exit(0);
    }
    favs = input.cmdOptionExists("-f") && !input.cmdOptionExists("-d");
    pins = input.cmdOptionExists("-p") && !input.cmdOptionExists("-d");
    if (auto forced_lang = input.getCmdOption("-l"); !forced_lang.empty()){
        lang = forced_lang;
    } else {
        lang = get_locale();
    }
    std::cout << "Locale: " << lang << "\n";
    
    if (auto cols = input.getCmdOption("-n"); !cols.empty()) {
        int n_c;
        auto [p, ec] = std::from_chars(cols.data(), cols.data() + cols.size(), n_c);
        if (ec == std::errc()) {
            if (n_c > 0 && n_c < 100) {
                num_col = n_c;
            } else {
                std::cerr << "\nERROR: Columns must be in range 1 - 99\n\n";
            }
        } else {
            std::cerr << "\nERROR: Invalid number of columns\n\n";
        }
    }

    if (auto css_name = input.getCmdOption("-c"); !css_name.empty()){
        custom_css_file = css_name;
    }
    
    if (auto wm_name = input.getCmdOption("-wm"); !wm_name.empty()){
        wm = wm_name;
    } else {
        wm = detect_wm();
    }
    std::cout << "WM: " << wm << "\n";

    auto background_color = input.get_background_color(0.9);

    auto i_size = input.getCmdOption("-s");
    if (!i_size.empty()){
        int i_s;
        auto [p, ec] = std::from_chars(i_size.data(), i_size.data() + i_size.size(), i_s);
        if (ec == std::errc()) {
            if (i_s >= 16 && i_s <= 256) {
                image_size = i_s;
            } else {
                std::cerr << "\nERROR: Size must be in range 16 - 256\n\n";
            }
        } else {
            std::cerr << "\nERROR: Invalid image size\n\n";
        }
    }


    auto cache_home = get_cache_home();
    // This will be read-only, to find n most clicked items (n = number of grid columns)
    std::vector<CacheEntry> favourites;
    if (favs) {
        cache_file = cache_home / "nwg-fav-cache";
        try {
            auto cache = json_from_file(cache_file);
            if (cache.size() > 0) {
                std::cout << cache.size() << " cache entries loaded\n";
            } else {
                std::cout << "No cache entries loaded\n";
            }
            auto n = std::min(num_col, cache.size());
            favourites = get_favourites(std::move(cache), n);
        }  catch (...) {
            // TODO: only save cache if favs were changed
            std::cerr << "Failed to read cache file '" << cache_file << "'\n";
        }
    }

    std::vector<std::string> pinned;
    if (pins) {
        pinned_file = cache_home / "nwg-pin-cache";
        pinned = get_pinned(pinned_file);
        if (pinned.size() > 0) {
            std::cout << pinned.size() << " pinned entries loaded\n";
        } else {
            std::cout << "No pinned entries found\n";
        }
    }

    auto config_dir = get_config_dir("nwggrid");
    if (!fs::is_directory(config_dir)) {
        std::cout << "Config dir not found, creating...\n";
        fs::create_directories(config_dir);
    }

    term = get_term(config_dir.native());

    // default and custom style sheet
    auto default_css_file = config_dir / "style.css";
    // css file to be used
    auto css_file = config_dir / custom_css_file;
    // copy default file if not found
    if (!fs::exists(default_css_file)) {
        try {
            fs::copy_file(DATA_DIR_STR "/nwggrid/style.css", default_css_file, fs::copy_options::overwrite_existing);
        } catch (...) {
            std::cerr << "ERROR: Failed copying default style.css\n";
        }
    }

    std::vector<std::filesystem::path> dirs;
    if (auto special_dirs = input.getCmdOption("-d"); !special_dirs.empty()) {
        using namespace std::string_view_literals;
        // use special dirs specified with -d argument (feature request #122)
        auto dirs_ = split_string(special_dirs, ":");
        std::cout << "\nUsing custom .desktop files path(s):\n";
        std::array status { "' [INVALID]\n"sv, "' [OK]\n"sv };
        for (auto && dir: dirs_) {
            std::error_code ec;
            auto is_dir = std::filesystem::is_directory(dir, ec) && !ec;
            std::cout << '\'' << dir << status[is_dir];
            if (is_dir) {
                dirs.emplace_back(dir);
            }
        }
    } else {
        // get all applications dirs
        dirs = get_app_dirs();
    }

    gettimeofday(&tp, NULL);
    long int commons_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    // Maps desktop-ids to their table indices, nullopt stands for 'hidden'
    std::unordered_map<std::string, std::optional<std::size_t>> desktop_ids;

    // Table, only contains shown entries
    std::vector<DesktopEntry> desktop_entries;
    std::vector<std::string>  execs;
    std::vector<Stats>        stats;
    std::vector<Gtk::Image*>  images;

    auto desktop_id = [](auto& path) {
        return path.string(); // actual desktop_id requires '/' to be replaced with '-'
    };

    for (auto& dir : dirs) {
        std::error_code ec;
        auto dir_iter = fs::directory_iterator(dir, ec);
        for (auto& entry : dir_iter) {
            if (ec) {
                std::cerr << ec.message() << '\n';
                ec.clear();
                continue;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            auto& path = entry.path();
            auto&& rel_path = path.lexically_relative(dir);
            auto&& id = desktop_id(rel_path);
            if (auto [at, inserted] = desktop_ids.try_emplace(id, std::nullopt); inserted) {
                if (auto entry = desktop_entry(path, lang)) {
                    at->second = execs.size(); // set index
                    execs.emplace_back(entry->exec);
                    desktop_entries.emplace_back(std::move(*entry));
                    stats.emplace_back(0, 0, Stats::Common, Stats::Unpinned);
                    images.emplace_back(nullptr);
                }
            }
        }
    }

    int pin_index = 0; // preserve pins order
    for (auto& pin : pinned) {
        if (auto result = desktop_ids.find(pin); result != desktop_ids.end() && result->second) {
            stats[*result->second].pinned = Stats::Pinned;
            stats[*result->second].position = pin_index;
            pin_index++;
        }
    }
    for (auto& [fav, clicks] : favourites) {
        if (auto result = desktop_ids.find(fav); result != desktop_ids.end() && result->second) {
            stats[*result->second].clicks   = clicks;
            stats[*result->second].favorite = Stats::Favorite;
        }
    }

    gettimeofday(&tp, NULL);
    long int bs_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    auto app = Gtk::Application::create();

    auto provider = Gtk::CssProvider::create();
    auto display = Gdk::Display::get_default();
    auto screen = display->get_default_screen();
    if (!provider || !display || !screen) {
        std::cerr << "ERROR: Failed to initialize GTK\n";
        return EXIT_FAILURE;
    }
    Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_USER);
    auto icon_theme = Gtk::IconTheme::get_for_screen(screen);
    if (!icon_theme) {
        std::cerr << "ERROR: Failed to load icon theme\n";
        std::exit(EXIT_FAILURE);
    }
    auto& icon_theme_ref = *icon_theme.get();
    auto icon_missing = Gdk::Pixbuf::create_from_file(DATA_DIR_STR "/nwgbar/icon-missing.svg");

    if (!std::filesystem::is_regular_file(css_file)) {
        css_file = default_css_file;
    }
    provider->load_from_path(css_file);
    std::cout << "Using " << css_file << '\n';

    MainWindow window(execs, stats);
    window.set_background_color(background_color);
    window.show();

    gettimeofday(&tp, NULL);
    long int images_ms  = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    // The most expensive part
    for (std::size_t i = 0; i < desktop_entries.size(); i++) {
        images[i] = app_image(icon_theme_ref, desktop_entries[i].icon, icon_missing);
    }

    gettimeofday(&tp, NULL);
    long int boxes_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    for (auto& [desktop_id, pos_] : desktop_ids) {
        if (pos_) {
            auto pos = *pos_;
            auto& entry = desktop_entries[pos];
            auto& ab = window.emplace_box(std::move(entry.name),
                                          std::move(entry.comment),
                                          desktop_id,
                                          pos);
            ab.set_image_position(Gtk::POS_TOP);
            ab.set_image(*images[pos]);
        }
    }

    gettimeofday(&tp, NULL);
    long int grids_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    window.build_grids();

    gettimeofday(&tp, NULL);
    long int end_ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    auto format = [&cout=std::cout](auto&& title, auto from, auto to) {
        cout << title << to - from << "ms\n";
    };
    format("Total: ", start_ms, end_ms);
    format("\tgrids:   ", grids_ms, end_ms);
    format("\tboxes:   ", boxes_ms, grids_ms);
    format("\timages:  ", images_ms, boxes_ms);
    format("\tbs:      ", bs_ms, images_ms);
    format("\tcommons: ", commons_ms, bs_ms);

    return app->run(window);
}
