
#include <wm/all.hpp>

#include <wm/commands.hpp>
#include <wm/extra/fullscreen.hpp>
#include <wm/extra/previous_view.hpp>
#include <wm/extra/time_applet.hpp>
#include <wm/extra/volume_applet.hpp>
#include <wm/extra/bar_view_applet.hpp>
#include <wm/extra/battery_applet.hpp>
#include <wm/extra/gnus_applet.hpp>
#include <wm/extra/erc_applet.hpp>
#include <wm/extra/view_columns.hpp>
#include <wm/extra/network_applet.hpp>
#include <wm/extra/device_applet.hpp>
#include <wm/extra/cwd.hpp>

#include <wm/extra/web_browser.hpp>

#include <boost/filesystem/path.hpp>

#include <menu/file_completion.hpp>

#include <util/spawn.hpp>

#include <style/db.hpp>

#include <util/path.hpp>

const char *terminal_emulator = "/usr/bin/urxvtc";


void get_xprop_info_for_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    Window w = frame->client().xwin();
    char id[30];
    sprintf(id, "0x%08x", w);
    spawnl(0, terminal_emulator, terminal_emulator, "-hold",
           "-e", "xprop", "-id", id, (const char *)0);
  }
}

void get_xwininfo_info_for_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    Window w = frame->client().xwin();
    char id[30];
    sprintf(id, "0x%08x", w);
    spawnl(0, terminal_emulator, terminal_emulator, "-hold",
           "-e", "xwininfo", "-all", "-id", id, (const char *)0);
  }
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

void edit_file(const std::string &cwd,
               const utf8_string &filename)
{
  const char *program = "/home/jbms/bin/emacs";
  if (filename.empty())
    spawnl(cwd.c_str(), program, program, (const char *)0);
  spawnl(cwd.c_str(), program, program, filename.c_str(), (const char *)0);
}

void edit_file_interactive(WM &wm, const menu::file_completion::EntryStyler &entry_styler)
{
  utf8_string cwd = get_selected_cwd(wm);
  if (cwd.empty())
  {
    char buf[256];
    buf[255] = 0;
    getcwd(buf, 256);
    cwd = compact_path_home(buf);
  }
  wm.menu.read_string("Emacs", menu::InitialState::selected_prefix(cwd + "/"),
                      boost::bind(&edit_file, expand_path_home(cwd), _1),
                      menu::Menu::FailureAction(),
                      menu::file_completion::file_completer(expand_path_home(cwd), entry_styler),
                      true, /* use delay */
                      true); /* use separate thread */
}

void dictionary_lookup(const utf8_string &name)
{
  const char *program = "/home/jbms/bin/dictionary-lookup";
  spawnl(0, program, program, name.c_str(), (char *)0);
}

