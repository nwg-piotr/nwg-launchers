# nwg-launchers

[![Build Status](https://github.com/nwg-piotr/nwg-launchers/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/nwg-piotr/nwg-launchers/actions/workflows/ubuntu.yml) 
[![Build Status](https://github.com/nwg-piotr/nwg-launchers/actions/workflows/freebsd.yml/badge.svg)](https://github.com/nwg-piotr/nwg-launchers/actions/workflows/freebsd.yml)

## This project is community-driven

As it seems I'm not going to live long enough to learn C++ properly, I decided to develop my launchers from scratch in Go. You'll find them in the
[nwg-shell](https://github.com/nwg-piotr/nwg-shell) project. They only support sway, and partially other wlroots-based compositors. The nwg-launchers code
is a community-driven project from now on, if it comes to further development. I'll only support bug fixes, if any.

## Description

It's damned difficult to make all the stuff behave properly on all window managers. My priorities are:

1. it **must work well** on sway;
2. it **should work as well as possible** on Wayfire, i3, dwm and Openbox.

Feel free to report issues you encounter on other window managers / desktop environments, but they may or may not be resolved.

## Packages

The latest released version is [available](https://aur.archlinux.org/packages/nwg-launchers) in Arch User Repository.
Current development version (`master` branch) may be installed as the `nwg-launchers-git` AUR package.
For other Linux distributions see the table below.

[![Packaging status](https://repology.org/badge/vertical-allrepos/nwg-launchers.svg)](https://repology.org/project/nwg-launchers/versions)

## Building and installing

To build nwg-launchers from source, you need a copy of the source code, which
can be obtained by cloning the repository or by downloading and unpacking [the
latest release](https://github.com/nwg-piotr/nwg-launchers/releases/latest).

### Dependencies

- `gtkmm3` (`libgtkmm-3.0-dev`)
- `meson` and `ninja` - build dependencies
- `nlohmann-json` - will be downloaded as a subproject if not found on the system
- `gtk-layer-shell` - optional, set to `auto` by default; will be downloaded as a subproject if explicitly enabled, but not found on the system
- `librsvg` - optional, required to support SVG icons

### Building

This project uses the Meson build system for building and installing the
executables and the necessary data. The options that can be passed to the
`meson` command can be found in the `meson_options.txt` file, and can be used to
disable building some of the available programs.

```
$ git clone https://github.com/nwg-piotr/nwg-launchers.git
$ cd nwg-launchers
$ meson builddir -Dbuildtype=release
$ ninja -C builddir
```

### Installation

To install:

```
$ sudo ninja -C builddir install
```

To uninstall:

```
$ sudo ninja -C builddir uninstall
```

**Note: the descriptions below apply to the `master` branch. Certain features may or may not be available in the latest
release, as well as in the current package for your Linux distribution.**

## nwggrid

This command creates a GNOME-like application grid, with the search box, optionally prepended with a row of `-f` favourites
(most frequently used apps) or `-p` pinned program icons.

This only works with the `-p` argument:

- to pin up a program icon, right click its icon in the applications grid;
- to unpin a program, right click its icon in the pinned programs grid.

*Hit "Delete" key to clear the search box.*

[![Swappshot-Mon-Mar-23-205030-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-205030-2020.th.png)](https://scrot.cloud/image/jb3k) [![Swappshot-Mon-Mar-23-205157-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-205157-2020.th.png)](https://scrot.cloud/image/jOWg) [![Swappshot-Mon-Mar-23-205248-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-205248-2020.th.png)](https://scrot.cloud/image/joh5)

Starting with version 0.6.0 nwggrid can be run in server mode which drastically improves responsiveness.
First, start a server with `nwggrid-server` command.
When it's up and running, run `nwggrid -client` to show the grid.

### Usage

```
$ nwggrid -h
GTK application grid: nwggrid 0.6.0 (c) 2021 Piotr Miller, Sergey Smirnykh & Contributors

Usage:
    nwggrid -client      sends -SIGUSR1 to nwggrid-server, requires nwggrid-server running
    nwggrid [ARGS...]    launches nwggrid-server -oneshot ARGS...

See also:
    nwggrid-server -h
```

```
$ nwggrid-server -h
GTK application grid: nwggrid 0.6.0 (c) 2021 Piotr Miller, Sergey Smirnykh & Contributors

Options:
-h               show this help message and exit
-f               display favourites (most used entries); does not work with -d
-p               display pinned entries; does not work with -d
-d               look for .desktop files in custom paths (-d '/my/path1:/my/another path:/third/path')
-o <opacity>     default (black) background opacity (0.0 - 1.0, default 0.9)
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)
-n <col>         number of grid columns (default: 6)
-s <size>        button image size (default: 72)
-c <name>        css file name (default: style.css)
-l <ln>          force use of <ln> language
-wm <wmname>     window manager name (if can not be detected)
-oneshot         run in the foreground, exit when window is closed
                 generally you should not use this option, use simply `nwggrid` instead
[requires layer-shell]:
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},         default: OVERLAY
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto
```

### Terminal applications

`.desktop` files with the `Terminal=true` line should be started in a terminal emulator. There's no common method
to determine which terminal to use. The `nwggrid` command since v0.4.1 at the first run will look for installed
terminals in the following order: alacritty, kitty, urxvt, foot, lxterminal, sakura, st, termite, terminator, xfce4-terminal,
gnome-terminal. The name of the first one found will be saved to the `~/.config/nwg-launchers/nwggrid/terminal` file.
If none of above is found, the fallback `xterm` value will be saved, regardless of whether xterm is installed or not.
You may edit the `term` file to use another terminal.

### Custom background

Use -b <RRGGBB> | <RRGGBBAA> argument (w/o #) to define custom background colour. If alpha value given, it overrides
the opacity, as well default, as defined with the -o <opacity> argument.

### Custom styling

On first run the program creates the `nwg-launchers/nwggrid` folder in your .config directory. You'll find the `style.css` files inside.
You may edit the style sheet to your liking.

## nwgbar

This command creates a horizontal or vertical button bar, out of a template file.

[![Swappshot-Mon-Mar-23-210713-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-210713-2020.th.png)](https://scrot.cloud/image/jRPQ) [![Swappshot-Mon-Mar-23-210652-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-210652-2020.th.png)](https://scrot.cloud/image/j8LU)

### Usage

```
$ nwgbar -h
GTK button bar: nwgbar 0.6.0 (c) Piotr Miller & Contributors 2021

Options:
-h               show this help message and exit
-v               arrange buttons vertically
-ha <l>|<r>      horizontal alignment left/right (default: center)
-va <t>|<b>      vertical alignment top/bottom (default: middle)
-t <name>        template file name (default: bar.json)
-c <name>        css file name (default: style.css)
-o <opacity>     background opacity (0.0 - 1.0, default 0.9)
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)
-s <size>        button image size (default: 72)
-wm <wmname>     window manager name (if can not be detected)

[requires layer-shell]:
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},        default: OVERLAY
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto
```

### Custom background

Use -b <RRGGBB> | <RRGGBBAA> argument (w/o #) to define custom background colour. If alpha value given, it overrides
the opacity, as well default, as defined with the -o <opacity> argument.

### Customization

On first run the program creates the `nwg-launchers/nwgbar` folder in your .config directory. You'll find a sample
template `bar.json` and the `style.css` files inside.

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

To set a keyboard shortcut (using Alt+KEY) for an entry, you can add an underscore before the letter you want to use.
Example to set `s` as the shortcut:

```
[
...
  {
    "name": "Lock _screen",
    "exec": "swaylock -f -c 000000",
    "icon": "system-lock-screen"
  }
...
]
```

**Note for underscore ("_")**

If you want to use an underscore in the name, you have to double it ("__").

**Wayfire note**

For the Logout button, as in the bar above, you may use [wayland-logout](https://github.com/soreau/wayland-logout) by @soreau.

You may use as many templates as you need, with the `-t` argument. All of them must be placed in the config directory.
You may use own icon files instead of icon names, like `/path/to/the/file/my_icon.svg`.

The style sheet makes the buttons look similar to `nwggrid`. You can customize them as well.

## nwgdmenu

This program provides 2 commands:

- `<input> | nwgdmenu` - displays newline-separated stdin input as a GTK menu
- `nwgdmenu` - creates a GTK menu out of commands found in $PATH

*Hit "Delete" to clear the search box.*
*Hit "Insert" to switch case sensitivity.*

[![Swappshot-Mon-Mar-23-211702-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-211702-2020.th.png)](https://scrot.cloud/image/jfHK) [![Swappshot-Mon-Mar-23-211911-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-211911-2020.th.png)](https://scrot.cloud/image/j3MG) [![Swappshot-Mon-Mar-23-211736-2020.th.png](https://scrot.cloud/images/2020/03/23/Swappshot-Mon-Mar-23-211736-2020.th.png)](https://scrot.cloud/image/jvOi)

### Usage

```
$ nwgdmenu -h
GTK dynamic menu: nwgdmenu 0.6.0 (c) Piotr Miller & Contributors 2021

<input> | nwgdmenu - displays newline-separated stdin input as a GTK menu
nwgdmenu - creates a GTK menu out of commands found in $PATH

Options:
-h               show this help message and exit
-n               no search box
-ha <l>|<r>      horizontal alignment left/right (default: center)
-va <t>|<b>      vertical alignment top/bottom (default: middle)
-r <rows>        number of rows (default: 20)
-c <name>        css file name (default: style.css)
-o <opacity>     background opacity (0.0 - 1.0, default 0.3)
-b <background>  background colour in RRGGBB or RRGGBBAA format (RRGGBBAA alpha overrides <opacity>)
-wm <wmname>     window manager name (if can not be detected)
-run             ignore stdin, always build from commands in $PATH

[requires layer-shell]:
-layer-shell-layer          {BACKGROUND,BOTTOM,TOP,OVERLAY},        default: OVERLAY
-layer-shell-exclusive-zone {auto, valid integer (usually -1 or 0)}, default: auto

Hotkeys:
Delete        clear search box
Insert        switch case sensitivity
```

(1) _The program should auto-detect if something has been passed in `stdin`, and build the menu out of the `stdin` content
or from commands found in `$PATH` accordingly. However, in some specific cases (e.g. if you use gdm and start nwgdmenu
from a key binding) the `stdin` content detection may be false-positive, which results in displaying an empty menu.
In such case use the `nwgdmenu -run` instead, to force building the menu out of commands in `$PATH`._

Notice: if you start your WM from a script (w/o DM), only sway and i3 will be auto-detected. You may need to pass the WM name as the argument:

`nwgdmenu -wm dwm`

The generic name `tiling` will be accepted as well.

### Custom background

Use -b <RRGGBB> | <RRGGBBAA> argument (w/o #) to define custom background colour. If alpha value given, it overrides
the opacity, as well default, as defined with the -o <opacity> argument.

### Custom styling

On first run the program creates the `nwg-launchers/nwgdmenu` folder in your .config directory. You'll find the
default `style.css` files inside. Use it to adjust styling and a vertical margin to the menu, if needed.

## i3 note

In case you use default window borders, an exclusion like this may be necessary:

```
for_window [title="~nwg"] border none
```

### Openbox Note

To start nwgdmenu from a key binding, use the `-run` argument, e.g.:

```xml
<keybind key="W-D">
  <action name="Execute">
    <command>nwgdmenu -run</command>
  </action>
</keybind>
```

### wlr-layer-shell protocol

From 0.5.0 the nwg-launchers support wlr-layer-shell protocol (via gtk-layer-shell), and use it where preferred. The default layer is `OVERLAY` and the default exclusive zone is `auto`, but you can change it using command line arguments. Notably, you may want to set exclusive zone to `-1` to show nwggrid or nwgbar on top of panels (waybar, wf-panel, etc).

### Tips & tricks

### Hide unwanted icons in nwggrid

See: [https://wiki.archlinux.org/index.php/desktop_entries#Hide_desktop_entries](https://wiki.archlinux.org/index.php/desktop_entries#Hide_desktop_entries)
