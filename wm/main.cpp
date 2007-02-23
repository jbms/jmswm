
#include <wm/all.hpp>

#include <wm/commands.hpp>
#include <wm/extra/fullscreen.hpp>
#include <wm/extra/previous_view.hpp>
#include <wm/extra/time_applet.hpp>
#include <wm/extra/volume_applet.hpp>
#include <wm/extra/bar_view_applet.hpp>
#include <wm/extra/battery_applet.hpp>
#include <wm/extra/gnus_applet.hpp>
#include <wm/extra/cwd.hpp>

static bool is_search_query(const utf8_string &text)
{
  if (boost::algorithm::contains(text, "://"))
    return false;

  if (text.find(' ') != utf8_string::npos)
    return true;

  if (text.find('.') != utf8_string::npos)
    return false;

  return true;
}

void launch_browser(WM &wm, const utf8_string &text)
{
  ascii_string program = "/home/jbms/bin/browser";
  utf8_string arg = text;
  if (text.empty())
  {
    arg = "http://www.google.com";
  } else if (is_search_query(text))
  {
    program = "/home/jbms/bin/browser-google-results";
  }
  if (fork() == 0)
  {
    setsid();
    close(STDIN_FILENO);
    execl(program.c_str(), program.c_str(), arg.c_str(), (char *)0);
    exit(-1);
  }
}

void launch_browser_interactive(WM &wm)
{
  wm.menu.read_string("URL: ",
                      boost::bind(&launch_browser, boost::ref(wm), _1));
}

