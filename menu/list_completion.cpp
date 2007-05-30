
#include <wm/all.hpp>
#include <menu/list_completion.hpp>

namespace menu
{
  namespace list_completion
  {
    
    class ListCompletions : public Completions
    {
    private:
      typedef std::vector<Entry> CompletionList;
      CompletionList completions;
      StringCompletionApplicator applicator;  
      bool complete_common_prefix;
  
      int selected;
      int columns;
      int column_width;
      int lines;
      int line_height;
    public:
      ListCompletions(const InputState &initial_input,
                      const CompletionList &list,
                      const StringCompletionApplicator &applicator,
                      bool complete_common_prefix);

      virtual void compute_dimensions(Menu &menu, int width, int height, int &out_width, int &out_height);
      virtual void draw(Menu &menu, const WRect &rect, WDrawable &d);
      virtual bool complete(InputState &input);
      virtual ~ListCompletions();
    };

    ListCompletions::ListCompletions(const InputState &initial_input,
                                     const CompletionList &list,
                                     const StringCompletionApplicator &applicator,
                                     bool complete_common_prefix)
      : completions(list),
        applicator(applicator),
        complete_common_prefix(complete_common_prefix),
        selected(-1)
    {
      if (completions.empty())
        throw std::invalid_argument("empty completion list");

      /* For improved appearance, if there is only a single completion and
         it is equal to the current input, select that completion. */
      if (completions.size() == 1)
      {
        InputState temp_state(initial_input);
        applicator(temp_state, completions.front().first);
        if (temp_state == initial_input)
        {
          selected = 0;
          this->complete_common_prefix = false;
        }
      }
    }

    void ListCompletions::compute_dimensions(Menu &menu, int width, int height, int &out_width, int &out_height)
    {
      const menu::Style &style = menu.style();
      int available_height = height - (style.border_pixels + 2 * style.completions_spacing);

      int maximum_width = 0;
      BOOST_FOREACH (const utf8_string &str, boost::make_transform_range(completions, select1st))
      {
        if ((int)str.size() > maximum_width)
          maximum_width = (int)str.size();
      }

      int maximum_pixels = maximum_width * style.label.font.approximate_width();

      int available_width = width - 2 * style.border_pixels;

      // FIXME: use a style entry instead of a constant here for padding
      column_width = maximum_pixels + 10
        + 2 * style.label.horizontal_padding;

      if (column_width > available_width)
        column_width = available_width;

      columns = available_width / column_width;

      line_height = style.label.vertical_padding * 2
        + style.label.font.height() + 2 * style.completions_spacing;

      int max_lines = available_height / line_height;

      lines = (completions.size() + columns - 1)
        / columns;
      if (lines > max_lines)
        lines = max_lines;


      height = style.border_pixels + 2 * style.completions_spacing + lines * line_height;

      out_height = height;

      int required_columns;

      if (lines == 1)
        required_columns = completions.size();
      else
        required_columns = columns;

      int required_width = required_columns * column_width + 2 * style.border_pixels;
  
      out_width = required_width;
    }
  
    void ListCompletions::draw(Menu &menu, const WRect &rect, WDrawable &d)
    {
      const menu::Style &style = menu.style();
      fill_rect(d, style.completions_background, rect);

      int spacing = style.completions_spacing;
      int border_pixels = style.border_pixels;

      draw_border(d, style.border_color,
                  border_pixels, border_pixels, border_pixels, 0,
                  rect);

      WRect rect2 = rect.inside_border(border_pixels, border_pixels,
                                       border_pixels, 0);

      

      size_t begin_pos_index, end_pos_index;
  
      if (selected >= lines * columns)
      {
        end_pos_index = selected - (selected % columns) + columns;
        begin_pos_index = end_pos_index - lines * columns;
      } else
      {
        end_pos_index = lines * columns;
        begin_pos_index = 0;
      }
  
      if (end_pos_index > completions.size())
        end_pos_index = completions.size();
  
      int base_y = rect2.y + spacing;

      for (size_t pos_index = begin_pos_index; pos_index < end_pos_index; ++pos_index)
      {
        int row = (pos_index - begin_pos_index) / columns;
        int col = (pos_index - begin_pos_index) % columns;

        WRect label_rect1(rect2.x + column_width * col,
                          base_y + line_height * row,
                          column_width, line_height);

        WRect label_rect2 = label_rect1.inside_lr_tb_border(0, spacing);

        WRect label_rect3 = label_rect2.inside_lr_tb_border
          (style.label.horizontal_padding,
           style.label.vertical_padding);

        const EntryStyle &entry_style = *completions[pos_index].second;

        const style::TextColor &text_color =
          ((int)pos_index == selected) ? entry_style.selected : entry_style.normal;

        draw_label_with_text_background(d, completions[pos_index].first,
                                        style.label.font,
                                        text_color.foreground, text_color.background,
                                        label_rect3);
      }
    }

    /* Returns true if completions should be recomputed */
    bool ListCompletions::complete(InputState &input)
    {
      if (complete_common_prefix)
      {
        // Find the largest common prefix

        utf8_string prefix = completions[0].first;
        for (size_t i = 1; i < completions.size(); ++i)
        {
          const utf8_string &str = completions[i].first;
          if (prefix.empty())
            break;
          if (prefix.length() > str.length())
            prefix.resize(str.length());
          utf8_string::iterator it = boost::mismatch(prefix, str.begin()).first;
          prefix.erase(it, prefix.end());
        }

        InputState new_state = input;
        applicator(new_state, prefix);
        complete_common_prefix = false;
        if (new_state != input)
        {
          input = new_state;
          return true;
        }
      }

      if (selected  < 0)
        selected = 0;
      else
        selected = (selected + 1) % completions.size();
      
      applicator(input, completions[selected].first);
      return false;
    }
  
    ListCompletions::~ListCompletions()
    {
    }

    void apply_completion_simple(InputState &state, const utf8_string &completion)
    {
      state.text = completion;
      state.cursor_position = completion.size();
    }

    Menu::CompletionsPtr completion_list(const InputState &initial_input,
                                         const std::vector<Entry> &list,
                                         const StringCompletionApplicator &applicator,
                                         bool complete_common_prefix)
    {
      Menu::CompletionsPtr result;
      if (!list.empty())
        result.reset(new ListCompletions(initial_input, list, applicator, complete_common_prefix));
  
      return result;
    }


    class PrefixCompleter
    {
      boost::shared_ptr<std::vector<utf8_string> > list;
      const EntryStyle &entry_style;
    public:
      PrefixCompleter(const std::vector<utf8_string> &list,
                      const EntryStyle &style, bool sort = false)
        : list(new std::vector<utf8_string>(list)),
          entry_style(style)
      {
        if (sort)
          boost::sort(*this->list);
      }
  
      Menu::CompletionsPtr operator()(const InputState &state) const
      {
        std::vector<Entry> results;
        BOOST_FOREACH (const utf8_string &str, *list)
        {
          if (boost::algorithm::starts_with(str, state.text))
          {
            results.push_back(Entry(str, &entry_style));
          }
        }
        return completion_list(state, results);
      }
    };

    Menu::Completer prefix_completer(const std::vector<utf8_string> &list,
                                     const EntryStyle &style)
    {
      return PrefixCompleter(list, style);
    }
  } // namespace menu::list_completion
} // namespace menu
