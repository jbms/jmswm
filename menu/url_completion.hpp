#ifndef _MENU_URL_COMPLETION_HPP
#define _MENU_URL_COMPLETION_HPP

#include <menu/menu.hpp>
#include <boost/filesystem/path.hpp>

class URLSpec
{
public:
  ascii_string url;
  utf8_string title;

  URLSpec()
  {}
  
  URLSpec(const ascii_string &url, const utf8_string &title)
    : url(url), title(title)
  {}
};

typedef std::vector<URLSpec> URLSpecList;

boost::shared_ptr<WMenuCompletions> make_url_completions(const URLSpecList &list); 

#endif /* _MENU_URL_COMPLETION_HPP */
