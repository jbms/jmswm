
#include "draw.hpp"

#include <util/log.hpp>

#include <pango/pangoxft.h>

WXDisplay::WXDisplay(const char *display_name)
{
  dpy = XOpenDisplay(display_name);

  if (!dpy)
    ERROR("Failed to connect to X display: %s", XDisplayName(display_name));
}

WXDisplay::~WXDisplay()
{
  XCloseDisplay(dpy);
}

WXContext::WXContext(Display *dpy)
  : dpy(dpy), screen_no(DefaultScreen(dpy))
{
}

WXContext::WXContext(Display *dpy, int screen_no)
  : dpy(dpy), screen_no(screen_no)
{
}

Window WXContext::root_window()
{
  return RootWindow(dpy, screen_no);
}

Colormap WXContext::default_colormap()
{
  return DefaultColormap(dpy, screen_no);
}

Visual *WXContext::default_visual()
{
  return DefaultVisual(dpy, screen_no);
}

int WXContext::screen_width()
{
  return DisplayWidth(dpy, screen_no);
}

int WXContext::screen_height()
{
  return DisplayHeight(dpy, screen_no);
}

int WXContext::default_depth()
{
  return DefaultDepth(dpy, screen_no);
}

WDrawContext::WDrawContext(WXContext &c)
  : c(c)
{
  pango_context_ = pango_xft_get_context(c.display(), c.screen_number());
  gc_ = XCreateGC(c.display(), c.root_window(), 0, NULL);
}

WDrawContext::~WDrawContext()
{
  XFreeGC(c.display(), gc_);
  pango_xft_shutdown_display(c.display(), c.screen_number());
}

WColor::WColor(WDrawContext &c, const ascii_string &name)
  : c(c)
{
  XColor xc, xc_exact;
  if (!XAllocNamedColor(c.xcontext().display(),
                        c.xcontext().default_colormap(), name.c_str(),
                        &xc, &xc_exact))
    ERROR("Failed to allocate color: %s", name.c_str());
  red_ = xc.red;
  green_ = xc.green;
  blue_ = xc.blue;
  pixel_ = xc.pixel;
}

WColor::WColor(WDrawContext &c,
       unsigned short red,
       unsigned short green,
       unsigned short blue)
  : c(c)
{
  XColor xc;
  xc.red = red;
  xc.green = green;
  xc.blue = blue;
  if (!XAllocColor(c.xcontext().display(), c.xcontext().default_colormap(), &xc))
    ERROR("Failed to allocate color: rgb:%04x/%04x/%04x",
          red, green, blue);
  red_ = xc.red;
  green_ = xc.green;
  blue_ = xc.blue;
  pixel_ = xc.pixel;
}

WColor::~WColor()
{
  XFreeColors(c.xcontext().display(),
              c.xcontext().default_colormap(), &pixel_, 1, 0);
}

WFont::WFont(WDrawContext &c, const ascii_string &name)
  : c(c)
{
  pango_font_description_ = pango_font_description_from_string(name.c_str());

  ascii_string loc = setlocale(LC_CTYPE, NULL);
  ascii_string::size_type p = loc.find_first_of(".@");
  if (p != ascii_string::npos)
    loc.resize(p);

  PangoFontMetrics *m
    = pango_context_get_metrics(c.pango_context(),
                                pango_font_description_,
                                pango_language_from_string(loc.c_str()));

  ascent_ = PANGO_PIXELS(pango_font_metrics_get_ascent(m));
  descent_ = PANGO_PIXELS(pango_font_metrics_get_descent(m));
  approx_width_ = PANGO_PIXELS(pango_font_metrics_get_approximate_char_width(m));
  pango_font_metrics_unref(m);
}

WFont::~WFont()
{
  pango_font_description_free(pango_font_description_);
}

WDrawable::WDrawable(WDrawContext &c)
  : c(c), d_(0), xft_draw_(0)
{}

WDrawable::WDrawable(WDrawContext &c, Drawable d)
  : c(c), d_(d)
{
  if (d)
    xft_draw_ =  XftDrawCreate(c.xcontext().display(),
                               d,
                               c.xcontext().default_visual(),
                               c.xcontext().default_colormap());
  /* FIXME: it is not clear whether xft_draw_ needs to be checked for
     NULL */
}

