#ifndef _WM_COMMANDS_HPP
#define _WM_COMMANDS_HPP

#include <wm/all.hpp>

class WM;

class WCommandList
{
private:
  typedef boost::function<void (WM &)> Action;
  typedef std::map<ascii_string, Action> CommandMap;
  CommandMap commands;

  WMenu::Completer completer() const;
  
public:
  void add(const ascii_string &name,
           const Action &action);
  
#define WM_COMMAND_ADD(name) add(#name, &name)

  void execute(WM &wm, const ascii_string &name) const;

  void execute_interactive(WM &wm) const;
};


void select_left(WM &wm);
void select_right(WM &wm);
void select_up(WM &wm);
void select_down(WM &wm);
void move_left(WM &wm);
void move_right(WM &wm);
void move_up(WM &wm);
void move_down(WM &wm);
void toggle_shaded(WM &wm);
void toggle_decorated(WM &wm);
void toggle_bar_visible(WM &wm);
void toggle_marked(WM &wm);
void increase_priority(WM &wm);
void decrease_priority(WM &wm);
void increase_column_priority(WM &wm);
void decrease_column_priority(WM &wm);
void close_current_client(WM &wm);
void kill_current_client(WM &wm);

void execute_shell_command(const ascii_string &command);


void execute_shell_command_cwd(const ascii_string &command,
                               const utf8_string &cwd);
void execute_shell_command_selected_cwd(WM &wm, const ascii_string &command);

void execute_shell_command_interactive(WM &wm);
void execute_shell_command_cwd_interactive(WM &wm);

void switch_to_view(WM &wm, const utf8_string &name);
void switch_to_view_interactive(WM &wm);
void switch_to_view_by_letter(WM &wm, char c);


void copy_current_frame_to_other_view_interactive(WM &wm);
void move_current_frame_to_other_view_interactive(WM &wm);
void remove_current_frame(WM &wm);

void move_next_by_activity_in_column(WM &wm);
void move_next_by_activity_in_view(WM &wm);
void move_next_by_activity(WM &wm);


void copy_marked_frames_to_current_view(WM &wm);
void move_marked_frames_to_current_view(WM &wm);

#endif /* _WM_COMMANDS_HPP */
