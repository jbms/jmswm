#ifndef _MENU_MENU_HPP
#define _MENU_MENU_HPP

#include <util/string.hpp>

#include <wm/key.hpp>
#include <draw/draw.hpp>
#include <boost/function.hpp>

#include <util/event.hpp>

#include <boost/shared_ptr.hpp>
#include <style/style.hpp>
#include <style/common.hpp>

class WM;

namespace menu
{

  STYLE_DEFINITION(Style,
                   ((label, style::Label, style::Spec),
                    (border_pixels, int),
                    (border_color, WColor, ascii_string),
                    (prompt, style::TextColor, style::Spec),
                    (input, style::TextColor, style::Spec),
                    (selected, style::TextColor, style::Spec),
                    (cursor, style::TextColor, style::Spec),
                    (completions_background, WColor, ascii_string),
                    (completions_spacing, int)))
  
  class Completions;

  class InputState
  {
  public:
    InputState(const utf8_string &text)
      : text(text), cursor_position(text.size())
    {}
    InputState(const utf8_string &text, utf8_string::size_type cursor_position)
      : text(text), cursor_position(cursor_position)
    {}
    InputState() : cursor_position(0) {}
    utf8_string text;
    utf8_string::size_type cursor_position;
    bool operator==(const InputState &state) const
    {
      return text == state.text && cursor_position == state.cursor_position;
    }
    bool operator!=(const InputState &state) const
    {
      return !(*this == state);
    }
  };

  class InitialState
  {
  public:
    InitialState()
      : mark_position(-1) {}
    InitialState(const InputState &input_state, int mark_position = -1)
      : input_state(input_state), mark_position(mark_position) {}
    InputState input_state;
    int mark_position;
    static InitialState selected_prefix(const utf8_string &text);
    static InitialState selected_suffix(const utf8_string &text);
  };

  class Menu
  {
    WM &wm_;
    Style style_;
  public:


    const Style &style() const { return style_; }
    
    typedef boost::shared_ptr<Completions> CompletionsPtr;
    typedef enum { COMMAND_ENTER, COMMAND_CONTROL_ENTER } success_command_t;
  
    typedef boost::function<void (const utf8_string &, success_command_t)> SuccessAction;
    typedef boost::function<void (void)> FailureAction;
    typedef boost::function<CompletionsPtr (const InputState &state)> Completer;
    
  private:
    
    int mark_position;

    bool completions_valid;

    CompletionsPtr completions;
    int queued_completions;
    bool use_delay;
    bool use_separate_thread;

    class CompletionState
    {
    public:
      Menu &menu;
      bool expired;
      bool recompute;
    
      CompletionState(Menu &menu)
        : menu(menu), expired(false), recompute(false)
      {}
    };
  
    boost::shared_ptr<CompletionState> completion_state;
  
    Window xwin_;
    Window completions_xwin_;

    bool active;

    friend class WM;
  public:
    WKeyBindingContext bindctx;
  private:

    WRect current_window_bounds, current_completions_window_bounds;

    bool scheduled_update_server;
    bool scheduled_draw;

    bool keyboard_grabbed;

    enum map_state_t { STATE_MAPPED, STATE_UNMAPPED } map_state, completions_map_state;

    InputState input;
    utf8_string prompt;

  
    Completer completer;
    SuccessAction success_action;
    FailureAction failure_action;

    WRect bounds, completions_bounds, input_rect, input_text_rect, prompt_rect, prompt_text_rect;

    // recompute input line and completion bounds
    void compute_bounds();

    // recompute just completions bounds
    void compute_completions_bounds();

    void handle_input_changed();
    void update_completions();

    TimerEvent completion_delay;

    bool initialized;

    void inactivate();
    void perform_completion();
    void set_completions(const CompletionsPtr &);
    static void finished_updating_completions(const boost::weak_ptr<CompletionState> &completion_state,
                                              const CompletionsPtr &completions);
    static void update_completions_separate_thread(const boost::weak_ptr<CompletionState> &completion_state,
                                                   const Completer &completer,
                                                   const InputState &input,
                                                   EventService &event_service);

  public:
    void initialize();


    Menu(WM &wm_, const WModifierInfo &mod_info, const style::Spec &style_spec);
  
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
                     const InitialState &initial_state,
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


    void backspace();
    void enter();
    void abort();
    void forward_char();
    void backward_char();
    void beginning_of_line();
    void end_of_line();
    void delete_char();
    void kill_line();
    void complete();
    void set_mark();
  };

  class Completions
  {
  public:
    virtual void compute_dimensions(Menu &menu, int width, int height, int &out_width, int &out_height) = 0;
    virtual void draw(Menu &menu, const WRect &rect, WDrawable &d) = 0;

    /* Returns true if completions should be recomputed */
    virtual bool complete(InputState &input) = 0;
    virtual ~Completions();
  };
} // namespace menu

#endif /* _MENU_MENU_HPP */