WDrawable::~WDrawable()
{
  if (xft_draw_)
    XftDrawDestroy(xft_draw_);
}

void WDrawable::reset(Drawable d)
{
  if (xft_draw_)
    XftDrawDestroy(xft_draw_);
  d_ = d;
  if (d)
    xft_draw_ =  XftDrawCreate(c.xcontext().display(),
                               d,
                               c.xcontext().default_visual(),
                               c.xcontext().default_colormap());
}

WPixmap::WPixmap(WDrawContext &c)
  : d_(c)
{
}

WPixmap::WPixmap(WDrawContext &c, unsigned int width, unsigned int height)
  : d_(c, XCreatePixmap(c.xcontext().display(), c.xcontext().root_window(),
                        width, height, c.xcontext().default_depth()))
{}

WPixmap::~WPixmap()
{
  if (d_.drawable())
    XFreePixmap(d_.draw_context().xcontext().display(), d_.drawable());
}

void WPixmap::reset(unsigned int width, unsigned int height)
{
  Drawable d = d_.drawable();
  WDrawContext &c = d_.draw_context();
  d_.reset(XCreatePixmap(c.xcontext().display(), c.xcontext().root_window(),
                         width, height, c.xcontext().default_depth()));
  if (d)
    XFreePixmap(c.xcontext().display(), d);
}

static void wcolor_to_xftcolor(const WColor &c, XftColor &xftc)
{
  xftc.color.red = c.red();
  xftc.color.green = c.green();
  xftc.color.blue = c.blue();
  xftc.color.alpha = 0xFFFF;
  xftc.pixel = c.pixel();
}

void draw_label(WDrawable &d, const utf8_string &text,
                const WFont &font, const WColor &c,
                const WRect &rect)
{
  PangoLayout *pl = pango_layout_new(d.draw_context().pango_context());
  pango_layout_set_text(pl, text.data(), text.length());
  pango_layout_set_font_description(pl, font.pango_font_description());
  pango_layout_set_width(pl, rect.width * PANGO_SCALE);
  pango_layout_set_single_paragraph_mode(pl, TRUE);
  pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_MIDDLE);

  XftColor xftc;
  wcolor_to_xftcolor(c, xftc);

  int y = (rect.y + (rect.height - font.height()) / 2 + font.ascent());
  int x = rect.x;

  pango_xft_render_layout_line(d.xft_draw(), &xftc, pango_layout_get_line(pl, 0),
                               x * PANGO_SCALE, y * PANGO_SCALE);
  g_object_unref(pl);
}

void draw_label_with_text_background(WDrawable &d, const utf8_string &text,
                                     const WFont &font, const WColor &c,
                                     const WColor &background,
                                     const WRect &rect)
{
  PangoLayout *pl = pango_layout_new(d.draw_context().pango_context());
  pango_layout_set_text(pl, text.data(), text.length());
  pango_layout_set_font_description(pl, font.pango_font_description());
  pango_layout_set_width(pl, rect.width * PANGO_SCALE);
  pango_layout_set_single_paragraph_mode(pl, TRUE);
  pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_MIDDLE);

  PangoAttrList *attr_list = pango_attr_list_new();
  PangoAttribute *attr1
    = pango_attr_background_new(background.red(),
                                background.green(),
                                background.blue());
  attr1->start_index = 0;
  attr1->end_index = text.size();
  pango_attr_list_insert(attr_list, attr1);

  pango_layout_set_attributes(pl, attr_list);
  

  XftColor xftc;
  wcolor_to_xftcolor(c, xftc);

  int y = (rect.y + (rect.height - font.height()) / 2 + font.ascent());
  int x = rect.x;

  pango_xft_render_layout_line(d.xft_draw(), &xftc, pango_layout_get_line(pl, 0),
                               x * PANGO_SCALE, y * PANGO_SCALE);
  g_object_unref(pl);

  // It appears that pango_attribute_destroy must not be called on
  // attr1.
  pango_attr_list_unref(attr_list);
}

