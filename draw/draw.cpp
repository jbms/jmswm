
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

void fill_rect(WDrawable &d, const WColor &background,
               const WRect &rect)
{
  XftColor xftc;
  wcolor_to_xftcolor(background, xftc);

  XftDrawRect(d.xft_draw(), &xftc, rect.x, rect.y,
              rect.width, rect.height);
}

void draw_rect_border(WDrawable &d, const WColor &border_color,
                      int border_width,
                      const WRect &rect)
{
  XftColor xftc;
  wcolor_to_xftcolor(border_color, xftc);
  XftDrawRect(d.xft_draw(), &xftc, rect.x, rect.y, rect.width, border_width);
  XftDrawRect(d.xft_draw(), &xftc, rect.x, rect.y + rect.height - border_width,
              rect.width, border_width);
  XftDrawRect(d.xft_draw(), &xftc, rect.x, rect.y + border_width, border_width,
              rect.height - 2 * border_width);
  XftDrawRect(d.xft_draw(), &xftc, rect.x + rect.width - border_width,
              rect.y + border_width, border_width,
              rect.height - 2 * border_width);
}

void draw_tile(WDrawable &d,
               const WColor &border_color,
               const WColor &background,
               int border_width,
               const WRect &rect)
{
  draw_rect_border(d, border_color, border_width, rect);
  fill_rect(d, background, rect.inside_border(border_width));
}
