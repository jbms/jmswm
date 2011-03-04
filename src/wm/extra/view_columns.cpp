#include <wm/extra/view_columns.hpp>
#include <boost/bind.hpp>

std::size_t get_desired_column_count(WView *view)
{
  if (Property<std::size_t> prop = desired_column_count(view))
    return prop.get();

  // Default
  return 2;
}

DesiredColumnCountByName::DesiredColumnCountByName(WM &wm)
  : c(wm.create_view_hook.connect(boost::bind(&DesiredColumnCountByName::handle_create_view, this, _1)))
{
}

DesiredColumnCountByName::~DesiredColumnCountByName()
{}

void DesiredColumnCountByName::set(const utf8_string &name, std::size_t count)
{
  map.insert(std::make_pair(name, count));
}

void DesiredColumnCountByName::handle_create_view(WView *view) const
{
  Map::const_iterator it = map.find(view->name());
  if (it == map.end())
    return;
  desired_column_count(view) = it->second;
}
