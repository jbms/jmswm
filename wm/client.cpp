
#include <wm/all.hpp>

WClient::WClient(WM &wm, Window w)
  : wm_(wm),
    scheduled_tasks(0),
    xwin_(w),
    flags_(0),
    fixed_height_(0),
    window_type_flags_(0)
{}

WFrame *WClient::visible_frame()
{
  ViewFrameMap::iterator it = view_frames_.find(wm().selected_view());
  if (it != view_frames_.end())
    return it->second;
  else
    return 0;
}


void WM::manage_client(Window w, bool map_request)
{
  XWindowAttributes attr;

  /* Window might be destroyed while this function is running */
  XSelectInput(display(), w, WM_EVENT_MASK_CLIENTWIN);

  if (!XGetWindowAttributes(display(), w, &attr))
  {
    /* Window disappeared */
    XSelectInput(display(), w, 0);
    return;
  }

  /* Unmanaged window */
  if (attr.override_redirect)
  {
    XSelectInput(display(), w, 0);
    return;
  }

  /* Check WM_STATE and attr.map_state == IsViewable*/
  int wm_state;
  if (!map_request
      && attr.map_state != IsViewable
      && (!get_window_WM_STATE(w, wm_state)
          || (wm_state != NormalState && wm_state != IconicState)))
    return;

  std::auto_ptr<WClient> c(new WClient(*this, w));

  c->initial_geometry.x = attr.x;
  c->initial_geometry.y = attr.y;
  c->initial_geometry.width = attr.width;
  c->initial_geometry.height = attr.height;
  c->initial_net_wm_state = c->get_net_wm_state_from_server();
  c->current_net_wm_state = WClient::NET_WM_STATE_INVALID;

  XSetWindowAttributes fwa;
  fwa.event_mask = WM_EVENT_MASK_FRAMEWIN;
  //fwa.background_pixel = frame_style.frame_background_color->pixel();
  c->frame_xwin_ = XCreateWindow(display(), root_window(),
                                 attr.x, /* x */ attr.y, /* y */
                                 100, /* width */ 100, /* height */
                                 0, /* border width */
                                 default_depth(),
                                 InputOutput,
                                 default_visual(),
                                 CWEventMask /*| CWBackPixel*/,
                                 &fwa);

  c->current_frame_bounds = WRect(attr.x, attr.y, 100, 100);
  c->current_client_bounds = WRect(0, 0, attr.width, attr.height);

  XAddToSaveSet(display(), w);

  c->initial_border_width = attr.border_width;

  /* Set the window border to 0 */
  {
    XWindowChanges wc;
    wc.border_width = 0;
    XConfigureWindow(display(), w, CWBorderWidth, &wc);
  }

  XSelectInput(display(), w, WM_EVENT_MASK_CLIENTWIN & ~StructureNotifyMask);

  /* FIXME -- set proper x and y */
  XReparentWindow(display(), c->xwin_, c->frame_xwin_, 0, 0);

  XSelectInput(display(), c->xwin_, WM_EVENT_MASK_CLIENTWIN);

  if (!XGetWindowAttributes(display(), w, &attr))
  {
    /* Window disappeared while reparenting */
    XSelectInput(display(), w, 0);
    XDestroyWindow(display(), c->frame_xwin_);
    return;
  }

  c->update_size_hints_from_server();
  c->update_window_type_from_server();
  c->update_protocols_from_server();
  c->update_class_from_server();
  c->update_role_from_server();

  // This is done after updating the other information that may be
  // useful to client_update_name_hook functions.
  c->update_name_from_server();

  managed_clients.insert(std::make_pair(c->xwin_, c.get()));
  framewin_map.insert(std::make_pair(c->frame_xwin_, c.get()));

  if (attr.map_state == IsUnmapped)
    c->client_map_state = WClient::STATE_UNMAPPED;
  else
    c->client_map_state = WClient::STATE_MAPPED;

  c->frame_map_state = WClient::STATE_UNMAPPED;

  c->current_iconic_state = WClient::ICONIC_STATE_UNKNOWN;

  WClient *ptr = c.release();

  manage_client_hook(ptr);

  if (map_request || !place_existing_client(ptr))
  {
    if (!place_client_hook(ptr))
      place_client(ptr);
    post_place_client_hook(ptr);
  }
}

