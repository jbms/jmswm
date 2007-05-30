#ifndef _STYLE_COMMON_HPP
#define _STYLE_COMMON_HPP

#include <style/style.hpp>
#include <draw/draw.hpp>
#include <util/string.hpp>

namespace style
{
  STYLE_DEFINITION(TextColor,
                   ((foreground, WColor, ascii_string),
                    (background, WColor, ascii_string)))

  STYLE_DEFINITION(Label,
                   ((horizontal_padding, int),
                    (vertical_padding, int),
                    (font, WFont, ascii_string)))
  
} // namespace style

#endif /* _STYLE_COMMON_HPP */
