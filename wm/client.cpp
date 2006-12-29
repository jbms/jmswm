
#include <wm/wm.hpp>
#include <wm/xwindow.hpp>

#include <X11/Xatom.h>

#include <util/log.hpp>

WClient::WClient(WM &wm, Window w)
  : wm_(wm),
    dirty_state(CLIENT_NOT_DIRTY),
    xwin_(w)
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

  c->update_name_from_server();
  c->update_class_from_server();
  c->update_role_from_server();

  /* FIXME: get other information, like the protocols, class name, etc. */

  managed_clients.insert(std::make_pair(c->xwin_, c.get()));
  framewin_map.insert(std::make_pair(c->frame_xwin_, c.get()));

  if (attr.map_state == IsUnmapped)
    c->client_map_state = WClient::STATE_UNMAPPED;
  else
    c->client_map_state = WClient::STATE_MAPPED;

  c->frame_map_state = WClient::STATE_UNMAPPED;

  c->current_iconic_state = WClient::ICONIC_STATE_UNKNOWN;

  place_client(c.release());
}

void WClient::update_name_from_server()
{
  /* Check netwm name first */

  if (!xwindow_get_utf8_property(wm().display(), xwin_, wm().atom_net_wm_name, name_)
      && !xwindow_get_utf8_property(wm().display(), xwin_, XA_WM_NAME, name_))
  {
    DEBUG("Failed to get client name");
  }
  
}

void WClient::update_class_from_server()
{
}

void WClient::update_role_from_server()
{
}

void WM::place_client(WClient *c)
{
  WView::iterator column;

  WView *view = selected_view();

  /* TODO: Use a better policy */
  if (view->columns.size() < 3)
  {
    WView::iterator it = view->next_column(view->selected_position(), false);
  
    column = view->create_column(it);
  } else
  {
    column = view->selected_position();
  }

  WColumn::iterator it = column->next_frame(column->selected_position(), false);
  
  WColumn::iterator frame = column->add_client(c, it);

  /* TODO: don't always focus this client */
  view->select_frame(&*frame);
}

void WM::unmanage_client(WClient *client)
{
  for (WClient::ViewFrameMap::iterator it = client->view_frames_.begin(),
         next;
       it != client->view_frames_.end();
       it = next)
  {
    next = boost::next(it);
    
    WFrame *f = it->second;

    f->remove();

    delete f;
  }

  if (client_to_focus == client)
    client_to_focus = 0;

  /* TODO: check if different error handling needs to be used here */

  XReparentWindow(display(), client->xwin_, root_window(),
                  0, 0);
  XDestroyWindow(display(), client->frame_xwin_);
  
  managed_clients.erase(client->xwin_);
  framewin_map.erase(client->frame_xwin_);

  XRemoveFromSaveSet(display(), client->xwin_);

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

void WClient::perform_deferred_work()
{
  if (dirty_state == CLIENT_NOT_DIRTY)
    return;

  WFrame *f = visible_frame();

  if (dirty_state == CLIENT_POSITIONING_NEEDED)
  {
    if (f)
    {
      WRect desired_client_bounds = f->client_bounds();

      if (current_frame_bounds != f->bounds)
      {
        XMoveResizeWindow(wm().display(), frame_xwin_,
                          f->bounds.x, f->bounds.y,
                          f->bounds.width, f->bounds.height);
        current_frame_bounds = f->bounds;
      }

      if (current_client_bounds != desired_client_bounds)
      {
        XMoveResizeWindow(wm().display(), xwin_,
                          desired_client_bounds.x,
                          desired_client_bounds.y,
                          desired_client_bounds.width,
                          desired_client_bounds.height);
        current_client_bounds = desired_client_bounds;
      }

      if (client_map_state != STATE_MAPPED)
      {
        XMapWindow(wm().display(), xwin_);
        client_map_state = STATE_MAPPED;
      }

      if (frame_map_state != STATE_MAPPED)
      {
        XMapWindow(wm().display(), frame_xwin_);
        frame_map_state = STATE_MAPPED;
      }

      set_iconic_state(ICONIC_STATE_NORMAL);
    } else
    {

      set_iconic_state(ICONIC_STATE_ICONIC);

      if (frame_map_state != STATE_UNMAPPED)
      {
        XUnmapWindow(wm().display(), frame_xwin_);
        frame_map_state = STATE_UNMAPPED;
      }

      if (client_map_state != STATE_UNMAPPED)
      {
        /* TODO: perhaps avoid disabling events */
        XSelectInput(wm().display(), xwin_,
                     WM_EVENT_MASK_CLIENTWIN & ~StructureNotifyMask);
        XUnmapWindow(wm().display(), xwin_);
        XSelectInput(wm().display(), xwin_, WM_EVENT_MASK_CLIENTWIN);
        client_map_state = STATE_UNMAPPED;
      }
    }
  }

  if (f)
    f->draw();

  dirty_state = CLIENT_NOT_DIRTY;

  wm().dirty_clients.erase(wm().dirty_clients.current(*this));
}

void WClient::handle_configure_request(const XConfigureRequestEvent &ev)
{
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

void WClient::focus()
{
  /* TODO: handle WM_TAKE_FOCUS */
  xwindow_set_input_focus(wm().display(), xwin_);
}

void WM::schedule_focus_client(WClient *client)
{
  client_to_focus = client;
}
