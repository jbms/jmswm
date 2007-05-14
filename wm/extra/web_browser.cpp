#include <wm/extra/web_browser.hpp>
#include <wm/wm.hpp>
#include <menu/menu.hpp>
#include <menu/url_completion.hpp>
#include <boost/bind.hpp>
#include <util/range.hpp>
#include <util/path.hpp>
#include <boost/optional.hpp>
#include <map>
#include <set>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <util/spawn.hpp>

WMenu::Completer url_completer(const boost::shared_ptr<BookmarkSource> &source);



BookmarkSource::~BookmarkSource() {}

AggregateBookmarkSource::~AggregateBookmarkSource() {}

AggregateBookmarkSource::AggregateBookmarkSource() {}

void AggregateBookmarkSource::add_source(const boost::shared_ptr<BookmarkSource> &source)
{
  sources.push_back(source);
}

void AggregateBookmarkSource::get_bookmarks(std::vector<BookmarkSpec> &result)
{
  boost::for_each(sources,
                  boost::bind(&BookmarkSource::get_bookmarks, _1, boost::ref(result)));
}

namespace
{

  class FileCacheManager
  {
    boost::optional<std::time_t> last_update_time;
    boost::filesystem::path path_;
  public:
    FileCacheManager(const boost::filesystem::path &path);
    const boost::filesystem::path &path() const { return path_; }
    bool update_needed();
  };

  FileCacheManager::FileCacheManager(const boost::filesystem::path &path)
    : path_(path)
  {}

  bool FileCacheManager::update_needed()
  {
    try
    {
      std::time_t t = last_write_time(path_);
      if (!last_update_time || last_update_time.get() != t)
      {
        last_update_time = t;
        return true;
      }
      return false;
    } catch (boost::filesystem::filesystem_error &e)
    {
      // FIXME: decide how to handle this exception
      return false;
    }
  }
  
  class OrgFileBookmarkSource : public BookmarkSource
  {
    FileCacheManager file_manager;
    std::vector<BookmarkSpec> cache;
    void update_cache();
  public:
    OrgFileBookmarkSource(const boost::filesystem::path &path);
    const boost::filesystem::path &path() const { return file_manager.path(); }
    virtual void get_bookmarks(std::vector<BookmarkSpec> &result);
    virtual ~OrgFileBookmarkSource();
  };

  OrgFileBookmarkSource::~OrgFileBookmarkSource() {}

  void OrgFileBookmarkSource::get_bookmarks(std::vector<BookmarkSpec> &result)
  {
    update_cache();
    result.insert(result.end(), cache.begin(), cache.end());
  }

  OrgFileBookmarkSource::OrgFileBookmarkSource(const boost::filesystem::path &path)
    : file_manager(path)
  {}

  void OrgFileBookmarkSource::update_cache()
  {
    if (!file_manager.update_needed())
      return;

    boost::filesystem::ifstream ifs(path());

    // FIXME: this use of static may not be thread safe
    static const boost::regex category_regex("^#\\+CATEGORY:[ \t]*([^ \t].*)$");
    static const boost::regex outline_regex("^(\\*)+[ \t]*([^* \t][^*]*)$");
    static const boost::regex org_url("\\[\\[((?:http|ftp|https)://[^\\]]+)\\]\\[([^\\]]+)\\]\\]");
    static const boost::regex text_url("(?:http|ftp|https)://[0-9A-Za-z_!~*'().;?:@&=+$,%#-]+");

    cache.clear();

    std::string line;
    std::vector<utf8_string> categories;
    boost::smatch results;
    while (getline(ifs, line))
    {
      // Check for a #+CATEGORY   line
      if (regex_match(line, results, category_regex))
      {
        categories.resize(1);
        categories[0] = results[1];
        continue;
      }

      // Search for URLs
      std::string::const_iterator begin = line.begin(), end = line.end();
      while (begin != end)
      {
        if (regex_search(begin, end, results, org_url))
        {
          begin = results[0].second;
          ascii_string url = results[1];
          utf8_string title = results[2];
          cache.push_back(BookmarkSpec(url, title, categories));
        } else if (regex_search(begin, end, results, text_url))
        {
          begin = results[0].second;
          cache.push_back(BookmarkSpec(results[0], utf8_string(), categories));
        } else break;
      }

      // Check for an outline specification
      if (regex_match(line, results, outline_regex))
      {
        int level = results[1].length();
        categories.resize(level + 1);
        categories[level] = results[2];
      }
    }

    ifs.close();
  }

