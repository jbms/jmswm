
#include <boost/foreach.hpp>
#include <wm/wm.hpp>
#include <boost/utility.hpp>

#include <util/log.hpp>

WView::WView(WM &wm, const utf8_string &name)
  : wm(wm),  selected_column(0), name(name)
{
  compute_bounds();

  DEBUG("set view bounds to: %d, %d, %d, %d", bounds.x, bounds.y, bounds.width, bounds.height);
  
  wm.views.insert(std::make_pair(name, this));
}


WView::~WView()
{
  wm.views.erase(name);
}

void WView::compute_bounds()
{
  bounds.width = wm.xc.screen_width();
  bounds.height = wm.xc.screen_height();
  bounds.x = 0;
  bounds.y = 0;
}

/**
 * This returns a bogus value if there are no columns.
 */
int WView::available_column_width() const
{
  return bounds.width - (columns.size() - 1)
    * (wm.frame_style.column_margin * 2
       + wm.frame_style.column_border_width);
}


WColumn *WView::create_column(float fraction, ColumnList::iterator position)
{
  if (fraction == 0)
    fraction = 1.0f / (columns.size() + 1);

  int added_width;

  if (columns.empty())
    added_width = bounds.width;
  else
    added_width = (int)(available_column_width() * fraction / (1.0f - fraction));

  WColumn *column = new WColumn(wm, this);
  column->bounds.width = added_width;

  columns.insert(position, *column);

  if (!selected_column)
    selected_column = column;

  update_positions();

  return column;
}

void WView::update_positions()
{
  if (columns.empty())
    return;
  
  int x = bounds.x;
  int available_width = available_column_width();
  DEBUG("available_width: %d", available_width);
  int remaining_width = available_width;
  
  int actual_width = 0;
  BOOST_FOREACH (WColumn &c, columns)
    actual_width += c.bounds.width;

  for (ColumnList::iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    WColumn &c = *it;
    c.bounds.x = x;
    int width;
    if (boost::next(it) == columns.end())
      width = remaining_width;
    else
      width = c.bounds.width * available_width / actual_width;
    remaining_width -= width;
    x += width + 2 * wm.frame_style.column_margin
      + wm.frame_style.column_border_width;
    c.bounds.width = width;
    c.bounds.y = bounds.y;
    c.bounds.height = bounds.height;
    DEBUG("set column bounds: %d, %d, %d, %d", c.bounds.x, c.bounds.y, c.bounds.width, c.bounds.height);
    c.update_positions();
  }
}

/**
 * This returns a bogus value if there are no frames.
 */
int WColumn::available_frame_height() const
{
  return bounds.height - (frames.size() - 1) * wm.frame_style.frame_margin;
}

WFrame *WColumn::add_client(WClient *client, FrameList::iterator position)
{
  WFrame *frame = new WFrame(*client, this);

  client->view_frames.insert(std::make_pair(view, frame));

  int height;

  if (frames.empty())
    height = bounds.height;
  else
    height = available_frame_height() / frames.size();
  
  frame->bounds.height = height;

  frames.insert(position, *frame);

  if (!selected_frame)
    selected_frame = frame;

  update_positions();

  return frame;
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

  for (FrameList::iterator it = frames.begin();
       it != frames.end();
       ++it)
  {
    WFrame &f = *it;
    f.bounds.y = y;
    int height;
    if (boost::next(it) == frames.end())
      height = remaining_height;
    else
      height = f.bounds.height * available_height / actual_height;
    y += height + wm.frame_style.frame_margin;
    remaining_height -= height;
    f.bounds.height = height;
    f.bounds.x = bounds.x;
    f.bounds.width = bounds.width;
    DEBUG("set frame bounds to: %d, %d, %d, %d", f.bounds.x,
          f.bounds.y, f.bounds.width, f.bounds.height);
    f.client.mark_dirty(WClient::CLIENT_POSITIONING_NEEDED);
  }
}

void WFrame::remove()
{
  WView *view = column->view;
  client.view_frames.erase(view);
  WColumn::FrameList::iterator cur_frame_it = column->frames.current(*this);
  if (column->selected_frame == this)
  {
    WColumn::FrameList::iterator it = cur_frame_it;
    if (it == column->frames.begin())
      it = boost::next(it);
    else
      it = boost::prior(it);
    if (it != column->frames.end())
      column->selected_frame = &*it;
    /* TODO: handle focus change */
    else
    {
      WView::ColumnList::iterator cur_col_it
        = view->columns.current(*column);
      
      if (column == view->selected_column)
      {
        WView::ColumnList::iterator col_it = cur_col_it;
        if (col_it == view->columns.begin())
          col_it = boost::next(col_it);
        else
          col_it = boost::prior(col_it);
        if (col_it != view->columns.end())
          view->selected_column = &*col_it;
        else
          view->selected_column = 0;
        /* TODO: handle focus change */
      }
      
      view->columns.erase(cur_col_it);
      view->update_positions();
      delete column;
      column = 0;
      return;
    }
  }
  column->frames.erase(column->frames.current(*this));
  column->update_positions();
  column = 0;
}
