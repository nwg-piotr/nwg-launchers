# nwg-launchers
As it's never too late to learn something new, I decided to try and code my [sgtk-menu](https://github.com/nwg-piotr/sgtk-menu), written in python, from scratch in C++. For now the first one seems to work pretty good.

## nwggrid

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
