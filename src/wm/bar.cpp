#include <wm/all.hpp>

#include <boost/intrusive/list.hpp>
#include <unordered_map>

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define NET_SYSTEM_TRAY_ORIENTATION_VERT 1

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY			0
#define XEMBED_WINDOW_ACTIVATE  		1
#define XEMBED_WINDOW_DEACTIVATE  		2
#define XEMBED_REQUEST_FOCUS		 	3
#define XEMBED_FOCUS_IN 	 			4
#define XEMBED_FOCUS_OUT  				5
#define XEMBED_FOCUS_NEXT 				6
#define XEMBED_FOCUS_PREV 				7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON 				10
#define XEMBED_MODALITY_OFF 			11
#define XEMBED_REGISTER_ACCELERATOR     12
#define XEMBED_UNREGISTER_ACCELERATOR   13
#define XEMBED_ACTIVATE_ACCELERATOR     14


enum map_state_t { STATE_MAPPED, STATE_UNMAPPED } map_state;


class WBar::Cell : public boost::intrusive::list_base_hook<> {
public:
  WBar &bar;
  cell_position_t position;
  utf8_string text;
  const WColor *foreground_color;
  const WColor *background_color;

  // If this is set, text and {foreground,background}_color are ignored.
  Window tray_window = 0;
  WRect current_tray_window_bounds;
  map_state_t map_state = STATE_UNMAPPED;
  bool tray_should_be_mapped = false;

  Cell(WBar &bar,
       cell_position_t position,
       const WColor &foreground_color,
       const WColor &background_color,
       const utf8_string &text)
      : bar(bar), position(position), text(text), foreground_color(&foreground_color), background_color(&background_color) {}

  Cell(WBar &bar, cell_position_t position) : bar(bar), position(position), foreground_color(0), background_color(0) {}

  ~Cell();
};

struct WBar::Impl {
  WM &wm_;

  WM &wm() { return wm_; }
  WBarStyle style;
  Window xwin_;
  WRect bounds, current_window_bounds;

  Atom xa_xembed, xa_xembed_info;

  std::unordered_map<Window,boost::shared_ptr<Cell>> tray_windows;

  typedef boost::intrusive::list<Cell> CellList;

  CellList cells[2];

  bool scheduled_update_server, scheduled_draw;

  bool initialized;

  Impl(WM &wm_, const style::Spec &style_spec)
      : wm_(wm_), style(wm_.dc, style_spec), scheduled_update_server(true), scheduled_draw(true), initialized(false) {}

