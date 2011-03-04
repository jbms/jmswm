#include <wm/all.hpp>

int WM::bar_height() const
{
  return frame_style.label_font.height()
    + frame_style.label_vertical_padding * 2;
}

static int top_left_offset(const WM &wm)
{
  const WFrameStyle &style = wm.frame_style;
  int tl_off = style.highlight_pixels + style.padding_pixels + style.spacing;

  return tl_off;
}

static int bottom_right_offset(const WM &wm)
{
  const WFrameStyle &style = wm.frame_style;
  int br_off = style.shadow_pixels + style.padding_pixels + style.spacing;

  return br_off;
}

WRect WFrame::client_bounds() const
{
  WRect r;
  if (decorated() || shaded())
  {
    WFrameStyle &style = wm().frame_style;

    int tl_off = top_left_offset(wm());
    int br_off = bottom_right_offset(wm());

    r.x = tl_off;
    r.width = bounds.width - r.x - br_off;
    r.y = tl_off + wm().bar_height() + style.spacing;
    r.height = bounds.height - r.y - br_off;
  } else
  {
    r.x = 0;
    r.y = 0;
    r.width = bounds.width;
    r.height = bounds.height;
  }

  return r;
}

int WM::shaded_height() const
{
  return bar_height() + top_left_offset(*this) + bottom_right_offset(*this);
}

int WM::frame_decoration_height() const
{
  return bar_height() + frame_style.spacing + top_left_offset(*this) + bottom_right_offset(*this);
}

/* TODO: maybe optimize this */
void WFrame::draw()
{
  WDrawable &d = wm().buffer_pixmap.drawable();
  WFrameStyle &style = wm().frame_style;

  WFrameStyleScheme &scheme = (marked() ? style.marked : style.normal);

  WFrameStyleSpecialized &substyle
    = (this == column()->selected_frame() ?
       (column() == column()->view()->selected_column() ?
        scheme.active_selected : scheme.inactive_selected)
       : scheme.inactive);

  if (decorated() || shaded())
  {

    WRect rect(0, 0, bounds.width, bounds.height);

    fill_rect(d, substyle.background_color, rect);

    draw_border(d, substyle.highlight_color, style.highlight_pixels,
                substyle.shadow_color, style.shadow_pixels,
                rect);

    WRect rect2 = rect.inside_tl_br_border(style.highlight_pixels,
                                           style.shadow_pixels);

    draw_border(d, substyle.padding_color, style.padding_pixels, rect2);

    WRect rect3 = rect2.inside_border(style.padding_pixels + style.spacing);
    rect3.height = wm().bar_height();

    // Draw the tag names
    utf8_string tags;
    {
      std::vector<utf8_string> tag_names;
      std::transform(client().view_frames().begin(),
                     client().view_frames().end(),
                     std::back_inserter(tag_names),
                     boost::bind(&WView::name,
                                 boost::bind(&WClient::ViewFrameMap::value_type::first, _1)));
      std::sort(tag_names.begin(), tag_names.end());
      BOOST_FOREACH(const utf8_string &str, tag_names)
      {
        tags += str;
        if (&str != &tag_names.back())
          tags += ' ';
      }
    }
    int width = draw_label_with_background(d, tags,
                                           style.label_font,
                                           substyle.label_foreground_color,
                                           substyle.label_extra_color,
                                           rect3,
                                           style.label_horizontal_padding,
                                           style.label_vertical_padding,
                                           false);

    rect3.width -= (width + style.label_component_spacing);
    rect3.x += (width + style.label_component_spacing);

    // Draw the context info
    if (!client().context_info().empty())
    {
      int width_limit = rect3.width / 2;
      int width2 = draw_label_with_background
        (d, client().context_info(),
         style.label_font,
         substyle.label_foreground_color,
         substyle.label_extra_color,
         WRect(rect3.x + rect3.width - width_limit, rect3.y,
               width_limit, rect3.height),
         style.label_horizontal_padding,
         style.label_vertical_padding,
         true);
      rect3.width -= (width2 + style.label_component_spacing);
      //rect3.x += (width2 + style.label_component_spacing);
    }

    fill_rect(d, substyle.label_background_color, rect3);

    const utf8_string &name = client().visible_name().empty() ?
      client().name() : client().visible_name();

    draw_label(d, name, style.label_font, substyle.label_foreground_color,
               rect3.inside_lr_tb_border(style.label_horizontal_padding,
                                         style.label_vertical_padding));
  }

  if (!shaded())
  {
    WRect client_rect = client_bounds();

    fill_rect(d, style.client_background_color, client_rect);
  }


  XCopyArea(wm().display(), d.drawable(),
            client().frame_xwin(),
            wm().dc.gc(),
            0, 0, bounds.width, bounds.height,
            0, 0);
}
