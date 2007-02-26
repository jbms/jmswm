#include <wm/all.hpp>

#include <menu/url_completion.hpp>
#include <menu/list_completion.hpp>

#include <boost/filesystem/fstream.hpp>
#include <boost/regex.hpp>

#include <boost/optional.hpp>

class Bookmark
{
public:
  ascii_string url;
  utf8_string title;
  /* TODO: maybe add last visit or add date */

  Bookmark()
  {}
  
  Bookmark(const ascii_string &url, const utf8_string &title)
    : url(url), title(title)
  {}
};

class WMenuURLCompletions : public WMenuCompletions
{
private:
  typedef std::vector<Bookmark> CompletionList;
  CompletionList completions;
  
  int selected;
  int columns;
  int column_width;
  int column_margin;
  int lines;
  int line_height;
public:
  WMenuURLCompletions(const CompletionList &list, int columns);

  virtual int compute_height(WMenu &menu, int width, int height);
  virtual void draw(WMenu &menu, const WRect &rect, WDrawable &d);
  virtual bool complete(WMenu::InputState &input);
  virtual ~WMenuURLCompletions();
};

WMenuURLCompletions::WMenuURLCompletions(const CompletionList &list, int columns)
  : completions(list),
    selected(-1),
    columns(columns)
{
  if (list.empty())
    throw std::invalid_argument("empty completion list");
}

int WMenuURLCompletions::compute_height(WMenu &menu, int width, int height)
{
  // FIXME: don't use this style
  WFrameStyle &style = menu.wm().frame_style;

  int available_height = height;

  // FIXME: use a style entry instead of a constant here for padding
  column_margin = 15;

  int available_width = width - style.highlight_pixels
    - style.shadow_pixels
    - 2 * style.padding_pixels
    - (columns - 1) * column_margin
    - columns * 2 * style.spacing;

  // FIXME: use a style entry instead of a constant here for padding
  column_width = available_width / columns;

  line_height = style.label_vertical_padding * 2
    + style.label_font.height() + 2 * style.spacing;

  int max_lines = available_height / line_height;

  lines = (completions.size() + columns - 1)
    / columns;
  if (lines > max_lines)
    lines = max_lines;

  height = style.highlight_pixels + style.shadow_pixels
    + 2 * style.padding_pixels + 2 * style.spacing
    + lines * line_height;

  return height;
}
  
void WMenuURLCompletions::draw(WMenu &menu, const WRect &rect, WDrawable &d)
{
  // FIXME: this is not the correct style to use
  WFrameStyle &style = menu.wm().frame_style;
    
  WFrameStyleSpecialized &substyle
    = style.normal.inactive;

  // FIXME: don't hardcode this color
  WColor c(d.draw_context(), "grey10");

  fill_rect(d, c, rect);

  draw_border(d, substyle.highlight_color, style.highlight_pixels,
              substyle.shadow_color, style.shadow_pixels,
              rect);

  WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                         style.shadow_pixels);

  draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

    
  size_t max_pos_index = lines * columns;
    
  // FIXME: allow an offset
  if (max_pos_index > completions.size())
    max_pos_index = completions.size();

  size_t base_y = rect2.y + style.spacing;

  for (size_t pos_index = 0; pos_index < max_pos_index; ++pos_index)
  {
    size_t row = pos_index / columns;
    size_t col = pos_index % columns;

    WRect cell_rect(rect2.x + style.spacing + column_width * col,
                    base_y + style.spacing + line_height * row,
                    column_width, line_height);

    const WColor *url_color, *title_color;

    WColor title_unselected(d.draw_context(), "white");
    WColor title_selected(d.draw_context(), "black");

    if ((int)pos_index == selected)
    {
      fill_rect(d, substyle.label_background_color, cell_rect);

      url_color = &substyle.background_color;
      title_color = &title_selected;
    } else
    {
      url_color = &substyle.label_background_color;
      title_color = &title_unselected;
    }

    WRect url_rect(cell_rect.x, cell_rect.y, cell_rect.width / 2,
                   cell_rect.height);

    WRect title_rect(cell_rect.x + cell_rect.width / 2,
                     cell_rect.y,
                     cell_rect.width - cell_rect.width / 2, cell_rect.height);

    WRect url_rect2 = url_rect.inside_border(style.spacing);

    WRect url_rect3 = url_rect2.inside_lr_tb_border
      (style.label_horizontal_padding,
       style.label_vertical_padding);

    WRect title_rect2 = title_rect.inside_border(style.spacing);

    WRect title_rect3 = title_rect2.inside_lr_tb_border
      (style.label_horizontal_padding,
       style.label_vertical_padding);

    draw_label(d, completions[pos_index].url, style.label_font, *url_color, url_rect3);
    draw_label(d, completions[pos_index].title, style.label_font, *title_color, title_rect3);
  }
}