void WClient::update_size_hints_from_server()
{
  long junk;
  /* Get window manager size hints */
  if (!XGetWMNormalHints(wm().display(), xwin_,
                         &size_hints_, &junk))
    size_hints_.flags = 0;

  /* Have all columns containing this client update positions, so that
     minimum size and aspect ratio hints are handled. */
  BOOST_FOREACH (WFrame *f, boost::make_transform_range(view_frames_, select2nd))
    f->column()->schedule_update_positions();

  update_fixed_height();
}

unsigned int WClient::get_net_wm_state_from_server()
{
  Atom *atoms = 0;

  unsigned long count = xwindow_get_property(wm().display(), xwin_, wm().atom_net_wm_state,
                                             XA_ATOM, 1, true, (unsigned char **)&atoms);

  count /= 4;

  unsigned int flags = 0;

  for (unsigned long i = 0; i < count; ++i)
  {
    Atom a = atoms[i];
    if (a == wm().atom_net_wm_state_fullscreen)
      flags |= NET_WM_STATE_FULLSCREEN;
    else if (a == wm().atom_net_wm_state_shaded)
      flags |= NET_WM_STATE_SHADED;
  }

  if (atoms)
    XFree(atoms);

  return flags;
}

void WClient::update_window_type_from_server()
{
  Atom *atoms = 0;

  unsigned long count = xwindow_get_property(wm().display(), xwin_, wm().atom_net_wm_window_type,
                                             XA_ATOM, 1, true, (unsigned char **)&atoms);

  count /= 4;

  window_type_flags_ = 0;

  for (unsigned long i = 0; i < count; ++i)
  {
    Atom a = atoms[i];
    if (a == wm().atom_net_wm_window_type_desktop)
      window_type_flags_ |= WINDOW_TYPE_DESKTOP;
    else if (a == wm().atom_net_wm_window_type_dock)
      window_type_flags_ |= WINDOW_TYPE_DOCK;
    else if (a == wm().atom_net_wm_window_type_toolbar)
      window_type_flags_ |= WINDOW_TYPE_TOOLBAR;
    else if (a == wm().atom_net_wm_window_type_menu)
      window_type_flags_ |= WINDOW_TYPE_MENU;
    else if (a == wm().atom_net_wm_window_type_utility)
      window_type_flags_ |= WINDOW_TYPE_UTILITY;
    else if (a == wm().atom_net_wm_window_type_splash)
      window_type_flags_ |= WINDOW_TYPE_SPLASH;
    else if (a == wm().atom_net_wm_window_type_dialog)
      window_type_flags_ |= WINDOW_TYPE_DIALOG;
    else if (a == wm().atom_net_wm_window_type_dropdown_menu)
      window_type_flags_ |= WINDOW_TYPE_DROPDOWN_MENU;
    else if (a == wm().atom_net_wm_window_type_popup_menu)
      window_type_flags_ |= WINDOW_TYPE_POPUP_MENU;
    else if (a == wm().atom_net_wm_window_type_tooltip)
      window_type_flags_ |= WINDOW_TYPE_TOOLTIP;
    else if (a == wm().atom_net_wm_window_type_notification)
      window_type_flags_ |= WINDOW_TYPE_NOTIFICATION;
    else if (a == wm().atom_net_wm_window_type_combo)
      window_type_flags_ |= WINDOW_TYPE_COMBO;
    else if (a == wm().atom_net_wm_window_type_dnd)
      window_type_flags_ |= WINDOW_TYPE_DND;
    else if (a == wm().atom_net_wm_window_type_normal)
      window_type_flags_ |= WINDOW_TYPE_NORMAL;
  }

  if (atoms)
    XFree(atoms);

  update_fixed_height();
}

