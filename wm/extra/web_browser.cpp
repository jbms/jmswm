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

using menu::url_completion::URLSpec;

PROPERTY_ACCESSOR(WClient, utf8_string, web_browser_title)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_url)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_frame_id)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_frame_tag)

menu::Menu::Completer url_completer(const boost::shared_ptr<BookmarkSource> &source,
                                    const menu::url_completion::Style &style);



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
    static const boost::regex text_url("(?:http|ftp|https)://[/0-9A-Za-z_!~*'.;?:@&=+$,%#-]+");

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

      // Check for an outline specification
      if (regex_match(line, results, outline_regex))
      {
        int level = results[1].length();
        categories.resize(level + 1);
        categories[level] = results[2];
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
    if (!ifs)
      WARN("failed to open org list file: %s", path().native_file_string().c_str());
    std::set<boost::filesystem::path> new_paths;
    while (getline(ifs, line))
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
      {
        sources.insert(std::make_pair(path, boost::shared_ptr<OrgFileBookmarkSource>(new OrgFileBookmarkSource(path))));
      }
    }
  }
  
  void OrgFileListBookmarkSource::get_bookmarks(std::vector<BookmarkSpec> &result)
  {
    update_cache();
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

static bool order_url_to_remove_duplicates(const BookmarkSpec &a,
                                           const BookmarkSpec &b)
{
  int val = a.url.compare(b.url);
  if (val < 0)
    return true;
  if (val > 0)
    return false;

  val = a.title.size() - b.title.size();
  if (val < 0)
    return false;
  if (val > 0)
    return true;

  val = a.categories.size() - b.categories.size();
  if (val > 0)
    return true;
  return false;
}

static bool compare_url_to_remove_duplicates(const BookmarkSpec &a,
                                            const BookmarkSpec &b)
{
  return a.url == b.url;
}

static void remove_duplicate_bookmarks(std::vector<BookmarkSpec> &spec)
{
  boost::sort(spec, order_url_to_remove_duplicates);
  spec.erase(boost::unique(spec, compare_url_to_remove_duplicates), spec.end());
}

static menu::Menu::CompletionsPtr
url_completions(const boost::shared_ptr<boost::optional<std::vector<BookmarkSpec> > > &bookmarks,
                const boost::shared_ptr<BookmarkSource> &source,
                const menu::url_completion::Style &style,
                const menu::InputState &input)
{
  if (!*bookmarks)
  {
    *bookmarks = std::vector<BookmarkSpec>();
    source->get_bookmarks(bookmarks->get());
    remove_duplicate_bookmarks(bookmarks->get());
  }

  menu::Menu::CompletionsPtr completions;

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
    completions = make_url_completions(specs, style);
  }

  return completions;
}

menu::Menu::Completer url_completer(const boost::shared_ptr<BookmarkSource> &source,
                                    const menu::url_completion::Style &style)
{
  return boost::bind(&url_completions,
                     boost::shared_ptr<boost::optional<std::vector<BookmarkSpec> > >
                     (new boost::optional<std::vector<BookmarkSpec> >),
                     source, boost::cref(style), _1);
}


static bool is_search_query(const utf8_string &text)
{
  if (boost::algorithm::contains(text, "://"))
    return false;

  if (boost::algorithm::starts_with(text, "about:"))
    return false;

  if (text.find(' ') != utf8_string::npos)
    return true;

  if (text.find('.') != utf8_string::npos)
    return false;

  return true;
}

