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

  class CellRef
  {
    friend class WBar;
    boost::shared_ptr<Cell> cell;

    CellRef(const boost::shared_ptr<Cell> &cell)
      : cell(cell)
    {}
  public:

    CellRef(const CellRef &ref)
      : cell(ref.cell)
    {}

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
  };

  WBar(WM &wm, const WBarStyle::Spec &style_spec);
  ~WBar();

  CellRef insert_end(cell_position_t position,
                     const WColor &foreground_color,
                     const WColor &background_color,
                     const utf8_string &text = utf8_string());
  
  CellRef insert_begin(cell_position_t position,
                       const WColor &foreground_color,
                       const WColor &background_color,
                       const utf8_string &text = utf8_string());

  CellRef insert_after(const CellRef &ref,
                       cell_position_t position,
                       const WColor &foreground_color,
                       const WColor &background_color,
                       const utf8_string &text = utf8_string());

  CellRef insert_before(const CellRef &ref,
                        cell_position_t position,
                        const WColor &foreground_color,
                        const WColor &background_color,
                        const utf8_string &text = utf8_string());
};

#endif /* _WM_BAR_HPP */