void WClient::update_fixed_height()
{
  int previous_fixed_height = fixed_height_;

  const XSizeHints &s = size_hints();
  if ((s.flags & PMaxSize) &&
      (s.flags & PMinSize) &&
      s.min_height == s.max_height)
  {
    fixed_height_ = s.max_height;
  } else if (window_type_flags() & WINDOW_TYPE_DIALOG)
  {
    fixed_height_ = initial_geometry.height;
  } else
  {
    fixed_height_ = 0;
  }

  if (fixed_height_ != previous_fixed_height)
  {
    /* Have all columns containing this client update positions, so that
       the new fixed height is taken into account. */
    BOOST_FOREACH (WFrame *f, boost::make_transform_range(view_frames_, select2nd))
      f->column()->schedule_update_positions();
  }
}

void WClient::update_protocols_from_server()
{
  Atom *protocols;
  int count;

  flags_ &= ~PROTOCOL_FLAGS;

  if (XGetWMProtocols(wm().display(), xwin_, &protocols, &count))
  {
    for (int i = 0; i < count; ++i)
    {
      if (protocols[i] == wm().atom_wm_delete_window)
        flags_ |= WM_DELETE_WINDOW_FLAG;
      else if (protocols[i] == wm().atom_wm_take_focus)
        flags_ |= WM_TAKE_FOCUS_FLAG;
    }
    XFree(protocols);
  }
}

void WClient::update_name_from_server()
{
  /* Check netwm name first */

  if (!xwindow_get_utf8_property(wm().display(), xwin_, wm().atom_net_wm_name, name_)
      && !xwindow_get_utf8_property(wm().display(), xwin_, XA_WM_NAME, name_))
  {
    return;
  }

  schedule_draw();

  wm().update_client_name_hook(this);
}

void WClient::set_visible_name(const utf8_string &str)
{
  visible_name_ = str;
  schedule_draw();
}

void WClient::set_context_info(const utf8_string &str)
{
  context_info_ = str;
  schedule_draw();
}


void WClient::update_class_from_server()
{
  XClassHint class_hint;
  class_hint.res_name = 0;
  class_hint.res_class = 0;

  XGetClassHint(wm().display(), xwin_, &class_hint);

  if (class_hint.res_name)
  {
    instance_name_ = class_hint.res_name;
    XFree(class_hint.res_name);
  }

  if (class_hint.res_class)
  {
    class_name_ = class_hint.res_class;
    XFree(class_hint.res_class);
  }
}

void WClient::update_role_from_server()
{
  /* FIXME: this should not use utf8 */
  xwindow_get_utf8_property(wm().display(), xwin_, wm().atom_wm_window_role,
                            window_role_);
}

void WM::place_client(WClient *c)
{
  WView::iterator column;

  WView *view = selected_view();

  if (!view)
  {
    view = new WView(*this, "misc");
    select_view(view);
  }

  /* TODO: Use a better policy */
  if (view->columns.size() < 2)
  {
    WView::iterator it = view->selected_position();
    if (it != view->columns.end())
      ++it;
    column = view->create_column(it);
  } else
  {
    column = view->selected_position();
  }

  WColumn::iterator it = column->selected_position();
  if (it != column->frames.end())
    ++it;

  WColumn::iterator frame = column->add_frame(new WFrame(*c), it);

  /* TODO: don't always focus this client */
  view->select_frame(&*frame, true);
}

void WM::unmanage_client(WClient *client)
{
  unmanage_client_hook(client);

  // Note: BOOST_FOREACH is not used because the contents of
  // view_frames_ changes during the loop.  (This is why next is
  // computed at the beginning of each iteration.)
  for (WClient::ViewFrameMap::iterator it = client->view_frames_.begin(),
         end = client->view_frames_.end(), next;
       it != end;
       it = next)
  {
    next = boost::next(it);
    WFrame *f = it->second;
    delete f;
  }

  /* TODO: check if different error handling needs to be used here */

  XReparentWindow(display(), client->xwin_, root_window(),
                  0, 0);
  XDestroyWindow(display(), client->frame_xwin_);

  managed_clients.erase(client->xwin_);
  framewin_map.erase(client->frame_xwin_);

  XRemoveFromSaveSet(display(), client->xwin_);

  /* Remove WM_STATE property per ICCCM */
  XDeleteProperty(display(), client->xwin_, atom_wm_state);
  XDeleteProperty(display(), client->xwin_, atom_net_wm_state);

  XSelectInput(display(), client->xwin_, 0);

  delete client;
}

