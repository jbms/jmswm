#ifndef _WM_HPP
#define _WM_HPP

#include <sys/types.h>
#include <util/event.hpp>

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

#include <util/log.hpp>

#include <wm/define_style.hpp>

#include <wm/key.hpp>

#include <boost/function.hpp>

#include <menu/menu.hpp>
#include <wm/bar.hpp>

#include <boost/signal.hpp>

#include <util/weak_iptr.hpp>

#include <util/time.hpp>


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
                                     EnterWindowMask |
                                     FocusChangeMask);

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
                     ((WColor, ascii_string, label_background_color))
                     ((WColor, ascii_string, label_extra_color)),

                     /* regular features */
                     ()
                     )

WM_DEFINE_STYLE_TYPE(WFrameStyleScheme,
                     ()
                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       active_selected))
                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive_selected))
                     ((WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive)),

                     /* regular features */
                     ()
                     )
                     

WM_DEFINE_STYLE_TYPE(WFrameStyle,
                     
                     /* style type features */
                     ()
                     ((WFont, ascii_string, label_font))
                     ((WColor, ascii_string, client_background_color))

                     ((WFrameStyleScheme, WFrameStyleScheme::Spec, normal))
                     ((WFrameStyleScheme, WFrameStyleScheme::Spec, marked)),

                     /* regular features */
                     ()
                     ((int, highlight_pixels))
                     ((int, shadow_pixels))
                     ((int, padding_pixels))
                     ((int, spacing))
                     ((int, label_horizontal_padding))
                     ((int, label_vertical_padding))
                     ((int, label_component_spacing))

                     )

class WM : public WXContext
{
public:

  WM(int argc, char **argv,
     Display *dpy, EventService &event_service_,
     const WFrameStyle::Spec &style_spec,
     const WBarStyle::Spec &bar_style_spec);
  ~WM();

  /**
   * {{{ X context and Drawing context
   */
private:

  EventService &event_service_;
  FileEvent x_connection_event;
  SignalEvent sigint_event;

  bool hasXrandr;
  int xrandr_event_base, xrandr_error_base;
  Time last_timestamp;

  char **argv;
  int argc;

public:

  EventService &event_service() { return event_service_; }

  Time get_timestamp();
  
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
  > ScheduledTaskClientList;

  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_auto_base_hook<1>::value_traits<WColumn>,
    false /* constant-time size */
  > ScheduledTaskColumnList;
  
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_auto_base_hook<1>::value_traits<WView>,
    false /* constant-time size */
  > ScheduledTaskViewList;

  ScheduledTaskClientList scheduled_task_clients;
  ScheduledTaskColumnList scheduled_task_columns;
  ScheduledTaskViewList scheduled_task_views;
  
  friend class WClient;
  friend class WFrame;
  friend class WColumn;
  friend class WView;

public:
  
  void flush();
  
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

  int shaded_height() const;

  int frame_decoration_height() const;
  
  /**
   * }}}
   */

  /**
   * {{{ Frame layout
   */
public:
  
  typedef std::map<utf8_string, WView *> ViewMap;

  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<3, false>::value_traits<WFrame>,
    true /* constant-time size */
  > FrameListByActivity;
  
private:
  WView *selected_view_;
  ViewMap views_;

  TimerEvent frame_activity_event;
  
  FrameListByActivity frames_by_activity_;

  void handle_frame_activity();

  // Called when this->selected_frame() changes.  The selected frame
  // will already be set to new_frame when this function is called.
  void handle_selected_frame_changed(WFrame *old_frame, WFrame *new_frame);

public:

  const FrameListByActivity &frames_by_activity() const { return frames_by_activity_; }
  FrameListByActivity &frames_by_activity() { return frames_by_activity_; }

  /**
   * Returns true if the specified name is a valid name for a view.
   * Note that the result of this function does not depend on whether
   * such a view actually exists.
   */
  static bool valid_view_name(const utf8_string &name);

  WView *selected_view() { return selected_view_; }
  void select_view(WView *view);

  // Note: the second argument is the previously selected view.
  boost::signal<void (WView *, WView *)> select_view_hook;
  boost::signal<void (WView *)> create_view_hook;
  boost::signal<void (WView *)> destroy_view_hook;

  const ViewMap &views() { return views_; }

  WView *view_by_name(const utf8_string &name) const
  {
    ViewMap::const_iterator it = views_.find(name);
    if (it != views_.end())
      return it->second;
    return 0;
  }

  WFrame *selected_frame();
  WColumn *selected_column();

  /**
   * }}}
   */

