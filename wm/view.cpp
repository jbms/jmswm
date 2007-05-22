
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
  WARN("here");
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
    decorated_(true), marked_(false),
    priority_(frame_initial_priority),
    initial_position(-1)
{}

WColumn::WColumn(WView *view)
  : view_(view), selected_frame_(0),
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
  frames_by_activity_.push_front(*frame);
  view()->frames_by_activity_.push_front(*frame);
  wm().frames_by_activity_.push_front(*frame);

  if (!selected_frame())
    select_frame(it);

  schedule_update_positions();

  return it;
}

void WM::handle_frame_activity()
{
  if (WFrame *frame = selected_frame())
  {
    WColumn *column = frame->column();
    WView *view = frame->view();
    if (frame != &column->frames_by_activity_.front())
    {
      column->frames_by_activity_.splice(column->frames_by_activity_.begin(),
                                         column->frames_by_activity_,
                                         column->frames_by_activity_.current(*frame));
      column->schedule_update_positions();
    }

    if (frame != &view->frames_by_activity_.front())
    {
      view->frames_by_activity_.splice(view->frames_by_activity_.begin(),
                                       view->frames_by_activity_,
                                       view->frames_by_activity_.current(*frame));
    }

    if (frame != &frames_by_activity_.front())
    {
      frames_by_activity_.splice(frames_by_activity_.begin(),
                                 frames_by_activity_,
                                 frames_by_activity_.current(*frame));
    }
  }
}

void WM::handle_selected_frame_changed(WFrame *old_frame, WFrame *new_frame)
{
  if (new_frame)
    frame_activity_event.wait(time_duration::milliseconds(500));

  if (old_frame &&
      old_frame == old_frame->column()->selected_frame()
      && old_frame != &old_frame->column()->frames_by_activity().front())
  {
    old_frame->column()->select_frame(&old_frame->column()->frames_by_activity_.front());
  }

  // FIXME: invoke the appropriate hook once it is created.
}

