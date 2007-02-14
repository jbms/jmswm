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

void toggle_fullscreen(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WClient *client = &frame->client();

    if (WFrame *orig_frame = is_fullscreen_frame(frame))
    {
      delete frame;
      orig_frame->view()->select_frame(orig_frame);
      wm.select_view(orig_frame->view());
      return;
    }

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
    utf8_string new_name = frame->view()->name();
    new_name += '/';
    new_name += fullscreen_client_suffix(&frame->client());
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
}

void check_fullscreen_on_unmanage_client(WClient *client)
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
