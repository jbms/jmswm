#ifndef _WM_HPP
#define _WM_HPP

#include <sys/types.h>
#include <event.h>

#include <boost/intrusive/ilist.hpp>
#include <map>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xproto.h>
#include <X11/Xft/Xft.h>
#include <X11/Xcms.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <draw/draw.hpp>

#include <signal.h>
#include <sys/types.h>

#include <util/log.hpp>

#include <wm/define_style.hpp>

#include <wm/key.hpp>

#include <boost/function.hpp>


const long WM_UNMANAGED_CLIENT_EVENT_MASK = (StructureNotifyMask |
                                             PropertyChangeMask);

const long WM_EVENT_MASK_ROOT = (PropertyChangeMask |
                                 SubstructureRedirectMask |
                                 EnterWindowMask);

const long WM_EVENT_MASK_CLIENTWIN = (PropertyChangeMask |
                                      FocusChangeMask |
                                      StructureNotifyMask |
                                      EnterWindowMask);

const long WM_EVENT_MASK_FRAMEWIN = (SubstructureRedirectMask |
                                     SubstructureNotifyMask |
                                     ExposureMask |
                                     ButtonPressMask |
                                     //PointerMotionMask |
                                     ButtonReleaseMask |
                                     KeyPressMask |
                                     EnterWindowMask);

class WClient;
class WFrame;
class WView;
class WColumn;


WM_DEFINE_STYLE_TYPE(WFrameStyleSpecialized,
                     /* style type features */
                     ()
                     ((WColor, ascii_string, highlight_color))
                     ((WColor, ascii_string, shadow_color))
                     ((WColor, ascii_string, padding_color))
                     ((WColor, ascii_string, background_color))
                         
                     ((WColor, ascii_string, label_foreground_color))
                     ((WColor, ascii_string, label_background_color)),

                     /* regular features */
                     ()
                     )

WM_DEFINE_STYLE_TYPE(WFrameStyle,
                     
                     /* style type features */
                     ()
                     ((WFont, ascii_string, label_font))
                     ((WColor, ascii_string, client_background_color))

                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       active_selected))
                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive_selected))
                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive)),

                     /* regular features */
                     ()
                     ((int, highlight_pixels))
                     ((int, shadow_pixels))
                     ((int, padding_pixels))
                     ((int, spacing))
                     ((int, label_horizontal_spacing))
                     ((int, label_vertical_spacing))

                     )

class WM : public WXContext
{
public:

  WM(Display *dpy, event_base *eb, const WFrameStyle::Spec &style_spec);
  ~WM();

  /**
   * {{{ X context and Drawing context
   */
private:

  struct event_base *eb;
  struct event x_connection_event;

public:
  
  WDrawContext dc;

  /* This is used as a buffer for all drawing operations. */
  WPixmap buffer_pixmap;

  /**
   * }}}
   */
  
  /**
   * {{{ Deferred operation handling
   */
  
private:
  
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_auto_base_hook<0>::value_traits<WClient>,
    false /* constant-time size */
  > DirtyClientList;

  DirtyClientList dirty_clients;
  WClient *client_to_focus;
  
  friend class WClient;
  

public:
  
  void flush();
  
  void schedule_focus_client(WClient *client);

  /**
   * }}}
   */
  

  /**
   * {{{ Atoms
   */
#define DECLARE_ATOM(var, str) Atom var;
#include "atoms.hpp"
#undef DECLARE_ATOM

  /**
   * }}}
   */

  /**
   * {{{ Style
   */
public:  

  WFrameStyle frame_style;

  int bar_height() const;

  /**
   * }}}
   */

  /**
   * {{{ Frame layout
   */
public:
  
  typedef std::map<utf8_string, WView *> ViewMap;
  
private:
  WView *selected_view_;
  ViewMap views_;
  friend class WView;

public:

  WView *selected_view() { return selected_view_; }
  void select_view(WView *view);

  const ViewMap &views() { return views_; }

  /**
   * }}}
   */

  /**
   * {{{ Event handlers
   */
private:

  static void xwindow_handle_event(int, short, void *wm_ptr);

  void handle_map_request(const XMapRequestEvent &ev);
  void handle_configure_request(const XConfigureRequestEvent &ev);
  void handle_expose(const XExposeEvent &ev);
  void handle_destroy_window(const XDestroyWindowEvent &ev);
  void handle_property_notify(const XPropertyEvent &ev);
  void handle_unmap_notify(const XUnmapEvent &ev);
  void handle_enter_notify(const XCrossingEvent &ev);
  void handle_mapping_notify(const XMappingEvent &ev);
  void handle_keypress(const XKeyEvent &ev);

