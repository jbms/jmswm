
#include <wm/all.hpp>
#include <boost/thread/thread.hpp>

const static long MENU_WINDOW_EVENT_MASK = ExposureMask | KeyPressMask;
const static long COMPLETIONS_WINDOW_EVENT_MASK = ExposureMask;

WMenu::WMenu(WM &wm_, const WModifierInfo &mod_info)
  : wm_(wm_),
    completions_valid(false),
    bindctx(wm_, mod_info, None, false),
    completion_delay(wm_.event_service(),
                     boost::bind(&WMenu::update_completions, this)),
    initialized(false)
{}

void WMenu::initialize()
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

bool WMenu::read_string(const utf8_string &prompt,
                        const utf8_string &initial_text,
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

  input.text = initial_text;
  if (!initial_text.empty())
    mark_position = initial_text.length();
  else
    mark_position = -1;
  
  input.cursor_position = 0;


  this->prompt = prompt;

  scheduled_update_server = true;
  scheduled_draw = true;
  queued_completions = 0;
  
  compute_bounds();
  handle_input_changed();

  return true;
}

void WMenu::compute_bounds()
{
  // FIXME: don't use shaded_height()
  int height = wm().shaded_height();
  bounds.width = wm().screen_width();
  bounds.height = height;
  bounds.x = 0;
  bounds.y = wm().screen_height() - bounds.height;

  // FIXME: this is not the correct style to use
  WFrameStyle &style = wm().frame_style;

  WRect inner_rect = bounds.inside_tl_br_border(style.highlight_pixels,
                                                style.shadow_pixels);
  if (!prompt.empty())
  {
    // FIXME: don't use frame_style, instead use the proper style
    // FIXME: don't use bounds.width, instead use the proper width
    int prompt_width = compute_label_width(wm().buffer_pixmap.drawable(),
                                           prompt,
                                           style.label_font,
                                           inner_rect.width - 2 * style.label_horizontal_padding);
    prompt_rect = inner_rect;
    prompt_rect.width = prompt_width + 2 * style.label_horizontal_padding;
    prompt_text_rect = prompt_rect.inside_lr_tb_border(style.label_horizontal_padding,
                                                       style.label_vertical_padding);
    input_rect = inner_rect;
    input_rect.x += prompt_rect.width;
    input_rect.width -= prompt_rect.width;
  } else
  {
    input_rect = inner_rect;
  }
  input_text_rect = input_rect.inside_lr_tb_border(style.label_horizontal_padding,
                                                   style.label_vertical_padding);
  compute_completions_bounds();
}

void WMenu::compute_completions_bounds()
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

    // FIXME: this is not the correct style to use
    WFrameStyle &style = wm().frame_style;

    completions_bounds.height = out_height;
    completions_bounds.width = out_width;
    completions_bounds.x = input_rect.x - style.highlight_pixels;
    completions_bounds.y = bounds.y - out_height;
  }
}

void WMenu::handle_expose(const XExposeEvent &ev)
{
  if (!active)
    return;

  if (ev.count)
    return;
  
  scheduled_draw = true;
}

void WMenu::handle_screen_size_changed()
{
  if (active)
  {
    compute_bounds();
    scheduled_update_server = true;
  }
}

void WMenu::flush()
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
  }

  if (!active || !completions)
  {
    if (completions_map_state != STATE_UNMAPPED)
    {
      XUnmapWindow(wm().display(), completions_xwin_);
      completions_map_state = STATE_UNMAPPED;
    }
  }

  if (active && (scheduled_update_server || scheduled_draw))
  {
    draw();
  }

  scheduled_draw = false;
  scheduled_update_server = false;
}

// FIXME: don't duplicate this here
/*
static ascii_string rgb(unsigned char r, unsigned char g, unsigned char b)
{
  char buf[30];
  sprintf(buf, "#%02x%02x%02x", r, g, b);
  return ascii_string(buf);
}
*/

void WMenu::draw()
{
  WDrawable &d = wm().buffer_pixmap.drawable();

  // FIXME: this is not the correct style to use
  WFrameStyle &style = wm().frame_style;

  // FIXME: this is not the correct style to use
  //WFrameStyleSpecialized &substyle
  //  = style.normal.inactive;

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
    //WColor red_color(d.draw_context(), rgb(200,50,30));

    WColor white_color(d.draw_context(), "white");

    draw_border(d, white_color, style.highlight_pixels,
                white_color, style.shadow_pixels,
                bounds);

    // draw prompt area
    if (!prompt.empty())
    {
      WColor prompt_bg(d.draw_context(), "grey30");
      WColor prompt_fg(d.draw_context(), "white");
      fill_rect(d, prompt_bg, prompt_rect);
      draw_label(d, prompt, style.label_font, prompt_fg, prompt_text_rect);
    }


    WColor input_bg(d.draw_context(), "black");
    WColor input_fg(d.draw_context(), "grey85");

    fill_rect(d, input_bg, input_rect);

    utf8_string text;
    text += input.text;

    text += ' ';

    WColor selection_bg(d.draw_context(), "blue");
    WColor selection_fg(d.draw_context(), "grey85");

    draw_label_with_cursor_and_selection
      (d, text, style.label_font, input_fg, input_bg, input_fg,
       selection_fg, selection_bg,
       input_text_rect, input.cursor_position, mark_position);
    
    XCopyArea(wm().display(), d.drawable(),
              xwin(),
              wm().dc.gc(),
              bounds.x, bounds.y, bounds.width, bounds.height,
              0, 0);
  }
}