void WM::set_window_WM_STATE(Window w, int state)
{
  long data[] = { state, None };
  XChangeProperty(display(), w, atom_wm_state, atom_wm_state, 32,
                  PropModeReplace, reinterpret_cast<unsigned char *>(data), 2);
}

bool WM::get_window_WM_STATE(Window w, int &state_ret)
{
  CARD32 *p=NULL;

  if (!xwindow_get_property(display(), w, atom_wm_state,
                            atom_wm_state,
                            2L, false, (unsigned char **)&p))
    return false;

  state_ret=*p;

  XFree((void*)p);

  return true;
}

void WM::send_client_message(Window w, Atom a, Time timestamp)
{
  XClientMessageEvent ev;

  ev.type=ClientMessage;
  ev.window=w;
  ev.message_type=atom_wm_protocols;
  ev.format=32;
  ev.data.l[0]=a;
  ev.data.l[1]=timestamp;

  XSendEvent(display(), w, False, 0L, (XEvent*)&ev);
}

void WClient::set_iconic_state(iconic_state_t state)
{
  if (state != current_iconic_state)
  {
    wm().set_window_WM_STATE(xwin_,
                           state == ICONIC_STATE_NORMAL ?
                           NormalState : IconicState);
    current_iconic_state = state;
  }
}

void WClient::set_frame_map_state(map_state_t state)
{
  if (state != frame_map_state)
  {
    switch (state)
    {
    case STATE_MAPPED:
      XMapWindow(wm().display(), frame_xwin_);

      /* Lower the frame window; frames should not obscure any other
         windows (such as the menu). */
      XLowerWindow(wm().display(), frame_xwin_);
      break;
    case STATE_UNMAPPED:
      XUnmapWindow(wm().display(), frame_xwin_);
      break;
    }
    frame_map_state = state;
  }
}

void WClient::set_client_map_state(map_state_t state)
{
  if (state != client_map_state)
  {
    switch (state)
    {
    case STATE_MAPPED:
      XMapWindow(wm().display(), xwin_);
      break;
    case STATE_UNMAPPED:
      /* TODO: perhaps avoid disabling events */
      XSelectInput(wm().display(), xwin_,
                   WM_EVENT_MASK_CLIENTWIN & ~StructureNotifyMask);
      XUnmapWindow(wm().display(), xwin_);
      XSelectInput(wm().display(), xwin_, WM_EVENT_MASK_CLIENTWIN);
      break;
    }
    client_map_state = state;
  }
}