int compute_label_width(WDrawable &d,
                        const utf8_string &text,
                        const WFont &font,
                        int available_width)
{
  PangoLayout *pl = pango_layout_new(d.draw_context().pango_context());
  pango_layout_set_text(pl, text.data(), text.length());
  pango_layout_set_font_description(pl, font.pango_font_description());
  pango_layout_set_width(pl, available_width * PANGO_SCALE);
  pango_layout_set_single_paragraph_mode(pl, TRUE);
  pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_MIDDLE);

  PangoLayoutLine *line = pango_layout_get_line(pl, 0);

  PangoRectangle ink_rect;
  pango_layout_line_get_pixel_extents(line, &ink_rect, 0);

  g_object_unref(pl);
  return ink_rect.width;
}

int draw_label_with_background(WDrawable &d,
                               const utf8_string &text,
                               const WFont &font,
                               const WColor &foreground,
                               const WColor &background,
                               const WRect &rect,
                               int label_horizontal_padding,
                               int label_vertical_padding,
                               bool right_aligned)
{
  PangoLayout *pl = pango_layout_new(d.draw_context().pango_context());
  pango_layout_set_text(pl, text.data(), text.length());
  pango_layout_set_font_description(pl, font.pango_font_description());
  pango_layout_set_width(pl, (rect.width - 2 * label_horizontal_padding)
                         * PANGO_SCALE);
  pango_layout_set_single_paragraph_mode(pl, TRUE);
  pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_MIDDLE);

  PangoLayoutLine *line = pango_layout_get_line(pl, 0);

  PangoRectangle ink_rect;
  pango_layout_line_get_pixel_extents(line, &ink_rect, 0);

  int width = ink_rect.width;
  
  int frame_width = width + 2 * label_horizontal_padding;

  int base_x = rect.x;

  if (right_aligned)
    base_x += (rect.width - frame_width);

  XftColor xftc;
  wcolor_to_xftcolor(foreground, xftc);

  int y = (rect.y + label_vertical_padding
           + (rect.height - 2 * label_vertical_padding - font.height()) / 2
           + font.ascent());
  int x = base_x + label_horizontal_padding;


  /* Draw background */
  fill_rect(d, background,
            WRect(base_x, rect.y,
                  frame_width,
                  rect.height));

  pango_xft_render_layout_line(d.xft_draw(), &xftc, line,
                               x * PANGO_SCALE, y * PANGO_SCALE);
  g_object_unref(pl);

  return frame_width;
}

void draw_label_with_cursor_and_selection(WDrawable &d, const utf8_string &text,
                                          const WFont &font, const WColor &c,
                                          const WColor &cursor_foreground,
                                          const WColor &cursor_background,
                                          const WColor &selection_foreground,
                                          const WColor &selection_background,
                                          const WRect &rect,
                                          int cursor_position,
                                          int selection_position)
{
  PangoLayout *pl = pango_layout_new(d.draw_context().pango_context());
  pango_layout_set_text(pl, text.data(), text.length());
  pango_layout_set_font_description(pl, font.pango_font_description());
  pango_layout_set_width(pl, rect.width * PANGO_SCALE);
  pango_layout_set_single_paragraph_mode(pl, TRUE);
  pango_layout_set_ellipsize(pl, PANGO_ELLIPSIZE_MIDDLE);

  PangoAttrList *attr_list = pango_attr_list_new();
  PangoAttribute *attr1
    = pango_attr_background_new(cursor_background.red(),
                                cursor_background.green(),
                                cursor_background.blue());
  attr1->start_index = cursor_position;
  attr1->end_index = cursor_position + 1;
  pango_attr_list_insert(attr_list, attr1);

  PangoAttribute *attr2
    = pango_attr_foreground_new(cursor_foreground.red(),
                                cursor_foreground.green(),
                                cursor_foreground.blue());
  attr2->start_index = cursor_position;
  attr2->end_index = cursor_position + 1;
  pango_attr_list_insert(attr_list, attr2);

  if (selection_position != -1 && selection_position != cursor_position)
  {
    int selection_start, selection_end;
    if (selection_position < cursor_position)
    {
      selection_start = selection_position;
      selection_end = cursor_position;
    } else
    {
      selection_start = cursor_position + 1;
      selection_end = selection_position;
    }
    PangoAttribute *attr3
      = pango_attr_background_new(selection_background.red(),
                                  selection_background.green(),
                                  selection_background.blue());
    attr3->start_index = selection_start;
    attr3->end_index = selection_end;
    pango_attr_list_insert(attr_list, attr3);

    PangoAttribute *attr4
      = pango_attr_foreground_new(selection_foreground.red(),
                                  selection_foreground.green(),
                                  selection_foreground.blue());
    
    attr4->start_index = selection_start;
    attr4->end_index = selection_end;
    pango_attr_list_insert(attr_list, attr4);
  }

  
  pango_layout_set_attributes(pl, attr_list);

  XftColor xftc;
  wcolor_to_xftcolor(c, xftc);

  int y = (rect.y + (rect.height - font.height()) / 2 + font.ascent());
  int x = rect.x;

  pango_xft_render_layout_line(d.xft_draw(), &xftc, pango_layout_get_line(pl, 0),
                               x * PANGO_SCALE, y * PANGO_SCALE);
  g_object_unref(pl);

  // It appears that pango_attribute_destroy must not be called on
  // attr1, attr2.
  pango_attr_list_unref(attr_list);
}


