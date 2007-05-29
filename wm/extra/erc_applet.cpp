
#include <wm/extra/erc_applet.hpp>
#include <fstream>
#include <sys/inotify.h>
#include <boost/algorithm/string.hpp>
#include <wm/commands.hpp>
#include <wm/extra/place.hpp>

PROPERTY_ACCESSOR(WClient, ascii_string, erc_target)
PROPERTY_ACCESSOR(WClient, ascii_string, erc_network)

static const char *erc_status_filename = "/tmp/jbms/erc-status";

void ErcApplet::handle_inotify_event(int wd, uint32_t mask, uint32_t cookie,
                                      const char *pathname)
{
  if (wd == this->wd && mask & IN_CLOSE_WRITE)
    update();
}

void ErcApplet::update()
{
  BufferStatus new_status;
  {
    std::ifstream ifs(erc_status_filename);
    do
    {
      ascii_string buffer_name, server_name, network_name, count, face;
      if (ifs >> buffer_name >> server_name >> network_name >> count >> face)
      {
        if (buffer_name != "#digitalhive")
          new_status.push_back(BufferInfo(network_name, buffer_name));
      }
      else
        break;
    } while (1);
  }

  if (new_status == buffer_status)
    return;

  buffer_status.swap(new_status);

  cells.clear();
  BOOST_FOREACH (const BufferInfo &info, buffer_status)
  {
    cells.push_back(wm.bar.insert(WBar::before(placeholder), style,
                                  info.second));
  }
}

static bool client_matches(WClient *client)
{
  if (client->class_name() == "Emacs" && client->instance_name() == "erc")
    return true;
  return false;
}

static bool handle_update_client_name(WClient *client)
{
  if (client_matches(client))
  {
    const utf8_string &name = client->name();
    utf8_string::size_type dir_start = name.find(" [");
    if (dir_start != utf8_string::npos && name[name.size() - 1] == ']')
    {
      utf8_string network = name.substr(dir_start + 2,
                                        name.size() - dir_start - 2 - 1);
      utf8_string target = name.substr(0, dir_start);
      client->set_context_info(network);
      client->set_visible_name(target);
      erc_target(client) = target;
      erc_network(client) = network;
    } else
    {
      erc_target(client).remove();
      erc_network(client).remove();
      client->set_visible_name(name);
      client->set_context_info(utf8_string());
    }
    return true;
  }
  return false;
}

static bool handle_place_client(WClient *client)
{
  if (client_matches(client))
  {
    place_client_in_smallest_column(client->wm().get_or_create_view("erc"), client);
    return true;
  }
  return false;
}

ErcApplet::ErcApplet(WM &wm, const style::Spec &style_spec,
                       const WBar::InsertPosition &pos)
  : wm(wm), style(wm.dc, style_spec),
    inotify(wm.event_service(),
            boost::bind(&ErcApplet::handle_inotify_event, this, _1, _2, _3, _4)),
    name_conn(wm.update_client_name_hook.connect(-1, &handle_update_client_name)),
    place_conn(wm.place_client_hook.connect(&handle_place_client))
{
  {
    // Attempt to create file if it doesn't exist.
    std::ofstream ofs(erc_status_filename, std::ios_base::app | std::ios_base::out);
  }
  
  wd = inotify.add_watch(erc_status_filename, IN_CLOSE_WRITE);
  placeholder = wm.bar.placeholder(pos);
  update();
}

void ErcApplet::switch_to_buffer(WM &wm)
{
  WView *view = wm.view_by_name("erc");
  WFrame *matching_frame = 0;

  if (!buffer_status.empty())
  {
    const BufferInfo &info = buffer_status.front();
    if (view)
    {
      BOOST_FOREACH (WFrame &f, view->frames_by_activity())
      {
        if (erc_target(f.client()) == info.second && erc_network(f.client()) == info.first)
        {
          matching_frame = &f;
          break;
        }
      }
      if (matching_frame)
      {
        view->select_frame(matching_frame, true);
        wm.select_view(view);
      }
    }
  }

  if (!matching_frame)
  {
    switch_to_view(wm, "erc");
    execute_shell_command("~/bin/erc");
  }
}