  void initialize() {

    assert(initialized == false);

    map_state = STATE_UNMAPPED;

    compute_bounds();

    xa_xembed = XInternAtom(wm().display(), "_XEMBED", false);
    xa_xembed_info = XInternAtom(wm().display(), "_XEMBED_INFO", false);

    // FIXME: Set override_redirect to True as a convenient way to make
    // load_state_from_server ignore these windows.
    XSetWindowAttributes wa;
    wa.event_mask = ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel = BlackPixel(wm().display(), DefaultScreen(wm().display()));

    xwin_ = XCreateWindow(wm().display(),
                          wm().root_window(),
                          bounds.x,
                          bounds.y,
                          bounds.width,
                          bounds.height,
                          0,
                          wm().default_depth(),
                          InputOutput,
                          wm().default_visual(),
                          CWEventMask | CWOverrideRedirect | CWBackPixel,
                          &wa);
    current_window_bounds = bounds;

    Atom xa_net_system_tray_orientation = XInternAtom(wm().display(),
                                                      "_NET_SYSTEM_TRAY_ORIENTATION", false);
    uint32_t orient = NET_SYSTEM_TRAY_ORIENTATION_HORZ;
    XChangeProperty(wm().display(),
                    xwin_,
                    xa_net_system_tray_orientation,
                    xa_net_system_tray_orientation,
                    32,
                    PropModeReplace,
                    (unsigned char *)&orient,
                    1);

    // Acquire selection to indicate we will manage the tray
    Atom xa_tray_selection = XInternAtom(
        wm().display(), (std::string("_NET_SYSTEM_TRAY_S") + std::to_string(DefaultScreen(wm().display()))).c_str(), false);

    Time timestamp = wm().get_timestamp();

    XSetSelectionOwner(wm().display(), xa_tray_selection, xwin_, timestamp);
	if (XGetSelectionOwner(wm().display(), xa_tray_selection) != xwin_) {
      printf("Failed to set tray selection\n");
    } else {
      printf("Acquired tray selection\n");
    }

    // Send MANAGER ClientMessage to notify any waiting tray elements
    Atom xa_manager = XInternAtom(wm().display(), "MANAGER", false);
    xwindow_send_client_msg32(wm().display(),
                              DefaultRootWindow(wm().display()),
                              DefaultRootWindow(wm().display()),
                              xa_manager,
                              timestamp,
                              xa_tray_selection,
                              xwin_);

    initialized = true;
  }
  void flush() {
    bool visible = wm().bar_visible();

    if (scheduled_update_server) {
      if (visible) {
        if (current_window_bounds != bounds) {
          XMoveResizeWindow(wm().display(), xwin_, bounds.x, bounds.y, bounds.width, bounds.height);
          current_window_bounds = bounds;
        }

        if (map_state != STATE_MAPPED) {
          XMapWindow(wm().display(), xwin_);
          map_state = STATE_MAPPED;
        }
      } else {
        if (map_state != STATE_UNMAPPED) {
          XUnmapWindow(wm().display(), xwin_);
          map_state = STATE_UNMAPPED;
        }
      }
    }

    if (scheduled_update_server || scheduled_draw) {
      draw();
    }

    scheduled_update_server = false;
    scheduled_draw = false;
  }

  void compute_bounds() {
    bounds.x = 0;
    bounds.width = wm().screen_width();

    bounds.height = height();
    bounds.y = wm().screen_height() - bounds.height;
  }

  int label_height() { return style.label_font.height() + 2 * style.label_vertical_padding; }

  void draw() {
    WDrawable &d = wm().buffer_pixmap.drawable();
    WRect rect(0, 0, bounds.width, bounds.height);

    fill_rect(d, style.background_color, rect);

    draw_border(d, style.highlight_color, style.highlight_pixels, style.shadow_color, style.shadow_pixels, rect);

    WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels, style.shadow_pixels);

    draw_border(d, style.padding_color, style.padding_pixels, rect2);

    WRect rect3 = rect2.inside_border(style.padding_pixels + style.spacing);
    rect3.height = label_height();

    auto should_skip_tray = [&](Cell &c) {
      if (!c.tray_should_be_mapped) {
        if (c.map_state != STATE_UNMAPPED) {
          XUnmapWindow(wm().display(), c.tray_window);
          c.map_state = STATE_UNMAPPED;
        }
        return true;
      }
      return false;
    };

    auto handle_tray = [&](Cell &c, WRect const &icon_rect) {
      if (c.map_state != STATE_MAPPED) {
        XMapRaised(wm().display(), c.tray_window);
        c.map_state = STATE_MAPPED;
      }
      if (c.current_tray_window_bounds != icon_rect) {
        XMoveResizeWindow(wm().display(), c.tray_window, icon_rect.x, icon_rect.y, icon_rect.width, icon_rect.height);
        c.current_tray_window_bounds = icon_rect;
      }
    };

    for (Cell & c : cells[LEFT]) {
      int width;

      if (c.tray_window != None) {
        if (should_skip_tray(c))
          continue;

        width = rect3.height; // icons will be square
        WRect icon_rect = rect3;
        icon_rect.width = width;
        handle_tray(c, icon_rect);
      }
      // Check for placeholders
      else if (!c.foreground_color || !c.background_color || c.text.empty())
        continue;

      else {
        width = draw_label_with_background(d,
                                           c.text,
                                           style.label_font,
                                           *c.foreground_color,
                                           *c.background_color,
                                           rect3,
                                           style.label_horizontal_padding,
                                           style.label_vertical_padding,
                                           false);
      }

      rect3.width -= (width + style.cell_spacing);

      rect3.x += width + style.cell_spacing;
    }

