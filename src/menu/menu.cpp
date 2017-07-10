
#include <wm/all.hpp>
#include <boost/thread/thread.hpp>

namespace menu
{

  InitialState InitialState::selected_prefix(const utf8_string &text)
  {
    return InitialState(InputState(text, text.size()), 0);
  }

  InitialState InitialState::selected_suffix(const utf8_string &text)
  {
    return InitialState(InputState(text, 0), text.size());
  }

  const static long MENU_WINDOW_EVENT_MASK = ExposureMask | KeyPressMask;
  const static long COMPLETIONS_WINDOW_EVENT_MASK = ExposureMask;

  Menu::Menu(WM &wm_, const WModifierInfo &mod_info, const style::Spec &style_spec)
    : wm_(wm_),
      style_(wm_.dc, style_spec),
      completions_valid(false),
      bindctx(wm_, mod_info, None, false),
      completion_delay(wm_.event_service(),
                       boost::bind(&Menu::update_completions, this)),
      initialized(false)
  {}

  void Menu::initialize()
  {
    // FIXME: Set override_redirect to True as a convenient way to make
    // load_state_from_server ignore these windows.
    XSetWindowAttributes wa;
    wa.event_mask = MENU_WINDOW_EVENT_MASK;
    wa.save_under = True;
    wa.override_redirect = True;

    xwin_ = XCreateWindow(wm().display(), wm().root_window(),
                          0, 0, 100, 100,
                          0,
                          wm().default_depth(),
                          InputOutput,
                          wm().default_visual(),
                          CWEventMask | CWSaveUnder | CWOverrideRedirect, &wa);

    wa.event_mask = COMPLETIONS_WINDOW_EVENT_MASK;

    completions_xwin_ = XCreateWindow(wm().display(), wm().root_window(),
                                      0, 0, 100, 100,
                                      0,
                                      wm().default_depth(),
                                      InputOutput,
                                      wm().default_visual(),
                                      CWEventMask | CWSaveUnder | CWOverrideRedirect, &wa);

    current_window_bounds.x = 0;
    current_window_bounds.y = 0;
    current_window_bounds.width = 100;
    current_window_bounds.height = 100;

    current_completions_window_bounds = current_window_bounds;

    map_state = completions_map_state = STATE_UNMAPPED;

    active = false;

    keyboard_grabbed = false;

    scheduled_update_server = false;
    scheduled_draw = false;

  }

  bool Menu::read_string(const utf8_string &prompt,
                         const InitialState &initial_state,
                         const SuccessAction &success_action,
                         const FailureAction &failure_action,
                         const Completer &completer,
                         bool use_delay,
                         bool use_separate_thread)
  {
    if (active)
      return false;

    active = true;

    this->success_action = success_action;
    this->failure_action = failure_action;
    this->completer = completer;

    this->use_delay = use_delay;
    this->use_separate_thread = use_separate_thread;

    input = initial_state.input_state;
    mark_position = initial_state.mark_position;

    this->prompt = prompt;

    scheduled_update_server = true;
    scheduled_draw = true;
    queued_completions = 0;

    compute_bounds();
    handle_input_changed();

    return true;
  }

  void Menu::compute_bounds()
  {
    // FIXME: don't use shaded_height()
    int height = wm().shaded_height();
    bounds.width = wm().screen_width();
    bounds.height = height;
    bounds.x = 0;
    bounds.y = wm().screen_height() - bounds.height;

    WRect inner_rect = bounds.inside_border(style().border_pixels);
    if (!prompt.empty())
    {
      int available_text_width = inner_rect.width - 2 * style().label.horizontal_padding;
      int prompt_width = compute_label_width(wm().buffer_pixmap.drawable(),
                                             prompt,
                                             style().label.font,
                                             available_text_width);
      prompt_rect = inner_rect;
      prompt_rect.width = prompt_width + 2 * style().label.horizontal_padding;
      prompt_text_rect = prompt_rect.inside_lr_tb_border(style().label.horizontal_padding,
                                                         style().label.vertical_padding);
      if (prompt_text_rect.width < available_text_width * 4 / 5) {
        // Heuristic: if the prompt is not using the full width, provide a few extra pixels to avoid
        // spurious ellipses.
        prompt_text_rect.width += 5;
      }
      input_rect = inner_rect;
      input_rect.x += prompt_rect.width;
      input_rect.width -= prompt_rect.width;
    } else
    {
      input_rect = inner_rect;
    }
    input_text_rect = input_rect.inside_lr_tb_border(style().label.horizontal_padding,
                                                     style().label.vertical_padding);
    compute_completions_bounds();
  }