static  ascii_string url_escape(const utf8_string &in)
{
  static const char hexDigits[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  ascii_string out;
  utf8_string::const_iterator p = in.begin();
  while (p != in.end())
  {
    char c = *p;
    if (isalnum(c) || c == '_' || c == '-')
      out += c;
    else if (c == ' ')
      out += '+';
    else {
      out += '%';
      out += hexDigits[c / 16];
      out += hexDigits[c % 16];
    }
    p++;
  }
  return out;
}

static ascii_string get_search_query_url(const utf8_string &query, bool direct)
{
  ascii_string out("http://www.google.com/search?q=");
  out += url_escape(query);
  if (direct)
    out += "&btnI";
  return out;
}

const ascii_string get_url_from_user_input(const utf8_string &text, bool direct)
{
  if (text.empty())
    return "http://www.google.com";
  else if (is_search_query(text))
    return get_search_query_url(text, direct);
  else
    return text;
}

void launch_browser(const utf8_string &text, bool direct)
{
  ascii_string program = "/home/jbms/bin/conkeror";
  utf8_string arg = get_url_from_user_input(text, direct);
  spawnl(0, program.c_str(), program.c_str(), arg.c_str(), (const char *)0);
}

void load_url_in_existing_frame(const ascii_string &frame_id,
                                const utf8_string &text,
                                bool direct)
{
  const ascii_string &url = get_url_from_user_input(text, direct);
  ascii_string program = "/home/jbms/bin/conkeror-load-existing";
  spawnl(0, program.c_str(), program.c_str(), frame_id.c_str(), url.c_str(), (const char *)0);
}

void launch_browser_interactive(WM &wm, const boost::shared_ptr<BookmarkSource> &source,
                                const menu::url_completion::Style &style)
{
  menu::InitialState state;
  if (WFrame *frame = wm.selected_frame())
  {
    if (Property<ascii_string> url = web_browser_url(frame->client()))
      state = menu::InitialState::selected_suffix(url.get());
  }
  
  wm.menu.read_string("URL:", state,
                      boost::bind(&launch_browser, _1, false),
                      menu::Menu::FailureAction(),
                      url_completer(source, style),
                      true /* use delay */,
                      true /* use separate thread */);
}

void load_url_existing_interactive(WM &wm, const boost::shared_ptr<BookmarkSource> &source,
                                   const menu::url_completion::Style &style)
{
  menu::InitialState state;
  ascii_string frame_id;
  if (WFrame *frame = wm.selected_frame())
  {
    if (Property<ascii_string> id = web_browser_frame_id(frame->client()))
      frame_id = id.get();
    else
      return launch_browser_interactive(wm, source, style);
    
    if (Property<ascii_string> url = web_browser_url(frame->client()))
      state = menu::InitialState::selected_suffix(url.get());
  }
  
  wm.menu.read_string("URL:", state,
                      boost::bind(&load_url_in_existing_frame, frame_id, _1, false),
                      menu::Menu::FailureAction(),
                      url_completer(source, style),
                      true /* use delay */,
                      true /* use separate thread */);
}

void write_bookmark(const ascii_string &url, const utf8_string &title,
                    const boost::filesystem::path &output_org_path)
{
  boost::filesystem::ofstream ofs(output_org_path, std::ios_base::app | std::ios_base::out);
  if (!ofs)
  {
    // FIXME: decide how to handle this error
    WARN("Failed to write bookmark");
    return;
  }

  ofs << "* [[" << url << "][" << title << "]]\n";
}

void bookmark_current_url(WM &wm, const boost::filesystem::path &output_org_path)
{
  if (WFrame *frame = wm.selected_frame())
  {
    Property<ascii_string> url = web_browser_url(frame->client());
    Property<utf8_string> title = web_browser_title(frame->client());
    if (url && title)
      write_bookmark(url.get(), title.get(), output_org_path);
  }
}

static bool handle_client_name(WClient *client)
{
  // Handle Conkeror
  if (client->class_name() == "Xulrunner-bin"
           && !(client->window_type_flags() & WClient::WINDOW_TYPE_DIALOG))
  {
    std::vector<utf8_string> parts;
    utf8_string sep = "<#!#!#>";
    for (utf8_string::size_type x = 0, next; x < client->name().size(); x = next)
    {
      utf8_string::size_type end = client->name().find(sep, x);
      if (end == utf8_string::npos)
      {
        next = end;
        parts.push_back(client->name().substr(x));
      }
      else
      {
        next = end + sep.size();
        parts.push_back(client->name().substr(x, end - x));
      }
    }
    if (parts.size() >= 4)
    {
      web_browser_title(client) = parts[1];
      web_browser_url(client) = parts[0];
      web_browser_frame_id(client) = parts[2];
      web_browser_frame_tag(client) = parts[3];
      if (parts[1].empty())
        client->set_visible_name(parts[0]);
      else
        client->set_visible_name(parts[1]);
      client->set_context_info(parts[0]);
      return true;
    }
  }
  return false;
}

WebBrowserModule::WebBrowserModule(WM &wm)
  // -1, so it loads early
  : c(wm.update_client_name_hook.connect(-1, &handle_client_name))
{}
