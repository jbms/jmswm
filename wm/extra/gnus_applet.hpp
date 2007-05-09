#ifndef _WM_EXTRA_GNUS_APPLET_HPP
#define _WM_EXTRA_GNUS_APPLET_HPP

#include <wm/all.hpp>

class GnusApplet
{
  WM &wm;
  WBarCellStyle style;
  InotifyEvent inotify;
  int wd;
  WBar::CellRef placeholder;
  
  class CompareGroups
  {
  private:
    typedef boost::tuple<int, const ascii_string &> ComparisonPair;
    ComparisonPair get_pair(const ascii_string &a)
    {
      if (a == "mail.misc")
        return ComparisonPair(0, a);
      return ComparisonPair(50, a);
    }
  public:
    bool operator()(const ascii_string &a, const ascii_string &b)
    {
      return get_pair(a) < get_pair(b);
    }
  };

  typedef WBar::CellRef CellRef;
  typedef std::map<ascii_string, CellRef, CompareGroups> GroupMap;
  GroupMap groups;
  typedef std::map<ascii_string, ascii_string> AbbrevMap;
  AbbrevMap abbrevs;

  void handle_inotify_event(int wd, uint32_t mask, uint32_t cookie,
                            const char *pathname);

  void update();

public:

  GnusApplet(WM &wm, const WBarCellStyle::Spec &style_spec,
             const WBar::InsertPosition &pos);

  void switch_to_mail(WM &wm);
  
};

#endif /* _WM_EXTRA_GNUS_APPLET_HPP */