  class OrgFileListBookmarkSource : public BookmarkSource
  {
    typedef std::map<boost::filesystem::path, boost::shared_ptr<OrgFileBookmarkSource> > Sources;
    Sources sources;
    FileCacheManager file_manager;
  public:
    OrgFileListBookmarkSource(const boost::filesystem::path &path);
    const boost::filesystem::path &path() const { return file_manager.path(); }
    void update_cache();
    virtual void get_bookmarks(std::vector<BookmarkSpec> &result);
    virtual ~OrgFileListBookmarkSource();
  };

  OrgFileListBookmarkSource::OrgFileListBookmarkSource(const boost::filesystem::path &path)
    : file_manager(path)
  {}

  void OrgFileListBookmarkSource::update_cache()
  {
    if (!file_manager.update_needed())
      return;

    boost::filesystem::ifstream ifs(path());
    std::string line;
    std::set<boost::filesystem::path> new_paths;
    while (!getline(ifs, line))
    {
      try
      {
        boost::filesystem::path p(expand_path_home(line));
        new_paths.insert(p);
      } catch (boost::filesystem::filesystem_error &e)
      {
        // FIXME: decide how to handle this error
      }
    }

    ifs.close();

    for (Sources::iterator it = sources.begin(), next; it != sources.end(); it = next)
    {
      next = boost::next(it);
      if (!new_paths.count(it->first))
        sources.erase(it);
    }

    BOOST_FOREACH (const boost::filesystem::path &path, new_paths)
    {
      if (!sources.count(path))
        sources.insert(std::make_pair(path, new OrgFileBookmarkSource(path)));
    }
  }
  
  void OrgFileListBookmarkSource::get_bookmarks(std::vector<BookmarkSpec> &result)
  {
    boost::for_each(boost::make_transform_range(sources, select2nd),
                    boost::bind(&OrgFileBookmarkSource::get_bookmarks, _1, boost::ref(result)));
  }
  
  OrgFileListBookmarkSource::~OrgFileListBookmarkSource() {}


  class HTMLBookmarkSource : public BookmarkSource
  {
    FileCacheManager file_manager;
    std::vector<BookmarkSpec> cache;
    void update_cache();
  public:
    HTMLBookmarkSource(const boost::filesystem::path &path);
    const boost::filesystem::path &path() const { return file_manager.path(); }
    virtual void get_bookmarks(std::vector<BookmarkSpec> &result);
    virtual ~HTMLBookmarkSource();
  };

  HTMLBookmarkSource::~HTMLBookmarkSource() {}

  void HTMLBookmarkSource::get_bookmarks(std::vector<BookmarkSpec> &result)
  {
    update_cache();
    result.insert(result.end(), cache.begin(), cache.end());
  }

  HTMLBookmarkSource::HTMLBookmarkSource(const boost::filesystem::path &path)
    : file_manager(path)
  {}

  std::string decode_html(const std::string &str)
  {
    std::string result;

    for (size_t i = 0; i < str.length(); ++i)
    {
      if (str[i] == '&')
      {
        size_t end = str.find(';', i);
        if (end != std::string::npos)
        {
          std::string seq = str.substr(i, end - i + 1);
          if (seq == "&gt;")
            result += '>';
          else if (seq == "&lt;")
            result += '<';
          else if (seq == "&amp;")
            result += '&';
          else
            result += seq;
          i = end + 1;
          continue;
        }
      }
      result += str[i];
    }
    return result;
  }
  
  void HTMLBookmarkSource::update_cache()
  {
    static const boost::regex bookmark_regex("<A HREF=\"([^\"]*)\"[^>]*>([^<]*)</A>");
    
    if (!file_manager.update_needed())
      return;

    cache.clear();

    boost::filesystem::ifstream ifs(path());

    std::string line;
    while (getline(ifs, line))
    {
      boost::smatch results;
      if (regex_search(line, results, bookmark_regex))
      {
        ascii_string enc_url = results[1];
        utf8_string enc_title = results[2];

        cache.push_back(BookmarkSpec(decode_html(enc_url), decode_html(enc_title),
                                     std::vector<utf8_string>()));
      }
    }
  }
}


