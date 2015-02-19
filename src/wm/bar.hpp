#ifndef _WM_BAR_HPP
#define _WM_BAR_HPP

#include <draw/draw.hpp>
#include <style/style.hpp>
#include <memory>

#include <X11/Xlib.h>

#include <boost/shared_ptr.hpp>


STYLE_DEFINITION(WBarStyle,
                 ((label_font, WFont, ascii_string),
                  (highlight_color, WColor, ascii_string),
                  (shadow_color, WColor, ascii_string),
                  (padding_color, WColor, ascii_string),
                  (background_color, WColor, ascii_string),
                  (highlight_pixels, int),
                  (shadow_pixels, int),
                  (padding_pixels, int),
                  (spacing, int),
                  (label_horizontal_padding, int),
                  (label_vertical_padding, int),
                  (cell_spacing, int)))

STYLE_DEFINITION(WBarCellStyle,
                 ((foreground_color, WColor, ascii_string),
                  (background_color, WColor, ascii_string)))

class WM;

class WBar
{
public:
  struct Impl;
private:
  std::unique_ptr<Impl> impl_;

public:
  WM &wm();

  friend class WM;

private:

  Window xwin();

public:
  enum cell_position_t { LEFT = 0, RIGHT = 1};

private:

  void flush();

  void handle_screen_size_changed();
  void handle_expose(const XExposeEvent &ev);
  void handle_tray_opcode(const XClientMessageEvent &ev);
  void handle_unmap_notify(const XUnmapEvent &ev);

  void initialize();

public:
  class Cell;

  int height();
  void schedule_update_server();

  class  CellRef
  {
  public:
    boost::shared_ptr<Cell> cell;

    CellRef(const boost::shared_ptr<Cell> &cell)
      : cell(cell)
    {}

    CellRef() {}

    CellRef(const CellRef &ref)
      : cell(ref.cell)
    {}

    bool is_placeholder() const;

    const WColor &foreground() const;
    const WColor &background() const;

    const utf8_string &text() const;

    void set_text(const utf8_string &str);

    void set_foreground(const WColor &c);
    void set_background(const WColor &c);
    void set_style(const WBarCellStyle &s)
    {
      set_foreground(s.foreground_color);
      set_background(s.background_color);
    }
  };

  struct InsertPosition
  {
    cell_position_t side;
    CellRef ref;
    enum { BEFORE, AFTER, BEGIN, END } relative;
  };

  static InsertPosition before(const CellRef &r);
  static InsertPosition after(const CellRef &r);
  static InsertPosition begin(cell_position_t position);
  static InsertPosition end(cell_position_t position);

  WBar(WM &wm, const style::Spec &style_spec);
  ~WBar();

  CellRef insert(const InsertPosition &pos,
                 const WColor &foreground_color,
                 const WColor &background_color,
                 const utf8_string &text = utf8_string());

  CellRef insert(const InsertPosition &pos,
                 const WBarCellStyle &style,
                 const utf8_string &text = utf8_string())
  {
    return insert(pos, style.foreground_color,
                  style.background_color,
                  text);
  }

  CellRef placeholder(const InsertPosition &pos);
};

#endif /* _WM_BAR_HPP */