static void menu_perform_completion(WMenu &menu)
{
  if (menu.completions)
  {
    if (menu.completions->complete(menu.input))
      menu.handle_input_changed();
    menu.scheduled_draw = true;
    menu.mark_position = -1;
  }
}

static void set_menu_completions(WMenu &menu, const WMenu::Completions &completions)
{
  menu.completions = completions;
  menu.completions_valid = true;
  menu.scheduled_draw = true;
  menu.scheduled_update_server = true;
  menu.compute_bounds();

  while (menu.queued_completions > 0)
  {
    --menu.queued_completions;
    menu_perform_completion(menu);
  }
}


static void finished_updating_completions(const boost::weak_ptr<WMenu::CompletionState> &completion_state,
                                          const WMenu::Completions &completions)
{
  if (boost::shared_ptr<WMenu::CompletionState> s = completion_state.lock())
  {
    s->menu.completion_state.reset();
    if (s->recompute)
      s->menu.update_completions();
    else if (!s->expired)
      set_menu_completions(s->menu, completions);
    /* TODO: decide if expired completions should also be shown somehow. */
  }
}

static void update_completions_separate_thread(const boost::weak_ptr<WMenu::CompletionState> &completion_state,
                                               const WMenu::Completer &completer,
                                               const WMenu::InputState &input,
                                               EventService &event_service)
{
  WMenu::Completions completions(completer(input));

  event_service.post(boost::bind(&finished_updating_completions, completion_state, completions));
}



void WMenu::update_completions()
{
  if (use_separate_thread)
  {
    if (completion_state)
    {
      completion_state->recompute = true;
    } else
    {
      completion_state.reset(new CompletionState(*this));
      boost::thread(boost::bind(&update_completions_separate_thread,
                                boost::weak_ptr<CompletionState>(completion_state),
                                completer,
                                input,
                                boost::ref(wm().event_service())));
    }
  } else
  {
    Completions new_completions(completer(input));
    set_menu_completions(*this, new_completions);
  }
}

void WMenu::handle_input_changed()
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

void WMenu::handle_keypress(const XKeyEvent &ev)
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

void menu_backspace(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;
  
  if (menu.input.cursor_position > 0)
  {
    menu.input.text.erase(menu.input.cursor_position - 1, 1);
    menu.input.cursor_position--;
    menu.handle_input_changed();
  }
}

static void menu_inactivate(WMenu &menu)
{
  if (menu.active)
  {
    menu.active = false;
    menu.scheduled_update_server = true;
    menu.completion_delay.cancel();
    menu.success_action.clear();
    menu.failure_action.clear();
    menu.completer.clear();
    menu.completions.reset();
    menu.completion_state.reset();
  }
}

void menu_enter(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.success_action)
    menu.success_action(menu.input.text, WMenu::COMMAND_ENTER);

  menu_inactivate(menu);
}

void menu_abort(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.failure_action)
    menu.failure_action();

  menu_inactivate(menu);
}

void menu_forward_char(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.input.cursor_position < menu.input.text.size())
  {
    ++menu.input.cursor_position;
    menu.handle_input_changed();
  }

}

void menu_backward_char(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.input.cursor_position > 0)
  {
    --menu.input.cursor_position;
    menu.handle_input_changed();
  }
}

void menu_beginning_of_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.input.cursor_position > 0)
  {
    menu.input.cursor_position = 0;
    menu.handle_input_changed();
  }
}

void menu_end_of_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  menu.input.cursor_position = menu.input.text.size();
  menu.handle_input_changed();
}

void menu_delete(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.input.cursor_position < menu.input.text.size())
  {
    menu.input.text.erase(menu.input.cursor_position, 1);
    menu.handle_input_changed();
  }
}

void menu_kill_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.input.cursor_position < menu.input.text.size())
  {
    menu.input.text.erase(menu.input.cursor_position);
    menu.handle_input_changed();
  }
}

void menu_complete(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.completer)
  {
    if (menu.completions_valid)
      menu_perform_completion(menu);
    else
      ++menu.queued_completions;
  }
}

void menu_set_mark(WM &wm)
{
  WMenu &menu = wm.menu;
  if (!menu.active)
    return;

  menu.mark_position = menu.input.cursor_position;
  menu.scheduled_draw = true;
}

WMenuCompletions::~WMenuCompletions()
{
}
