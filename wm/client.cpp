
#include <wm/wm.hpp>
#include <wm/xwindow.hpp>

#include <X11/Xatom.h>

#include <util/log.hpp>

void WM::manage_client(Window w, bool map_request)
{
  XWindowAttributes attr;

  /* Window might be destroyed while this function is running */
  XSelectInput(xc.display(), w, WM_EVENT_MASK_CLIENTWIN);
  
  if (!XGetWindowAttributes(xc.display(), w, &attr))
  {
    /* Window disappeared */
    XSelectInput(xc.display(), w, 0);
    return;
  }

  /* Unmanaged window */
  if (attr.override_redirect)
  {
    XSelectInput(xc.display(), w, 0);
    return;
  }


  std::auto_ptr<WClient> c(new WClient(*this, w));

  c->initial_geometry.x = attr.x;
  c->initial_geometry.y = attr.y;
  c->initial_geometry.width = attr.width;
  c->initial_geometry.height = attr.height;

  XSetWindowAttributes fwa;
  fwa.event_mask = WM_EVENT_MASK_FRAMEWIN;
  fwa.background_pixel = frame_style.frame_background_color->pixel();
  c->frame_xwin = XCreateWindow(xc.display(), xc.root_window(),
                                attr.x, /* x */ attr.y, /* y */
                                100, /* width */ 100, /* height */
                                0, /* border width */
                                xc.default_depth(),
                                InputOutput,
                                xc.default_visual(),
                                CWEventMask | CWBackPixel,
                                &fwa);

  c->current_frame_bounds = WRect(attr.x, attr.y, 100, 100);
  c->current_client_bounds = WRect(0, 0, attr.width, attr.height);

  XAddToSaveSet(xc.display(), w);

  c->initial_border_width = attr.border_width;
  
  /* Set the window border to 0 */
  {
    XWindowChanges wc;
    wc.border_width = 0;
    XConfigureWindow(xc.display(), w, CWBorderWidth, &wc);
  }
  
  XSelectInput(xc.display(), w, WM_EVENT_MASK_CLIENTWIN & ~StructureNotifyMask);

  /* FIXME -- set proper x and y */
  XReparentWindow(xc.display(), c->xwin, c->frame_xwin, 0, 0);

  XSelectInput(xc.display(), c->xwin, WM_EVENT_MASK_CLIENTWIN);

  if (!XGetWindowAttributes(xc.display(), w, &attr))
  {
    /* Window disappeared while reparenting */
    XSelectInput(xc.display(), w, 0);
    XDestroyWindow(xc.display(), c->frame_xwin);
    return;
  }

  c->update_name_from_server();
  c->update_class_from_server();
  c->update_role_from_server();

  /* FIXME: get other information, like the protocols, class name, etc. */

  managed_clients.insert(std::make_pair(c->xwin, c.get()));
  framewin_map.insert(std::make_pair(c->frame_xwin, c.get()));

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

  if (!xwindow_get_utf8_property(wm.xc.display(), xwin, wm.atom_net_wm_name, name)
      && !xwindow_get_utf8_property(wm.xc.display(), xwin, XA_WM_NAME, name))
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
  WColumn *column;

  WView *view = selected_view;

  WView::ColumnList::iterator it = view->columns.end();

  if (view->selected_column)
    it = boost::next(view->columns.current(*view->selected_column));
  
  column = view->create_column(0, it);

  column->add_client(c, column->frames.end());
}

void WM::unmanage_client(WClient *client)
{
  for (WClient::ViewFrameMap::iterator it = client->view_frames.begin(),
         next;
       it != client->view_frames.end();
       it = next)
  {
    next = boost::next(it);
    
    WFrame *f = it->second;

    f->remove();

    delete f;
  }

  /* TODO: check if different error handling needs to be used here */

  XReparentWindow(xc.display(), client->xwin, xc.root_window(),
                  0, 0);
  XDestroyWindow(xc.display(), client->frame_xwin);
  
  managed_clients.erase(client->xwin);
  framewin_map.erase(client->frame_xwin);

  XRemoveFromSaveSet(xc.display(), client->xwin);

  XSelectInput(xc.display(), client->xwin, 0);
  
  delete client;
}

