#ifndef _WM_EXTRA_FULLSCREEN_HPP
#define _WM_EXTRA_FULLSCREEN_HPP

class WM;
class WClient;

void toggle_fullscreen(WM &wm);
void check_fullscreen_on_unmanage_client(WClient *client);

#endif /* _WM_EXTRA_FULLSCREEN_HPP */
