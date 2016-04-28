Dynamic tiling window manager, inspired by Ion, wmii.

Dependencies:
=============

Boost (Ubuntu: libboost-dev)
X11 (Ubuntu: libx11-dev)
xrandr (Ubuntu: libxrandr-dev)
Pango (Ubuntu: libpango1.0-dev)
libasound (Ubuntu: libasound2-dev)
libiw (Ubuntu: libiw-dev)
libevent (Ubuntu: libevent-dev)

Building:
=========

mkdir build
cd build
cmake ..
make -j8

Key command configuration:
==========================

Modify src/wm/main.cpp to contain the correct paths to your terminal
emulator, browser, etc. and to change key bindings as desired.

Style configuration:
====================

The style configuration is stored in the file:

~/.jmswm/style

Copy config/style to that location.

Per-window working directory:
=============================

src/wm/extra/cwd.{h,c}pp implements the concept of a per-window
working directory.  This allows, for example, the key binding for
opening a new terminal to start that terminal with the same working
directory as the current window, which is usually what is desired.

Because X itself doesn't have any standard way of exposing this
information, individual programs have to be configured to expose their
current directory in some manner, and corresponding code has to be
added to src/wm/extra/cwd.cpp to access it.

The easiest way to do this tends to be to encode in the window title
(WM_NAME X window property) both the intended user-visible title as
well as the working directory, since many programs already support
changing the window title.  For zsh support, see:
  config/zshrc.example
  
The modified window title is enabled only if the environment variable
JMSWM is set; you can arrange for your ~/.xsession file to set it.

For Emacs support, see:
  config/emacs.example.el

License:
========

Copyright 2006--2016 Jeremy Maitin-Shepard.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
