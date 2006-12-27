
#include <wm/wm.hpp>
#include <boost/utility.hpp>
#include <util/log.hpp>

static const char *event_type_to_string(int type)
{
  static const char *name_arr[] = {
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose	",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify"
  };

  if (type < KeyPress)
    return 0;

  if (type > MappingNotify)
    return 0;

  return name_arr[type - KeyPress];
}

/* Error handler based on Ion code */
/* Copyright (c) Tuomo Valkonen 1999-2006. */
static bool redirect_error;
static int xwindow_redirect_error_handler(Display *dpy, XErrorEvent *ev)
{
  redirect_error = true;
  return 0;
}

/* Error handler based on Ion code */
/* Copyright (c) Tuomo Valkonen 1999-2006. */
static int xwindow_error_handler(Display *dpy, XErrorEvent *ev)
{
  static char msg[128], request[64], num[32];
    
  /* Just ignore bad window and similar errors; makes the rest of
   * the code simpler.
   */
  if(ev->error_code==BadWindow ||
     (ev->error_code==BadMatch && ev->request_code==X_SetInputFocus) ||
     (ev->error_code==BadDrawable && ev->request_code==X_GetGeometry))
    return 0;

  XGetErrorText(dpy, ev->error_code, msg, 128);
  snprintf(num, 32, "%d", ev->request_code);
  XGetErrorDatabaseText(dpy, "XRequest", num, "", request, 64);

  if(request[0]=='\0')
    snprintf(request, 64, "<unknown request>");

  if(ev->minor_code!=0)
    WARN("[%d] %s (%d.%d) %#lx: %s", ev->serial, request,
         ev->request_code, ev->minor_code, ev->resourceid,msg);
  else
    WARN("[%d] %s (%d) %#lx: %s", ev->serial, request,
         ev->request_code, ev->resourceid,msg);
  
  kill(getpid(), SIGTRAP);
  
  return 0;
}

static void xwindow_handle_event(int, short, void *wm_ptr)
{
  XEvent ev;
  WM &wm = *(WM *)wm_ptr;

  if (!XPending(wm.xc.display()))
    return;
  do {
    XNextEvent(wm.xc.display(), &ev);
    
    DEBUG("Got event: %s", event_type_to_string(ev.type));

    switch (ev.type)
    {
    case MapRequest:
      wm.handle_map_request(ev.xmaprequest);
      break;
    case ConfigureRequest:
      wm.handle_configure_request(ev.xconfigurerequest);
      break;
    case Expose:
      wm.handle_expose(ev.xexpose);
      break;
    case DestroyNotify:
      wm.handle_destroy_window(ev.xdestroywindow);
      break;
    case PropertyNotify:
      wm.handle_property_notify(ev.xproperty);
      break;
    case UnmapNotify:
      wm.handle_unmap_notify(ev.xunmap);
      break;
    default:
      DEBUG("  Unhandled event: %s", event_type_to_string(ev.type));
      break;
    }
  } while (XPending(wm.xc.display()));
  
  wm.flush();
}

static void set_root_window_cursor(WXContext &xc)
{
  Cursor cur = XCreateFontCursor(xc.display(), XC_left_ptr);
  XDefineCursor(xc.display(), xc.root_window(), cur);
  XFreeCursor(xc.display(), cur);
}

WM::WM(Display *dpy, event_base *eb, const WFrameStyle::Spec &style_spec)
  : xc(dpy),
    dc(xc),
    frame_style(style_spec),
    buffer_pixmap(dc),
    eb(eb),
    selected_view(0)
{
  XSetErrorHandler(xwindow_error_handler);

  event_set(&x_connection_event, ConnectionNumber(dpy),
            EV_PERSIST | EV_READ, &xwindow_handle_event, this);
  
  if (event_base_set(eb, &x_connection_event) != 0)
    ERROR_SYS("event_base_set");

  if (event_add(&x_connection_event, NULL) != 0)
    ERROR_SYS("event_add");

#define DECLARE_ATOM(var, str) \
  var = XInternAtom(xc.display(), str, False);
#include "atoms.h"
#undef DECLARE_ATOM

  redirect_error = false;
  XSync(xc.display(), False);
  XSetErrorHandler(&xwindow_redirect_error_handler);
  XSelectInput(xc.display(), xc.root_window(), WM_EVENT_MASK_ROOT);
  XSync(xc.display(), False);
  XSetErrorHandler(&xwindow_error_handler);

  if (redirect_error)
    ERROR("Failed to set root window event mask for screen %d.",
          xc.screen_number());

  set_root_window_cursor(xc);

  buffer_pixmap.reset(xc.screen_width(), xc.screen_height());

  /* Add a default view */
  selected_view = new WView(*this, "def");

  /* Manage existing clients */

  /* TODO: actually do this */

  flush();

}

void WM::flush(void)
{
  for (DirtyClientList::iterator it = dirty_clients.begin(), next;
       it != dirty_clients.end();
       it = next)
  {
    next = boost::next(it);

    it->handle_pending_work();
  }
  
  XFlush(xc.display());
}
