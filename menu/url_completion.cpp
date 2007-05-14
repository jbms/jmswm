#include <wm/all.hpp>

#include <menu/url_completion.hpp>
#include <menu/list_completion.hpp>

//#include <boost/filesystem/fstream.hpp>
//#include <boost/regex.hpp>

//#include <boost/optional.hpp>


class WMenuURLCompletions : public WMenuCompletions
{
private:
  typedef std::vector<URLSpec> CompletionList;
  CompletionList completions;
  
  int selected;
  int column_width;
  int column_margin;
  int lines;
  int line_height;
public:
  WMenuURLCompletions(const CompletionList &list);

  virtual void compute_dimensions(WMenu &menu, int width, int height, int &out_width, int &out_height);
  virtual void draw(WMenu &menu, const WRect &rect, WDrawable &d);
  virtual bool complete(WMenu::InputState &input);
  virtual ~WMenuURLCompletions();
};

WMenuURLCompletions::WMenuURLCompletions(const CompletionList &list)
  : completions(list),
    selected(-1)
{
  if (list.empty())
    throw std::invalid_argument("empty completion list");
}


// Ignore columns -- always use 1 column
void WMenuURLCompletions::compute_dimensions(WMenu &menu, int width, int height, int &out_width, int &out_height)
{
  // FIXME: don't use this style
  WFrameStyle &style = menu.wm().frame_style;

  int available_height = height - (style.highlight_pixels + style.shadow_pixels
                                   + 2 * style.spacing);


  line_height = style.label_vertical_padding * 2
    + style.label_font.height() + 2 * style.spacing;

  int max_lines = available_height / line_height;

  lines = completions.size();
  
  if (lines > max_lines)
    lines = max_lines;

  height = style.highlight_pixels + style.shadow_pixels
    + 2 * style.spacing
    + lines * line_height;

  out_height = height;
  out_width = width;
}
  
void WMenuURLCompletions::draw(WMenu &menu, const WRect &rect, WDrawable &d)
{
  // FIXME: this is not the correct style to use
  WFrameStyle &style = menu.wm().frame_style;
    
  WFrameStyleSpecialized &substyle
    = style.normal.inactive;

  // FIXME: don't hardcode this color
  WColor white_color(d.draw_context(), "white");
  WColor c(d.draw_context(), "black");
  //WColor c(d.draw_context(), "black");  

  fill_rect(d, c, rect);

  draw_border(d, white_color, style.highlight_pixels,
              style.highlight_pixels, style.shadow_pixels, 0,
              rect);

  WRect rect2 = rect.inside_border(style.highlight_pixels,
                                   style.highlight_pixels,
                                   style.shadow_pixels, 0);

  //draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

    
  size_t max_pos_index = lines;
    
  // FIXME: allow an offset
  if (max_pos_index > completions.size())
    max_pos_index = completions.size();

  size_t base_y = rect2.y;

  for (size_t pos_index = 0; pos_index < max_pos_index; ++pos_index)
  {
    WRect cell_rect(rect2.x,
                    base_y + style.spacing + line_height * pos_index,
                    rect2.width, line_height);

    const WColor *url_color, *title_color;

    WColor title_unselected(d.draw_context(), "gold1");
    WColor title_selected(d.draw_context(), "black");

    if ((int)pos_index == selected)
    {
      fill_rect(d, substyle.label_background_color, cell_rect);

      url_color = &substyle.background_color;
      title_color = &title_selected;
    } else
    {
      WColor c(d.draw_context(), "black");
      fill_rect(d, c, cell_rect);
      
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

boost::shared_ptr<WMenuCompletions> make_url_completions(const URLSpecList &list)
{
  return boost::shared_ptr<WMenuCompletions>(new WMenuURLCompletions(list));
}
