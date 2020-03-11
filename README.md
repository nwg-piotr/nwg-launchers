# nwg-launchers
As it's never too late to learn something new, I decided to try and code my 
[sgtk-menu](https://github.com/nwg-piotr/sgtk-menu) set of launchers (written in python) from scratch in C++.
By the way I'm trying to simplify their usage, by reducing the number of arguments. Whatever possible, is being moved
to css style sheets.

For now first 2 launchers (and my favourites) seem to work pretty well:

## nwggrid

This command creates a GNOME-like application grid, with the search box, optionally prepended with a row of favourites
(most frequently used apps).

[screenshot](http://nwg.pl/Lychee/uploads/big/93a95e8b221fd1c7a11d213f0ee071ee.png) `nwggrid -f`

### Dependencies

- `gtkmm3`
- `nlohmann-json`

### Installation

```
git clone https://github.com/nwg-piotr/nwg-launchers.git
cd nwg-launchers/grid
sudo make clean install
```
To uninstall:

```
sudo make uninstall
```

### Usage

```
$ nwggrid -h
GTK application grid: nwggrid 0.0.1 (c) Piotr Miller 2020

nwggrid [-h] [-f] [-o <opacity>] [-c <col>] [-s <size>] [-l <ln>]

Options:
-h            show this help message and exit
-f            display favourites
-o <opacity>  background opacity (0.0 - 1.0, default 0.9)
-c <col>      number of grid columns (default: 6)
-s <size>     button image size (default: 72)
-l <ln>       force use of <ln> language
```

### Custom styling

To give the grid more gnomish look, the application uses the `/usr/share/nwggrid/style.css` style sheet. If you'd like
to change something, you may use your own `style.css` file. Replace `~/.config` below with your config home, if necessary.

```
mkdir ~/.config/nwggrid
cp /usr/share/nwggrid/style.css ~/.config/nwggrid
```

Edit the style sheet to your liking.

## nwgbar

This command creates a horizontal or vertical button bar, out of a template file.

[screenshot](http://nwg.pl/Lychee/uploads/big/d84cc934ccf639eb5dc8c01304b155db.png) `nwgbar` as the Exit menu.

### Installation

```
git clone https://github.com/nwg-piotr/nwg-launchers.git
cd nwg-launchers/bar
sudo make clean install
```
To uninstall:

```
sudo make uninstall
```

### Usage

```
$ nwgbar -h
GTK button bar: nwgbar 0.0.1 (c) Piotr Miller 2020

nwgbar [-h] [-v] [-ha <l>|<r>] [-va <t>|<b>] [-t <name>] [-o <opacity>] [-s <size>]

Options:
-h            show this help message and exit
-v            arrange buttons vertically
-ha <l>|<r>   horizontal alignment left/right (default: center)
-va <t>|<b>   vertical alignment top/bottom (default: middle)
-t <name>     template file name (default: bar.json)
-o <opacity>  background opacity (0.0 - 1.0, default 0.9)
-s <size>     button image size (default: 72)
```

### Customization

On first run the program creates the `nwggrid` folder in your .config directory. You'll find a sample template `bar.json`
and the `style.css` files inside.

Templates use json format. The default one defines an example Exit menu for sway window manager on Arch Linux:

```json
[
  {
    "name": "Lock screen",
    "exec": "swaylock -f -c 000000",
    "icon": "system-lock-screen"
  },
  {
    "name": "Logout",
    "exec": "swaymsg exit",
    "icon": "system-log-out"
  },
  {
    "name": "Reboot",
    "exec": "systemctl reboot",
    "icon": "system-reboot"
  },
  {
    "name": "Shutdown",
    "exec": "systemctl -i poweroff",
    "icon": "system-shutdown"
  }
]
```

You may use as many templates as you need, with the `-t` argument. All of them must be placed in the config directory.

The style sheet makes the buttons look similar to `nwggrid`. You can customize them as well.
