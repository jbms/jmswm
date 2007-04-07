#include <wm/all.hpp>

int WBar::label_height()
{
  return style.label_font.height()
    + 2 * style.label_vertical_padding;
}

int WBar::top_left_offset()
{
  return style.highlight_pixels + style.padding_pixels + style.spacing;
}

int WBar::bottom_right_offset()
{
  return style.shadow_pixels + style.padding_pixels + style.spacing;
}

void WBar::compute_bounds()
{
  bounds.x = 0;
  bounds.width = wm().screen_width();

  bounds.height = height();
  bounds.y = wm().screen_height() - bounds.height;
}

int WBar::height()
{
  return label_height() + top_left_offset() + bottom_right_offset();
}

bool WM::bar_visible()
{
  if (selected_view())
    return selected_view()->bar_visible();
  return true;
}

void WBar::flush()
{
  bool visible = wm().bar_visible();

  if (scheduled_update_server)
  {
    if (visible)
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
        XMapWindow(wm().display(), xwin_);
        map_state = STATE_MAPPED;
      }
    } else
    {
      if (map_state != STATE_UNMAPPED)
      {
        XUnmapWindow(wm().display(), xwin_);
        map_state = STATE_UNMAPPED;
      }
    }
  }

  if (scheduled_update_server || scheduled_draw)
  {
    draw();
  }

  scheduled_update_server = false;
  scheduled_draw = false;
}

void WBar::draw()
{
  WDrawable &d = wm().buffer_pixmap.drawable();
  WRect rect(0, 0, bounds.width, bounds.height);
  
  fill_rect(d, style.background_color, rect);

  draw_border(d, style.highlight_color, style.highlight_pixels,
              style.shadow_color, style.shadow_pixels,
              rect);

  WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                         style.shadow_pixels);

  draw_border(d, style.padding_color, style.padding_pixels, rect2);

  WRect rect3 = rect2.inside_border(style.padding_pixels + style.spacing);
  rect3.height = label_height();

  BOOST_FOREACH (Cell &c, cells[LEFT])
  {
    // Check for placeholders
    if (!c.foreground_color || !c.background_color || c.text.empty())
      continue;
    
    int width = draw_label_with_background(d, c.text,
                                           style.label_font,
                                           *c.foreground_color,
                                           *c.background_color,
                                           rect3,
                                           style.label_horizontal_padding,
                                           style.label_vertical_padding,
                                           false);

    rect3.width -= (width + style.cell_spacing);

    rect3.x += width + style.cell_spacing;
  }

  BOOST_FOREACH (Cell &c, boost::make_reverse_range(cells[RIGHT]))
  {
    // Check for placeholders
    if (!c.foreground_color || !c.background_color || c.text.empty())
      continue;
    
    int width = draw_label_with_background(d, c.text,
                                           style.label_font,
                                           *c.foreground_color,
                                           *c.background_color,
                                           rect3,
                                           style.label_horizontal_padding,
                                           style.label_vertical_padding,
                                           true);

    rect3.width -= (width + style.cell_spacing);
  }


  XCopyArea(wm().display(), d.drawable(),
            xwin(),
            wm().dc.gc(),
            0, 0, bounds.width, bounds.height,
            0, 0);
}

void WBar::handle_screen_size_changed()
{
  compute_bounds();
  
  if (!wm().bar_visible())
    return;
  
  scheduled_update_server = true;
}

void WBar::handle_expose(const XExposeEvent &ev)
{
  if (!wm().bar_visible())
    return;
  
  scheduled_draw = true;
}

void WBar::CellRef::set_text(const utf8_string &str)
{
  cell->text = str;
  
  if (!cell->bar.wm().bar_visible())
    return;
  
  cell->bar.scheduled_draw = true;
}

void WBar::CellRef::set_foreground(const WColor &c)
{
  cell->foreground_color = &c;
  
  if (!cell->bar.wm().bar_visible())
    return;
  
  cell->bar.scheduled_draw = true;
}

