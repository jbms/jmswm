#ifndef _WM_EXTRA_PREVIOUS_VIEW_HPP
#define _WM_EXTRA_PREVIOUS_VIEW_HPP

#include <wm/all.hpp>

#include <wm/commands.hpp>

class PreviousViewInfo
{
  utf8_string previous_view_;
  WM &wm_;
  boost::signals::connection conn_;

  void select_view(WView *new_view, WView *old_view)
  {
    if (old_view)
      previous_view_ = old_view->name();
  }
  
public:
  PreviousViewInfo(WM &wm)
    : wm_(wm),
      conn_(wm_.select_view_hook.connect
            (boost::bind(&PreviousViewInfo::select_view, this, _1, _2)))
  {}

  ~PreviousViewInfo()
  {
    conn_.disconnect();
  }
  
  const utf8_string &name() const { return previous_view_; }
  void switch_to()
  {
    switch_to_view(wm_, previous_view_);
  }
};

#endif /* _WM_EXTRA_PREVIOUS_VIEW_HPP */