boost::shared_ptr<BookmarkSource> org_file_bookmark_source(const boost::filesystem::path &path)
{
  return boost::shared_ptr<BookmarkSource>(new OrgFileBookmarkSource(path));
}

boost::shared_ptr<BookmarkSource> org_file_list_bookmark_source(const boost::filesystem::path &path)
{
  return boost::shared_ptr<BookmarkSource>(new OrgFileListBookmarkSource(path));
}

boost::shared_ptr<BookmarkSource> html_bookmark_source(const boost::filesystem::path &path)
{
  return boost::shared_ptr<BookmarkSource>(new HTMLBookmarkSource(path));
}

static bool compare_url_results(const std::pair<int, URLSpec> &a, const std::pair<int, URLSpec> &b)
{
  int val = a.first - b.first;
  if (val > 0)
    return true;
  if (val < 0)
    return false;
  val = a.second.url.size() - b.second.url.size();
  if (val < 0)
    return true;
  if (val > 0)
    return false;
  val = a.second.title.size() - b.second.title.size();
  if (val < 0)
    return true;
  return false;
}

static WMenu::Completions url_completions(const boost::shared_ptr<boost::optional<std::vector<BookmarkSpec> > > &bookmarks,
                                          const boost::shared_ptr<BookmarkSource> &source,
                                          const WMenu::InputState &input)
{
  if (!*bookmarks)
  {
    *bookmarks = std::vector<BookmarkSpec>();
    source->get_bookmarks(bookmarks->get());
  }
  
  WMenu::Completions completions;

  std::vector<std::pair<int, URLSpec> > results;
  std::vector<utf8_string> words;
  boost::algorithm::split(words, input.text, boost::algorithm::is_any_of(" "),
                          boost::algorithm::token_compress_on);
  
  BOOST_FOREACH (const BookmarkSpec &b, bookmarks->get())
  {
    bool good = true;
    bool none = true;
    int score = 0;
    BOOST_FOREACH (const utf8_string &word, words)
    {
      if (word.empty())
        continue;

      int cur_score = 0;

      if (boost::algorithm::ifind_first(b.url, word))
        ++cur_score;
      if (boost::algorithm::ifind_first(b.title, word))
        ++cur_score;
      BOOST_FOREACH (const utf8_string &cat, b.categories)
        if (boost::algorithm::ifind_first(cat, word))
          ++cur_score;
      
      if (cur_score == 0)
      {
        good = false;
        break;
      }
      score += cur_score;
      none = false;
    }

    if (good && !none)
      results.push_back(std::make_pair(score, URLSpec(b.url, b.title)));
  }

  boost::sort(results, compare_url_results);  
  if (!results.empty())
  {
    std::vector<URLSpec> specs;
    boost::copy(boost::make_transform_range(results, select2nd),
                std::back_inserter(specs));
    completions = make_url_completions(specs);
  }

  return completions;
}

WMenu::Completer url_completer(const boost::shared_ptr<BookmarkSource> &source)
{
  return boost::bind(&url_completions,
                     boost::shared_ptr<boost::optional<std::vector<BookmarkSpec> > >
                     (new boost::optional<std::vector<BookmarkSpec> >),
                     source, _1);
}


static bool is_search_query(const utf8_string &text)
{
  if (boost::algorithm::contains(text, "://"))
    return false;

  if (text.find(' ') != utf8_string::npos)
    return true;

  if (text.find('.') != utf8_string::npos)
    return false;

  return true;
}

void launch_browser(WM &wm, const utf8_string &text)
{
  ascii_string program = "/home/jbms/bin/browser";
  utf8_string arg = text;
  if (text.empty())
  {
    arg = "http://www.google.com";
  } else if (is_search_query(text))
  {
    program = "/home/jbms/bin/browser-google-results";
  }
  spawnl(0, program.c_str(), program.c_str(), arg.c_str(), (const char *)0);
}

void launch_browser_interactive(WM &wm, const boost::shared_ptr<BookmarkSource> &source)
{
  wm.menu.read_string("URL:",
                      boost::bind(&launch_browser, boost::ref(wm), _1),
                      WMenu::FailureAction(),
                      url_completer(source),
                      true /* use delay */,
                      true /* use separate thread */);
}
