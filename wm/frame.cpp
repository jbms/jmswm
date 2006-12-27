
#include <wm/wm.hpp>

#include <util/log.hpp>

int WM::bar_height() const
{
  return frame_style.label_font.height()
    + frame_style.label_vertical_spacing * 2;
}

WRect WFrame::client_bounds() const
{
  WRect r(client.wm.frame_style.bar_border_width
          + client.wm.frame_style.client_border_width,
          client.wm.frame_style.bar_height()
          + client.wm.frame_style.client_border_width,
          bounds.width - 2 * client.wm.frame_style.bar_border_width
          - 2 * client.wm.frame_style.client_border_width,
          bounds.height - client.wm.frame_style.bar_height()
            - 2 * client.wm.frame_style.client_border_width
          - client.wm.frame_style.bar_border_width);
  return r;
}

void WFrame::draw()
{
  WM &wm = client.wm;
  FrameStyle &style = wm.frame_style;

  FrameColors &colors = (this == column->selected_frame ?
                         (column == column->view->selected_column ?
                          style.active_colors : style.inactive_selected_colors)
                         : style.inactive_colors);

  WRect tile(0, 0, bounds.width, style.bar_height());

  draw_tile(wm.buffer_pixmap.drawable(), *colors.frame_border_color,
            *colors.bar_background_color, style.bar_border_width, tile);

  tile.width -= 2 * style.bar_border_width;
  tile.x = style.bar_border_width;

  draw_label(wm.buffer_pixmap.drawable(), client.name,
             *style.bar_font, *colors.bar_foreground_color,
             tile);

  tile = WRect(0, style.bar_height() - 1, bounds.width,
               bounds.height - style.bar_height() + 1);

  draw_rect_border(wm.buffer_pixmap.drawable(), *colors.frame_border_color,
                   style.bar_border_width, tile);

  draw_tile(wm.buffer_pixmap.drawable(), *colors.client_border_color,
            *style.frame_background_color, style.client_border_width,
            tile.inside_border(style.bar_border_width));

  XCopyArea(wm.xc.display(), wm.buffer_pixmap.drawable().drawable(),
            client.frame_xwin,
            wm.dc.gc(),
            0, 0, bounds.width, bounds.height,
            0, 0);
}
