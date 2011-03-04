#include <wm/all.hpp>

//#define DEBUG_DISPLAY_XEVENTS

#ifdef DEBUG_DISPLAY_XEVENTS
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
#endif

/**
 * Timestamp-related code copied from Ion.  Copyright (c) Tuomo
 * Valkonen 1999-2006.
 */
static void update_timestamp(Time &last_timestamp, XEvent *ev)
{
  Time tm;

  const unsigned int CLOCK_SKEW_MS = 30000;

#define CHKEV(E, T) case E: tm=((T*)ev)->time; break;

  switch(ev->type)
  {
    CHKEV(ButtonPress, XButtonPressedEvent);
    CHKEV(ButtonRelease, XButtonReleasedEvent);
    CHKEV(EnterNotify, XEnterWindowEvent);
    CHKEV(KeyPress, XKeyPressedEvent);
    CHKEV(KeyRelease, XKeyReleasedEvent);
    CHKEV(LeaveNotify, XLeaveWindowEvent);
    CHKEV(MotionNotify, XPointerMovedEvent);
    CHKEV(PropertyNotify, XPropertyEvent);
    CHKEV(SelectionClear, XSelectionClearEvent);
    CHKEV(SelectionNotify, XSelectionEvent);
    CHKEV(SelectionRequest, XSelectionRequestEvent);
  default:
    return;
  }

  if (tm > last_timestamp || last_timestamp - tm > CLOCK_SKEW_MS)
    last_timestamp = tm;
}

Time WM::get_timestamp()
{
  if (last_timestamp == CurrentTime)
  {
    Atom dummy = XInternAtom(display(), "_JMSWM_TIMEREQUEST", False);
    if (dummy == None)
    {
      WARN("failed");
      return 0;
    }

    /* TODO: use another window, i.e. the NET_WM support check window */
    XChangeProperty(display(), root_window(),
                    dummy, dummy, 8, PropModeAppend,
                    (unsigned char*)"", 0);
    XEvent ev;
    /* TODO: perhaps find a good way to handle some signals while
       waiting for this event */
    XMaskEvent(display(), PropertyChangeMask, &ev);
    update_timestamp(last_timestamp, &ev);
    XPutBackEvent(display(), &ev);
  }

  return last_timestamp;
}

static bool safe_XPending(Display *dpy)
{
  // Use an extra call to XPending to work around Xlib bugs
  XPending(dpy);

  return XPending(dpy);
}

void WM::xwindow_handle_event()
{
  XEvent ev;

  if (!safe_XPending(display()))
    return;
  do {
    XNextEvent(display(), &ev);

    update_timestamp(last_timestamp, &ev);

#ifdef DEBUG_DISPLAY_XEVENTS
    DEBUG("Got event: %s 0x%08x", event_type_to_string(ev.type), ev.xany.window);
#endif

    switch (ev.type)
    {
    case MapRequest:
      handle_map_request(ev.xmaprequest);
      break;
    case ConfigureRequest:
      handle_configure_request(ev.xconfigurerequest);
      break;
    case ClientMessage:
      handle_client_message(ev.xclient);
      break;
    case Expose:
      handle_expose(ev.xexpose);
      break;
    case DestroyNotify:
      handle_destroy_window(ev.xdestroywindow);
      break;
    case PropertyNotify:
      handle_property_notify(ev.xproperty);
      break;
    case UnmapNotify:
      handle_unmap_notify(ev.xunmap);
      break;
    case EnterNotify:
      handle_enter_notify(ev.xcrossing);
      break;
    case MappingNotify:
      handle_mapping_notify(ev.xmapping);
      break;
    case KeyPress:
      handle_keypress(ev.xkey);
      break;
    case FocusIn:
      handle_focus_in(ev.xfocus);
      break;
    default:
      if (hasXrandr
          && ev.type == xrandr_event_base + RRScreenChangeNotify)
      {
        handle_xrandr_event(ev);
        break;
      }
      //DEBUG("  Unhandled event: %s", event_type_to_string(ev.type));
      break;
    }
  } while (safe_XPending(display()));

  // No flush here; the event loop flushes
}


void WM::handle_map_request(const XMapRequestEvent &ev)
{
  if (ev.send_event)
    return;

  if (WClient *client = client_of_win(ev.window)) {
    // Client requests to be de-iconified
    if (WFrame *frame = client->visible_frame()) {
      frame->view()->select_frame(frame, true);
      handle_frame_activity(); // immediately unshade it
    }
    return;
  }

  manage_client(ev.window, true);
}

