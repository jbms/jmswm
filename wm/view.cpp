
#include <boost/foreach.hpp>
#include <wm/wm.hpp>
#include <boost/utility.hpp>

#include <util/log.hpp>

WView::WView(WM &wm, const utf8_string &name)
  : wm_(wm),  name_(name), selected_column_(0)
{
  compute_bounds();

  /* TODO: maybe check if the name is already in use */
  wm_.views_.insert(std::make_pair(name, this));
}

WView::~WView()
{
  wm_.views_.erase(name_);
}

void WView::compute_bounds()
{
  bounds.width = wm().screen_width();
  bounds.height = wm().screen_height();
  bounds.x = 0;
  bounds.y = 0;
}

/**
 * This returns a bogus value if there are no columns.
 */
int WView::available_column_width() const
{
  return bounds.width;
}


WView::iterator WView::create_column(WView::iterator position, float fraction)
{
  if (fraction == 0)
    fraction = 1.0f / (columns.size() + 1);

  int added_width;

  if (columns.empty())
    added_width = bounds.width;
  else
    added_width = (int)(available_column_width() * fraction / (1.0f - fraction));

  WColumn *column = new WColumn(this);
  column->bounds.width = added_width;

  iterator it = columns.insert(position, *column);

  if (!selected_column())
    select_column(it);

  update_positions();

  return it;
}

void WView::update_positions()
{
  if (columns.empty())
    return;
  
  int x = bounds.x;
  int available_width = available_column_width();
  int remaining_width = available_width;
  
  int actual_width = 0;
  BOOST_FOREACH (WColumn &c, columns)
    actual_width += c.bounds.width;

  for (iterator it = columns.begin(); it != columns.end(); ++it)
  {
    WColumn &c = *it;
    c.bounds.x = x;
    int width;
    if (boost::next(it) == columns.end())
      width = remaining_width;
    else
      width = c.bounds.width * available_width / actual_width;
    remaining_width -= width;
    x += width;
    c.bounds.width = width;
    c.bounds.y = bounds.y;
    c.bounds.height = bounds.height;
    c.update_positions();
  }
}

WFrame::WFrame(WClient &client, WColumn *column)
  : client_(client), column_(column), rolled_(false),
    bar_visible_(false)
{
}

WColumn::WColumn(WView *view)
  : view_(view), selected_frame_(0)
{}

/**
 * This returns a bogus value if there are no frames.
 */
int WColumn::available_frame_height() const
{
  return bounds.height;
}

WColumn::iterator WColumn::add_client(WClient *client, WColumn::iterator position)
{
  WFrame *frame = new WFrame(*client, this);

  client->view_frames_.insert(std::make_pair(view(), frame));

  int height;

  if (frames.empty())
    height = bounds.height;
  else
    height = available_frame_height() / frames.size();
  
  frame->bounds.height = height;

  iterator it = frames.insert(position, *frame);

  if (!selected_frame())
    select_frame(it);

  update_positions();

  return it;
}

void WColumn::update_positions()
{
  if (frames.empty())
    return;

  int y = bounds.y;
  int available_height = available_frame_height();
  int remaining_height = available_height;

  int actual_height = 0;
  BOOST_FOREACH (WFrame &f, frames)
    actual_height += f.bounds.height;

  for (iterator it = frames.begin(); it != frames.end(); ++it)
  {
    WFrame &f = *it;
    f.bounds.y = y;
    int height;
    if (boost::next(it) == frames.end())
      height = remaining_height;
    else
      height = f.bounds.height * available_height / actual_height;
    y += height;
    remaining_height -= height;
    f.bounds.height = height;
    f.bounds.x = bounds.x;
    f.bounds.width = bounds.width;
    f.client().schedule_positioning();
  }
}