  /**
   * {{{ Event handlers
   */
private:

  void xwindow_handle_event();

  void handle_map_request(const XMapRequestEvent &ev);
  void handle_configure_request(const XConfigureRequestEvent &ev);
  void handle_expose(const XExposeEvent &ev);
  void handle_destroy_window(const XDestroyWindowEvent &ev);
  void handle_property_notify(const XPropertyEvent &ev);
  void handle_unmap_notify(const XUnmapEvent &ev);
  void handle_enter_notify(const XCrossingEvent &ev);
  void handle_mapping_notify(const XMappingEvent &ev);
  void handle_keypress(const XKeyEvent &ev);
  void handle_xrandr_event(const XEvent &ev);
  void handle_focus_in(const XFocusChangeEvent &ev);

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

  boost::signal<void (WClient *)> manage_client_hook;
  boost::signal<void (WClient *)> unmanage_client_hook;
  boost::signal<void (WClient *)> update_client_name_hook;

  /**
   * }}}
   */

  /**
   * {{{ Persistence
   */
public:
  void save_state_to_server();
  void load_state_from_server();
  bool place_existing_client(WClient *client);

private:

  TimerEvent save_state_event;

  void handle_save_state_event();
  
  /**
   * }}}
   */
  
  /**
   * {{{ X windows utility functions
   */
public:  
  void set_window_WM_STATE(Window w, int state);
  bool get_window_WM_STATE(Window w, int &state_ret);

  void send_client_message(Window w, Atom a, Time timestamp);
  void send_client_message(Window w, Atom a)
  {
    send_client_message(w, a, get_timestamp());
  }
  
  /**
   * }}}
   */

  /**
   * {{{ Key bindings
   */
  
private:

  WModifierInfo mod_info;
  WKeyBindingContext global_bindctx;

  friend class WKeyBindingContext;

  /* This automatically creates a grab that does not depend on the
     state of locking modifiers. */
  /* grab_window = None implies grab_window = xc.root_window() */
  void grab_key(int keycode, unsigned int modifiers,
                Window grab_window,
                bool owner_events = false,
                int pointer_mode = GrabModeAsync,
                int keyboard_mode = GrabModeAsync);

  /* grab_window = None implies grab_window = xc.root_window() */
  void ungrab_key(int keycode, unsigned int modifiers,
                  Window grab_window);
  
public:

  struct timeval key_sequence_timeout;

  bool bind(const WKeySequence &seq,
            const KeyAction &action)
  {
    return global_bindctx.bind(seq, action);
  }

  bool unbind(const WKeySequence &seq)
  {
    return global_bindctx.unbind(seq);
  }
  
  /**
   * }}}
   */

  /**
   * {{{ Menu
   */
public:
  WMenu menu;

  /**
   * }}}
   */

  /**
   * {{{ Bar
   */
public:
  WBar bar;

  bool bar_visible();

  /**
   * }}}
   */
  

  /**
   * Actions
   */
public:
  void quit();
  void restart();
  /**
   *
   */
};

class WClient
/* For the WM scheduled work clients list */
  : public boost::intrusive::ilist_auto_base_hook<0>,

    public weak_iptr<WClient>::base
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
  
  const ViewFrameMap &view_frames() const { return view_frames_; }
  WFrame *visible_frame();
  WFrame *frame_by_view(WView *v)
  {
    ViewFrameMap::iterator it = view_frames_.find(v);
    if (it == view_frames_.end())
      return 0;
    return it->second;
  }
  
  /**
   * }}}
   */

  /**
   * {{{ Deferred operation handling: Drawing, positioning, X state
   */
private:
  typedef enum { STATE_MAPPED, STATE_UNMAPPED } map_state_t;

  map_state_t client_map_state, frame_map_state;

public:
  typedef enum { ICONIC_STATE_NORMAL, ICONIC_STATE_ICONIC,
                 ICONIC_STATE_UNKNOWN } iconic_state_t;
private:
  iconic_state_t current_iconic_state;
public:
  iconic_state_t iconic_state() const { return current_iconic_state; }
  
private:

  WRect current_frame_bounds;
  WRect current_client_bounds;

  friend class WFrame;

  void set_WM_STATE(int state);
  void set_iconic_state(iconic_state_t iconic_state);
  void set_frame_map_state(map_state_t state);
  void set_client_map_state(map_state_t state);

  WRect compute_actual_client_bounds(const WRect &available);

