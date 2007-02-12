
#include <wm/all.hpp>

const static float frame_initial_priority = 0.34f;
const static float frame_minimum_priority = 0.1f;
const static float frame_maximum_priority = 1.0f;



WView::WView(WM &wm, const utf8_string &name)
  : wm_(wm),  name_(name),
    scheduled_update_positions(false),    
    selected_column_(0),
    bar_visible_(true)
{
  compute_bounds();

  /* TODO: maybe check if the name is already in use */
  wm_.views_.insert(std::make_pair(name, this));

  wm_.create_view_hook(this);
}

WView::~WView()
{
  wm().destroy_view_hook(this);
  wm_.views_.erase(name_);
}

void WView::compute_bounds()
{
  bounds.width = wm().screen_width();
  bounds.height = wm().screen_height();
  if (bar_visible())
    bounds.height -= wm().bar.height();
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

WView::iterator WView::default_insert_position()
{
  iterator it = selected_position();
  if (it != columns.end())
    ++it;
  return it;
}



WView::iterator WView::create_column(WView::iterator position)
{
  WColumn *column = new WColumn(this);
  iterator it = columns.insert(position, *column);

  if (!selected_column())
    select_column(it);

  schedule_update_positions();

  return it;
}

void WView::perform_scheduled_tasks()
{
  assert(scheduled_update_positions);
  
  wm().scheduled_task_views.erase(wm().scheduled_task_views.current(*this));

  scheduled_update_positions = false;
  
  if (columns.empty())
    return;

  float total_priority = 0.0f;

  int available_width = available_column_width();
  int remaining_width = available_width;
  
  BOOST_FOREACH (WColumn &c, columns)
    total_priority += c.priority();

  int x = bounds.x;

  BOOST_FOREACH (WColumn &c, columns)
  {
    c.bounds.x = x;
    int width;
    if (&c == &columns.back())
      width = remaining_width;
    else
      width = (int)(c.priority() / total_priority * available_width);
    remaining_width -= width;
    x += width;
    c.bounds.width = width;
    c.bounds.y = bounds.y;
    c.bounds.height = bounds.height;
    c.schedule_update_positions();
  }
}

WFrame::WFrame(WClient &client)
  : client_(client), column_(0), shaded_(false),
    decorated_(true), priority_(frame_initial_priority),
    initial_position(-1)
{}

WColumn::WColumn(WView *view)
  : view_(view), selected_frame_(0),
    raise_frame_event(view->wm().event_service(),
                      boost::bind(&WColumn::raise_selected_frame, this))
    scheduled_update_positions(false),
    priority_(initial_priority)
{}

void WColumn::set_priority(float value)
{
  if (value < minimum_priority)
    value = minimum_priority;
  if (value > maximum_priority)
    value = maximum_priority;
  priority_ = value;
  view()->schedule_update_positions();
}

/**
 * This returns a bogus value if there are no frames.
 */
int WColumn::available_frame_height() const
{
  return bounds.height;
}

WColumn::iterator WColumn::default_insert_position()
{
  iterator it = selected_position();
  if (it != frames.end())
    ++it;
  return it;
}

WColumn::iterator WColumn::add_frame(WFrame *frame, WColumn::iterator position)
{
  frame->column_ = this;
  frame->client().view_frames_.insert(std::make_pair(view(), frame));

  iterator it = frames.insert(position, *frame);

  // TODO: decide on policy for placement in frames_by_activity list
  frames_by_activity.push_front(*frame);

  if (!selected_frame())
    select_frame(it);

  schedule_update_positions();

  return it;
}

/**
 * Note: If all frames are set to shaded, the last one is
 * automatically unshaded.
 *
 * Policy for handling a maximum size hint:
 *
 * If 
 *
 * TODO: handle the case where there are too many frames to show even
 * the bars
 *
 * First set the shading status of all frames based on activity.
 */
void WColumn::perform_scheduled_tasks()
{
  assert(scheduled_update_positions);
  
  wm().scheduled_task_columns.erase(wm().scheduled_task_columns.current(*this));
  scheduled_update_positions = false;
  
  if (frames.empty())
    return;

  // TODO: maybe change frame priority to a minimum height in pixels,
  // so that xrandr screen size changes automatically result in
  // possibly useful behavior.

  int available_height = available_frame_height();

  int shaded_height = wm().shaded_height();

  float reserved_height = available_height
    - shaded_height * frames.size();

  float total_priority = 0.0f;

  int total_unshaded_height = available_height;

  if (selected_frame_)
  {
    // TODO: avoid this code duplication
    float required_height = selected_frame_->priority() * available_height;
    reserved_height -= required_height;
    reserved_height += shaded_height;
    total_priority += selected_frame_->priority();
    
    selected_frame_->shaded_ = false;
  }

  BOOST_FOREACH (WFrame &f, frames_by_activity)
  {
    if (&f == selected_frame_)
      continue;
    
    // TODO: use size hints as follows: if minimum height is equal to
    // maximum height, use the minimum height as the required height
    // here, instead of basing it on the priority value.  As a result,
    // priority will be ignored for such clients.
    
    float required_height = f.priority() * available_height;
    
    if (required_height <= reserved_height)
      f.shaded_ = false;
    else
      f.shaded_ = true;

    if (!f.shaded_)
    {
      reserved_height -= required_height;
      reserved_height += shaded_height;
      total_priority += f.priority();
    } else
    {
      total_unshaded_height -= shaded_height;
    }
  }

  WFrame *last_unshaded = 0;
  BOOST_FOREACH (WFrame &f, frames)
  {
    if (!f.shaded())
      last_unshaded = &f;
  }

  int y = bounds.y;

  int remaining_unshaded_height = total_unshaded_height;

  BOOST_FOREACH (WFrame &f, frames)
  {
    f.bounds.y = y;
    int height;
    if (f.shaded())
      height = shaded_height;
    else if (&f == last_unshaded)
      height = remaining_unshaded_height;
    else
      height = (int)(f.priority() / total_priority * total_unshaded_height);
    y += height;
    if (!f.shaded())
      remaining_unshaded_height -= height;
    f.bounds.height = height;
    f.bounds.x = bounds.x;
    f.bounds.width = bounds.width;
    f.client().schedule_update_server();
  }
}

WColumn::~WColumn()
{
  assert(frames.empty());
  
  WView::iterator cur_col_it = view()->make_iterator(this);
      
  if (this == view()->selected_column())
  {
    WView::iterator col_it = cur_col_it;
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
  view()->schedule_update_positions();

  if (view()->columns.empty() && view() != wm().selected_view())
    delete view();
}

WFrame::~WFrame()
{
  if (column())
    remove();
}

void WFrame::remove()
{
  client().schedule_update_server();
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
      column()->select_frame(0);

  }
  column()->frames.erase(cur_frame_it);
  column()->frames_by_activity.erase(column()->frames_by_activity.current(*this));
  column()->schedule_update_positions();

  if (column()->frames.empty())
    delete column();

  column_ = 0;
}

void WFrame::set_shaded(bool value)
{
  if (shaded_ != value)
  {
    shaded_ = value;

    if (column())
    {
      column()->schedule_update_positions();
      if (this == view()->selected_frame())
        client().schedule_set_input_focus();
    }
  }
}

void WFrame::set_decorated(bool value)
{
  if (decorated_ != value)
  {
    decorated_ = value;

    /* FIXME: if frame positioning in columns is changed to depend on
       whether the frame is decorated, this will need to call
       column()->schedule_update_positions(), and possibly set input
       focus as well. */
    if (column())
      client().schedule_update_server();
  }
}

void WFrame::set_priority(float value)
{
  if (value < frame_minimum_priority)
    value = frame_minimum_priority;
  if (value > frame_maximum_priority)
    value = frame_maximum_priority;
  priority_ = value;
  if (column())
    column()->schedule_update_positions();
}

void WColumn::select_frame(WFrame *frame, bool warp)
{
  static const time_duration minimum_activity_duration = time_duration::seconds(1);
  
  if (frame == selected_frame_)
    return;

  time_point current_time = time_point::current();

  if (selected_frame_)
  {
    selected_frame_->client().schedule_draw();

    if (current_time - selected_frame_->last_focused > minimum_activity_duration)
      frames_by_activity.splice(frames_by_activity.begin(),
                                frames_by_activity,
                                frames_by_activity.current(*selected_frame_));
  }

  selected_frame_ = frame;

  if (frame)
  {
    frame->last_focused = current_time;
    
    frame->client().schedule_draw();

    if (view() == wm().selected_view()
        && this == view()->selected_column())
    {
      frame->client().schedule_set_input_focus();
      if (warp)
        frame->client().schedule_warp_pointer();
    }
  }

  schedule_update_positions();
}

void WColumn::select_frame(iterator it, bool warp)
{
  select_frame(get_frame(it), warp);
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


void WView::select_column(WColumn *column, bool warp)
{
  if (column == selected_column())
    return;

  if (selected_column())
  {
    if (WFrame *f = selected_column()->selected_frame())
      f->client().schedule_draw();
  }

  selected_column_ = column;

  if (column)
  {
    if (WFrame *f = column->selected_frame())
    {
      f->client().schedule_draw();

      if (this == wm().selected_view())
      {
        f->client().schedule_set_input_focus();
        if (warp)
          f->client().schedule_warp_pointer();
      }
    }
  }
}

void WView::select_column(WView::iterator it, bool warp)
{
  select_column(get_column(it), warp);
}

void WView::select_frame(WFrame *frame, bool warp)
{
  if (!frame)
    select_column(0);

  else
  {
    frame->column()->select_frame(frame, warp);
    select_column(frame->column(), warp);
  }
}

void WView::set_bar_visible(bool value)
{
  if (bar_visible_ != value)
  {
    bar_visible_ = value;
    if (this == wm().selected_view())
      wm().bar.schedule_update_server();
    compute_bounds();
    schedule_update_positions();
  }
}

void WM::select_view(WView *view)
{
  if (view == selected_view_)
    return;

  select_view_hook(view, selected_view_);
  
  if (selected_view_)
  {
    BOOST_FOREACH(WColumn &c, selected_view_->columns)
      BOOST_FOREACH(WFrame &f, c.frames)
        f.client().schedule_update_server();

    /* Remove the old selected view if it is empty */
    if (selected_view_->columns.empty())
      delete selected_view_;
  }

  selected_view_ = view;

  if (selected_view_)
  {
    BOOST_FOREACH(WColumn &c, selected_view_->columns)
      BOOST_FOREACH(WFrame &f, c.frames)
        f.client().schedule_update_server();

    if (WFrame *frame = view->selected_frame())
    {
      frame->client().schedule_set_input_focus();
      frame->client().schedule_warp_pointer();
    }
  }

  bar.scheduled_update_server = true;
}

WFrame *WM::selected_frame()
{
  if (WView *view = selected_view())
    return view->selected_frame();
  return 0;
}

WColumn *WM::selected_column()
{
  if (WView *view = selected_view())
    return view->selected_column();
  return 0;
}

void WView::place_frame_in_smallest_column(WFrame *frame)
{
  iterator col;
  // 3 should not be hardcoded here
  if (columns.size() < 2)
    col = create_column(columns.end());
  else
  {
    col = columns.begin();
    for (iterator it = boost::next(col); it != columns.end(); ++it)
    {
      if (col->frames.size() > it->frames.size())
        col = it;
    }
  }

  col->add_frame(frame, col->frames.end());
}