    BOOST_REVERSE_FOREACH(Cell & c, cells[RIGHT]) {
      int width;

      if (c.tray_window != None) {
        if (!c.tray_should_be_mapped)
          continue;

        width = rect3.height; // icons will be square
        WRect icon_rect = rect3;
        icon_rect.x = icon_rect.x + icon_rect.width - width;
        icon_rect.width = width;
        handle_tray(c, icon_rect);
      }

      // Check for placeholders
      else if (!c.foreground_color || !c.background_color || c.text.empty())
        continue;
      else  {
        width = draw_label_with_background(d,
                                           c.text,
                                           style.label_font,
                                           *c.foreground_color,
                                           *c.background_color,
                                           rect3,
                                           style.label_horizontal_padding,
                                           style.label_vertical_padding,
                                           true);
      }

      rect3.width -= (width + style.cell_spacing);
    }

    XCopyArea(wm().display(), d.drawable(), xwin(), wm().dc.gc(), 0, 0, bounds.width, bounds.height, 0, 0);
  }

  Window xwin() { return xwin_; }

  int height() { return label_height() + top_left_offset() + bottom_right_offset(); }

  int top_left_offset() { return style.highlight_pixels + style.padding_pixels + style.spacing; }

  int bottom_right_offset() { return style.shadow_pixels + style.padding_pixels + style.spacing; }
};

int WBar::height() { return impl_->height(); }

Window WBar::xwin() { return impl_->xwin(); }

void WBar::schedule_update_server() { impl_->scheduled_update_server = true; }

WM &WBar::wm() { return impl_->wm(); }

bool WM::bar_visible() {
  if (selected_view())
    return selected_view()->bar_visible();
  return true;
}

void WBar::flush() { impl_->flush(); }

void WBar::handle_screen_size_changed()
{
  impl_->compute_bounds();

  if (!wm().bar_visible())
    return;

  impl_->scheduled_update_server = true;
}

void WBar::handle_expose(const XExposeEvent &ev)
{
  if (!wm().bar_visible())
    return;

  impl_->scheduled_draw = true;
}


static WBar::CellRef do_insert(WBar::Impl *impl_, boost::shared_ptr<WBar::Cell> const &cell, const WBar::InsertPosition &pos) {
  using InsertPosition = WBar::InsertPosition;

  switch (pos.relative) {
  case InsertPosition::BEFORE:
    impl_->cells[pos.side].insert(impl_->cells[pos.side].iterator_to(*pos.ref.cell), *cell);
    break;
  case InsertPosition::AFTER:
    impl_->cells[pos.side].insert(boost::next(impl_->cells[pos.side].iterator_to(*pos.ref.cell)), *cell);
    break;
  case InsertPosition::BEGIN:
    impl_->cells[pos.side].push_front(*cell);
    break;
  case InsertPosition::END:
  default:
    impl_->cells[pos.side].push_back(*cell);
    break;
  }

  if (impl_->wm().bar_visible())
    impl_->scheduled_draw = true;

  return WBar::CellRef(cell);
}


