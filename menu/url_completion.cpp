#include <wm/all.hpp>

#include <menu/url_completion.hpp>
#include <menu/list_completion.hpp>

namespace menu
{
  namespace url_completion
  {
    class URLCompletions : public Completions
    {
    private:
      typedef std::vector<URLSpec> CompletionList;
      CompletionList completions;
      const Style &entry_style;
  
      int selected;
      int column_width;
      int column_margin;
      int lines;
      int line_height;
    public:
      URLCompletions(const CompletionList &list, const Style &style);

      virtual void compute_dimensions(Menu &menu, int width, int height, int &out_width, int &out_height);
      virtual void draw(Menu &menu, const WRect &rect, WDrawable &d);
      virtual bool complete(InputState &input);
      virtual ~URLCompletions();
    };

    URLCompletions::URLCompletions(const CompletionList &list, const Style &style)
      : completions(list),
        entry_style(style),
        selected(-1)
    {
      if (list.empty())
        throw std::invalid_argument("empty completion list");
    }


    // Ignore columns -- always use 1 column
    void URLCompletions::compute_dimensions(Menu &menu, int width, int height, int &out_width, int &out_height)
    {
      const menu::Style &style = menu.style();
      int available_height = height - (style.border_pixels + 2 * style.completions_spacing);

      line_height = style.label.vertical_padding * 2
        + style.label.font.height() + 2 * style.completions_spacing;

      int max_lines = available_height / line_height;

      lines = completions.size();
  
      if (lines > max_lines)
        lines = max_lines;

      height = style.border_pixels
        + 2 * style.completions_spacing
        + lines * line_height;

      out_height = height;
      out_width = width;
    }
  
    void URLCompletions::draw(Menu &menu, const WRect &rect, WDrawable &d)
    {
      const menu::Style &style = menu.style();

      fill_rect(d, style.completions_background, rect);

      draw_border(d, style.border_color, style.border_pixels,
                  style.border_pixels, style.border_pixels, 0,
                  rect);

      WRect rect2 = rect.inside_border(style.border_pixels, style.border_pixels, style.border_pixels, 0);

      size_t begin_pos_index, end_pos_index;

      if (selected >= lines)
      {
        end_pos_index = selected + 1;
        begin_pos_index = end_pos_index - lines;
      } else
      {
        end_pos_index = lines;
        begin_pos_index = 0;
      }
      if (end_pos_index > completions.size())
        end_pos_index = completions.size();

      size_t base_y = rect2.y;

      for (size_t pos_index = begin_pos_index; pos_index < end_pos_index; ++pos_index)
      {
        int row = pos_index - begin_pos_index;
        WRect cell_rect(rect2.x,
                        base_y + style.completions_spacing + line_height * row,
                        rect2.width, line_height);

        const WColor *url_color, *title_color;

        WColor title_unselected(d.draw_context(), "gold1");
        WColor title_selected(d.draw_context(), "black");

        if ((int)pos_index == selected)
        {
          fill_rect(d, entry_style.selected_background, cell_rect);

          url_color = &entry_style.selected_url_foreground;
          title_color = &entry_style.selected_title_foreground;
        } else
        {
          url_color = &entry_style.normal_url_foreground;
          title_color = &entry_style.normal_title_foreground;
        }

        WRect url_rect(cell_rect.x, cell_rect.y, cell_rect.width / 2,
                       cell_rect.height);

        WRect title_rect(cell_rect.x + cell_rect.width / 2,
                         cell_rect.y,
                         cell_rect.width - cell_rect.width / 2, cell_rect.height);

        WRect url_rect2 = url_rect.inside_border(style.completions_spacing);

        WRect url_rect3 = url_rect2.inside_lr_tb_border
          (style.label.horizontal_padding,
           style.label.vertical_padding);

        WRect title_rect2 = title_rect.inside_border(style.completions_spacing);

        WRect title_rect3 = title_rect2.inside_lr_tb_border
          (style.label.horizontal_padding,
           style.label.vertical_padding);

        draw_label(d, completions[pos_index].url, style.label.font, *url_color, url_rect3);
        draw_label(d, completions[pos_index].title, style.label.font, *title_color, title_rect3);
      }
    }

    /* Returns true if completions should be recomputed */
    bool URLCompletions::complete(InputState &input)
    {
      if (selected  < 0)
        selected = 0;
      else
        selected = (selected + 1) % completions.size();

      list_completion::apply_completion_simple(input, completions[selected].url);
      return false;
    }
  
    URLCompletions::~URLCompletions()
    {
    }

    Menu::CompletionsPtr make_url_completions(const URLSpecList &list, const Style &style)
    {
      return Menu::CompletionsPtr(new URLCompletions(list, style));
    }
    
  } // namespace menu::url_completion
} // namespace menu
