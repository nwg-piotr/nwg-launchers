# nwg-launchers
As it's never too late to learn something new, I decided to try and code my 
[sgtk-menu](https://github.com/nwg-piotr/sgtk-menu) set of launchers, written in python, from scratch in C++.
By the way I'm trying to simplify their usage, by reducing the number of arguments. Whatever possible, is being moved
to css style sheets.

For now the first launcher seems to work pretty good:

## nwggrid

This command creates a GNOME-like application grid, with the search box, optionally prepended with one row of favourites
(most frequently used) apps.

[screenshot](http://nwg.pl/Lychee/uploads/big/93a95e8b221fd1c7a11d213f0ee071ee.png)

### Dependencies

- `gtkmm3`
- `nlohmann-json`

### Installation

```shell
git clone https://github.com/nwg-piotr/nwg-launchers.git
cd nwg-launchers/grid
sudo make clean install
```
To uninstall:

```shell
sudo make uninstall
```

### Usage

```shell
$ nwggrid -h
GTK application grid: nwggrid 0.0.1 (c) Piotr Miller 2020

Options:
-h            show this help message and exit
-f            display favourites
-o <opacity>  background opacity (0.0 - 1.0, default 0.9)
-c <col>      number of grid columns (default: 6)
-s <size>     button image size (default: 72)
-l <ln>       force use of <ln> language
```

### Custom styling

To give the grid more gnomish look, the application uses the `/usr/share/nwggrid/style.css` style sheet. If you'd' like
to change something, you may use your own `style.css` file. Replace `~/.config` below with your config home, if necessary.

```shell
mkdir ~/.config/nwggrid
cp /usr/share/nwggrid/style.css ~/.config/nwggrid
```

Edit the style sheet to your liking.

### TODO

- prevent from launching multiple instances;
- detect display geometry not in sway only: at the moment FVWM always places the grid on primary display.
