#ifndef _WM_EXTRA_ERC_APPLET_HPP
#define _WM_EXTRA_ERC_APPLET_HPP

#include <wm/all.hpp>

class ErcApplet
{
  WM &wm;
  WBarCellStyle style;
  InotifyEvent inotify;
  int wd;
  WBar::CellRef placeholder;

  boost::signals2::connection name_conn, place_conn;

  // first element is server, second is buffer name
  typedef std::pair<ascii_string, ascii_string> BufferInfo;
  typedef std::vector<BufferInfo> BufferStatus;
  BufferStatus buffer_status;
  std::vector<WBar::CellRef> cells;

  void handle_inotify_event(int wd, uint32_t mask, uint32_t cookie,
                            const char *pathname);

  void update();

public:

  ErcApplet(WM &wm, const style::Spec &style_spec,
             const WBar::InsertPosition &pos);

  void switch_to_buffer();

};

#endif /* _WM_EXTRA_ERC_APPLET_HPP */
