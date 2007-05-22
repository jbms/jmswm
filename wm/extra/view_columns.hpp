#ifndef _WM_EXTRA_VIEW_COLUMNS_HPP
#define _WM_EXTRA_VIEW_COLUMNS_HPP

#include <wm/wm.hpp>

PROPERTY_ACCESSOR(WView, std::size_t, desired_column_count)

std::size_t get_desired_column_count(WView *view);

class DesiredColumnCountByName
{
  typedef std::map<utf8_string, std::size_t> Map;
  Map map;
  boost::signals::scoped_connection c;
  void handle_create_view(WView *view) const;
public:
  DesiredColumnCountByName(WM &wm);
  ~DesiredColumnCountByName();
  void set(const utf8_string &name, std::size_t count);
};

#endif /* _WM_EXTRA_VIEW_COLUMNS_HPP */
