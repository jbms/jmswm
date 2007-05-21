
#include <wm/all.hpp>

#include <wm/commands.hpp>
#include <wm/extra/fullscreen.hpp>
#include <wm/extra/previous_view.hpp>
#include <wm/extra/time_applet.hpp>
#include <wm/extra/volume_applet.hpp>
#include <wm/extra/bar_view_applet.hpp>
#include <wm/extra/battery_applet.hpp>
#include <wm/extra/gnus_applet.hpp>
#include <wm/extra/network_applet.hpp>
#include <wm/extra/cwd.hpp>

#include <wm/extra/web_browser.hpp>

#include <boost/filesystem/path.hpp>

#include <util/spawn.hpp>

#include <style/db.hpp>

static ascii_string rgb(unsigned char r, unsigned char g, unsigned char b)
{
  char buf[30];
  sprintf(buf, "#%02x%02x%02x", r, g, b);
  return ascii_string(buf);
}

void switch_to_agenda(WM &wm)
{
  /* Look for plan view */
  WView *view = wm.view_by_name("plan");
  WFrame *matching_frame = 0;

  if (view)
  {
    BOOST_FOREACH (WFrame &f, view->frames_by_activity())
    {
      if (f.client().name() == "*Org Agenda*" && f.client().class_name() == "Emacs")
      {
        matching_frame = &f;
        break;
      }
    }
    if (matching_frame)
    {
      view->select_frame(matching_frame, true);
    }
    wm.select_view(view);
  } else
  {
    switch_to_view(wm, "plan");
  }

  if (!matching_frame)
    execute_shell_command("~/bin/org-agenda");
}

