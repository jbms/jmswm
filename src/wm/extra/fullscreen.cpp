#include <wm/all.hpp>

static utf8_string fullscreen_client_suffix(WClient *client)
{
  return client->instance_name();
}

static WFrame *is_fullscreen_frame(WFrame *frame)
{
  WClient *client = &frame->client();
  const utf8_string &view_name = frame->view()->name();

  utf8_string::size_type sep_pos = view_name.rfind('/');
  if (sep_pos != utf8_string::npos)
  {
    utf8_string original_name = view_name.substr(0, sep_pos);

    if (WView *orig_view = frame->wm().view_by_name(original_name))
    {
      if (WFrame *orig_frame = client->frame_by_view(orig_view))
        return orig_frame;
    }
  }
  return 0;
}

static void enable_fullscreen(WClient *client) {
  WM &wm = client->wm();
  // Frame is assumed to not be a fullscreen frame.

  // Check if frame already has a fullscreen frame
  for (WClient::ViewFrameMap::const_iterator it = client->view_frames().begin();
       it != client->view_frames().end();
       ++it)
  {
    if (is_fullscreen_frame(it->second))
    {
      it->first->select_frame(it->second);
      wm.select_view(it->first);
      return;
    }
  }

  // Otherwise, a fullscreen frame must be created
  utf8_string new_name;
  if (WFrame *frame = client->visible_frame())
    new_name = frame->view()->name();
  else if (WView *view = wm.selected_view())
    new_name = view->name();
  new_name += '/';
  new_name += fullscreen_client_suffix(client);
  utf8_string extra_suffix;
  int count = 1;
  while (wm.view_by_name(new_name + extra_suffix))
  {
    char buffer[32];
    sprintf(buffer, "<%d>", count);
    extra_suffix = buffer;
    ++count;
  }
  WView *view = new WView(wm, new_name + extra_suffix);
  view->set_bar_visible(false);
  WFrame *new_frame = new WFrame(*client);
  new_frame->set_decorated(false);
  view->create_column()->add_frame(new_frame);
  wm.select_view(view);

}

static void disable_fullscreen(WFrame *frame, WFrame *orig_frame = 0) {
  WM &wm = frame->wm();
  if (!orig_frame)
    orig_frame = is_fullscreen_frame(frame);
  if (!orig_frame)
    return;
  delete frame;
  orig_frame->view()->select_frame(orig_frame);
  wm.select_view(orig_frame->view());
}

static void toggle_fullscreen(WFrame *frame) {
  WClient *client = &frame->client();
  if (WFrame *orig_frame = is_fullscreen_frame(frame))
    disable_fullscreen(frame, orig_frame);
  else
    enable_fullscreen(client);

}

void toggle_fullscreen(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    toggle_fullscreen(frame);
  }
}


static void check_fullscreen_on_unmanage_client(WClient *client)
{
  if (WFrame *frame = client->visible_frame())
  {
    if (frame->column()->frames.size() == 1
        && frame->view()->columns.size() == 1)
    {

      if (WFrame *orig_frame = is_fullscreen_frame(frame))
      {
        client->wm().select_view(orig_frame->view());
      }
    }
  }
}

static void check_fullscreen_on_post_place_client(WClient *client)
{
  if (client->initial_net_wm_state & WClient::NET_WM_STATE_FULLSCREEN) {
    /*
  if (client->initial_geometry.x == 0 &&
      client->initial_geometry.y == 0 &&
      client->initial_geometry.width == client->wm().screen_width() &&
      client->initial_geometry.height == client->wm().screen_height()) {
    */
    enable_fullscreen(client);
  }
}


static void check_fullscreen_on_configure_request(WClient *client, const XConfigureRequestEvent &ev)
{
  /*
  if (ev.x == 0 &&
      ev.y == 0 &&
      ev.width == client->wm().screen_width() &&
      ev.height == client->wm().screen_height())
    enable_fullscreen(client);
  */
}

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */


static bool check_fullscreen_on_net_wm_state_request(WFrame *frame, const XClientMessageEvent &ev)
{
  WM &wm = frame->wm();
  if (ev.data.l[1] == wm.atom_net_wm_state_fullscreen
      || ev.data.l[2] == wm.atom_net_wm_state_fullscreen) {
    if (ev.data.l[0] == _NET_WM_STATE_ADD) {
      if (!is_fullscreen_frame(frame))
        enable_fullscreen(&frame->client());
    } else if (ev.data.l[0] == _NET_WM_STATE_REMOVE) {
      disable_fullscreen(frame);
    } else if (ev.data.l[0] == _NET_WM_STATE_TOGGLE) {
      toggle_fullscreen(frame);
    }
    return true;
  }
  return false;
}


static bool check_fullscreen_on_place_client(WClient *newClient)
{
  WM &wm = newClient->wm();
  if (WFrame *frame = wm.selected_frame())
    disable_fullscreen(frame);
  return false;
}

static void maybe_set_fullscreen_net_wm_state(WFrame *frame, unsigned int &desired)
{
  if (is_fullscreen_frame(frame))
    desired |= WClient::NET_WM_STATE_FULLSCREEN;
}

void fullscreen_init(WM &wm)
{
  wm.post_place_client_hook.connect(&check_fullscreen_on_post_place_client);
  wm.unmanage_client_hook.connect(&check_fullscreen_on_unmanage_client);
  wm.client_configure_request_hook.connect(&check_fullscreen_on_configure_request);
  wm.place_client_hook.connect(&check_fullscreen_on_place_client, boost::signals::at_front);
  wm.request_net_wm_state_change_hook.connect(&check_fullscreen_on_net_wm_state_request);
  wm.update_desired_net_wm_state_hook.connect(&maybe_set_fullscreen_net_wm_state);
}