void WClient::set_WM_STATE(int state)
{
  long data[] = { state, None };
  XChangeProperty(wm.xc.display(), xwin, wm.atom_wm_state, wm.atom_wm_state, 32,
                  PropModeReplace, reinterpret_cast<unsigned char *>(data), 2);
}

void WClient::set_iconic_state(iconic_state_t state)
{
  if (state != current_iconic_state)
  {
    set_WM_STATE(state == ICONIC_STATE_NORMAL ? NormalState : IconicState);
    current_iconic_state = state;
  }
}

void WClient::handle_pending_work()
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
        DEBUG("moving frame to %d, %d, %d %d", f->bounds.x, f->bounds.y,
              f->bounds.width, f->bounds.height);
        XMoveResizeWindow(wm.xc.display(), frame_xwin,
                          f->bounds.x, f->bounds.y,
                          f->bounds.width, f->bounds.height);
        current_frame_bounds = f->bounds;
      }

      if (current_client_bounds != desired_client_bounds)
      {
        DEBUG("moving client to %d, %d, %d, %d", desired_client_bounds.x,
              desired_client_bounds.y, desired_client_bounds.width,
              desired_client_bounds.height);
        XMoveResizeWindow(wm.xc.display(), xwin,
                          desired_client_bounds.x,
                          desired_client_bounds.y,
                          desired_client_bounds.width,
                          desired_client_bounds.height);
        current_client_bounds = desired_client_bounds;
      }

      if (client_map_state != STATE_MAPPED)
      {
        DEBUG("mapping client");
        XMapWindow(wm.xc.display(), xwin);
        client_map_state = STATE_MAPPED;
      }

      if (frame_map_state != STATE_MAPPED)
      {
        DEBUG("mapping frame");
        XMapWindow(wm.xc.display(), frame_xwin);
        frame_map_state = STATE_MAPPED;
      }

      set_iconic_state(ICONIC_STATE_NORMAL);
    } else
    {

      set_iconic_state(ICONIC_STATE_ICONIC);

      if (frame_map_state != STATE_UNMAPPED)
      {
        DEBUG("unmapping frame");
        XUnmapWindow(wm.xc.display(), frame_xwin);
        frame_map_state = STATE_UNMAPPED;
      }

      if (client_map_state != STATE_UNMAPPED)
      {
        DEBUG("unmapping client");
        /* TODO: perhaps avoid disabling events */
        XSelectInput(wm.xc.display(), xwin,
                     WM_EVENT_MASK_CLIENTWIN & ~StructureNotifyMask);
        XUnmapWindow(wm.xc.display(), xwin);
        XSelectInput(wm.xc.display(), xwin, WM_EVENT_MASK_CLIENTWIN);
        client_map_state = STATE_UNMAPPED;
      }
    }
  }

  if (f)
    f->draw();

  dirty_state = CLIENT_NOT_DIRTY;

  wm.dirty_clients.erase(wm.dirty_clients.current(*this));
}

void WClient::handle_configure_request(const XConfigureRequestEvent &ev)
{
  notify_client_of_root_position();
}

void WClient::notify_client_of_root_position()
{
  XEvent ce;
    
  ce.xconfigure.type=ConfigureNotify;
  ce.xconfigure.event=xwin;
  ce.xconfigure.window=xwin;
  ce.xconfigure.x=current_frame_bounds.x + current_client_bounds.x;
  ce.xconfigure.y=current_frame_bounds.y + current_client_bounds.y;
  ce.xconfigure.width=current_client_bounds.width;
  ce.xconfigure.height=current_client_bounds.height;
  ce.xconfigure.border_width=0;
  ce.xconfigure.above=None;
  ce.xconfigure.override_redirect=False;

  /* TODO: determine if it is really necessary to disable these
     events */
  XSelectInput(wm.xc.display(), xwin,
               WM_EVENT_MASK_CLIENTWIN&~StructureNotifyMask);
  XSendEvent(wm.xc.display(), xwin, False, StructureNotifyMask, &ce);
  XSelectInput(wm.xc.display(), xwin, WM_EVENT_MASK_CLIENTWIN);
}
