#include <wm/all.hpp>
#include <wm/commands.hpp>
#include <menu/list_completion.hpp>
#include <util/spawn.hpp>
#include <util/path.hpp>
#include <wm/extra/cwd.hpp>
#include <wm/extra/place.hpp>

void WCommandList::add(const ascii_string &name,
                       const Action &action)
{
  assert(commands.count(name) == 0);

  commands.insert(std::make_pair(name, action));
}

WMenu::Completer WCommandList::completer() const
{
  return prefix_completer(boost::make_transform_range(commands, select1st));
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
  wm.menu.read_string("Command:", "",
                      boost::bind(&WCommandList::execute, this, boost::ref(wm), _1),
                      boost::function<void (void)>(),
                      completer(),
                      false /* no delay */);
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
  spawnl(0, "/bin/sh", "/bin/sh", "-c", command.c_str(), (char *)0);
}

utf8_string get_selected_cwd(WM &wm)
{
  utf8_string result;
  if (WFrame *frame = wm.selected_frame())
  {
    if (Property<ascii_string> dir = client_cwd(frame->client()))
      result = dir.get();
  }
  return result;
}

void execute_shell_command_cwd(const ascii_string &command,
                               const utf8_string &cwd)
{
  ascii_string resolved_cwd(expand_path_home(cwd));
  spawnl(cwd.empty() ? 0 : resolved_cwd.c_str(),
         "/bin/sh", "/bin/sh", "-c", command.c_str(), (char *)0);
}

void execute_shell_command_selected_cwd(WM &wm, const ascii_string &command)
{
  return execute_shell_command_cwd(command, get_selected_cwd(wm));
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
  wm.menu.read_string("Command:", "", boost::bind(&execute_shell_command, _1));
}

void execute_shell_command_cwd_interactive(WM &wm)
{
  utf8_string cwd = get_selected_cwd(wm);
  if (cwd.empty())
  {
    char buf[256];
    buf[255] = 0;
    getcwd(buf, 256);
    cwd = compact_path_home(buf);
  }
  
  wm.menu.read_string(cwd + " $", "",
                      boost::bind(&execute_shell_command_cwd, _1, cwd));
}

void switch_to_view(WM &wm, const utf8_string &name)
{
  if (!wm.valid_view_name(name))
    return;

  wm.select_view(wm.get_or_create_view(name));
}

void switch_to_view_interactive(WM &wm)
{
  wm.menu.read_string("Switch to tag:", "",
                      boost::bind(&switch_to_view, boost::ref(wm), _1),
                      WMenu::FailureAction(),
                      prefix_completer(boost::make_transform_range(wm.views(), select1st)),
                      false /* no delay */);
}

void switch_to_view_by_letter(WM &wm, char c)
{
  /** Note: the contents of views() may change during the loop, because
   * select_view may cause the existing selected view to be destroyed
   * if it contains no columns.
   *
   * BOOST_FOREACH can be used, nonetheless, because the loop ends
   * immediately after any changes might occur.
   */
  BOOST_FOREACH (const WM::ViewMap::value_type &x, wm.views())
  {
    if (x.first[0] == c)
    {
      wm.select_view(x.second);
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
    place_frame_in_smallest_column(view, frame);
  }
}


void move_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Move to view:", "",
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

    place_client_in_smallest_column(view, &frame->client());
  }
}

void copy_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Duplicate to view:", "",
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


void copy_marked_frames_to_current_view(WM &wm)
{
  if (!wm.selected_view())
    return;

  /**
   * Note: BOOST_FOREACH can be used here because the loop contents 
   */
  BOOST_FOREACH (WView *view, boost::make_transform_range(wm.views(), select2nd))
  {
    BOOST_FOREACH (WColumn &col, view->columns)
    {
      BOOST_FOREACH (WFrame &frame, col.frames)
      {
        if (!frame.marked())
          continue;
        frame.set_marked(false);
        if (frame.client().frame_by_view(wm.selected_view()))
          continue;

        wm.selected_view()->select_frame
          (place_client_in_smallest_column(wm.selected_view(), &frame.client()));
      }
    }
  }
}

void move_marked_frames_to_current_view(WM &wm)
{
  if (!wm.selected_view())
    return;

  // Note: BOOST_FOREACH cannot be used here because the columns and
  // frames lists may change.
  for (WM::ViewMap::const_iterator view_it = wm.views().begin(),
         view_next, view_end = wm.views().end();
       view_it != view_end;
       view_it = view_next)
  {
    view_next = boost::next(view_it);
    WView *view = view_it->second;

    for (WView::ColumnList::iterator col_it = view->columns.begin(),
           col_next, col_end = view->columns.end();
         col_it != col_end;
         col_it = col_next)
    {
      col_next = boost::next(col_it);
      WColumn &col = *col_it;

      for (WColumn::FrameList::iterator frame_it = col.frames.begin(),
           frame_next, frame_end = col.frames.end();
           frame_it != frame_end;
           frame_it = frame_next)
      {
        WFrame &frame = *frame_it;
        
        if (!frame.marked())
          continue;
        frame.set_marked(false);
        
        frame.remove();
        if (frame.client().frame_by_view(wm.selected_view()))
          delete &frame;
        else
        {
          place_frame_in_smallest_column(wm.selected_view(), &frame);
          wm.selected_view()->select_frame(&frame);
        }
      }
    }
  }
}


void move_next_by_activity_in_column(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WColumn *column = frame->column();
    WColumn::FrameListByActivity::iterator it
      = column->frames_by_activity().current(*frame);
    ++it;
    if (it == column->frames_by_activity().end())
      it = column->frames_by_activity().begin();
    WFrame *next = &*it;
    column->select_frame(next, true);
  }
}

void move_next_by_activity_in_view(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WView *view = frame->view();
    WView::FrameListByActivity::iterator it
      = view->frames_by_activity().current(*frame);
    ++it;
    if (it == view->frames_by_activity().end())
      it = view->frames_by_activity().begin();
    WFrame *next = &*it;
    view->select_frame(next, true);
  }
}

void move_next_by_activity(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WM::FrameListByActivity::iterator it
      = wm.frames_by_activity().current(*frame);
    ++it;
    if (it == wm.frames_by_activity().end())
      it = wm.frames_by_activity().begin();
    WFrame *next = &*it;
    next->view()->select_frame(next, true);
    wm.select_view(next->view());
  }
}
