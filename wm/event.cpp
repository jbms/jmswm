
#include <wm/wm.hpp>

void WM::handle_map_request(const XMapRequestEvent &ev)
{
  if (ev.send_event)
    return;

  if (client_of_win(ev.window))
    return;

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
    XConfigureWindow(xc.display(), ev.window, ev.value_mask, &wc);
  }
}

void WM::handle_expose(const XExposeEvent &ev)
{
  if (WClient *client = client_of_framewin(ev.window))
  {
    if (ev.count > 0)
      return;
    
    client->mark_dirty(WClient::CLIENT_DRAWING_NEEDED);
  }
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
    {
      client->update_name_from_server();
      client->mark_dirty(WClient::CLIENT_DRAWING_NEEDED);
    }
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
