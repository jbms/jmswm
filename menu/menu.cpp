
#include <menu/menu.hpp>

#include <ctype.h>

#include <wm/wm.hpp>

const static long MENU_WINDOW_EVENT_MASK = ExposureMask | KeyPressMask;

WMenu::WMenu(WM &wm_, const WModifierInfo &mod_info)
  : wm_(wm_), bindctx(wm_, mod_info, None, false),
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
                 const boost::function<void (const utf8_string &)> &success_action,
                 const boost::function<void (void)> &failure_action)
{

  if (active)
    return false;

  active = true;

  this->success_action = success_action;
  this->failure_action = failure_action;

  current_input.clear();
  cursor_position = 0;

  this->prompt = prompt;

  scheduled_update_server = true;
  scheduled_draw = true;
  
  compute_bounds();

  return true;
}

void WMenu::compute_bounds()
{
  bounds.width = wm().screen_width();
  bounds.height = wm().shaded_height();
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
  WRect rect(0, 0, bounds.width, bounds.height);

  WFrameStyle &style = wm().frame_style;

  WFrameStyleSpecialized &substyle
    = style.inactive;
  
  fill_rect(d, substyle.background_color, rect);

  draw_border(d, substyle.highlight_color, style.highlight_pixels,
              substyle.shadow_color, style.shadow_pixels,
              rect);

  WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                         style.shadow_pixels);

  draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

  WRect rect3 = rect2.inside_border(style.padding_pixels + style.spacing);
  rect3.height = wm().bar_height();

  fill_rect(d, substyle.label_background_color, rect3);

  utf8_string text;
  text = prompt;

  if (!prompt.empty())
    text += ": ";

  int cursor_base = text.length();

  text += current_input;

  int actual_cursor_position = cursor_base + cursor_position;

  text += ' ';

  draw_label_with_cursor
    (d, text, style.label_font,
     substyle.label_foreground_color,
     substyle.label_background_color,
     substyle.label_foreground_color,
     rect3.inside_lr_tb_border(style.label_horizontal_padding,
                               style.label_vertical_padding),
     actual_cursor_position);

  XCopyArea(wm().display(), d.drawable(),
            xwin(),
            wm().dc.gc(),
            0, 0, bounds.width, bounds.height,
            0, 0);
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
      current_input.insert(cursor_position, buffer, 1);
      cursor_position++;

      scheduled_draw = true;
    }
  }
}

void menu_backspace(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;
  
  if (menu.cursor_position > 0)
  {
    menu.current_input.erase(menu.cursor_position - 1, 1);
    menu.cursor_position--;
    menu.scheduled_draw = true;
  }
}

void menu_enter(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.success_action)
    menu.success_action(menu.current_input);

  menu.active = false;
  
  menu.scheduled_update_server = true;
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
}

void menu_forward_char(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.cursor_position < menu.current_input.size())
  {
    ++menu.cursor_position;
    menu.scheduled_draw = true;
  }

}

void menu_backward_char(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.cursor_position > 0)
  {
    --menu.cursor_position;
    menu.scheduled_draw = true;
  }
}

void menu_beginning_of_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.cursor_position > 0)
  {
    menu.cursor_position = 0;
    menu.scheduled_draw = true;
  }
}

void menu_end_of_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  menu.cursor_position = menu.current_input.size();
  menu.scheduled_draw = true;
}

void menu_delete(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.cursor_position < menu.current_input.size())
  {
    menu.current_input.erase(menu.cursor_position, 1);
    menu.scheduled_draw = true;
  }
}

void menu_kill_line(WM &wm)
{
  WMenu &menu = wm.menu;

  if (!menu.active)
    return;

  if (menu.cursor_position < menu.current_input.size())
  {
    menu.current_input.erase(menu.cursor_position);
    menu.scheduled_draw = true;
  }
}