void fill_rect(WDrawable &d, const WColor &background,
               const WRect &rect)
{
  XftColor xftc;
  wcolor_to_xftcolor(background, xftc);

  XftDrawRect(d.xft_draw(), &xftc, rect.x, rect.y,
              rect.width, rect.height);
}

void draw_horizontal_line(WDrawable &d, const WColor &c,
                          int x, int y, int length)
{
  fill_rect(d, c, WRect(x, y, length, 1));
}

void draw_vertical_line(WDrawable &d, const WColor &c,
                          int x, int y, int length)
{
  fill_rect(d, c, WRect(x, y, 1, length));
}

void draw_border(WDrawable &d,
                 const WColor &highlight_color, int highlight_pixels,
                 const WColor &shadow_color, int shadow_pixels,
                 const WRect &rect)
{
  int a, b;

  a = (shadow_pixels != 0);
  b = 0;
  for (int i = 0; i < highlight_pixels; ++i)
  {
    draw_horizontal_line(d, highlight_color, rect.x + i, rect.y + i,
                         rect.width - a - i);
    draw_vertical_line(d, highlight_color, rect.x + i, rect.y + i + 1,
                       rect.height - b - 1 - i);

    if (a < shadow_pixels)
      ++a;

    if (b < shadow_pixels)
      ++b;
  }

  a = (highlight_pixels != 0);
  b = 0;
  for (int i = 0; i < shadow_pixels; ++i)
  {
    draw_horizontal_line(d, shadow_color,
                         rect.x + a,
                         rect.y + rect.height - 1 - i,
                         rect.width - a - i);
    draw_vertical_line(d, shadow_color,
                       rect.x + rect.width - 1 - i,
                       rect.y + b,
                       rect.height - b - 1 - i);

    if (a < highlight_pixels)
      ++a;

    if (b < highlight_pixels)
      ++b;
  }
}

void draw_border(WDrawable &d,
                 const WColor &c, int width,
                 const WRect &rect)
{
  fill_rect(d, c, WRect(rect.x, rect.y, width, rect.height));
  fill_rect(d, c, WRect(rect.x + width, rect.y, rect.width - width, width));
  
  fill_rect(d, c, WRect(rect.x + rect.width - width, rect.y + width, width,
                        rect.height - width));
  fill_rect(d, c, WRect(rect.x + width, rect.y + rect.height - width,
                        rect.width - width, width));
}

void draw_border(WDrawable &d,
                 const WColor &c,
                 int left_width, int top_width,
                 int right_width, int bottom_width,
                 const WRect &rect)
{
  fill_rect(d, c, WRect(rect.x, rect.y, left_width, rect.height));
  fill_rect(d, c, WRect(rect.x + left_width, rect.y, rect.width - right_width, top_width));
  
  fill_rect(d, c, WRect(rect.x + rect.width - right_width, rect.y, right_width,
                        rect.height));
  fill_rect(d, c, WRect(rect.x + left_width, rect.y + rect.height - bottom_width,
                        rect.width - right_width, bottom_width));
  
}
