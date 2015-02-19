#ifndef _WM_XWINDOW_HPP
#define _WM_XWINDOW_HPP

#include <util/string.hpp>

#include <X11/Xlib.h>

bool xwindow_get_utf8_property(Display *dpy,
                               Window win, Atom a, utf8_string &out);

unsigned long xwindow_get_property(Display *dpy,
                                   Window win, Atom atom, Atom type,
                                   unsigned long n32expected,
                                   bool more, unsigned char **p);

void xwindow_set_input_focus(Display *dpy, Window w);

bool xwindow_send_client_msg32(Display *dpy,
                               Window dst,
                               Window wnd,
                               Atom type,
                               long data0 = 0,
                               long data1 = 0,
                               long data2 = 0,
                               long data3 = 0,
                               long data4 = 0);

#endif /* _UTIL_XWINDOW_HPP */
