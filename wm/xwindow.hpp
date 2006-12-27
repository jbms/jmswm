#ifndef _WM_XWINDOW_HPP
#define _WM_XWINDOW_HPP

#include <util/string.hpp>

#include <X11/Xlib.h>

bool xwindow_get_utf8_property(Display *dpy,
                               Window win, Atom a, utf8_string &out);

#endif /* _UTIL_XWINDOW_HPP */
