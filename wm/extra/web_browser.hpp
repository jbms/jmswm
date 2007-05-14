#ifndef _WM_EXTRA_WEB_BROWSER_HPP
#define _WM_EXTRA_WEB_BROWSER_HPP

#include <util/string.hpp>
#include <vector>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <util/range.hpp>

class BookmarkSpec
{
public:
  BookmarkSpec(const ascii_string &url, const utf8_string &title,
               const std::vector<utf8_string> &categories)
    : url(url), title(title), categories(categories) {}
  BookmarkSpec() {}
  ascii_string url;
  utf8_string title;
  std::vector<utf8_string> categories;
};

class BookmarkSource
{
public:
  virtual void get_bookmarks(std::vector<BookmarkSpec> &result) = 0;
  virtual ~BookmarkSource();
};

class AggregateBookmarkSource : public BookmarkSource
{
  std::vector<boost::shared_ptr<BookmarkSource> > sources;
public:
  AggregateBookmarkSource();
  void add_source(const boost::shared_ptr<BookmarkSource> &source);
  virtual void get_bookmarks(std::vector<BookmarkSpec> &result);
  virtual ~AggregateBookmarkSource();
};

boost::shared_ptr<BookmarkSource> org_file_bookmark_source(const boost::filesystem::path &path);
boost::shared_ptr<BookmarkSource> org_file_list_bookmark_source(const boost::filesystem::path &path);
boost::shared_ptr<BookmarkSource> html_bookmark_source(const boost::filesystem::path &path);

class WM;

void launch_browser(WM &wm, const utf8_string &text);
void launch_browser_interactive(WM &wm, const boost::shared_ptr<BookmarkSource> &source);

#endif /* _WM_EXTRA_WEB_BROWSER_HPP */
