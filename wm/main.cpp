
#include <wm/wm.hpp>
#include <draw/draw.hpp>
#include <util/log.hpp>
#include <util/close_on_exec.hpp>

#include <event.h>

#include <getopt.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>

#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>

void select_left(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    WView::iterator it = view->prior_column(view->selected_position(), true);
    view->select_column(it, true);
  }
}

void select_right(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    WView::iterator it = view->next_column(view->selected_position(), true);
    view->select_column(it, true);
  }
}

void select_up(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    if (WColumn *column = view->selected_column())
    {
      WColumn::iterator it = column->prior_frame(column->selected_position(), true);
      column->select_frame(it, true);
    }
  }
}

void select_down(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    if (WColumn *column = view->selected_column())
    {
      WColumn::iterator it = column->next_frame(column->selected_position(), true);
      column->select_frame(it, true);
    }
  }
}


void move_left(WM &wm)
{
  WARN("here");
  if (WFrame *frame = wm.selected_frame())
  {
    WView::iterator cur = frame->view()->make_iterator(frame->column());
    WView::iterator it = frame->view()->prior_column(cur, false);
    if (it == cur)
      it = frame->view()->create_column(frame->view()->columns.begin());
    frame->remove();
    it->add_frame(frame);
    frame->view()->select_frame(frame, true);
  }
}

void move_right(WM &wm)
{
  WARN("here");
  if (WFrame *frame = wm.selected_frame())
  {
    WView::iterator cur = frame->view()->make_iterator(frame->column());
    WView::iterator it = frame->view()->next_column(cur, false);
    if (it == cur)
      it = frame->view()->create_column(frame->view()->columns.end());
    frame->remove();
    it->add_frame(frame);
    frame->view()->select_frame(frame, true);
  }
}

void move_up(WM &wm)
{
  WARN("here");
  if (WFrame *frame = wm.selected_frame())
  {
    WColumn::iterator cur = frame->column()->make_iterator(frame);
    WColumn::iterator it;
    if (cur == frame->column()->frames.begin())
      it = frame->column()->frames.end();
    else
      it = frame->column()->prior_frame(cur, false);
    if (it != cur)
    {
      frame->column()->frames.splice(it, frame->column()->frames, cur);
      frame->column()->schedule_update_positions();
    }
  }
}

void move_down(WM &wm)
{
  WARN("here");
  if (WFrame *frame = wm.selected_frame())
  {
    WColumn::iterator cur = frame->column()->make_iterator(frame);
    WColumn::iterator it;

    if (boost::next(cur) == frame->column()->frames.end())
      it = frame->column()->frames.begin();
    else
      it = boost::next(boost::next(cur));
    
    if (it != cur)
    {
      frame->column()->frames.splice(it, frame->column()->frames, cur);
      frame->column()->schedule_update_positions();
    }
  }
}

void toggle_shaded(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    frame->set_shaded(!frame->shaded());
  }
}

void increase_priority(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    float priority = frame->priority();

    frame->set_priority(priority * 1.2);
  }
}

void decrease_priority(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    float priority = frame->priority();

    frame->set_priority(priority / 1.2);
  }
}

void increase_column_priority(WM &wm)
{
  if (WColumn *column = wm.selected_column())
  {
    float priority = column->priority();

    column->set_priority(priority * 1.2);
  }
}

void decrease_column_priority(WM &wm)
{
  if (WColumn *column = wm.selected_column())
  {
    float priority = column->priority();

    column->set_priority(priority / 1.2);
  }
}

void execute_command(const ascii_string &command)
{
  if (fork() == 0)
  {
    setsid();
    close(STDIN_FILENO);
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), 0);
    exit(-1);
  }
}

void execute_action(WM &wm, const ascii_string &command)
{
  if (command == "quit")
  {
    wm.quit();
  } else if (command == "restart")
  {
    wm.restart();
  }
}

void execute_action_interactive(WM &wm)
{
  wm.menu.read_string("Action",
                      boost::bind(&execute_action, boost::ref(wm), _1),
                      boost::function<void (void)>());
}


void close_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
    frame->client().request_close();
}

void kill_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
    frame->client().kill();
}

void execute_command_interactive(WM &wm)
{
  wm.menu.read_string("Command", &execute_command,
                      boost::function<void (void)>());
}

void switch_to_view(WM &wm, const utf8_string &name)
{
  WView *view = wm.view_by_name(name);
  if (!view)
  {
    view = new WView(wm, name);
  }
  wm.select_view(view);
}

void switch_to_view_interactive(WM &wm)
{
  wm.menu.read_string("Switch to tag",
                      boost::bind(&switch_to_view, boost::ref(wm), _1));
}