private:

  const static unsigned int DRAW_FLAG =            0x1;
  const static unsigned int UPDATE_SERVER_FLAG =   0x2;
  const static unsigned int SET_INPUT_FOCUS_FLAG = 0x4;
  const static unsigned int WARP_POINTER_FLAG =    0x8;

  unsigned int scheduled_tasks;

  void schedule_task(unsigned int task);
  void perform_scheduled_tasks();
  
public:

  void schedule_update_server() { schedule_task(UPDATE_SERVER_FLAG); }
  void schedule_draw() { schedule_task(DRAW_FLAG); }
  void schedule_set_input_focus() { schedule_task(SET_INPUT_FOCUS_FLAG); }
  void schedule_warp_pointer() { schedule_task(WARP_POINTER_FLAG); }
  
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
  XSizeHints size_hints_;

  const static unsigned int WM_TAKE_FOCUS_FLAG =    0x1;
  const static unsigned int WM_DELETE_WINDOW_FLAG = 0x2;

  const static unsigned int PROTOCOL_FLAGS = (WM_TAKE_FOCUS_FLAG |
                                              WM_DELETE_WINDOW_FLAG);

  unsigned int flags_;

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

  utf8_string visible_name_;

  int fixed_height_;
  unsigned int window_type_flags_;
public:
  const static unsigned int WINDOW_TYPE_DESKTOP       =    0x1;
  const static unsigned int WINDOW_TYPE_DOCK          =    0x2;
  const static unsigned int WINDOW_TYPE_TOOLBAR       =    0x4;
  const static unsigned int WINDOW_TYPE_MENU          =    0x8;
  const static unsigned int WINDOW_TYPE_UTILITY       =   0x10;
  const static unsigned int WINDOW_TYPE_SPLASH        =   0x20;
  const static unsigned int WINDOW_TYPE_DIALOG        =   0x40;
  const static unsigned int WINDOW_TYPE_DROPDOWN_MENU =   0x80;
  const static unsigned int WINDOW_TYPE_POPUP_MENU    =  0x100;
  const static unsigned int WINDOW_TYPE_TOOLTIP       =  0x200;
  const static unsigned int WINDOW_TYPE_NOTIFICATION  =  0x400;
  const static unsigned int WINDOW_TYPE_COMBO         =  0x800;
  const static unsigned int WINDOW_TYPE_DND           = 0x1000;
  const static unsigned int WINDOW_TYPE_NORMAL        = 0x2000;

  unsigned int window_type_flags() const { return window_type_flags_; }

private:

  /**
   * For terminals and file editors, this should be the current
   * directory.  For web browsers, it might be set to the current URL.
   * Otherwise, it can be empty.
   */
  utf8_string context_info_;

  void update_name_from_server();
  void update_class_from_server();
  void update_role_from_server();

  void update_protocols_from_server();
  void update_size_hints_from_server();
  void update_window_type_from_server();

  void update_fixed_height();

public:

  const XSizeHints &size_hints() const { return size_hints_; }
  
  const ascii_string &class_name() const { return class_name_; }
  const ascii_string &instance_name() const { return instance_name_; }
  const ascii_string &window_role() const { return window_role_; }

  const utf8_string &name() const { return name_; }

  const utf8_string &visible_name() const { return visible_name_; }
  const utf8_string &context_info() const { return context_info_; }

  void set_visible_name(const utf8_string &s);
  void set_context_info(const utf8_string &s);

  /* Returns 0 if the client is not fixed-height. */
  int fixed_height() const { return fixed_height_; }

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

  /**
   * {{{ Actions
   */

  /* Note: none of these actions immediately unmanage the client;
     instead, the client will be unmanaged when the client window is
     destroyed or unmapped as usual. */
  void kill();
  void destroy();
  void request_close();
  
  /**
   * }}}
   */

};

/**
 * A frame can be removed from a column.
 */
class WFrame
  :
  /* For WColumn::frames */
  public boost::intrusive::ilist_base_hook<0, false>,

  /* For WColumn::frames_by_activity */
  public boost::intrusive::ilist_base_hook<1, false>,

  /* For WView::frames_by_activity */
  public boost::intrusive::ilist_base_hook<2, false>,

  /* For WM::frames_by_activity */
  public boost::intrusive::ilist_base_hook<3, false>,

  public weak_iptr<WFrame>::base
{
private:
  WClient &client_;
  WColumn *column_;

public:

  WClient &client() const { return client_; }
  WColumn *column() const { return column_; }
  WView *view() const;
  WM &wm() const { return client().wm(); }
  
  WFrame(WClient &client);



  /* frame is removed from column, if it is in one */
  ~WFrame();
  
  WRect bounds;

  friend class WColumn;
  friend class WM;

private:
  
  bool shaded_;
  bool decorated_;
  bool marked_;
  float priority_;
  
  /* Note: this is only valid if this frame is currently focused in
     its column. */
  time_point last_focused;

public:
  /* Used only for persistence: -1 implies no initial position */
  /* FIXME: find a better solution */
  int initial_position;

public:

  float priority() const { return priority_; }

  void set_priority(float value);

  bool shaded() const { return shaded_; }

  void set_shaded(bool value);

  bool decorated() const { return decorated_; }

  void set_decorated(bool value);

  bool marked() const { return marked_; }
  void set_marked(bool value);

  WRect client_bounds() const;

  void draw();

  void remove();
  
};

