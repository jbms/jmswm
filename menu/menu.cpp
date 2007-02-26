
#include <wm/all.hpp>
#include <boost/thread/thread.hpp>

const static long MENU_WINDOW_EVENT_MASK = ExposureMask | KeyPressMask;

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
  XSetWindowAttributes wa;
  wa.event_mask = MENU_WINDOW_EVENT_MASK;
  
  xwin_ = XCreateWindow(wm().display(), wm().root_window(),
                        0, 0, 100, 100,
                        0,
                        wm().default_depth(),
                        InputOutput,
                        wm().default_visual(),
                        CWEventMask, &wa);

  current_window_bounds.x = 0;
  current_window_bounds.y = 0;
  current_window_bounds.width = 100;
  current_window_bounds.height = 100;

  map_state = STATE_UNMAPPED;

  active = false;

  keyboard_grabbed = false;

  scheduled_update_server = false;
  scheduled_draw = false;

}

bool WMenu::read_string(const utf8_string &prompt,
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

  input.text.clear();
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
  // Fixme: don't use shaded_height()
  int height = wm().shaded_height();
  bounds.width = wm().screen_width();

  if (completions)
  {
    // FIXME: don't hardcode this 2/3 constant
    // Limit completions to 2/3 of remaining height
    int available_height = (wm().screen_height() - height) * 2 / 3;

    
    height += completions->compute_height(*this, bounds.width, available_height);
  }
  
  bounds.height = height;
  bounds.x = 0;
  bounds.y = wm().screen_height() - bounds.height;
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

  if (active && (scheduled_update_server || scheduled_draw))
  {
    draw();
  }

  scheduled_draw = false;
  scheduled_update_server = false;
}

void WMenu::draw()
{
  WDrawable &d = wm().buffer_pixmap.drawable();

  // FIXME: this is not the correct style to use
  WFrameStyle &style = wm().frame_style;

  // FIXME: this is not the correct style to use
  WFrameStyleSpecialized &substyle
    = style.normal.inactive;

  // Draw completions
  if (completions)
  {
    WRect rect(0, 0, bounds.width, bounds.height - wm_.shaded_height());

    completions->draw(*this, rect, d);
  }

  // Draw input area
  {
    WRect rect(0, bounds.height - wm_.shaded_height(), bounds.width, wm_.shaded_height());

    fill_rect(d, substyle.background_color, rect);

    draw_border(d, substyle.highlight_color, style.highlight_pixels,
                substyle.shadow_color, style.shadow_pixels,
                rect);

    WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                           style.shadow_pixels);

    draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

    WRect rect3 = rect2.inside_border(style.padding_pixels + style.spacing);
    rect3.height = wm().bar_height();

    //fill_rect(d, substyle.label_background_color, rect3);
    fill_rect(d, substyle.background_color, rect3);

    utf8_string text;
    text = prompt;

    int cursor_base = text.length();

    text += input.text;

    int actual_cursor_position = cursor_base + input.cursor_position;

    text += ' ';

    draw_label_with_cursor
      (d, text, style.label_font,
       substyle.label_background_color,
       substyle.label_foreground_color,
       substyle.label_background_color,
       rect3.inside_lr_tb_border(style.label_horizontal_padding,
                                 style.label_vertical_padding),
       actual_cursor_position);
  }

  XCopyArea(wm().display(), d.drawable(),
            xwin(),
            wm().dc.gc(),
            0, 0, bounds.width, bounds.height,
            0, 0);
}

static void menu_perform_completion(WMenu &menu)
{
  if (menu.completions)
  {
    if (menu.completions->complete(menu.input))
      menu.handle_input_changed();
    menu.scheduled_draw = true;
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

WMenuCompletions::~WMenuCompletions()
{
}
