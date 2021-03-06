#ifndef _WM_EXTRA_BAR_VIEW_APPLET_HPP
#define _WM_EXTRA_BAR_VIEW_APPLET_HPP

#include <wm/all.hpp>

STYLE_DEFINITION(BarViewAppletStyle,
                 ((selected, WBarCellStyle, style::Spec),
                  (normal, WBarCellStyle, style::Spec)))

class BarViewApplet
{

  WM &wm;
  BarViewAppletStyle style;
  struct CompareViewByName
  {
    bool operator()(WView *a, WView *b)
    {
      return a->name() < b->name();
    }
    
  };
  typedef WBar::CellRef CellRef;
  typedef std::map<WView *, CellRef, CompareViewByName> ViewMap;
  ViewMap views;

  void create_view(WView *view);
  void destroy_view(WView *view);
  void select_view(WView *new_view, WView *old_view);
  
public:

  // Commands
  void select_next();
  void select_prev();
  
  BarViewApplet(WM &wm, const style::Spec &style_spec);
  
};

#endif /* _WM_EXTRA_BAR_VIEW_APPLET_HPP */
