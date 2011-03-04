#ifndef _WM_EXTRA_PLACE_HPP
#define _WM_EXTRA_PLACE_HPP

#include <wm/wm.hpp>

void place_frame_in_smallest_column(WView *view, WFrame *frame);
WFrame *place_client_in_smallest_column(WView *view, WClient *client);

#endif /* _WM_EXTRA_PLACE_HPP */