void WClient::perform_scheduled_tasks()
{
  assert(scheduled_tasks != 0);

  wm().scheduled_task_clients.erase(wm().scheduled_task_clients.current(*this));


  WFrame *f = visible_frame();

  if (scheduled_tasks & UPDATE_SERVER_FLAG)
  {
    map_state_t desired_client_state, desired_frame_state;
    iconic_state_t desired_iconic_state;
    unsigned int desired_net_wm_state = 0;
    if (f)
    {
      desired_frame_state = STATE_MAPPED;

      /* Check if a warp should be performed */
      if (!(scheduled_tasks & WARP_POINTER_FLAG)
          && f == wm().selected_frame())
      {
        Window root;
        Window child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(wm().display(), wm().root_window(),
                      &root, &child, &root_x, &root_y,
                      &win_x, &win_y, &mask);
        if (!f->bounds.contains_point(root_x, root_y))
          scheduled_tasks |= WARP_POINTER_FLAG;
      }

      if (current_frame_bounds != f->bounds)
      {

        XMoveResizeWindow(wm().display(), frame_xwin_,
                          f->bounds.x, f->bounds.y,
                          f->bounds.width, f->bounds.height);
        current_frame_bounds = f->bounds;
      }

      if (!f->shaded())
      {
        desired_client_state = STATE_MAPPED;
        desired_iconic_state = ICONIC_STATE_NORMAL;

        WRect desired_client_bounds
          = compute_actual_client_bounds(f->client_bounds());

        if (current_client_bounds != desired_client_bounds)
        {
          XMoveResizeWindow(wm().display(), xwin_,
                            desired_client_bounds.x,
                            desired_client_bounds.y,
                            desired_client_bounds.width,
                            desired_client_bounds.height);
          current_client_bounds = desired_client_bounds;
        }
        wm().update_desired_net_wm_state_hook(f, desired_net_wm_state);
      } else
      {
        desired_net_wm_state = NET_WM_STATE_SHADED;
        desired_client_state = STATE_UNMAPPED;
        desired_iconic_state = ICONIC_STATE_ICONIC;
      }
    } else
    {
      desired_frame_state = STATE_UNMAPPED;
      desired_client_state = STATE_UNMAPPED;
      desired_iconic_state = ICONIC_STATE_ICONIC;
    }

    set_client_map_state(desired_client_state);
    set_frame_map_state(desired_frame_state);
    set_iconic_state(desired_iconic_state);
    if (current_net_wm_state != desired_net_wm_state) {
      Atom value[2];
      int nelements = 0;
      if (desired_net_wm_state & NET_WM_STATE_SHADED)
        value[nelements++] = wm().atom_net_wm_state_shaded;
      if (desired_net_wm_state & NET_WM_STATE_FULLSCREEN)
        value[nelements++] = wm().atom_net_wm_state_fullscreen;
      XChangeProperty(wm().display(), xwin_, wm().atom_net_wm_state, XA_ATOM, 32, PropModeReplace,
                      (unsigned char *)value, nelements);
      current_net_wm_state = desired_net_wm_state;
    }
  }

  if (f && (scheduled_tasks & (UPDATE_SERVER_FLAG | DRAW_FLAG)))
    f->draw();

  if (f && f == wm().selected_frame())

  {
    if (scheduled_tasks & SET_INPUT_FOCUS_FLAG)
    {
      /* TODO: handle WM_TAKE_FOCUS */
      /* TODO: fix this hack */
      if (f->shaded())
        xwindow_set_input_focus(wm().display(), frame_xwin_);
      else
        xwindow_set_input_focus(wm().display(), xwin_);
    }

    if (scheduled_tasks & WARP_POINTER_FLAG)
    {
      XWarpPointer(wm().display(), None, frame_xwin_, 0, 0, 0, 0,
                   f->bounds.width / 2, /*f->bounds.height / 2*/ 5  /* warp to top middle instead of center */);
    }
  }

  scheduled_tasks = 0;
}

void WClient::handle_configure_request(const XConfigureRequestEvent &ev)
{
  wm().client_configure_request_hook(this, ev);
  notify_client_of_root_position();
}

void WClient::notify_client_of_root_position()
{
  XEvent ce;

  ce.xconfigure.type=ConfigureNotify;
  ce.xconfigure.event=xwin_;
  ce.xconfigure.window=xwin_;
  ce.xconfigure.x=current_frame_bounds.x + current_client_bounds.x;
  ce.xconfigure.y=current_frame_bounds.y + current_client_bounds.y;
  ce.xconfigure.width=current_client_bounds.width;
  ce.xconfigure.height=current_client_bounds.height;
  ce.xconfigure.border_width=0;
  ce.xconfigure.above=None;
  ce.xconfigure.override_redirect=False;

  /* TODO: determine if it is really necessary to disable these
     events */
  XSelectInput(wm().display(), xwin_,
               WM_EVENT_MASK_CLIENTWIN&~StructureNotifyMask);
  XSendEvent(wm().display(), xwin_, False, StructureNotifyMask, &ce);
  XSelectInput(wm().display(), xwin_, WM_EVENT_MASK_CLIENTWIN);
}

void WClient::kill()
{
  XKillClient(wm().display(), xwin_);
}

void WClient::destroy()
{
  XDestroyWindow(wm().display(), xwin_);
}

void WClient::request_close()
{
  if (flags_ & WM_DELETE_WINDOW_FLAG)
  {
    wm().send_client_message(xwin_, wm().atom_wm_delete_window);
  } else
  {
    destroy();
  }
}