  /**
   * }}}
   */

  /**
   * {{{ Client management
   */

public:

  typedef std::map<Window, WClient *> ClientMap;
  typedef std::map<Window, WClient *> ClientFrameMap;

  ClientMap managed_clients;
  ClientFrameMap framewin_map;

  WClient *client_of_win(Window win)
  {
    ClientMap::iterator it = managed_clients.find(win);
    if (it != managed_clients.end())
      return it->second;
    return 0;
  }

  WClient *client_of_framewin(Window win)
  {
    ClientMap::iterator it = framewin_map.find(win);
    if (it != framewin_map.end())
      return it->second;
    return 0;
  }
  
  void manage_client(Window w, bool map_request);
  void unmanage_client(WClient *client);
  
  void place_client(WClient *client);

  /**
   * }}}
   */
  
  /**
   * {{{ X windows utility functions
   */
  void set_window_WM_STATE(Window w, int state);
  bool get_window_WM_STATE(Window w, int &state_ret);
  /**
   * }}}
   */

  /**
   * {{{ Key bindings
   */
  
private:
  
  WKeyBindingState *key_binding_state;
  void initialize_key_handler();
  void deinitialize_key_handler();
  void reset_current_key_sequence();
  static void key_map_reset_timeout_handler(int, short, void *wm_ptr);

  /* This automatically creates a grab that does not depend on the
     state of locking modifiers. */
  /* grab_window = None implies grab_window = xc.root_window() */
  void grab_key(int keycode, unsigned int modifiers,
                Window grab_window = None,
                bool owner_events = false,
                int pointer_mode = GrabModeAsync,
                int keyboard_mode = GrabModeAsync);

  /* grab_window = None implies grab_window = xc.root_window() */
  void ungrab_key(int keycode, unsigned int modifiers,
                  Window grab_window = None);
  
public:

  struct timeval key_sequence_timeout;
  
  bool bind_key(const WKeySequence &seq,
                const boost::function<void ()> &action);

  bool unbind_key(const WKeySequence &seq);

  /**
   * }}}
   */

};

class WClient
/* For the WM dirty clients list */
  : public boost::intrusive::ilist_auto_base_hook<0>
{
private:
  WM &wm_;
public:
  WM &wm() const { return wm_; }

private:

  /**
   * {{{ Constructor and destructor
   */
  
  /* Instances should not be constructed or destructed directly. */
  friend class WM;

  WClient(WM &wm, Window w);
  
  /**
   * }}}
   */

  /**
   * {{{ Frames
   */

public:
  typedef std::map<WView *, WFrame *> ViewFrameMap;
private:
  friend class WColumn;
  ViewFrameMap view_frames_;
public:
  
  const ViewFrameMap &view_frames() { return view_frames_; }
  WFrame *visible_frame();
  
  /**
   * }}}
   */

  /**
   * {{{ Deferred operation handling: Drawing, positioning, X state
   */
private:
  enum map_state_t { STATE_MAPPED,
                     STATE_UNMAPPED } client_map_state, frame_map_state;

  enum iconic_state_t { ICONIC_STATE_NORMAL, ICONIC_STATE_ICONIC,
                        ICONIC_STATE_UNKNOWN } current_iconic_state;

  WRect current_frame_bounds;
  WRect current_client_bounds;

  friend class WFrame;

  void set_WM_STATE(int state);
  void set_iconic_state(iconic_state_t iconic_state);
  
public:
  void schedule_positioning()
  {
    mark_dirty(CLIENT_POSITIONING_NEEDED);
  }

  void schedule_drawing()
  {
    mark_dirty(CLIENT_DRAWING_NEEDED);
  }
  
private:
  /* Reposition needed implies draw needed. */
  enum dirty_state_t { CLIENT_NOT_DIRTY,
         CLIENT_DRAWING_NEEDED,
         CLIENT_POSITIONING_NEEDED } dirty_state;

  void mark_dirty(dirty_state_t state)
  {
    assert(state != CLIENT_NOT_DIRTY);
    if (dirty_state == CLIENT_NOT_DIRTY)
      wm().dirty_clients.push_back(*this);

    if (state > dirty_state)
      dirty_state = state;
  }
private:
  void perform_deferred_work();

  /* Note: this function does not select any frames in any views; it
     should only be used to actually set the input focus to a client
     that is already the selected active frame in a view. */
  /* This should normally only be called when flushing deferred
     operations. */
  void focus();

  /**
   * }}}
   */

  /**
   * {{{ Miscellaneous
   */
private:
  Window xwin_;
  Window frame_xwin_;
public:
  Window xwin() { return xwin_; }
  Window frame_xwin() { return frame_xwin_; }
private:

  WRect initial_geometry;
  int initial_border_width;
  XSizeHints size_hints;

  /**
   * }}}
   */

  /**
   * {{{ Properties
   */
private:
  ascii_string class_name_;
  ascii_string instance_name_;
  ascii_string window_role_;

  utf8_string name_;

  void update_name_from_server();
  void update_class_from_server();
  void update_role_from_server();

public:
  const ascii_string &class_name() const { return class_name_; }
  const ascii_string &instance_name() const { return instance_name_; }
  const ascii_string &window_role() const { return window_role_; }

  const utf8_string &name() const { return name_; }

  /**
   * }}}
   */

  /**
   * {{{ Event handling
   */
  void notify_client_of_root_position(void);
  void handle_configure_request(const XConfigureRequestEvent &ev);
  /**
   * }}}
   */

};

