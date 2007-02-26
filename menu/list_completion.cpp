
#include <wm/all.hpp>
#include <menu/list_completion.hpp>



class WMenuListCompletions : public WMenuCompletions
{
private:
  typedef std::vector<utf8_string> CompletionList;
  CompletionList completions;
  StringCompletionApplicator applicator;  
  bool complete_common_prefix;
  
  int selected;
  int columns;
  int column_width;
  int lines;
  int line_height;
public:
  WMenuListCompletions(const CompletionList &list, const StringCompletionApplicator &applicator,
                       bool complete_common_prefix);

  virtual int compute_height(WMenu &menu, int width, int height);
  virtual void draw(WMenu &menu, const WRect &rect, WDrawable &d);
  virtual bool complete(WMenu::InputState &input);
  virtual ~WMenuListCompletions();
};

WMenuListCompletions::WMenuListCompletions(const CompletionList &list, const StringCompletionApplicator &applicator,
                                           bool complete_common_prefix)
  : completions(list),
    applicator(applicator),
    complete_common_prefix(complete_common_prefix),
    selected(-1)
{
  if (list.empty())
    throw std::invalid_argument("empty completion list");
}

int WMenuListCompletions::compute_height(WMenu &menu, int width, int height)
{
  // FIXME: don't use this style
  WFrameStyle &style = menu.wm().frame_style;

  int available_height = height;

  int maximum_width = 0;
  BOOST_FOREACH (const utf8_string &str, completions)
  {
    if ((int)str.size() > maximum_width)
      maximum_width = (int)str.size();
  }

  int maximum_pixels = maximum_width * style.label_font.approximate_width();

  int available_width = width - style.highlight_pixels
    - style.shadow_pixels
    - 2 * style.padding_pixels;

  // FIXME: use a style entry instead of a constant here for padding
  column_width = maximum_pixels + 10 + style.spacing * 2
    + 2 * style.label_horizontal_padding;

  if (column_width > available_width)
    column_width = available_width;

  columns = available_width / column_width;

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
  
void WMenuListCompletions::draw(WMenu &menu, const WRect &rect, WDrawable &d)
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

    
  int max_pos_index = lines * columns;
    
  // FIXME: allow an offset
  if (max_pos_index > (int)completions.size())
    max_pos_index = (int)completions.size();

  int base_y = rect2.y + style.spacing;

  for (int pos_index = 0; pos_index < max_pos_index; ++pos_index)
  {
    int row = pos_index / columns;
    int col = pos_index % columns;

    WRect label_rect1(rect2.x + column_width * col,
                      base_y + line_height * row,
                      column_width, line_height);

    WRect label_rect2 = label_rect1.inside_border(style.spacing);

    WRect label_rect3 = label_rect2.inside_lr_tb_border
      (style.label_horizontal_padding,
       style.label_vertical_padding);

    if (pos_index == selected)
    {
      const WColor &text_color = substyle.background_color;
      draw_label_with_text_background(d, completions[pos_index],
                                      style.label_font,
                                      text_color, substyle.label_background_color,
                                      label_rect3);
    } else
    {
      const WColor &text_color = substyle.label_background_color;        
      draw_label(d, completions[pos_index], style.label_font,
                 text_color, label_rect3);
    }
  }
}

/* Returns true if completions should be recomputed */
bool WMenuListCompletions::complete(WMenu::InputState &input)
{
  if (!complete_common_prefix)
  {
    if (selected  < 0)
      selected = 0;
    else
      selected = (selected + 1) % completions.size();
      
    applicator(input, completions[selected]);
    return false;
  }
  else
  {
    // Find the largest common prefix

    utf8_string prefix = completions[0];
    for (size_t i = 1; i < completions.size(); ++i)
    {
      const utf8_string &str = completions[i];
      if (prefix.empty())
        break;
      if (prefix.length() > str.length())
        prefix.resize(str.length());
      utf8_string::iterator it = boost::mismatch(prefix, str.begin()).first;
      prefix.erase(it, prefix.end());
    }

    WMenu::InputState new_state = input;
    applicator(new_state, prefix);
    if (new_state.text != input.text || new_state.cursor_position != input.cursor_position)
    {
      input = new_state;
      return true;
    }
    complete_common_prefix = false;
    return false;
  }
}
  
WMenuListCompletions::~WMenuListCompletions()
{
}

void apply_completion_simple(WMenu::InputState &state, const utf8_string &completion)
{
  state.text = completion;
  state.cursor_position = completion.size();
}

boost::shared_ptr<WMenuCompletions> completion_list(const std::vector<utf8_string> &list,
                                                    const StringCompletionApplicator &applicator,
                                                    bool complete_common_prefix)
{
  boost::shared_ptr<WMenuCompletions> result;
  if (!list.empty())
    result.reset(new WMenuListCompletions(list, applicator, complete_common_prefix));
  
  return result;
}


class PrefixCompleter
{
  boost::shared_ptr<std::vector<utf8_string> > list;
public:
  PrefixCompleter(const std::vector<utf8_string> &list, bool sort = false)
    : list(new std::vector<utf8_string>(list))
  {
    if (sort)
      boost::sort(*this->list);
  }
  
  WMenu::Completions operator()(const WMenu::InputState &state) const
  {
    std::vector<utf8_string> results;
    BOOST_FOREACH (const utf8_string &str, *list)
    {
      if (boost::algorithm::starts_with(str, state.text))
        results.push_back(str);
    }
    return completion_list(results);
  }
};

WMenu::Completer prefix_completer(const std::vector<utf8_string> &list)
{
  return PrefixCompleter(list);
}
