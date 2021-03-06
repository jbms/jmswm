#include <wm/all.hpp>
#include "wm/client.hpp"  // ensure that client.cpp is linked in
#include "wm/view.hpp"  // ensure that view.cpp is linked in
#include "wm/frame.hpp"  // ensure that frame.cpp is linked in
#include "wm/persistence.hpp"  // ensure that persistence.cpp is linked in
#include "wm/sizehint.hpp"  // ensure that sizehint.cpp is linked in
#include "wm/event.hpp"  // ensure that event.cpp is linked in

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

static void set_root_window_cursor(WXContext &xc)
{
  Cursor cur = XCreateFontCursor(xc.display(), XC_left_ptr);
  XDefineCursor(xc.display(), xc.root_window(), cur);
  XFreeCursor(xc.display(), cur);
}

WM::WM(int argc, char **argv,
       Display *dpy, EventService &event_service_,
       const style::Spec &style_spec,
       const style::Spec &bar_style_spec,
       const style::Spec &menu_style_spec)
  : WXContext(dpy),
    event_service_(event_service_),
    x_connection_event(event_service_, ConnectionNumber(dpy), EV_READ,
                       boost::bind(&WM::xwindow_handle_event, this)),
    sigint_event(event_service_, SIGINT,
                 boost::bind(&WM::quit, this)),
    last_timestamp(CurrentTime),
    argv(argv), argc(argc),
    dc(*this),
    buffer_pixmap(dc),
    frame_style(dc, style_spec),
    selected_view_(0),
    frame_activity_event(event_service_, boost::bind(&WM::handle_frame_activity, this)),
    save_state_event(event_service_, boost::bind(&WM::start_saving_state_to_server, this)),
    global_bindctx(*this, mod_info,
                   root_window(), true),
    menu(*this, mod_info, menu_style_spec),
    bar(*this, bar_style_spec)
{
  XSetErrorHandler(xwindow_error_handler);

#define DECLARE_ATOM(var, str) \
  var = XInternAtom(display(), str, False);
#include "atoms.hpp"
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

  /* Xrandr */
  hasXrandr = XRRQueryExtension(display(), &xrandr_event_base,
                                &xrandr_error_base);

  if (hasXrandr)
  {
    XRRSelectInput(display(), root_window(), RRScreenChangeNotifyMask);
  } else
  {
    WARN("Xrandr is not supported on this display");
  }

  set_root_window_cursor(*this);

  buffer_pixmap.reset(screen_width(), screen_height());

  mod_info.update(display());

  key_sequence_timeout.tv_sec = 5;
  key_sequence_timeout.tv_usec = 0;

  menu.initialize();
  bar.initialize();

  /* Use bar window for _NET_SUPPORTING_WM_CHECK */
  {
    Window w = bar.xwin();
    XChangeProperty(display(), w, atom_net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&w, 1);
    utf8_string name = "jmswm";
    XChangeProperty(display(), w, atom_net_wm_name, atom_utf8_string, 8, PropModeReplace,
                    (unsigned char *)name.c_str(), name.length() + 1);
    Atom supported_list[] = {atom_net_wm_state, atom_net_wm_state_shaded, atom_net_active_window, atom_net_wm_state_fullscreen};
    XChangeProperty(display(), root_window(), atom_net_supported, XA_ATOM, 32, PropModeReplace,
                    (unsigned char *)&supported_list, 4);
    XChangeProperty(display(), root_window(), atom_net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&w, 1);
  }
}

WM::~WM()
{
}

void WM::schedule_set_input_focus_to_root()
{
  scheduled_set_input_focus_to_root = true;
}

void WClient::schedule_task(unsigned int task)
{
  if (!scheduled_tasks)
    wm().scheduled_task_clients.push_back(*this);
  scheduled_tasks |= task;
}

void WColumn::schedule_update_positions()
{
  if (!scheduled_update_positions)
  {
    wm().scheduled_task_columns.push_back(*this);
    scheduled_update_positions = true;
  }
}

void WView::schedule_update_positions()
{
  if (!scheduled_update_positions)
  {
    wm().scheduled_task_views.push_back(*this);
    scheduled_update_positions = true;
  }
}

void WM::flush(void)
{
  {
    ScheduledTaskViewList temp;
    temp.swap(scheduled_task_views);
    BOOST_FOREACH (WView &v, temp)
      v.perform_scheduled_tasks();
  }

  {
    ScheduledTaskColumnList temp;
    temp.swap(scheduled_task_columns);
    BOOST_FOREACH (WColumn &c, temp)
      c.perform_scheduled_tasks();
  }

  {
    ScheduledTaskClientList temp;
    temp.swap(scheduled_task_clients);
    BOOST_FOREACH (WClient &c, temp)
      c.perform_scheduled_tasks();
  }

  if (scheduled_set_input_focus_to_root)
  {
    if (!selected_frame())
      xwindow_set_input_focus(display(), root_window());
    scheduled_set_input_focus_to_root = false;
  }

  menu.flush();

  bar.flush();

  XFlush(display());
}

void WM::quit()
{
  save_state_to_server();

  flush();

  WARN("Exiting");

  exit(0);
}

void WM::restart()
{
  save_state_to_server();

  flush();

  WARN("Exiting");

  execvp(argv[0], argv);
}

bool WM::valid_view_name(const utf8_string &name)
{
  if (name.empty())
    return false;

  /* TODO: fix this */
  BOOST_FOREACH (char c, name)
  {
    if (!isgraph(c))
      return false;
  }

  return true;
}
