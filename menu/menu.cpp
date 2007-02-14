
#include <wm/all.hpp>

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
                        const Completer &completer)
{

  if (active)
    return false;

  active = true;

  this->success_action = success_action;
  this->failure_action = failure_action;
  this->completer = completer;

  input.text.clear();
  input.cursor_position = 0;

  this->prompt = prompt;

  scheduled_update_server = true;
  scheduled_draw = true;

  completions.clear();
  
  compute_bounds();
  handle_input_changed();

  return true;
}

void WMenu::compute_bounds()
{
  // Fixme: don't use shaded_height()
  int height = wm().shaded_height();
  bounds.width = wm().screen_width();

  /* Determine how many columns to use */

  if (!completions.empty())
  {
    WFrameStyle &style = wm().frame_style;
    
    int maximum_width = 0;
    for (CompletionList::iterator it = completions.begin();
         it != completions.end();
         ++it)
    {
      if ((int)it->first.size() > maximum_width)
        maximum_width = (int)it->first.size();
    }

    int maximum_pixels = maximum_width * style.label_font.approximate_width();

    int available_width = bounds.width - style.highlight_pixels
      - style.shadow_pixels
      - 2 * style.padding_pixels;

    // FIXME: use a style entry instead of a constant here for padding
    int column_width = maximum_pixels + 10 + style.spacing * 2
      + 2 * style.label_horizontal_padding;

    if (column_width > available_width)
      column_width = available_width;

    completion_columns = available_width / column_width;

    completion_column_width = column_width;

    int line_height = style.label_vertical_padding * 2
      + style.label_font.height();

    int available_height = (wm_.screen_height() - height) * 2 / 3;

    int max_lines = available_height / line_height;

    completion_lines = (completions.size() + completion_columns - 1)
      / completion_columns;
    if (completion_lines > max_lines)
      completion_lines = max_lines;

    height += style.highlight_pixels + style.shadow_pixels
      + 2 * style.padding_pixels + 2 * style.spacing
      + completion_lines * line_height;
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
  {
    WRect rect(0, 0, bounds.width, bounds.height - wm_.shaded_height());

    WColor c(wm_.dc, "grey10");

    fill_rect(d, c, rect);

    draw_border(d, substyle.highlight_color, style.highlight_pixels,
                substyle.shadow_color, style.shadow_pixels,
                rect);

    WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                           style.shadow_pixels);

    draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

    
    int max_pos_index = completion_lines * completion_columns;

    int line_height = style.label_vertical_padding * 2
      + style.label_font.height();
    
    // FIXME: allow an offset
    if (max_pos_index > (int)completions.size())
      max_pos_index = (int)completions.size();

    int base_y = rect2.y + style.spacing;

    for (int pos_index = 0; pos_index < max_pos_index; ++pos_index)
    {
      int row = pos_index / completion_columns;
      int col = pos_index % completion_columns;

      WRect label_rect1(rect2.x + completion_column_width * col,
                       base_y + line_height * row,
                       completion_column_width, line_height);

      WRect label_rect2 = label_rect1.inside_border(style.spacing);

      WRect label_rect3 = label_rect2.inside_lr_tb_border
        (style.label_horizontal_padding,
         style.label_vertical_padding);

      if (pos_index == selected_completion)
      {
        const WColor &text_color = substyle.background_color;
        draw_label_with_text_background(d, completions[pos_index].first,
                                        style.label_font,
                                        text_color, substyle.label_background_color,
                                        label_rect3);
      } else
      {
        const WColor &text_color = substyle.label_background_color;        
        draw_label(d, completions[pos_index].first, style.label_font,
                   text_color, label_rect3);
      }
    }
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

void WMenu::update_completions()
{
  WARN("here");
  CompletionList new_list;

  completer(new_list, input);

  completions = new_list;
  completions_valid = true;
  scheduled_draw = true;
  scheduled_update_server = true;
  selected_completion = -1;
  compute_bounds();
}

void WMenu::handle_input_changed()
{
  scheduled_draw = true;

  if (completer.empty())
    return;
  
  completions_valid = false;
  selected_completion = -1;

  // 100ms delay
  completion_delay.wait_for(0, 100000);

  WARN("here");
}

void WMenu::handle_keypress(const XKeyEvent &ev)
{
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

void menu_enter(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.success_action)
    menu.success_action(menu.input.text);

  menu.active = false;
  
  menu.scheduled_update_server = true;

  menu.completion_delay.cancel();
}

void menu_abort(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.failure_action)
    menu.failure_action();

  menu.active = false;
  
  menu.scheduled_update_server = true;

  menu.completion_delay.cancel();
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

  if (!menu.completions_valid)
    return;

  if (menu.completions.empty())
    return;

  if (menu.selected_completion < 0)
    menu.selected_completion = 0;

  else
    menu.selected_completion =
      (menu.selected_completion + 1) % menu.completions.size();

  menu.input = menu.completions[menu.selected_completion].second;

  menu.scheduled_draw = true;
}