  void Menu::compute_completions_bounds()
  {
    if (completions)
    {
      // FIXME: don't hardcode this constant
      // Limit completions to a fraction of remaining height
      int available_height = (wm().screen_height() - bounds.height) * 3 / 8;

      // FIXME: don't hardcode this constant
      int width = bounds.width / 2;

      int out_width, out_height;

      completions->compute_dimensions(*this, width, available_height, out_width, out_height);

      completions_bounds.height = out_height;
      completions_bounds.width = out_width;
      completions_bounds.x = input_rect.x - style().border_pixels;
      completions_bounds.y = bounds.y - out_height;
    }
  }

  void Menu::handle_expose(const XExposeEvent &ev)
  {
    if (!active)
      return;

    if (ev.count)
      return;

    scheduled_draw = true;
  }

  void Menu::handle_screen_size_changed()
  {
    if (active)
    {
      compute_bounds();
      scheduled_update_server = true;
    }
  }

  void Menu::flush()
  {
    if (scheduled_update_server)
    {
      if (active)
      {
        if (current_window_bounds != bounds)
        {
          XMoveResizeWindow(wm().display(), xwin_,
                            bounds.x, bounds.y,
                            bounds.width, bounds.height);
          current_window_bounds = bounds;
        }

        if (map_state != STATE_MAPPED)
        {
          XMapRaised(wm().display(), xwin_);
          map_state = STATE_MAPPED;
        }

        if (completions)
        {

          if (current_completions_window_bounds != completions_bounds)
          {
            XMoveResizeWindow(wm().display(), completions_xwin_,
                              completions_bounds.x, completions_bounds.y,
                              completions_bounds.width, completions_bounds.height);
            current_completions_window_bounds = completions_bounds;
          }

          if (completions_map_state != STATE_MAPPED)
          {
            XMapRaised(wm().display(), completions_xwin_);
            completions_map_state = STATE_MAPPED;
          }
        }

        if (!keyboard_grabbed)
        {
          if (XGrabKeyboard(wm().display(), xwin(), false,
                            GrabModeAsync, GrabModeAsync, CurrentTime) == Success)
            keyboard_grabbed = true;
          else
          {
            DEBUG("failed to grab keyboard");
          }
        }
      } else
      {
        if (keyboard_grabbed)
        {
          XUngrabKeyboard(wm().display(), CurrentTime);
          keyboard_grabbed = false;
        }
        if (map_state != STATE_UNMAPPED)
        {
          XUnmapWindow(wm().display(), xwin_);
          map_state = STATE_UNMAPPED;
        }
      }

      if (!active || !completions)
      {
        if (completions_map_state != STATE_UNMAPPED)
        {
          XUnmapWindow(wm().display(), completions_xwin_);
          completions_map_state = STATE_UNMAPPED;
        }
      }
    }

    if (active && (scheduled_update_server || scheduled_draw))
    {
      draw();
    }

    scheduled_draw = false;
    scheduled_update_server = false;
  }

  void Menu::draw()
  {
    WDrawable &d = wm().buffer_pixmap.drawable();

    // Draw completions
    if (completions)
    {
      WRect rect(0, 0, completions_bounds.width, completions_bounds.height);

      completions->draw(*this, rect, d);
      XCopyArea(wm().display(), d.drawable(),
                completions_xwin(),
                wm().dc.gc(),
                0, 0, completions_bounds.width, completions_bounds.height,
                0, 0);
    }

    // Draw input area
    {
      draw_border(d, style().border_color, style().border_pixels, bounds);

      // draw prompt area
      if (!prompt.empty())
      {
        fill_rect(d, style().prompt.background, prompt_rect);
        draw_label(d, prompt, style().label.font, style().prompt.foreground, prompt_text_rect);
      }

      fill_rect(d, style().input.background, input_rect);

      utf8_string text;
      text += input.text;

      text += ' ';

      draw_label_with_cursor_and_selection
        (d, text, style().label.font,
         style().input.foreground,
         style().cursor.foreground, style().cursor.background,
         style().selected.foreground, style().selected.background,
         input_text_rect, input.cursor_position, mark_position);

      XCopyArea(wm().display(), d.drawable(),
                xwin(),
                wm().dc.gc(),
                bounds.x, bounds.y, bounds.width, bounds.height,
                0, 0);
    }
  }

  void Menu::perform_completion()
  {
    if (completions)
    {
      if (completions->complete(input))
        handle_input_changed();
      scheduled_draw = true;
      mark_position = -1;
    }
  }

  void Menu::set_completions(const Menu::CompletionsPtr &completions)
  {
    this->completions = completions;
    completions_valid = true;
    scheduled_draw = true;
    scheduled_update_server = true;
    compute_bounds();

    while (queued_completions > 0)
    {
      --queued_completions;
      perform_completion();
    }
  }


  void Menu::finished_updating_completions(const boost::weak_ptr<Menu::CompletionState> &completion_state,
                                           const Menu::CompletionsPtr &completions)
  {
    if (boost::shared_ptr<CompletionState> s = completion_state.lock())
    {
      s->menu.completion_state.reset();
      if (s->recompute)
        s->menu.update_completions();
      else if (!s->expired)
        s->menu.set_completions(completions);
      /* TODO: decide if expired completions should also be shown somehow. */
    }
  }