void WFrame::remove()
{
  client().view_frames_.erase(view());
  WColumn::iterator cur_frame_it = column()->make_iterator(this);
  if (column()->selected_frame() == this)
  {
    WColumn::iterator it = cur_frame_it;
    if (it == column()->frames.begin())
      it = boost::next(it);
    else
      it = boost::prior(it);
    if (it != column()->frames.end())
      column()->select_frame(it);
    else
    {
      /* The column is empty, so it must be removed. */
      WView::ColumnList::iterator cur_col_it
        = view()->make_iterator(column());
      
      if (column() == view()->selected_column())
      {
        WView::ColumnList::iterator col_it = cur_col_it;
        if (col_it == view()->columns.begin())
          col_it = boost::next(col_it);
        else
          col_it = boost::prior(col_it);
        if (col_it != view()->columns.end())
          view()->select_column(col_it);
        else
          view()->select_column(0);
      }
      
      view()->columns.erase(cur_col_it);
      view()->update_positions();
      delete column();
      column_ = 0;
      return;
    }
  }
  column()->frames.erase(cur_frame_it);
  column()->update_positions();
  column_ = 0;
}

void WColumn::select_frame(WFrame *frame)
{
  if (frame == selected_frame_)
    return;

  if (selected_frame_)
    selected_frame_->client().schedule_drawing();

  selected_frame_ = frame;

  if (frame)
  {
    frame->client().schedule_drawing();

    if (view() == wm().selected_view()
        && this == view()->selected_column())
      wm().schedule_focus_client(&frame->client());
  }
}

void WColumn::select_frame(iterator it)
{
  select_frame(get_frame(it));
}

WColumn::iterator WColumn::next_frame(iterator it, bool wrap)
{
  if (it == frames.end())
    return it;

  iterator p = boost::next(it);
  if (p == frames.end())
  {
    if (wrap)
      p = frames.begin();
    else
      p = it;
  }
  return p;
}

WColumn::iterator WColumn::prior_frame(iterator it, bool wrap)
{
  if (it == frames.end())
    return it;

  if (it == frames.begin())
  {
    if (wrap)
      return boost::prior(frames.end());
    return it;
  }

  return boost::prior(it);
}

WView::iterator WView::next_column(iterator it, bool wrap)
{
  if (it == columns.end())
    return it;

  iterator p = boost::next(it);
  if (p == columns.end())
  {
    if (wrap)
      p = columns.begin();
    else
      p = it;
  }
  return p;
}

WView::iterator WView::prior_column(iterator it, bool wrap)
{
  if (it == columns.end())
    return it;

  if (it == columns.begin())
  {
    if (wrap)
      return boost::prior(columns.end());
    return it;
  }

  return boost::prior(it);
}


void WView::select_column(WColumn *column)
{
  if (column == selected_column())
    return;

  if (selected_column())
  {
    if (WFrame *f = selected_column()->selected_frame())
      f->client().schedule_drawing();
  }

  selected_column_ = column;

  if (column)
  {
    if (WFrame *f = column->selected_frame())
    {
      f->client().schedule_drawing();

      if (this == wm().selected_view())
        wm().schedule_focus_client(&f->client());
    }
  }
}

void WView::select_column(WView::iterator it)
{
  select_column(get_column(it));
}

void WView::select_frame(WFrame *frame)
{
  if (!frame)
    select_column(0);

  else
  {
    frame->column()->select_frame(frame);
    select_column(frame->column());
  }
}

void WM::select_view(WView *view)
{
  if (selected_view_)
  {
    BOOST_FOREACH(WColumn &c, selected_view_->columns)
      BOOST_FOREACH(WFrame &f, c.frames)
        f.client().schedule_positioning();
  }

  selected_view_ = view;

  if (selected_view_)
  {
    BOOST_FOREACH(WColumn &c, selected_view_->columns)
      BOOST_FOREACH(WFrame &f, c.frames)
        f.client().schedule_positioning();

    if (WColumn *column = view->selected_column())
    {
      if (WFrame *frame = column->selected_frame())
      {
        schedule_focus_client(&frame->client());
      }
    }
  }
}