/**
 * Note: If all frames are set to shaded, the last one is
 * automatically unshaded.
 *
 * Policy for handling a maximum size hint:
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
  int decoration_height = wm().frame_decoration_height();

  float reserved_height = available_height
    - shaded_height * frames.size();

  float total_priority = 0.0f;

  int total_unshaded_height = available_height;

  int unflexible_height = 0;

  BOOST_FOREACH (WFrame &f, frames_by_activity_)
  {
    float required_height;
    int fixed_height = f.client().fixed_height();

    if (fixed_height)
    {
      required_height = fixed_height;
      if (f.decorated())
        required_height += decoration_height;
    } else {
      required_height = f.priority() * available_height;
    }

    bool new_shaded;
    
    if (&f == &frames_by_activity_.front()
        || required_height <= reserved_height)
      new_shaded = false;
    else
      new_shaded = true;

    // FIXME: maybe make this a separate function.  Note that
    // WFrame::set_shaded is not used because that calls
    // column()->schedule_update_positions.
    if (new_shaded != f.shaded_)
    {
      f.shaded_ = new_shaded;
      if (&f == view()->selected_frame())
        f.client().schedule_set_input_focus();
    }
    
    if (!f.shaded_)
    {
      reserved_height -= required_height;
      reserved_height += shaded_height;
      if (!fixed_height)
        total_priority += f.priority();
      else
        unflexible_height += (int)required_height;
    } else
    {
      total_unshaded_height -= shaded_height;
    }
  }

  WFrame *last_flexible = 0;
  {
    WFrame *last_unshaded = 0;
    BOOST_FOREACH (WFrame &f, frames)
    {
      if (!f.shaded())
      {
        if (!f.client().fixed_height())
          last_flexible =  &f;
        last_unshaded = &f;
      }
    }
    if (!last_flexible)
      last_flexible = last_unshaded;
  }

  int y = bounds.y;

  int total_flexible_height = total_unshaded_height - unflexible_height;

  int remaining_unshaded_height = total_unshaded_height;
  int remaining_fixed_height = unflexible_height;

  BOOST_FOREACH (WFrame &f, frames)
  {
    f.bounds.y = y;
    int height;
    
    if (f.shaded())
      height = shaded_height;
    else
    {
      int fixed_height = f.client().fixed_height();
      if (fixed_height)
      {
        remaining_fixed_height -= fixed_height;
        if (f.decorated())
          remaining_fixed_height -= decoration_height;
      }
      
      if (&f == last_flexible)
        height = remaining_unshaded_height - remaining_fixed_height;
      else if (fixed_height)
      {
        height = fixed_height;
        if (f.decorated())
          height += decoration_height;
      }
      else
        height = (int)(f.priority() / total_priority * total_flexible_height);
    }
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
  WARN("here");
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
  WARN("here");
  WColumn::iterator cur_frame_it = column()->make_iterator(this);
  if (column()->selected_frame() == this)
  {
    // Select most recently active frame
    WColumn::FrameListByActivity::iterator cur_activity_it
      = column()->frames_by_activity_.current(*this), it;
    it = column()->frames_by_activity_.begin();
    if (it == cur_activity_it)
      ++it;
    if (it == column()->frames_by_activity_.end())
      column()->select_frame(0);
    else
      column()->select_frame(&*it);
  }

  client().schedule_update_server();
  client().view_frames_.erase(view());  
  
  column()->frames.erase(cur_frame_it);
  column()->frames_by_activity_.erase(column()->frames_by_activity_.current(*this));
  view()->frames_by_activity_.erase(view()->frames_by_activity_.current(*this));
  wm().frames_by_activity_.erase(wm().frames_by_activity_.current(*this));
  
  column()->schedule_update_positions();

  if (column()->frames.empty())
    delete column();

  column_ = 0;
}

// Note: this function is no longer used, because frame shading is
// managed automatically based on activity.
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

    if (column())
      column()->schedule_update_positions();
  }
}

void WFrame::set_marked(bool value)
{
  if (marked_ != value)
  {
    marked_ = value;

    if (column())
      client().schedule_draw();
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
  if (frame == selected_frame_)
    return;

  if (selected_frame_)
    selected_frame_->client().schedule_draw();

  WFrame *old_frame = selected_frame_;

  selected_frame_ = frame;

  if (this == wm().selected_column())
    wm().handle_selected_frame_changed(old_frame, frame);

  if (frame)
  {
    frame->client().schedule_draw();

    if (view() == wm().selected_view()
        && this == view()->selected_column())
    {
      frame->client().schedule_set_input_focus();
      if (warp)
        frame->client().schedule_warp_pointer();
    }
  }
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

  WFrame *old_frame = 0;

  if (selected_column())
  {
    if (WFrame *f = selected_column()->selected_frame())
      f->client().schedule_draw();

    old_frame = selected_column()->selected_frame();
  }

  selected_column_ = column;

  if (this == wm().selected_view())
    wm().handle_selected_frame_changed(old_frame,
                                       column ? column->selected_frame() : 0);

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

  WView *old_view = selected_view_;

  if (old_view)
  {
    BOOST_FOREACH(WColumn &c, old_view->columns)
      BOOST_FOREACH(WFrame &f, c.frames)
        f.client().schedule_update_server();

  }

  selected_view_ = view;
  select_view_hook(view, old_view);
  handle_selected_frame_changed(old_view ? old_view->selected_frame() : 0,
                                view ? view->selected_frame() : 0);
  
  if (old_view)
  {
    /* Remove the old selected view if it is empty */
    if (old_view->columns.empty())
      delete old_view;
  }

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

WView *WM::get_or_create_view(const utf8_string &name)
{
  if (WView *view = view_by_name(name))
    return view;
  return new WView(*this, name);
}
