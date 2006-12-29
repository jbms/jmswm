#ifndef _DRAW_HPP
#define _DRAW_HPP

#include <util/string.hpp>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xcms.h>
#include <pango/pango.h>

class WXDisplay
{
private:
  Display *dpy;
public:

  WXDisplay(const char *display_name);
  ~WXDisplay();

  Display *display()
  {
    return dpy;
  }
};

class WXContext
{
private:
  Display *dpy;
  int screen_no;
public:
  WXContext(Display *dpy);
  WXContext(Display *dpy, int screen_no);

  Display *display()
  {
    return dpy;
  }

  int screen_number()
  {
    return screen_no;
  }

  Window root_window();
  Colormap default_colormap();
  Visual *default_visual();
  int screen_width();
  int screen_height();
  int default_depth();
  
};

class WDrawContext
{
private:
  WXContext &c;
  PangoContext *pango_context_;
  GC gc_;
public:
  WDrawContext(WXContext &c);
  ~WDrawContext();

  WXContext &xcontext()
  {
    return c;
  }

  PangoContext *pango_context()
  {
    return pango_context_;
  }

  GC gc()
  {
    return gc_;
  }
};

class WColor
{
private:
  WDrawContext &c;
  unsigned long pixel_;
  unsigned short red_, green_, blue_;
public:
  WColor(WDrawContext &c, const ascii_string &name);
  WColor(WDrawContext &c,
         unsigned short red,
         unsigned short green,
         unsigned short blue);
  ~WColor();

  unsigned short red() const
  {
    return red_;
  }
  
  unsigned short green() const
  {
    return green_;
  }
  
  unsigned short blue() const
  {
    return blue_;
  }

  unsigned long pixel() const
  {
    return pixel_;
  }
};

class WFont
{
private:
  WDrawContext &c;
  PangoFontDescription *pango_font_description_;
  int ascent_;
  int descent_;
public:
  WFont(WDrawContext &c, const ascii_string &name);
  ~WFont();

  PangoFontDescription *pango_font_description() const
  {
    return pango_font_description_;
  }
  
  int ascent() const
  {
    return ascent_;
  }
  
  int pango_descent() const
  {
    return descent_;
  }
  
  int height() const
  {
    return ascent_ + descent_;
  }
};

class WRect
{
public:
  int x, y, width, height;

  WRect()
  {}
  
  WRect(int x, int y, int width, int height)
    : x(x), y(y), width(width), height(height)
  {}

  WRect inside_border(int l, int t, int r, int b) const
  {
    return WRect(x + l, y + t, width - l - r, height - t - b);
  }

  WRect inside_border(int w) const
  {
    return inside_border(w, w, w, w);
  }

  WRect inside_lr_tb_border(int lr, int tb) const
  {
    return inside_border(lr, tb, lr, tb);
  }

  WRect inside_tl_br_border(int tl, int br) const
  {
    return inside_border(tl, tl, br, br);
  }
};

inline bool operator==(WRect const &a, WRect const &b)
{
  return (a.x == b.x)
    && (a.y == b.y)
    && (a.width == b.width)
    && (a.height == b.height);
}

inline bool operator!=(WRect const &a, WRect const &b)
{
  return !(a == b);
}


class WDrawable
{
private:
  WDrawContext &c;
  Drawable d_;
  XftDraw *xft_draw_;
public:
  WDrawable(WDrawContext &c);
  WDrawable(WDrawContext &c, Drawable d);
  ~WDrawable();

  void reset(Drawable d);

  WDrawContext &draw_context() const
  {
    return c;
  }

  Drawable drawable() const
  {
    return d_;
  }
  
  XftDraw *xft_draw() const
  {
    return xft_draw_;
  }
};

class WPixmap
{
private:
  WDrawable d_;
public:
  WPixmap(WDrawContext &c);
  WPixmap(WDrawContext &c, unsigned int width, unsigned int height);
  ~WPixmap();
  void reset(unsigned int width, unsigned int height);
  
  WDrawable &drawable()
  {
    return d_;
  }
};

void draw_label(WDrawable &d, const utf8_string &text,
                const WFont &font, const WColor &c,
                const WRect &rect);

void fill_rect(WDrawable &d, const WColor &background,
               const WRect &rect);

/* These draw 1-pixel wide lines */
void draw_horizontal_line(WDrawable &d, const WColor &c,
                          int x, int y, int length);

void draw_vertical_line(WDrawable &d, const WColor &c,
                          int x, int y, int length);

void draw_border(WDrawable &d,
                 const WColor &highlight_color, int highlight_pixels,
                 const WColor &shadow_color, int shadow_pixels,
                 const WRect &rect);

void draw_border(WDrawable &d,
                 const WColor &c, int width,
                 const WRect &rect);


#endif /* _DRAW_HPP */