int main(int argc, char **argv)
{

  boost::filesystem::path::default_name_check(boost::filesystem::no_check);

  /* Set up child handling */
  {
    struct sigaction act;
    act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    act.sa_handler = SIG_IGN;
    while (sigaction(SIGCHLD, &act, 0) != 0 && errno == EINTR);
  }
  
  WXDisplay xdisplay(NULL);
  set_close_on_exec_flag(ConnectionNumber(xdisplay.display()), true);

  EventService event_service;

  // Style database
  style::DB style_db;

  try
  {
    style_db.load(boost::filesystem::path(getenv("HOME")) / ".jmswm" / "style");
  } catch (style::LoadError &e)
  {
    WARN("%s:%d:%d Style load error: %s", e.filename().c_str(),
         e.line_number(), e.column_number(),
         e.message().c_str());
    if (e.line_number() > 0)
    {
      WARN("%s", e.line().c_str());
      WARN("%*s^", e.column_number(), "");
    }
    ERROR("fatal error");
  }
  
  WM wm(argc, argv, xdisplay.display(), event_service,
        style_db["wm_frame"], style_db["bar"]);
  /**
   * Commands
   */

  WCommandList command_list;

  command_list.add("quit", boost::bind(&WM::quit, _1));
  command_list.add("restart", boost::bind(&WM::restart, _1));
  
  command_list.WM_COMMAND_ADD(select_right);
  command_list.WM_COMMAND_ADD(select_left);
  command_list.WM_COMMAND_ADD(select_up);
  command_list.WM_COMMAND_ADD(select_down);
  
  command_list.WM_COMMAND_ADD(move_left);
  command_list.WM_COMMAND_ADD(move_right);
  command_list.WM_COMMAND_ADD(move_up);
  command_list.WM_COMMAND_ADD(move_down);

  command_list.WM_COMMAND_ADD(toggle_fullscreen);
  command_list.add("save_state", boost::bind(&WM::save_state_to_server, _1));

  wm.bind("mod4-f", select_right);
  wm.bind("mod4-b", select_left);
  wm.bind("mod4-p", select_up);
  wm.bind("mod4-n", select_down);

  wm.bind("mod4-k any-f", move_right);
  wm.bind("mod4-k any-b", move_left);
  wm.bind("mod4-k any-p", move_up);
  wm.bind("mod4-k any-n", move_down);
  
  //wm.bind("mod4-d", toggle_shaded);
  /* JUST FOR TESTING */
  wm.bind("mod4-k d", toggle_decorated);
  wm.bind("mod4-d", toggle_bar_visible);

  wm.bind("mod4-space", toggle_marked);

  
  
  /* */
  wm.bind("mod4-u", decrease_priority);
  wm.bind("mod4-i", increase_priority);

  wm.bind("mod4-k any-u", decrease_column_priority);
  wm.bind("mod4-k any-i", increase_column_priority);

  wm.bind("mod4-x x",
          boost::bind(&execute_shell_command_selected_cwd,
                      boost::ref(wm),
                      "xterm"));

  wm.bind("mod4-x mod4-x", execute_shell_command_cwd_interactive);

  wm.bind("mod4-a", boost::bind(&WCommandList::execute_interactive,
                                boost::ref(command_list),  _1));

  wm.bind("mod4-t", switch_to_view_interactive);

  wm.bind("mod4-x e",
              boost::bind(&execute_shell_command_selected_cwd,
                          boost::ref(wm),
                          "~/bin/emacs"));

  boost::shared_ptr<AggregateBookmarkSource> bookmark_source(new AggregateBookmarkSource());
  bookmark_source->add_source(html_bookmark_source("/home/jbms/.firefox-profile/bookmarks.html"));
  bookmark_source->add_source(org_file_list_bookmark_source("/home/jbms/misc/plan/org-agenda-files"));
  bookmark_source->add_source(org_file_bookmark_source("/home/jbms/.jmswm/bookmarks.org"));
  
  wm.bind("mod4-x b", boost::bind(&launch_browser_interactive, boost::ref(wm), bookmark_source));
  wm.bind("mod4-x mod4-b", boost::bind(&load_url_existing_interactive, boost::ref(wm), bookmark_source));
  wm.bind("mod4-x k", boost::bind(&bookmark_current_url, boost::ref(wm), "/home/jbms/.jmswm/bookmarks.org"));

  wm.bind("mod4-x c",
          boost::bind(&execute_shell_command, "~/bin/browser-clipboard"));

  wm.bind("mod4-z",
          boost::bind(&execute_shell_command, "xscreensaver-command -lock"));

  wm.bind("mod4-c", close_current_client);
  wm.bind("mod4-k c", kill_current_client);
  
  wm.bind("mod4-k r", move_next_by_activity);

  wm.bind("mod4-l", move_next_by_activity_in_column);
  wm.bind("mod4-j", move_next_by_activity_in_view);


  wm.bind("mod4-Return", toggle_fullscreen);

  wm.unmanage_client_hook.connect(&check_fullscreen_on_unmanage_client);

  wm.update_client_name_hook.connect(&update_client_visible_name_and_context);

  for (WM::ClientMap::iterator it = wm.managed_clients.begin();
       it != wm.managed_clients.end();
       ++it)
  {
    update_client_visible_name_and_context(it->second);
  }

  wm.menu.bind("BackSpace", menu_backspace);
  wm.menu.bind("any-Return", menu_enter);
  wm.menu.bind("control-c", menu_abort);
  wm.menu.bind("Escape", menu_abort);
  wm.menu.bind("control-f", menu_forward_char);
  wm.menu.bind("control-b", menu_backward_char);
  wm.menu.bind("Right", menu_forward_char);
  wm.menu.bind("Left", menu_backward_char);
  wm.menu.bind("control-a", menu_beginning_of_line);
  wm.menu.bind("control-e", menu_end_of_line);
  wm.menu.bind("Home", menu_beginning_of_line);
  wm.menu.bind("End", menu_end_of_line);
  wm.menu.bind("control-d", menu_delete);
  wm.menu.bind("Delete", menu_delete);
  wm.menu.bind("control-k", menu_kill_line);
  wm.menu.bind("Tab", menu_complete);
  wm.menu.bind("control-space", menu_set_mark);

  BarViewApplet bar_view_info(wm, style_db["view_applet"]);

  wm.bind("mod4-comma", boost::bind(&BarViewApplet::select_prev,
                                boost::ref(bar_view_info)));
  
  wm.bind("mod4-period", boost::bind(&BarViewApplet::select_next,
                                boost::ref(bar_view_info)));

  BatteryApplet battery_applet(wm, style_db["battery_applet"],
                               WBar::end(WBar::RIGHT));

  /* TODO: check for exceptions */
  VolumeApplet volume_applet(wm, style_db["volume_applet"],
                             WBar::end(WBar::RIGHT));

  wm.bind("XF86AudioLowerVolume",
          boost::bind(&VolumeApplet::lower_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioRaiseVolume",
          boost::bind(&VolumeApplet::raise_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioMute",
          boost::bind(&VolumeApplet::toggle_mute,
                      boost::ref(volume_applet)));

  TimeApplet time_applet(wm, style_db["time_applet"],
                         WBar::end(WBar::RIGHT));

  NetworkApplet net_applet(wm, style_db["network_applet"],
                           WBar::begin(WBar::RIGHT));

  GnusApplet gnus_applet(wm, style_db["gnus_applet"],
                         WBar::begin(WBar::RIGHT));

  for (char c = 'a'; c <= 'z'; ++c)
  {
    ascii_string str;
    str = "mod4-shift-";
    str += c;
    wm.bind(str, boost::bind(switch_to_view_by_letter, _1, c));
    str = "mod4-mod1-";
    str += c;
    wm.bind(str, boost::bind(switch_to_view_by_letter, _1, c));
  }

  PreviousViewInfo prev_info(wm);

  wm.bind("mod4-r", boost::bind(&PreviousViewInfo::switch_to, boost::ref(prev_info)));

  wm.bind("mod4-k m", move_current_frame_to_other_view_interactive);
  wm.bind("mod4-k j", copy_current_frame_to_other_view_interactive);
  wm.bind("mod4-k k", remove_current_frame);
  wm.bind("mod4-y", move_marked_frames_to_current_view);
  wm.bind("mod4-k y", copy_marked_frames_to_current_view);

  wm.bind("mod4-x p", switch_to_agenda);

  wm.bind("mod4-x m", boost::bind(&GnusApplet::switch_to_mail, boost::ref(gnus_applet), _1));

  do
  {
    wm.flush();
    if (event_service.handle_events() != 0)
      ERROR_SYS("event service handle events");
  } while (1);
}
