#ifndef _MENU_URL_COMPLETION_HPP
#define _MENU_URL_COMPLETION_HPP

#include <menu/menu.hpp>
#include <vector>
#include <style/style.hpp>
#include <style/common.hpp>

namespace menu
{
  namespace url_completion
  {

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

    STYLE_DEFINITION(Style,
                     ((normal_title_foreground, WColor, ascii_string),
                      (selected_title_foreground, WColor, ascii_string),
                      (normal_url_foreground, WColor, ascii_string),
                      (selected_url_foreground, WColor, ascii_string),
                      (selected_background, WColor, ascii_string)))

    typedef std::vector<URLSpec> URLSpecList;

    Menu::CompletionsPtr make_url_completions(const URLSpecList &list,
                                              const Style &style);
    
  } // namespace menu::url_completion
} // namespace menu

#endif /* _MENU_URL_COMPLETION_HPP */
