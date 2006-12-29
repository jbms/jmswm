
#include <boost/foreach.hpp>
#include <wm/wm.hpp>
#include <boost/utility.hpp>
#include <util/log.hpp>
#include <wm/xwindow.hpp>

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

void WM::xwindow_handle_event(int, short, void *wm_ptr)
{
  XEvent ev;
  WM &wm = *(WM *)wm_ptr;

  if (!XPending(wm.display()))
    return;
  do {
    XNextEvent(wm.display(), &ev);
    
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
    case EnterNotify:
      wm.handle_enter_notify(ev.xcrossing);
      break;
    case MappingNotify:
      wm.handle_mapping_notify(ev.xmapping);
      break;
    case KeyPress:
      wm.handle_keypress(ev.xkey);
      break;
    default:
      DEBUG("  Unhandled event: %s", event_type_to_string(ev.type));
      break;
    }
  } while (XPending(wm.display()));
  
  wm.flush();
}

static void set_root_window_cursor(WXContext &xc)
{
  Cursor cur = XCreateFontCursor(xc.display(), XC_left_ptr);
  XDefineCursor(xc.display(), xc.root_window(), cur);
  XFreeCursor(xc.display(), cur);
}


WM::WM(Display *dpy, event_base *eb, const WFrameStyle::Spec &style_spec)
  : WXContext(dpy),
    eb(eb),
    dc(*this),
    buffer_pixmap(dc),
    client_to_focus(0),
    frame_style(dc, style_spec),
    selected_view_(0),
    key_binding_state(0)
{
  XSetErrorHandler(xwindow_error_handler);

  event_set(&x_connection_event, ConnectionNumber(dpy),
            EV_PERSIST | EV_READ, &WM::xwindow_handle_event, this);
  
  if (event_base_set(eb, &x_connection_event) != 0)
    ERROR_SYS("event_base_set");

  if (event_add(&x_connection_event, NULL) != 0)
    ERROR_SYS("event_add");

#define DECLARE_ATOM(var, str) \
  var = XInternAtom(display(), str, False);
#include "atoms.h"
#undef DECLARE_ATOM

  redirect_error = false;
  XSync(display(), False);
  XSetErrorHandler(&xwindow_redirect_error_handler);
  XSelectInput(display(), root_window(), WM_EVENT_MASK_ROOT);
  XSync(display(), False);
  XSetErrorHandler(&xwindow_error_handler);

  if (redirect_error)
    ERROR("Failed to set root window event mask for screen %d.",
          screen_number());

  set_root_window_cursor(*this);

  buffer_pixmap.reset(screen_width(), screen_height());

  initialize_key_handler();

  /* Add a default view */
  select_view(new WView(*this, "def"));

  /* Manage existing clients */
  /* FIXME: improve this */
  {
    Window junk1, junk2;
    Window *top_level_windows = 0;
    unsigned int top_level_window_count = 0;
    XQueryTree(display(), root_window(), &junk1, &junk2,
               &top_level_windows, &top_level_window_count);
    for (unsigned int i = 0; i < top_level_window_count; ++i)
    {
      manage_client(top_level_windows[i], false);
    }
    XFree(top_level_windows);
  }

  flush();
}

WM::~WM()
{
  deinitialize_key_handler();
  event_del(&x_connection_event);
}


void WM::flush(void)
{
  for (DirtyClientList::iterator it = dirty_clients.begin(), next;
       it != dirty_clients.end();
       it = next)
  {
    next = boost::next(it);
    it->perform_deferred_work();
  }

  if (client_to_focus)
  {
    client_to_focus->focus();
    client_to_focus = 0;
  }
  
  XFlush(display());
}

