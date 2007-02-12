#ifndef _MENU_HPP
#define _MENU_HPP

#include <util/string.hpp>

#include <wm/key.hpp>
#include <draw/draw.hpp>
#include <boost/function.hpp>

#include <util/event.hpp>

#include <vector>

class WM;

class WMenu
{
public:
  WM &wm_;

  class InputState
  {
  public:
    utf8_string text;
    utf8_string::size_type cursor_position;
  };

  typedef std::pair<utf8_string, InputState> CompletionResult;
  typedef std::vector<CompletionResult> CompletionList;

  CompletionList completions;
  bool completions_valid;
  int selected_completion;
  int completion_columns;
  int completion_column_width;
  int completion_lines;
  
  Window xwin_;

  bool active;

  friend class WM;

  WKeyBindingContext bindctx;

  WRect current_window_bounds;

  bool scheduled_update_server;
  bool scheduled_draw;

  bool keyboard_grabbed;

  enum map_state_t { STATE_MAPPED, STATE_UNMAPPED } map_state;

  InputState input;
  utf8_string prompt;
  
  typedef boost::function<void (const utf8_string &)> SuccessAction;
  typedef boost::function<void (void)> FailureAction;
  typedef boost::function<void (CompletionList &list, const InputState &state)>
    Completer;

  Completer completer;
  SuccessAction success_action;
  FailureAction failure_action;

  WRect bounds;

  void compute_bounds();

  void initialize();

  void handle_input_changed();
  void update_completions();

  TimerEvent completion_delay;

  bool initialized;

public:

  WMenu(WM &wm_, const WModifierInfo &mod_info);
  
  Window xwin() { return xwin_; }

  WM &wm() { return wm_; }

  bool bind(const WKeySequence &seq, const KeyAction &action)
  {
    return bindctx.bind(seq, action);
  }
  
  bool unbind(const WKeySequence &seq)
  {
    return bindctx.unbind(seq);
  }

  bool read_string(const utf8_string &prompt,
                   const SuccessAction &success_action,
                   const FailureAction &failure_action = FailureAction(),
                   const Completer &completer = Completer());

  void handle_expose(const XExposeEvent &ev);
  void handle_screen_size_changed();
  void handle_keypress(const XKeyEvent &ev);

  void draw();

  void flush();

};

void menu_backspace(WM &wm);
void menu_enter(WM &wm);
void menu_abort(WM &wm);
void menu_forward_char(WM &wm);
void menu_backward_char(WM &wm);
void menu_beginning_of_line(WM &wm);
void menu_end_of_line(WM &wm);
void menu_delete(WM &wm);
void menu_kill_line(WM &wm);
void menu_complete(WM &wm);

#endif /* _MENU_HPP */
