#include <wm/wm.hpp>

static void correct_aspect(int max_w, int max_h, int ax, int ay,
                           int &w, int &h)
{
  if(ax > ay)
  {
    h = (max_w*ay)/ax;
    if (h > max_h)
    {
      h = max_h;
      w= (h*ax) / ay;
    } else
      w = max_w;
  } else
  {
    w = (max_h*ax) / ay;
    if (w > max_w)
    {
      w = max_w;
      h = (w*ay)/ax;
    } else
      h = max_h;
  }
}


WRect WClient::compute_actual_client_bounds(const WRect &available)
{
  WRect bounds = available;

  const XSizeHints &s = size_hints();

  int w = available.width, h = available.height;

  if (s.flags & PAspect)
  {
    /* Handle the aspect hint.  Ensure that:
     s.min_aspect.x/s.min_aspect.y > w/h > s.max_aspect.x/s.max_aspect.y
    */
    if (available.width * s.max_aspect.y > available.height * s.max_aspect.x)
      correct_aspect(available.width, available.height,
                     s.max_aspect.x, s.max_aspect.y,
                     w, h);
    if (available.width * s.min_aspect.y < available.height * s.min_aspect.x)
      correct_aspect(available.width, available.height,
                     s.min_aspect.x, s.min_aspect.y,
                     w, h);
  } else if (s.flags & PResizeInc)
  {
    int base_w = 0, base_h = 0;
    int extra_w, extra_h;

    if (s.flags & PBaseSize) {
      base_w = s.base_width;
      base_h = s.base_height;
    } else if (s.flags & PMinSize) {
      /* base_{width,height} default to min_{width,height} */
      base_w = s.min_width;
      base_h = s.min_height;
    }
    /* client_width = base_width + i * s->width_inc for an integer i */
    
    if (base_w >= 0 && s.width_inc > 0)
    {
      extra_w = available.width - base_w;
      if (extra_w > 0)
        w = available.width - extra_w % s.width_inc;
    }
    
    if(base_h >= 0 && s.height_inc > 0)
    {
      extra_h = available.height - base_h;
      if (extra_h > 0)
        h = available.height - extra_h % s.height_inc;
    }
  }

  return WRect(available.x + (available.width - w) / 2,
               available.y + (available.height - h) / 2,
               w, h);
}
