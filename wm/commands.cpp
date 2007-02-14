#include <wm/all.hpp>
#include <wm/commands.hpp>

void WCommandList::add(const ascii_string &name,
                       const Action &action)
{
  assert(commands.count(name) == 0);

  commands.insert(std::make_pair(name, action));
}

void WCommandList::get_completions(WMenu::CompletionList &list,
                                   const WMenu::InputState &s) const
{
  for (CommandMap::const_iterator it = commands.begin();
       it != commands.end();
       ++it)
  {
    if (boost::algorithm::starts_with(it->first, s.text))
    {
      WMenu::InputState state;
      state.text = it->first;
      state.cursor_position = state.text.size();
      list.push_back(std::make_pair(state.text, state));
    }
  }
}

void WCommandList::execute(WM &wm, const ascii_string &name) const
{
  CommandMap::const_iterator it = commands.find(name);
  if (it == commands.end())
  {
    WARN("invalid command: %s", name.c_str());
    return;
  }

  it->second(wm);
}

void WCommandList::execute_interactive(WM &wm) const
{
  wm.menu.read_string("Command: ",
                      boost::bind(&WCommandList::execute, this, boost::ref(wm), _1),
                      boost::function<void (void)>(),
                      boost::bind(&WCommandList::get_completions, this, _1, _2));
}

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

void toggle_decorated(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    frame->set_decorated(!frame->decorated());
  }
}

void toggle_bar_visible(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    view->set_bar_visible(!view->bar_visible());
  }
}

void toggle_marked(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    frame->set_marked(!frame->marked());
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

void execute_shell_command(const ascii_string &command)
{
  if (fork() == 0)
  {
    setsid();
    close(STDIN_FILENO);
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), (char *)0);
    exit(-1);
  }
}

utf8_string get_selected_cwd(WM &wm)
{
  utf8_string cwd;
  if (WFrame *frame = wm.selected_frame())
    cwd = frame->client().context_info();
  return cwd;
}

utf8_string resolve_cwd(const utf8_string &path)
{
  utf8_string cwd(path);
  if (!cwd.empty() && cwd[0] == '~')
  {
    cwd = "/home/jbms" + cwd.substr(1);
  }
  return cwd;
}

utf8_string make_path_pretty(const utf8_string &path)
{
  utf8_string context = path;
  if (context.find("/home/jbms") == 0)
    context = "~" + context.substr(10);
  return context;
}

void execute_shell_command_cwd(const ascii_string &command,
                               const utf8_string &cwd)
{
  if (fork() == 0)
  {
    if (!cwd.empty())
      chdir(resolve_cwd(cwd).c_str());
    setsid();
    close(STDIN_FILENO);
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), (char *)0);
    exit(-1);
  }
}

void execute_shell_command_selected_cwd(WM &wm, const ascii_string &command)
{
  return execute_shell_command_cwd(command, get_selected_cwd(wm));
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

void get_action_completions(WMenu::CompletionList &list,
                            const WMenu::InputState &s)
{
  WMenu::InputState state;

  state.text = "quit";
  state.cursor_position = state.text.size();
  list.push_back(std::make_pair(state.text, state));

  state.text = "restart";
  state.cursor_position = state.text.size();
  list.push_back(std::make_pair(state.text, state));
}

void execute_action_interactive(WM &wm)
{
  wm.menu.read_string("Action: ",
                      boost::bind(&execute_action, boost::ref(wm), _1),
                      boost::function<void (void)>(),
                      &get_action_completions);
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

void execute_shell_command_interactive(WM &wm)
{
  wm.menu.read_string("Command: ", &execute_shell_command,
                      boost::function<void (void)>());
}

void execute_shell_command_cwd_interactive(WM &wm)
{
  utf8_string cwd = get_selected_cwd(wm);
  if (cwd.empty())
  {
    char buf[256];
    buf[255] = 0;
    getcwd(buf, 256);
    cwd = make_path_pretty(buf);
  }
  
  wm.menu.read_string(cwd + " $ ",
                      boost::bind(&execute_shell_command_cwd, _1, cwd));
}

void switch_to_view(WM &wm, const utf8_string &name)
{
  WView *view = wm.view_by_name(name);
  if (!view)
  {
    if (!wm.valid_view_name(name))
      return;
    view = new WView(wm, name);
  }
  wm.select_view(view);
}

void switch_to_view_interactive(WM &wm)
{
  wm.menu.read_string("Switch to tag: ",
                      boost::bind(&switch_to_view, boost::ref(wm), _1));
}

void switch_to_view_by_letter(WM &wm, char c)
{
  for (WM::ViewMap::const_iterator it = wm.views().begin();
       it != wm.views().end();
       ++it)
  {
    if (it->first[0] == c)
    {
      wm.select_view(it->second);
      break;
    }
  }
}

void move_frame_to_view(const weak_iptr<WFrame> &weak_frame,
                        const utf8_string &view_name)
{
  if (WFrame *frame = weak_frame.get())
  {
    // TODO: make this logic a separate function, possibly of class WM
    WView *view = frame->wm().view_by_name(view_name);
    if (!view)
    {
      if (!frame->wm().valid_view_name(view_name))
        return;
      view = new WView(frame->wm(), view_name);
    }
    if (frame->client().frame_by_view(view))
      return;
    
    frame->remove();
    view->place_frame_in_smallest_column(frame);
  }
}


void move_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Move to view: ",
                        boost::bind(&move_frame_to_view, weak_iptr<WFrame>(frame),
                                    _1));
  }
}

void copy_frame_to_view(const weak_iptr<WFrame> &weak_frame,
                        const utf8_string &view_name)
{
  if (WFrame *frame = weak_frame.get())
  {
    // TODO: make this logic a separate function, possibly of class WM
    WView *view = frame->wm().view_by_name(view_name);
    if (!view)
    {
      if (!frame->wm().valid_view_name(view_name))
        return;
      view = new WView(frame->wm(), view_name);
    }
    if (frame->client().frame_by_view(view))
      return;

    WFrame *new_frame = new WFrame(frame->client());
    view->place_frame_in_smallest_column(new_frame);
  }
}

void copy_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Duplicate to view: ",
                        boost::bind(&copy_frame_to_view, weak_iptr<WFrame>(frame),
                                    _1));
  }
}

void remove_current_frame(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    if (frame->client().view_frames().size() > 1)
    {
      delete frame;
    }
  }
}