void WBar::CellRef::set_background(const WColor &c)
{
  cell->background_color = &c;
  
  if (cell->bar.wm().bar_visible())
    cell->bar.scheduled_draw = true;
}

WBar::Cell::~Cell()
{
  bar.cells[position].erase(bar.cells[position].current(*this));

  if (bar.wm().bar_visible())
    bar.scheduled_draw = true;
}

WBar::WBar(WM &wm_, const WBarStyle::Spec &style_spec)
  : wm_(wm_),
    style(wm_.dc, style_spec),
    scheduled_update_server(true),
    scheduled_draw(true),
    initialized(false)
{
}

void WBar::initialize()
{
  assert(initialized == false);

  map_state = STATE_UNMAPPED;

  compute_bounds();

  XSetWindowAttributes wa;
  wa.event_mask = ExposureMask;
  
  xwin_ = XCreateWindow(wm().display(), wm().root_window(),
                        bounds.x, bounds.y, bounds.width, bounds.height,
                        0,
                        wm().default_depth(),
                        InputOutput,
                        wm().default_visual(),
                        CWEventMask, &wa);

  current_window_bounds = bounds;
  
  initialized = true;
}

WBar::~WBar()
{
  /* TODO: complete this */
}

WBar::CellRef WBar::insert_end(cell_position_t position,
                               const boost::shared_ptr<Cell> &cell)
{
  cells[position].push_back(*cell);
  
  if (wm().bar_visible())
    scheduled_draw = true;
  
  return CellRef(cell);
}

WBar::CellRef WBar::insert_begin(cell_position_t position,
                                 const boost::shared_ptr<Cell> &cell)
{
  cells[position].push_front(*cell);
  if (wm().bar_visible())
    scheduled_draw = true;
  
  return CellRef(cell);
}


WBar::CellRef WBar::insert_after(const CellRef &ref,
                                 cell_position_t position,
                                 const boost::shared_ptr<Cell> &cell)
{
  assert(ref.cell->position == position);
  cells[position].insert(boost::next(cells[position].current(*ref.cell)), *cell);
  
  if (wm().bar_visible())
    scheduled_draw = true;
  
  return CellRef(cell);
}

WBar::CellRef WBar::insert_before(const CellRef &ref,
                                  cell_position_t position,
                                  const boost::shared_ptr<Cell> &cell)                                  
{
  assert(ref.cell->position == position);
  cells[position].insert(cells[position].current(*ref.cell), *cell);
  
  if (wm().bar_visible())
    scheduled_draw = true;
  
  return CellRef(cell);
}

WBar::CellRef WBar::insert(const InsertPosition &pos,
                           const WColor &foreground_color,
                           const WColor &background_color,
                           const utf8_string &text)
{
  boost::shared_ptr<Cell> cell(new Cell(*this, pos.side, foreground_color, background_color, text));
  switch (pos.relative)
  {
  case InsertPosition::BEFORE:
    return insert_before(pos.ref, pos.ref.cell->position, cell);
    
  case InsertPosition::AFTER:
    return insert_after(pos.ref, pos.ref.cell->position, cell);
    
  case InsertPosition::BEGIN:
    return insert_begin(pos.side, cell);
    
  case InsertPosition::END:
  default:
    return insert_end(pos.side, cell);
  }
}


WBar::CellRef WBar::placeholder(const InsertPosition &pos)
{
  boost::shared_ptr<Cell> cell(new Cell(*this, pos.side));
  switch (pos.relative)
  {
  case InsertPosition::BEFORE:
    return insert_before(pos.ref, pos.ref.cell->position, cell);
    
  case InsertPosition::AFTER:
    return insert_after(pos.ref, pos.ref.cell->position, cell);
    
  case InsertPosition::BEGIN:
    return insert_begin(pos.side, cell);
    
  case InsertPosition::END:
  default:
    return insert_end(pos.side, cell);
  }
}
