#ifndef _MENU_HPP
#define _MENU_HPP

#include <util/string.hpp>

#include <wm/key.hpp>
#include <draw/draw.hpp>
#include <boost/function.hpp>

#include <util/event.hpp>

#include <boost/shared_ptr.hpp>

class WM;

class WMenuCompletions;

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

  bool completions_valid;
  typedef boost::shared_ptr<WMenuCompletions> Completions;
  Completions completions;
  int queued_completions;
  bool use_delay;
  bool use_separate_thread;

  class CompletionState
  {
  public:
    WMenu &menu;
    bool expired;
    bool recompute;
    
    CompletionState(WMenu &menu)
      : menu(menu), expired(false), recompute(false)
    {}
  };
  
  boost::shared_ptr<CompletionState> completion_state;
  
  Window xwin_;
  Window completions_xwin_;

  bool active;

  friend class WM;

  WKeyBindingContext bindctx;

  WRect current_window_bounds, current_completions_window_bounds;

  bool scheduled_update_server;
  bool scheduled_draw;

  bool keyboard_grabbed;

  enum map_state_t { STATE_MAPPED, STATE_UNMAPPED } map_state, completions_map_state;

  InputState input;
  utf8_string prompt;

  typedef enum { COMMAND_ENTER, COMMAND_CONTROL_ENTER } success_command_t;
  
  typedef boost::function<void (const utf8_string &, success_command_t)> SuccessAction;
  typedef boost::function<void (void)> FailureAction;
  typedef boost::function<Completions (const InputState &state)> Completer;
  
  Completer completer;
  SuccessAction success_action;
  FailureAction failure_action;

  WRect bounds, completions_bounds, input_rect, input_text_rect, prompt_rect, prompt_text_rect;

  // recompute input line and completion bounds
  void compute_bounds();

  // recompute just completions bounds
  void compute_completions_bounds();

  void initialize();

  void handle_input_changed();
  void update_completions();

  TimerEvent completion_delay;

  bool initialized;

public:

  WMenu(WM &wm_, const WModifierInfo &mod_info);
  
  Window xwin() { return xwin_; }
  Window completions_xwin() { return completions_xwin_; }

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
                   const Completer &completer = Completer(),
                   bool use_delay = true,
                   bool use_separate_thread = false);

  void handle_expose(const XExposeEvent &ev);
  void handle_screen_size_changed();
  void handle_keypress(const XKeyEvent &ev);

  void draw();

  void flush();

};

class WMenuCompletions
{
public:
  virtual void compute_dimensions(WMenu &menu, int width, int height, int &out_width, int &out_height) = 0;
  virtual void draw(WMenu &menu, const WRect &rect, WDrawable &d) = 0;

  /* Returns true if completions should be recomputed */
  virtual bool complete(WMenu::InputState &input) = 0;
  virtual ~WMenuCompletions();
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
