#include <wm/all.hpp>

/* FIXME: this function should be fixed to do correct conversion,
   check the type atom, etc. */

/*
 * This function is based on code from Ion.
 * Copyright (c) Tuomo Valkonen 1999-2006.
 */
bool xwindow_get_utf8_property(Display *dpy,
                               Window win, Atom a, utf8_string &out)
{
  XTextProperty prop;
  char **list=NULL;
  int n=0;
  Status st;

  st=XGetTextProperty(dpy, win, &prop, a);

  if(!st)
    return false;

  st = Xutf8TextPropertyToTextList(dpy, &prop, &list, &n);

  XFree(prop.value);

  if (st != Success)
    return false;

  if (n < 1)
  {
    XFreeStringList(list);
    return false;
  }

  out = *list;

  XFreeStringList(list);

  return true;
}

/* FIXME: the interface to this function should be improved */

/* Based on Ion code */
/* Copyright (c) Tuomo Valkonen 1999-2006. */
static unsigned long
xwindow_get_property_(Display *dpy,
                      Window win, Atom atom, Atom type,
                      unsigned long n32expected, bool more,
                      unsigned char **p,
                      int *format)
{
  Atom real_type;
  unsigned long n=0, extra=0;
  int status;

  do
  {
    status=XGetWindowProperty(dpy, win, atom, 0L, n32expected,
                              False, type, &real_type, format, &n,
                              &extra, p);

    if(status!=Success || *p==NULL)
      return 0;

    if(extra==0 || !more)
      break;

    XFree((void*)*p);
    n32expected+=(extra+4)/4;
    more=false;
  } while(1);

  if (n==0)
  {
    XFree((void*)*p);
    *p = 0;
    return 0;
  }

  return n;
}

unsigned long xwindow_get_property(Display *dpy,
                                   Window win, Atom atom, Atom type,
                                   unsigned long n32expected, bool more,
                                   unsigned char **p)
{
  int format = 0;
  unsigned long result
    =  xwindow_get_property_(dpy,
                             win, atom, type, n32expected, more, p,
                             &format);
  return result * format / 8;
}

void xwindow_set_input_focus(Display *dpy, Window w)
{
  //XSetInputFocus(dpy, w, RevertToParent, CurrentTime);
  XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
}

bool xwindow_send_client_msg32(
    Display *dpy, Window dst, Window wnd, Atom type, long data0, long data1, long data2, long data3, long data4) {
  XEvent ev;
  ev.xclient.type = ClientMessage;
  ev.xclient.serial = 0;
  ev.xclient.send_event = true;
  ev.xclient.message_type = type;
  ev.xclient.window = wnd;
  ev.xclient.format = 32;
  ev.xclient.data.l[0] = data0;
  ev.xclient.data.l[1] = data1;
  ev.xclient.data.l[2] = data2;
  ev.xclient.data.l[3] = data3;
  ev.xclient.data.l[4] = data4;
  return XSendEvent(dpy, dst, false, 0xFFFFFF /* no mask needed for ClientMessage */, &ev) != 0;
}