class WFrame : public boost::intrusive::ilist_base_hook<0, false>
{
private:
  WClient &client_;
  WColumn *column_;
public:

  WClient &client() const { return client_; }
  WColumn *column() const { return column_; }
  WView *view() const;
  WM &wm() const { return client().wm(); }
  
  WFrame(WClient &client, WColumn *column);
  
  /* The height field is the only semi-persistent field. */
  WRect bounds;

private:
  
  bool rolled_;
  bool bar_visible_;

public:

  WRect client_bounds() const;
  
  void draw();
  
  void remove();

};

class WColumn : public boost::intrusive::ilist_base_hook<0, false>
{
public:

  WColumn(WView *view);

  /**
   * {{{ View
   */
private:
  WView *view_;
public:
  WView *view() const { return view_; };
  WM &wm() const;
  /**
   * }}}
   */

  /**
   * {{{ Frames
   */
public:
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<0, false>::value_traits<WFrame>,
    true /* constant-time size */
    > FrameList;
  FrameList frames;
  typedef FrameList::iterator iterator;
private:
  WFrame *selected_frame_;
public:

  iterator selected_position() { return make_iterator(selected_frame_); }
  WFrame *selected_frame() { return selected_frame_; }
  void select_frame(iterator it);
  void select_frame(WFrame *frame);
  iterator make_iterator(WFrame *frame)
  {
    if (!frame)
      return frames.end();
    return frames.current(*frame);
  }
  
  WFrame *get_frame(iterator it)
  {
    if (it != frames.end())
      return &*it;
    return 0;
  }

  iterator next_frame(iterator it, bool wrap);
  iterator prior_frame(iterator it, bool wrap);

  iterator add_client(WClient *client, iterator position);
  
  /**
   * }}}
   */

  /**
   * {{{ Bounds
   */
  
  /* Computes the positions of frames */
private:
  int available_frame_height() const;
public:
  WRect bounds;
  void update_positions();

  /**
   * }}}
   */
  
};

class WView
{
private:
  WM &wm_;
public:

  WM &wm() const { return wm_; }

  WView(WM &wm, const utf8_string &name);
  ~WView();
  
  
  /**
   * {{{ Name
   */
private:
  utf8_string name_;
public:
  const utf8_string &name() const { return name_; }
  /**
   * }}}
   */

  /**
   * {{{ Bounds
   */
public:
  WRect bounds;
  void compute_bounds();
  void update_positions();
  
private:
  int available_column_width() const;
  /**
   * }}}
   */

  /**
   * {{{ Columns
   */
public:
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<0, false>::value_traits<WColumn>,
    true /* constant-time size */
    > ColumnList;

  ColumnList columns;
  typedef ColumnList::iterator iterator;
  
private:  
  WColumn *selected_column_;
public:
  WColumn *selected_column() { return selected_column_; }
  iterator selected_position() { return make_iterator(selected_column_); }
  

  /* If fraction is 0, use an `equal' portion.  The new column is
     inserted before `position'. */
  iterator create_column(iterator position, float fraction = 0);

  iterator make_iterator(WColumn *column)
  {
    if (!column)
      return columns.end();
    return columns.current(*column);
  }
  
  WColumn *get_column(iterator it)
  {
    if (it == columns.end())
      return 0;
    return &*it;
  }

  void select_column(iterator it);
  void select_column(WColumn *column);
  
  void select_frame(WFrame *frame);

  iterator next_column(iterator pos, bool wrap);
  iterator prior_column(iterator pos, bool wrap);

  /**
   * }}}
   */
};

inline WM &WColumn::wm() const { return view()->wm(); }
inline WView *WFrame::view() const { return column()->view(); }

#endif /* _WM_HPP */