/* Returns true if completions should be recomputed */
bool WMenuURLCompletions::complete(WMenu::InputState &input)
{
  if (selected  < 0)
    selected = 0;
  else
    selected = (selected + 1) % completions.size();

  apply_completion_simple(input, completions[selected].url);
  return false;
}
  
WMenuURLCompletions::~WMenuURLCompletions()
{
}



static std::string decode_html(const std::string &str)
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

typedef std::vector<Bookmark> BookmarkList;

typedef boost::shared_ptr<BookmarkList> BookmarkListPtr;

static void load_bookmarks(BookmarkList &list, const boost::filesystem::path &mozilla_profile_dir)
{

  static const boost::regex bookmark_regex("<A HREF=\"([^\"]*)\"[^>]*>([^<]*)</A>");

  
  boost::filesystem::path path(mozilla_profile_dir / "bookmarks.html");
  boost::filesystem::ifstream ifs(path);
  if (!ifs)
  {
    WARN("Failed to load Mozilla bookmarks from: %s", path.native_file_string().c_str());
    return;
  }

  std::string line;
  while (getline(ifs, line))
  {
    boost::smatch results;
    if (regex_search(line, results, bookmark_regex))
    {
      ascii_string enc_url = results[1];
      utf8_string enc_title = results[2];

      list.push_back(Bookmark(decode_html(enc_url), decode_html(enc_title)));
    }
  }
}

static WMenu::Completions url_completions(const boost::shared_ptr<boost::optional<BookmarkList> > &bookmarks,
                                          const boost::filesystem::path &mozilla_profile_dir,
                                          const WMenu::InputState &input)
{
  if (!*bookmarks)
  {
    *bookmarks = BookmarkList();
    load_bookmarks(bookmarks->get(), mozilla_profile_dir);
  }
  
  WMenu::Completions completions;

  std::vector<Bookmark> results;
  std::vector<utf8_string> words;
  boost::algorithm::split(words, input.text, boost::algorithm::is_any_of(" "),
                          boost::algorithm::token_compress_on);
  
  BOOST_FOREACH (const Bookmark &b, bookmarks->get())
  {
    bool good = true;
    bool none = true;
    BOOST_FOREACH (const utf8_string &word, words)
    {
      if (word.empty())
        continue;

      if (!boost::algorithm::ifind_first(b.url, word)
          && !boost::algorithm::ifind_first(b.title, word))
      {
        good = false;
        break;
      }
      none = false;
    }

    if (good && !none)
    {
      results.push_back(b);
    }
  }

  if (!results.empty())
    completions.reset(new WMenuURLCompletions(results, 2 /* 2 column display */));

  return completions;
}

WMenu::Completer url_completer(const boost::filesystem::path &mozilla_profile_dir)
{
  return boost::bind(&url_completions,
                     boost::shared_ptr<boost::optional<BookmarkList> >
                     (new boost::optional<BookmarkList>),
                     mozilla_profile_dir, _1);
}