  void Menu::update_completions_separate_thread(const boost::weak_ptr<Menu::CompletionState> &completion_state,
                                                const Menu::Completer &completer,
                                                const InputState &input,
                                                EventService &event_service)
  {
    CompletionsPtr completions(completer(input));

    event_service.post(boost::bind(&Menu::finished_updating_completions, completion_state, completions));
  }

  void Menu::update_completions()
  {
    if (use_separate_thread)
    {
      if (completion_state)
      {
        completion_state->recompute = true;
      } else
      {
        completion_state.reset(new CompletionState(*this));
        boost::thread(boost::bind(&Menu::update_completions_separate_thread,
                                  boost::weak_ptr<CompletionState>(completion_state),
                                  completer,
                                  input,
                                  boost::ref(wm().event_service())));
      }
    } else
    {
      set_completions(completer(input));
    }
  }

  void Menu::handle_input_changed()
  {
    scheduled_draw = true;

    if (completer.empty())
      return;

    completions_valid = false;

    if (completion_state)
      completion_state->expired = true;

    if (use_delay)
    {
      // 100ms delay
      completion_delay.wait(time_duration::milliseconds(100));
    } else
    {
      update_completions();
    }
  }

  void Menu::handle_keypress(const XKeyEvent &ev)
  {
    if (!active)
      return;

    if (bindctx.process_keypress(ev))
      return;

    const int BUFFER_SIZE = 32;

    char buffer[BUFFER_SIZE];

    XKeyEvent ev_tmp = ev;

    int count = XLookupString(&ev_tmp, buffer, BUFFER_SIZE, 0, 0);

    if (count == 1)
    {
      char c = buffer[0];

      if (isgraph(c) || c == ' ')
      {
        if (mark_position != -1 && mark_position > (int)input.cursor_position)
          input.text.erase(input.cursor_position, mark_position - input.cursor_position);
        mark_position = -1;
        input.text.insert(input.cursor_position, buffer, 1);
        input.cursor_position++;

        handle_input_changed();
      }
    }
  }

  void Menu::backspace()
  {
    if (!active)
      return;

    if (input.cursor_position > 0)
    {
      if (mark_position != -1 && (int)input.cursor_position > mark_position)
      {
        size_t erase_count = input.cursor_position - (size_t)mark_position;
        input.text.erase(mark_position, erase_count);
        mark_position = -1;
        input.cursor_position -= erase_count;
        handle_input_changed();
      } else
      {
        input.text.erase(input.cursor_position - 1, 1);
        input.cursor_position--;
        handle_input_changed();
      }
    }
  }

  void Menu::inactivate()
  {
    if (active)
    {
      active = false;
      scheduled_update_server = true;
      completion_delay.cancel();
      success_action.clear();
      failure_action.clear();
      completer.clear();
      completions.reset();
      completion_state.reset();
    }
  }

  void Menu::enter()
  {
    if (!active)
      return;

    if (success_action)
      success_action(input.text, Menu::COMMAND_ENTER);

    inactivate();
  }

  void Menu::abort()
  {
    if (!active)
      return;

    if (failure_action)
      failure_action();

    inactivate();
  }

  void Menu::forward_char()
  {
    if (!active)
      return;

    if (input.cursor_position < input.text.size())
    {
      ++input.cursor_position;
      handle_input_changed();
    }

  }

  void Menu::backward_char()
  {
    if (!active)
      return;

    if (input.cursor_position > 0)
    {
      --input.cursor_position;
      handle_input_changed();
    }
  }

  void Menu::beginning_of_line()
  {
    if (!active)
      return;

    if (input.cursor_position > 0)
    {
      input.cursor_position = 0;
      handle_input_changed();
    }
  }

  void Menu::end_of_line()
  {
    if (!active)
      return;

    input.cursor_position = input.text.size();
    handle_input_changed();
  }

  void Menu::delete_char()
  {
    if (!active)
      return;

    if (input.cursor_position < input.text.size())
    {
      if (mark_position != -1 && mark_position > (int)input.cursor_position)
        input.text.erase(input.cursor_position, mark_position - input.cursor_position);
      mark_position = -1;
      input.text.erase(input.cursor_position, 1);
      handle_input_changed();
    }
  }

  void Menu::kill_line()
  {
    if (!active)
      return;

    if (input.cursor_position < input.text.size())
    {
      input.text.erase(input.cursor_position);
      handle_input_changed();
    }
  }

  void Menu::complete()
  {
    if (!active)
      return;

    if (completer)
    {
      if (completions_valid)
        perform_completion();
      else
        ++queued_completions;
    }
  }

  void Menu::set_mark()
  {
    if (!active)
      return;

    mark_position = input.cursor_position;
    scheduled_draw = true;
  }

  Completions::~Completions()
  {
  }

}