int main(int argc, char **argv)
{

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
  
  // Style

  WFrameStyle::Spec style_spec;

  ascii_string font_name = "fixed 10";
  
  style_spec.label_font = font_name;
  style_spec.client_background_color = "black";

  style_spec.normal.active_selected.highlight_color = "gold1";
  style_spec.normal.active_selected.shadow_color = "gold1";
  style_spec.normal.active_selected.padding_color = "gold3";
  style_spec.normal.active_selected.background_color = "black";
  style_spec.normal.active_selected.label_foreground_color = "black";
  style_spec.normal.active_selected.label_background_color = "gold1";
  style_spec.normal.active_selected.label_extra_color = "gold3";


#if 1
  style_spec.normal.inactive_selected.highlight_color = "grey20";
  style_spec.normal.inactive_selected.shadow_color = "grey20";
  style_spec.normal.inactive_selected.padding_color = "black";
  style_spec.normal.inactive_selected.background_color = "black";
  style_spec.normal.inactive_selected.label_foreground_color = "black";
  style_spec.normal.inactive_selected.label_background_color = "gold3";
  style_spec.normal.inactive_selected.label_extra_color = "gold4";

  style_spec.normal.inactive.highlight_color = "grey20";
  style_spec.normal.inactive.shadow_color = "grey20";
  style_spec.normal.inactive.padding_color = "black";
  style_spec.normal.inactive.background_color = "black";
  style_spec.normal.inactive.label_foreground_color = "black";
  style_spec.normal.inactive.label_background_color = "grey85";
  style_spec.normal.inactive.label_extra_color = "grey70";
#else
  style_spec.normal.inactive_selected.highlight_color = "grey20";
  style_spec.normal.inactive_selected.shadow_color = "grey20";
  style_spec.normal.inactive_selected.padding_color = "black";
  style_spec.normal.inactive_selected.background_color = "black";
  style_spec.normal.inactive_selected.label_foreground_color = "black";
  style_spec.normal.inactive_selected.label_background_color = "grey85";
  style_spec.normal.inactive_selected.label_extra_color = "grey70";

  style_spec.normal.inactive.highlight_color = "grey20";
  style_spec.normal.inactive.shadow_color = "grey20";
  style_spec.normal.inactive.padding_color = "black";
  style_spec.normal.inactive.background_color = "black";
  style_spec.normal.inactive.label_foreground_color = "black";
  style_spec.normal.inactive.label_background_color = "grey75";
  style_spec.normal.inactive.label_extra_color = "grey60";
#endif

  {

    ascii_string royalblue1 = "#4169e1",
      royalblue2 = "#3e64d6",
      royalblue3 = "#3a5eca",
      royalblue4 = "#3759bf";
    style_spec.marked.active_selected.highlight_color = "gold1";
    style_spec.marked.active_selected.shadow_color = "gold1";
    style_spec.marked.active_selected.padding_color = "gold3";
    style_spec.marked.active_selected.background_color = "black";
    style_spec.marked.active_selected.label_foreground_color = "black";
    style_spec.marked.active_selected.label_background_color = "royalblue1";
    style_spec.marked.active_selected.label_extra_color = "royalblue3";
  
    style_spec.marked.inactive_selected.highlight_color = "grey20";
    style_spec.marked.inactive_selected.shadow_color = "grey20";
    style_spec.marked.inactive_selected.padding_color = "black";
    style_spec.marked.inactive_selected.background_color = "black";
    style_spec.marked.inactive_selected.label_foreground_color = "black";
    style_spec.marked.inactive_selected.label_background_color = "royalblue3";
    style_spec.marked.inactive_selected.label_extra_color = "royalblue4";

    style_spec.marked.inactive.highlight_color = "grey20";
    style_spec.marked.inactive.shadow_color = "grey20";
    style_spec.marked.inactive.padding_color = "black";
    style_spec.marked.inactive.background_color = "black";
    style_spec.marked.inactive.label_foreground_color = "black";
    style_spec.marked.inactive.label_background_color = "light steel blue";
    style_spec.marked.inactive.label_extra_color = "steel blue";

  }
  
  style_spec.highlight_pixels = 1;
  style_spec.shadow_pixels = 1;
  style_spec.padding_pixels = 1;
  style_spec.spacing = 2;
  style_spec.label_horizontal_padding = 5;
  style_spec.label_vertical_padding = 2;
  style_spec.label_component_spacing = 1;

  WBarStyle::Spec bar_style_spec;

  bar_style_spec.label_font = style_spec.label_font;
  bar_style_spec.highlight_pixels = 1;
  bar_style_spec.shadow_pixels = 1;
  bar_style_spec.padding_pixels = 1;
  bar_style_spec.spacing = 2;
  bar_style_spec.label_horizontal_padding = 5;
  bar_style_spec.label_vertical_padding = 2;
  bar_style_spec.cell_spacing = 2;
  bar_style_spec.highlight_color = "grey20";
  bar_style_spec.shadow_color = "grey20";
  bar_style_spec.padding_color = "black";
  bar_style_spec.background_color = "black";
  
  WM wm(argc, argv, xdisplay.display(), event_service,
        style_spec, bar_style_spec);

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
  wm.bind("mod4-mod1-d", toggle_decorated);
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
  
  wm.bind("mod4-x b", launch_browser_interactive);

    wm.bind("mod4-x c",
              boost::bind(&execute_shell_command, "~/bin/browser-clipboard"));

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

  WBarCellStyle::Spec def_bar_style;
  def_bar_style.foreground_color = "black";
  def_bar_style.background_color = "grey85";

  BarViewAppletStyle::Spec bar_view_info_style;

  bar_view_info_style.normal = def_bar_style;
  
  bar_view_info_style.selected.foreground_color = "black";
  bar_view_info_style.selected.background_color = "gold1";

  BarViewApplet bar_view_info(wm, bar_view_info_style);

  wm.bind("mod4-comma", boost::bind(&BarViewApplet::select_prev,
                                boost::ref(bar_view_info)));
  
  wm.bind("mod4-period", boost::bind(&BarViewApplet::select_next,
                                boost::ref(bar_view_info)));


  WBarCellStyle::Spec bat_inactive_style, bat_charging_style, bat_discharging_style;

  bat_inactive_style = def_bar_style;
  bat_charging_style = def_bar_style;
  bat_discharging_style.foreground_color = "white";
  bat_discharging_style.background_color = "grey25";

  BatteryApplet battery_applet(wm, bat_charging_style,
                               bat_discharging_style,
                               bat_inactive_style,
                               WBar::end(WBar::RIGHT));

  WBarCellStyle::Spec muted_style, unmuted_style;

  muted_style.foreground_color = "white";
  muted_style.background_color = "red";
  
  unmuted_style.foreground_color = "white";
  unmuted_style.background_color = "blue";

  /* TODO: check for exceptions */
  VolumeApplet volume_applet(wm, unmuted_style, muted_style,
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


  TimeApplet time_applet(wm, def_bar_style,
                         WBar::end(WBar::RIGHT));

  WBarCellStyle::Spec gnus_style;
  gnus_style.foreground_color = "black";
  gnus_style.background_color = "gold1";
  

  GnusApplet gnus_applet(wm, gnus_style,
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

  do
  {
    wm.flush();
    if (event_service.handle_events() != 0)
      ERROR_SYS("event service handle events");
  } while (1);
}