void WBar::handle_tray_opcode(const XClientMessageEvent &ev) {
  switch (ev.data.l[1]) {
  case SYSTEM_TRAY_REQUEST_DOCK: {
    Window w = ev.data.l[2];
    if (impl_->tray_windows.count(w)) {
      printf("Ignoring duplicate dock request for window: %u\n", (unsigned int)w);
      return;
    }

    printf("Got dock request for window: %u\n", (unsigned int)w);

    if (XAddToSaveSet(wm().display(), w) == 0) {
      printf("Tray window already gone\n");
      return;
    }

    /* Set the window border to 0 */
    {
      XWindowChanges wc;
      wc.border_width = 0;
      XConfigureWindow(wm().display(), w, CWBorderWidth, &wc);
    }

    XSelectInput(wm().display(), w, StructureNotifyMask | PropertyChangeMask);
    XReparentWindow(wm().display(), w, xwin(), 0, 0);
    Time timestamp = wm().get_timestamp();
    // FIXME: handle xembed map info
    if (xwindow_send_client_msg32(wm().display(), w, w, impl_->xa_xembed, timestamp, XEMBED_EMBEDDED_NOTIFY, 0, xwin())) {
      boost::shared_ptr<Cell> cell(new Cell(*this, RIGHT));
      cell->tray_window = w;
      cell->tray_should_be_mapped = true;
      do_insert(impl_.get(), cell, this->begin(RIGHT));
      impl_->tray_windows.emplace(w, cell);
      printf("Adding tray window\n");
    } else {
      XRemoveFromSaveSet(wm().display(), w);
    }
    return;
  }
  default:
    break;
  }
}

void WBar::handle_unmap_notify(XUnmapEvent const &ev) {
  if (impl_->tray_windows.erase(ev.window)) {
    printf("Tray window was unmapped\n");
  }
}

void WBar::CellRef::set_text(const utf8_string &str)
{
  cell->text = str;

  if (!cell->bar.wm().bar_visible())
    return;

  cell->bar.impl_->scheduled_draw = true;
}

void WBar::CellRef::set_foreground(const WColor &c)
{
  cell->foreground_color = &c;

  if (!cell->bar.wm().bar_visible())
    return;

  cell->bar.impl_->scheduled_draw = true;
}

void WBar::CellRef::set_background(const WColor &c)
{
  cell->background_color = &c;

  if (cell->bar.wm().bar_visible())
    cell->bar.impl_->scheduled_draw = true;
}

bool WBar::CellRef::is_placeholder() const { return cell->foreground_color == 0; }

const WColor &WBar::CellRef::foreground() const { return *cell->foreground_color; }

const WColor &WBar::CellRef::background() const { return *cell->background_color; }

const utf8_string &WBar::CellRef::text() const { return cell->text; }

WBar::InsertPosition WBar::before(const CellRef &r) {
  InsertPosition p;
  p.side = r.cell->position;
  p.ref = r;
  p.relative = InsertPosition::BEFORE;
  return p;
}

WBar::InsertPosition WBar::after(const CellRef &r) {
  InsertPosition p;
  p.side = r.cell->position;
  p.ref = r;
  p.relative = InsertPosition::AFTER;
  return p;
}

WBar::InsertPosition WBar::begin(cell_position_t position) {
  InsertPosition p;
  p.side = position;
  p.relative = InsertPosition::BEGIN;
  return p;
}

WBar::InsertPosition WBar::end(cell_position_t position) {
  InsertPosition p;
  p.side = position;
  p.relative = InsertPosition::END;
  return p;
}

WBar::Cell::~Cell()
{
  bar.impl_->cells[position].erase(bar.impl_->cells[position].iterator_to(*this));

  if (bar.wm().bar_visible())
    bar.impl_->scheduled_draw = true;
}

WBar::WBar(WM &wm_, const style::Spec &style_spec)
  : impl_(new Impl(wm_, style_spec)) {}

void WBar::initialize()
{
  impl_->initialize();
}

WBar::~WBar()
{
  /* TODO: complete this */
}

WBar::CellRef WBar::insert(const InsertPosition &pos,
                           const WColor &foreground_color,
                           const WColor &background_color,
                           const utf8_string &text)
{
  boost::shared_ptr<Cell> cell(new Cell(*this, pos.side, foreground_color, background_color, text));
  return do_insert(impl_.get(), cell, pos);
}


WBar::CellRef WBar::placeholder(const InsertPosition &pos)
{
  boost::shared_ptr<Cell> cell(new Cell(*this, pos.side));
  return do_insert(impl_.get(), cell, pos);
}
