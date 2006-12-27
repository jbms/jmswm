
#include <wm/xwindow.hpp>
#include <X11/Xutil.h>

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