void WM::handle_configure_request(const XConfigureRequestEvent &ev)
{
  if (WClient *client = client_of_win(ev.window))
    client->handle_configure_request(ev);
  else
  {
    /* Allow unmanaged windows to be configured; just perform the
       request */
    XWindowChanges wc;
    wc.border_width=ev.border_width;
    wc.sibling=ev.above;
    wc.stack_mode=ev.detail;
    wc.x=ev.x;
    wc.y=ev.y;
    wc.width=ev.width;
    wc.height=ev.height;
    XConfigureWindow(display(), ev.window, ev.value_mask, &wc);
  }
}

void WM::handle_expose(const XExposeEvent &ev)
{
  if (WClient *client = client_of_framewin(ev.window))
  {
    if (ev.count > 0)
      return;

    client->schedule_draw();
  }
  // TODO: maybe separate these two cases
  else if (ev.window == menu.xwin() || ev.window == menu.completions_xwin())
    menu.handle_expose(ev);

  else if (ev.window == bar.xwin())
    bar.handle_expose(ev);
}

void WM::handle_destroy_window(const XDestroyWindowEvent &ev)
{
  if (WClient *client = client_of_win(ev.window))
  {
    unmanage_client(client);
  }
}

void WM::handle_property_notify(const XPropertyEvent &ev)
{
  if (WClient *client = client_of_win(ev.window))
  {
    if (ev.atom == XA_WM_NAME
        || ev.atom == atom_net_wm_name)
      client->update_name_from_server();

    else if (ev.atom == atom_wm_protocols)
      client->update_protocols_from_server();

    else if (ev.atom == XA_WM_NORMAL_HINTS)
      client->update_size_hints_from_server();

    else if (ev.atom == atom_net_wm_window_type)
      client->update_window_type_from_server();
  }
}

void WM::handle_unmap_notify(const XUnmapEvent &ev)
{
  /* We are not interested in SubstructureNotify -unmaps. */
  /* Courtesy of ion */
  if(ev.event!=ev.window && ev.send_event!=True)
    return;

  if (WClient *client = client_of_win(ev.window))
    unmanage_client(client);
}

void WM::handle_enter_notify(const XCrossingEvent &ev)
{
  if (WClient *client = client_of_framewin(ev.window))
  {
    if (WFrame *frame = client->visible_frame())
    {
      frame->view()->select_frame(frame);
    }
  }
}

void WM::handle_xrandr_event(const XEvent &ev)
{
  if (ev.xany.send_event)
    return;

  XEvent copy = ev;

  if (XRRUpdateConfiguration(&copy))
  {
    WARN("Screen configuration changed");

    buffer_pixmap.reset(screen_width(), screen_height());

    BOOST_FOREACH (WView *view, boost::adaptors::transform(views_, select2nd_compat<WView*>()))
    {
      view->compute_bounds();
      view->schedule_update_positions();
    }
  }

  menu.handle_screen_size_changed();
  bar.handle_screen_size_changed();
}

void WM::handle_focus_in(const XFocusChangeEvent &ev)
{
  /* Prevent programs like matlab from stealing the input focus.
     Allow programs like emacs to switch input focus (if white list is
     enabled). */

  if (ev.detail == NotifyNonlinear
      || ev.detail == NotifyNonlinearVirtual)
  {
    if (WFrame *frame = selected_frame())
    {
      if (WClient *client = client_of_framewin(ev.window))
      {
        if (&frame->client() != client)
        {
          /**
           * White listing functionality is disabled because it isn't
           * needed.  Selecting a frame by setting input focus is
           * fragile and doesn't work in many cases.
           */
#if 0
          WFrame *visible_frame = client->visible_frame();
          // White-listed programs can receive input focus.
          if (visible_frame && client->class_name() == "Emacs")
            visible_frame->view()->select_frame(visible_frame, true);
          else
#endif
            frame->client().schedule_set_input_focus();
        }
      }
    }
  }
}

void WM::handle_client_message(const XClientMessageEvent &ev)
{
  if (ev.message_type == atom_net_active_window) {
    if (ev.format != 32)
      return; // invalid message, format must be 32
    if (WClient *client = client_of_win(ev.window)) {
      if (WFrame *frame = client->visible_frame()) {
        frame->view()->select_frame(frame, true);
        handle_frame_activity(); // immediately unshade it
      }
    }
    return;
  }
  if (ev.message_type == atom_net_wm_state) {
    if (ev.format != 32)
      return; // invalid message, format must be 32
    if (WClient *client = client_of_win(ev.window)) {
      if (WFrame *frame = client->visible_frame()) {
        request_net_wm_state_change_hook(frame, ev);
      }
    }
    return;
  }
}
