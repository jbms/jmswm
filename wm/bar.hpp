#ifndef _WM_BAR_HPP
#define _WM_BAR_HPP

#include <draw/draw.hpp>

#include <boost/intrusive/ilist.hpp>

#include <wm/define_style.hpp>

#include <boost/shared_ptr.hpp>

WM_DEFINE_STYLE_TYPE(WBarStyle,
                     
                     /* style type features */
                     ()
                     ((WFont, ascii_string, label_font))

                     ((WColor, ascii_string, highlight_color))
                     ((WColor, ascii_string, shadow_color))
                     ((WColor, ascii_string, padding_color))
                     ((WColor, ascii_string, background_color)),


                     /* regular features */
                     ()
                     ((int, highlight_pixels))
                     ((int, shadow_pixels))
                     ((int, padding_pixels))
                     ((int, spacing))
                     ((int, label_horizontal_padding))
                     ((int, label_vertical_padding))
                     ((int, cell_spacing))

                     )

WM_DEFINE_STYLE_TYPE(WBarCellStyle,
                     /* style type features */
                     ()
                     ((WColor, ascii_string, foreground_color))
                     ((WColor, ascii_string, background_color)),
                     
                     /* regular features */
                     ()
                     )


class WM;

class WBar
{
private:
  WM &wm_;
  WBarStyle style;
public:
  WM &wm() { return wm_; }

  friend class WM;

private:
  Window xwin_;
  WRect bounds, current_window_bounds;

  Window xwin() { return xwin_; }
  
public:
  enum cell_position_t { LEFT = 0, RIGHT = 1};
  
private:

  class Cell : public boost::intrusive::ilist_base_hook<0>
  {
  public:
    WBar &bar;
    cell_position_t position;
    utf8_string text;
    const WColor *foreground_color;
    const WColor *background_color;

    Cell(WBar &bar,
         cell_position_t position,
         const WColor &foreground_color,
         const WColor &background_color,
         const utf8_string &text)
      : bar(bar),
        position(position),
        text(text),
        foreground_color(&foreground_color),
        background_color(&background_color)
    {}

    Cell(WBar &bar,
         cell_position_t position)
      : bar(bar),
        position(position),
        foreground_color(0),
        background_color(0)
    {}

    ~Cell();
  };

  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<0>::value_traits<Cell>,
    true /* constant-time size */ > CellList;

  CellList cells[2];

  bool scheduled_update_server, scheduled_draw;

  bool initialized;

  enum map_state_t { STATE_MAPPED, STATE_UNMAPPED } map_state;

  int label_height();
  int top_left_offset();
  int bottom_right_offset();

  void compute_bounds();
  void flush();
  void draw();

  void handle_screen_size_changed();
  void handle_expose(const XExposeEvent &ev);

  void initialize();

public:

  int height();

  void schedule_update_server() { scheduled_update_server = true; }

  class CellRef
  {
    friend class WBar;
    boost::shared_ptr<Cell> cell;

    CellRef(const boost::shared_ptr<Cell> &cell)
      : cell(cell)
    {}
  public:

    CellRef() {}

    CellRef(const CellRef &ref)
      : cell(ref.cell)
    {}

    bool is_placeholder() const
    {
      return cell->foreground_color == 0;
    }

    const WColor &foreground() const
    {
      return *cell->foreground_color;
    }
    
    const WColor &background() const
    {
      return *cell->background_color;
    }
    
    const utf8_string &text() const
    {
      return cell->text;
    }

    void set_text(const utf8_string &str);

    void set_foreground(const WColor &c);
    void set_background(const WColor &c);
    void set_style(const WBarCellStyle &s)
    {
      set_foreground(s.foreground_color);
      set_background(s.background_color);
    }
  };

  class InsertPosition
  {
  private:
    cell_position_t side;
    CellRef ref;
    enum { BEFORE, AFTER, BEGIN, END } relative;
    friend class WBar;
  };

  static InsertPosition before(const CellRef &r)
  {
    InsertPosition p;
    p.side = r.cell->position;
    p.ref = r;
    p.relative = InsertPosition::BEFORE;
    return p;
  }

  static InsertPosition after(const CellRef &r)
  {
    InsertPosition p;
    p.side = r.cell->position;
    p.ref = r;
    p.relative = InsertPosition::AFTER;
    return p;
  }

  static InsertPosition begin(cell_position_t position)
  {
    InsertPosition p;
    p.side = position;
    p.relative = InsertPosition::BEGIN;
    return p;
  }

  static InsertPosition end(cell_position_t position)
  {
    InsertPosition p;
    p.side = position;
    p.relative = InsertPosition::END;
    return p;
  }

  WBar(WM &wm, const WBarStyle::Spec &style_spec);
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

private:

  CellRef insert_end(cell_position_t position,
                     const boost::shared_ptr<Cell> &cell);
  
  CellRef insert_begin(cell_position_t position,
                       const boost::shared_ptr<Cell> &cell);

  CellRef insert_after(const CellRef &ref,
                       cell_position_t position,
                       const boost::shared_ptr<Cell> &cell);

  CellRef insert_before(const CellRef &ref,
                        cell_position_t position,
                        const boost::shared_ptr<Cell> &cell);
};

#endif /* _WM_BAR_HPP */
