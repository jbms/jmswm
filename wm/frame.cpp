
#include <wm/wm.hpp>

#include <util/log.hpp>

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
  WFrameStyle &style = wm().frame_style;

  WRect r;

  int tl_off = top_left_offset(wm());
  int br_off = bottom_right_offset(wm());

  r.x = tl_off;
  r.width = bounds.width - r.x - br_off;
  r.y = tl_off + wm().bar_height() + style.spacing;
  r.height = bounds.height - r.y - br_off;
  
  return r;
}

int WM::shaded_height() const
{
  return bar_height() + top_left_offset(*this) + bottom_right_offset(*this);
}

/* TODO: maybe optimize this */
void WFrame::draw()
{
  WDrawable &d = wm().buffer_pixmap.drawable();
  WFrameStyle &style = wm().frame_style;

  WFrameStyleSpecialized &substyle
    = (this == column()->selected_frame() ?
       (column() == column()->view()->selected_column() ?
        style.active_selected : style.inactive_selected)
       : style.inactive);

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

  fill_rect(d, substyle.label_background_color, rect3);

  draw_label(d, client().name(), style.label_font, substyle.label_foreground_color,
             rect3.inside_lr_tb_border(style.label_horizontal_padding,
                                       style.label_vertical_padding));

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