void dictionary_lookup_interactive(WM &wm)
{
  wm.menu.read_string("Lookup word:", menu::InitialState(),
                      boost::bind(&dictionary_lookup, _1));
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
        style_db["wm_frame"], style_db["bar"], style_db["menu"]);
  /**
   * List completion style
   */
  menu::list_completion::EntryStyle default_list_entry_style(wm.dc, style_db["list_completion_entry_default"]);

  /**
   * File completion style
   */
  menu::file_completion::EntryStyler file_completion_styler(wm.dc, style_db["file_completion"]);
  
  /**
   * URL completion style
   */
  menu::url_completion::Style url_completion_style(wm.dc, style_db["url_completion"]);

  /**
   * Commands
   */

  WCommandList command_list;

  command_list.add("quit", boost::bind(&WM::quit, boost::ref(wm)));
  command_list.add("restart", boost::bind(&WM::restart, boost::ref(wm)));
  
  command_list.add("select_right", boost::bind(&select_right, boost::ref(wm)));
  command_list.add("select_left", boost::bind(&select_left, boost::ref(wm)));
  command_list.add("select_up", boost::bind(&select_up, boost::ref(wm)));
  command_list.add("select_down", boost::bind(&select_down, boost::ref(wm)));
  
  command_list.add("move_left", boost::bind(&move_left, boost::ref(wm)));
  command_list.add("move_right", boost::bind(&move_right, boost::ref(wm)));
  command_list.add("move_up", boost::bind(&move_up, boost::ref(wm)));
  command_list.add("move_down", boost::bind(&move_down, boost::ref(wm)));

  command_list.add("toggle_fullscreen", boost::bind(&toggle_fullscreen, boost::ref(wm)));
  command_list.add("save_state", boost::bind(&WM::save_state_to_server, boost::ref(wm)));

  command_list.add("xprop", boost::bind(&get_xprop_info_for_current_client, boost::ref(wm)));
  command_list.add("xwininfo", boost::bind(&get_xwininfo_info_for_current_client, boost::ref(wm)));

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
                      terminal_emulator));

  wm.bind("mod4-x mod4-x", execute_shell_command_cwd_interactive);

  wm.bind("mod4-a", boost::bind(&WCommandList::execute_interactive,
                                boost::ref(command_list), boost::ref(wm),
                                boost::cref(default_list_entry_style)));

  wm.bind("mod4-t", boost::bind(&switch_to_view_interactive,
                                boost::ref(wm), boost::cref(default_list_entry_style)));

  /*
  wm.bind("mod4-x e",
              boost::bind(&execute_shell_command_selected_cwd,
                          boost::ref(wm),
                          "~/bin/emacs"));
  */
  wm.bind("mod4-x e", boost::bind(&edit_file_interactive,
                                  boost::ref(wm),
                                  boost::cref(file_completion_styler)));

  wm.bind("mod4-x n", boost::bind(&execute_shell_command_selected_cwd,
                                  boost::ref(wm),
                                  "/home/jbms/bin/emacs-note"));

  wm.bind("mod4-x d", boost::bind(&dictionary_lookup_interactive,
                                  boost::ref(wm)));

  boost::shared_ptr<AggregateBookmarkSource> bookmark_source(new AggregateBookmarkSource());
  bookmark_source->add_source(html_bookmark_source("/home/jbms/.firefox-profile/bookmarks.html"));
  bookmark_source->add_source(org_file_list_bookmark_source("/home/jbms/misc/plan/org-agenda-files"));
  bookmark_source->add_source(org_file_bookmark_source("/home/jbms/.jmswm/bookmarks.org"));
  
  wm.bind("mod4-x b", boost::bind(&launch_browser_interactive, boost::ref(wm), bookmark_source,
                                  boost::cref(url_completion_style)));
  wm.bind("mod4-x mod4-b", boost::bind(&load_url_existing_interactive, boost::ref(wm), bookmark_source,
                                       boost::cref(url_completion_style)));
  wm.bind("mod4-x k", boost::bind(&bookmark_current_url, boost::ref(wm), "/home/jbms/.jmswm/bookmarks.org"));

  wm.bind("mod4-x c",
          boost::bind(&execute_shell_command, "~/bin/browser-clipboard"));

  wm.bind("mod4-z",
          boost::bind(&execute_shell_command, "~/bin/xscreensaver-lock"));

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

  wm.menu.bind("BackSpace", boost::bind(&menu::Menu::backspace, boost::ref(wm.menu)));
  wm.menu.bind("any-Return", boost::bind(&menu::Menu::enter, boost::ref(wm.menu)));
  wm.menu.bind("control-c", boost::bind(&menu::Menu::abort, boost::ref(wm.menu)));
  wm.menu.bind("Escape", boost::bind(&menu::Menu::abort, boost::ref(wm.menu)));
  wm.menu.bind("control-f", boost::bind(&menu::Menu::forward_char, boost::ref(wm.menu)));
  wm.menu.bind("control-b", boost::bind(&menu::Menu::backward_char, boost::ref(wm.menu)));
  wm.menu.bind("Right", boost::bind(&menu::Menu::forward_char, boost::ref(wm.menu)));
  wm.menu.bind("Left", boost::bind(&menu::Menu::backward_char, boost::ref(wm.menu)));
  wm.menu.bind("control-a", boost::bind(&menu::Menu::beginning_of_line, boost::ref(wm.menu)));
  wm.menu.bind("control-e", boost::bind(&menu::Menu::end_of_line, boost::ref(wm.menu)));
  wm.menu.bind("Home", boost::bind(&menu::Menu::beginning_of_line, boost::ref(wm.menu)));
  wm.menu.bind("End", boost::bind(&menu::Menu::end_of_line, boost::ref(wm.menu)));
  wm.menu.bind("control-d", boost::bind(&menu::Menu::delete_char, boost::ref(wm.menu)));
  wm.menu.bind("Delete", boost::bind(&menu::Menu::delete_char, boost::ref(wm.menu)));
  wm.menu.bind("control-k", boost::bind(&menu::Menu::kill_line, boost::ref(wm.menu)));
  wm.menu.bind("Tab", boost::bind(&menu::Menu::complete, boost::ref(wm.menu)));
  wm.menu.bind("control-space", boost::bind(&menu::Menu::set_mark, boost::ref(wm.menu)));

  BarViewApplet bar_view_info(wm, style_db["view_applet"]);

  wm.bind("mod4-comma", boost::bind(&BarViewApplet::select_prev,
                                boost::ref(bar_view_info)));
  
  wm.bind("mod4-period", boost::bind(&BarViewApplet::select_next,
                                boost::ref(bar_view_info)));

  TimeApplet time_applet(wm, style_db["time_applet"],
                         WBar::begin(WBar::RIGHT));

  /* TODO: check for exceptions */
  VolumeApplet volume_applet(wm, style_db["volume_applet"],
                             WBar::begin(WBar::RIGHT));

  wm.bind("XF86AudioLowerVolume",
          boost::bind(&VolumeApplet::lower_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioRaiseVolume",
          boost::bind(&VolumeApplet::raise_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioMute",
          boost::bind(&VolumeApplet::toggle_mute,
                      boost::ref(volume_applet)));

  DeviceApplet dev_applet(wm, style_db["device_applet"],
                          WBar::begin(WBar::RIGHT));


  BatteryApplet battery_applet(wm, style_db["battery_applet"],
                               WBar::begin(WBar::RIGHT));

  NetworkApplet net_applet(wm, style_db["network_applet"],
                           WBar::begin(WBar::RIGHT));

  GnusApplet gnus_applet(wm, style_db["gnus_applet"],
                         WBar::begin(WBar::RIGHT));

  ErcApplet erc_applet(wm, style_db["erc_applet"],
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

  WebBrowserModule web_browser_module(wm);

  DesiredColumnCountByName desired_column_count(wm);
  desired_column_count.set("erc", 3);

  wm.bind("mod4-r", boost::bind(&PreviousViewInfo::switch_to, boost::ref(prev_info)));

  wm.bind("mod4-k m", boost::bind(&move_current_frame_to_other_view_interactive,
                                  boost::ref(wm),
                                  boost::cref(default_list_entry_style)));
  wm.bind("mod4-k j", boost::bind(&copy_current_frame_to_other_view_interactive,
                                  boost::ref(wm),
                                  boost::cref(default_list_entry_style)));
  wm.bind("mod4-k k", remove_current_frame);
  wm.bind("mod4-y", move_marked_frames_to_current_view);
  wm.bind("mod4-k y", copy_marked_frames_to_current_view);

  wm.bind("mod4-x p", switch_to_agenda);

  wm.bind("mod4-x m", boost::bind(&GnusApplet::switch_to_mail, boost::ref(gnus_applet)));

  wm.bind("mod4-x r", boost::bind(&ErcApplet::switch_to_buffer, boost::ref(erc_applet)));


  // Finally start window management

  // This is called here rather than in the WM constructor so that the
  // initialization code that precedes this can run first.
  
  /* Manage existing clients */
  wm.load_state_from_server();

  // Re-save state
  wm.start_saving_state_to_server();

  do
  {
    wm.flush();
    if (event_service.handle_events() != 0)
      ERROR_SYS("event service handle events");
  } while (1);
}