int main(int argc, char **argv)
{

  char const *display_name = NULL;

#if 0
  {
    static struct option long_options[] =
      {
        {"display", 1, 0, 'd'},
        {0, 0, 0, 0}
      };
    
    while (1)
    {
      char c = getopt_long_only(argc, argv, "d:", long_options, NULL);
      if (c == -1)
        break;
      switch (c)
      {
      case 'd':
        display_name = optarg;
        break;
      case '?':
        exit(EXIT_FAILURE);
      }
    }

    if (optind < argc)
      ERROR("extra command-line arguments specified");
  }
#endif 

  /* Set up child handling */
  {
    struct sigaction act;
    act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    act.sa_handler = SIG_IGN;
    while (sigaction(SIGCHLD, &act, 0) != 0 && errno == EINTR);
  }
  
  WXDisplay xdisplay(display_name);
  set_close_on_exec_flag(ConnectionNumber(xdisplay.display()), true);
  
  event_base *eb = static_cast<event_base *>(event_init());

  // Style

  WFrameStyle::Spec style_spec;
  
  style_spec.label_font = "fixed 11";
  style_spec.client_background_color = "black";

  style_spec.active_selected.highlight_color = "gold1";
  style_spec.active_selected.shadow_color = "gold1";
  style_spec.active_selected.padding_color = "gold3";
  style_spec.active_selected.background_color = "black";
  style_spec.active_selected.label_foreground_color = "black";
  style_spec.active_selected.label_background_color = "gold1";

  style_spec.inactive_selected.highlight_color = "grey20";
  style_spec.inactive_selected.shadow_color = "grey20";
  style_spec.inactive_selected.padding_color = "black";
  style_spec.inactive_selected.background_color = "black";
  style_spec.inactive_selected.label_foreground_color = "black";
  style_spec.inactive_selected.label_background_color = "gold3";

  style_spec.inactive.highlight_color = "grey20";
  style_spec.inactive.shadow_color = "grey20";
  style_spec.inactive.padding_color = "black";
  style_spec.inactive.background_color = "black";
  style_spec.inactive.label_foreground_color = "black";
  style_spec.inactive.label_background_color = "grey85";

  style_spec.highlight_pixels = 1;
  style_spec.shadow_pixels = 1;
  style_spec.padding_pixels = 1;
  style_spec.spacing = 3;
  style_spec.label_horizontal_padding = 2;
  style_spec.label_vertical_padding = 2;

  WBarStyle::Spec bar_style_spec;

  bar_style_spec.label_font = style_spec.label_font;
  bar_style_spec.highlight_pixels = 1;
  bar_style_spec.shadow_pixels = 1;
  bar_style_spec.padding_pixels = 1;
  bar_style_spec.spacing = 3;
  bar_style_spec.label_horizontal_padding = 2;
  bar_style_spec.label_vertical_padding = 2;
  bar_style_spec.cell_spacing = 1;
  bar_style_spec.highlight_color = "grey20";
  bar_style_spec.shadow_color = "grey20";
  bar_style_spec.padding_color = "black";
  bar_style_spec.background_color = "black";
  
  WM wm(argc, argv, xdisplay.display(), eb, style_spec, bar_style_spec);

  wm.bind("mod4-f", select_right);
  wm.bind("mod4-b", select_left);
  wm.bind("mod4-p", select_up);
  wm.bind("mod4-n", select_down);

  wm.bind("mod4-k any-f", move_right);
  wm.bind("mod4-k any-b", move_left);
  wm.bind("mod4-k any-p", move_up);
  wm.bind("mod4-k any-n", move_down);
  
  wm.bind("mod4-d", toggle_shaded);
  wm.bind("mod4-u", decrease_priority);
  wm.bind("mod4-i", increase_priority);

  wm.bind("mod4-k any-u", decrease_column_priority);
  wm.bind("mod4-k any-i", increase_column_priority);

  wm.bind("mod4-x x",
              boost::bind(&execute_command, "xterm"));

  wm.bind("mod4-x mod4-x", execute_command_interactive);

  wm.bind("mod4-a", execute_action_interactive);

  wm.bind("mod4-t", switch_to_view_interactive);

  wm.bind("mod4-x e",
              boost::bind(&execute_command, "~/bin/emacs"));

  wm.bind("mod4-c", close_current_client);
  wm.bind("mod4-k c", kill_current_client);

  wm.menu.bind("BackSpace", menu_backspace);
  wm.menu.bind("Return", menu_enter);
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

  /* Bar test */

  WColor background(wm.dc, "gray80");
  WColor foreground(wm.dc, "black");

  WBar::CellRef cellref
    = wm.bar.insert_end(WBar::RIGHT, foreground, background, "2007-01-01");

  
  event_base_dispatch(eb);

  return EXIT_FAILURE;
}