/**
 * For its entire lifetime, a column is part of a single view.  Note:
 * this is different from WFrame.
 */
class WColumn
  :
  /* For WView::columns */
  public boost::intrusive::ilist_base_hook<0, false>,
  /* For WM::scheduled_task_columns */
  public boost::intrusive::ilist_auto_base_hook<1>,

  public weak_iptr<WColumn>::base
{
  friend class WFrame;
  friend class WView;
public:

  WColumn(WView *view);

  /* Column must be empty; it is removed from its view */
  ~WColumn();

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

  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<1, false>::value_traits<WFrame>,
    true /* constant-time size */
  > FrameListByActivity;
  
  typedef FrameList::iterator iterator;
private:
  WFrame *selected_frame_;

  FrameListByActivity frames_by_activity_;
  
public:

  const FrameListByActivity &frames_by_activity() const { return frames_by_activity_; }
  FrameListByActivity &frames_by_activity() { return frames_by_activity_; }

  iterator selected_position() { return make_iterator(selected_frame_); }
  WFrame *selected_frame() { return selected_frame_; }
  void select_frame(iterator it, bool warp = false);
  void select_frame(WFrame *frame, bool warp = false);
  iterator default_insert_position();
  
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

  iterator add_frame(WFrame *frame, iterator position);

  iterator add_frame(WFrame *frame)
  {
    return add_frame(frame, default_insert_position());
  }
  
  /**
   * }}}
   */

  /**
   * {{{ Bounds
   */
  
  /* Computes the positions of frames */
private:
  int available_frame_height() const;
  bool scheduled_update_positions;
  void perform_scheduled_tasks();
  float priority_;
public:
  WRect bounds;
  void schedule_update_positions();

  friend class WM;

  const static float initial_priority = 1.0f;
  const static float minimum_priority = 0.1f;
  const static float maximum_priority = 10.0f;

  float priority() const { return priority_; }

  void set_priority(float value);

  /**
   * }}}
   */
  
};

class WView
  :
  /* For WM::deferred_task_views */
  public boost::intrusive::ilist_auto_base_hook<1>,

  public weak_iptr<WView>::base
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
  void schedule_update_positions();

  friend class WM;
  friend class WColumn;
  friend class WFrame;
  
private:
  bool scheduled_update_positions;
  void perform_scheduled_tasks();
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

  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<2, false>::value_traits<WFrame>,
    true /* constant-time size */
  > FrameListByActivity;

  ColumnList columns;
  typedef ColumnList::iterator iterator;
  
private:  
  WColumn *selected_column_;

  FrameListByActivity frames_by_activity_;
  
public:

  const FrameListByActivity &frames_by_activity() const { return frames_by_activity_; }
  FrameListByActivity &frames_by_activity() { return frames_by_activity_; }

  
  WColumn *selected_column() { return selected_column_; }
  iterator selected_position() { return make_iterator(selected_column_); }
  iterator default_insert_position();
  

  /* The new column is inserted before `position'. */
  iterator create_column(iterator position);

  iterator create_column()
  {
    return create_column(default_insert_position());
  }

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

  void select_column(iterator it, bool warp = false);
  void select_column(WColumn *column, bool warp = false);
  
  void select_frame(WFrame *frame, bool warp = false);

  iterator next_column(iterator pos, bool wrap);
  iterator prior_column(iterator pos, bool wrap);

  WFrame *selected_frame()
  {
    if (WColumn *column = selected_column())
      return column->selected_frame();
    return 0;
  }

  void place_frame_in_smallest_column(WFrame *frame);

  /**
   * }}}
   */

  /**
   * {{{ Bar
   */
private:
  bool bar_visible_;
public:
  bool bar_visible() const { return bar_visible_; }
  void set_bar_visible(bool value);
  /**
   *
   */

};

inline WM &WColumn::wm() const { return view()->wm(); }
inline WView *WFrame::view() const { return column()->view(); }

#endif /* _WM_HPP */
