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
                                     PointerMotionMask |
                                     ButtonReleaseMask |
                                     KeyPressMask);

class WClient;
class WFrame;
class WView;
class WColumn;

#define WM_DEFINE_STYLE_TYPE_HELPER1(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER2(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(3, 1, elem) BOOST_PP_TUPLE_ELEM(3, 2, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER3(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(3, 0, elem) BOOST_PP_TUPLE_ELEM(3, 2, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER4(r, _, elem)                  \
  BOOST_PP_TUPLE_ELEM(3, 2, elem)                           \
    (dc_, spec_.BOOST_PP_TUPLE_ELEM(3, 2, elem))            \

#define WM_DEFINE_STYLE_TYPE_HELPER5(r, _, elem)      \
  BOOST_PP_TUPLE_ELEM(2, 1, elem)               \
    (spec_.BOOST_PP_TUPLE_ELEM(2, 1, elem))

#define WM_DEFINE_STYLE_TYPE_HELPER6(seq)                                     \
  BOOST_PP_LIST_REST(BOOST_PP_ARRAY_TO_LIST                                   \
                     BOOST_PP_SEQ_TO_ARRAY(seq))

#define WM_DEFINE_STYLE_TYPE(name, features1, features2) \
  class name                                             \
  {                                                      \
  public:                                                \
    class Spec                                                          \
    {                                                                   \
    public:                                                             \
      BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER2, _,           \
                             WM_DEFINE_STYLE_TYPE_HELPER6(features1));  \
      BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER1, _,           \
                             WM_DEFINE_STYLE_TYPE_HELPER6(features2));  \
    };                                                                  \
    BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER3, _,                       \
                           WM_DEFINE_STYLE_TYPE_HELPER6(features1));
    BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER1, _,                        \
                           BOOST_PP_LIST_REST(BOOST_PP_ARRAY_TO_LIST               \
                                              BOOST_PP_SEQ_TO_ARRAY(features2)));  \
    name(WDrawContext &dc_, const Spec &spec_)                              \
      : BOOST_LIST_ENUM                                                      \
      (BOOST_PP_LIST_APPEND                                                  \
       (BOOST_PP_LIST_TRANSFORM(WM_DEFINE_STYLE_TYPE_HELPER4, _,             \
                                BOOST_PP_LIST_REST(BOOST_PP_TUPLE_TO_LIST   \
                                                    BOOST_PP_SEQ_TO_ARRAY(features1))), \
        BOOST_PP_LIST_TRANSFORM(WM_DEFINE_STYLE_TYPE_HELPER5, _,                        \
                                BOOST_PP_LIST_REST(BOOST_PP_TUPLE_TO_LIST     \
                                                   BOOST_PP_SEQ_TO_ARRAY(features2))))) \
    {}
  };

WM_DEFINE_STYLE_TYPE(WFrameStyleSpecialized,
                     /* style type features */
                     ()
                     (WColor, ascii_string, highlight_color)
                     (WColor, ascii_string, shadow_color)
                     (WColor, ascii_string, padding_color)
                     (WColor, ascii_string, background_color)
                         
                     (WColor, ascii_string, label_foreground_color)
                     (WColor, ascii_string, label_background_color),

                     /* regular features */
                     ()
                     )

WM_DEFINE_STYLE_TYPE(WFrameStyle,
                     
                     /* style type features */
                     ()
                     (WFont, ascii_string, label_font)
                     (WColor, ascii_string, client_background_color)

                     (WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       active_selected)
                     (WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive_selected)
                     (WFrameStyleSpecialized, WFrameStyleSpecialized::Spec,
                       inactive),

                     /* regular features */x
                     
                     (int, highlight_pixels)
                     (int, shadow_pixels)
                     (int, padding_pixels)
                     (int, spacing)
                     (int, label_horizontal_spacing)
                     (int, label_vertical_spacing)

                     )



class WM
{
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_auto_base_hook<0>::value_traits<WClient>,
    false /* constant-time size */
  > DirtyClientList;

  DirtyClientList dirty_clients;

  friend class WClient;
  
public:
  
  WM(Display *dpy, event_base *eb, const WFrameStyle::Spec &style_spec);
  
  WXContext xc;
  WDrawContext dc;

  /* This is used as a buffer for all drawing operations. */
  WPixmap buffer_pixmap;

  /* Atoms */
#define DECLARE_ATOM(var, str) Atom var;
#include "atoms.hpp"
#undef DECLARE_ATOM

  WFrameStyle frame_style;

  struct event_base *eb;
  struct event x_connection_event;
  
  typedef std::map<utf8_string, WView *> ViewMap;

  ViewMap views;
  
  WView *selected_view;

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

  void flush();

  void handle_map_request(const XMapRequestEvent &ev);
  void handle_configure_request(const XConfigureRequestEvent &ev);
  void handle_expose(const XExposeEvent &ev);
  void handle_destroy_window(const XDestroyWindowEvent &ev);
  void handle_property_notify(const XPropertyEvent &ev);
  void handle_unmap_notify(const XUnmapEvent &ev);

  void manage_client(Window w, bool map_request);
  void unmanage_client(WClient *client);

  void place_client(WClient *client);
};

class WClient
/* For the WM dirty clients list */
  : public boost::intrusive::ilist_auto_base_hook<0>
{
public:
  enum map_state_t { STATE_MAPPED,
                     STATE_UNMAPPED } client_map_state, frame_map_state;

  enum iconic_state_t { ICONIC_STATE_NORMAL, ICONIC_STATE_ICONIC,
                        ICONIC_STATE_UNKNOWN } current_iconic_state;

private:
  WM &wm;

  friend class WFrame;

  void set_WM_STATE(int state);
  void set_iconic_state(iconic_state_t iconic_state);
  
public:

  WClient(WM &wm, Window w)
    : wm(wm), dirty_state(CLIENT_NOT_DIRTY), xwin(w)
  {}

  typedef std::map<WView *, WFrame *> ViewFrameMap;
  ViewFrameMap view_frames;

  WFrame *visible_frame()
  {
    ViewFrameMap::iterator it = view_frames.find(wm.selected_view);
    if (it != view_frames.end())
      return it->second;
    else
      return 0;
  }

  /* Reposition needed implies draw needed. */
  enum dirty_state_t { CLIENT_NOT_DIRTY,
         CLIENT_DRAWING_NEEDED,
         CLIENT_POSITIONING_NEEDED } dirty_state;

  void mark_dirty(dirty_state_t state)
  {
    if (dirty_state == CLIENT_NOT_DIRTY)
      wm.dirty_clients.push_back(*this);

    if (state > dirty_state)
      dirty_state = state;
  }

  void handle_pending_work();

  Window xwin;
  Window frame_xwin;

  ascii_string class_name;
  ascii_string instance_name;
  ascii_string window_role;

  utf8_string name;

  WRect initial_geometry;
  int initial_border_width;
  XSizeHints size_hints;

  WRect current_frame_bounds;
  WRect current_client_bounds;


  void update_name_from_server();
  void update_class_from_server();
  void update_role_from_server();

  void notify_client_of_root_position(void);
  
  void handle_configure_request(const XConfigureRequestEvent &ev);
};

class WFrame : public boost::intrusive::ilist_base_hook<0, false>
{
public:
  WFrame(WClient &client, WColumn *column)
    : client(client), column(column), rolled(false),
      bar_visible(false) {}
  
  WClient &client;
  WColumn *column;

  /* The height field is the only semi-persistent field. */
  WRect bounds;

  bool rolled;

  bool bar_visible;

  WRect client_bounds() const;
  void draw();
  void remove();
};

class WColumn : public boost::intrusive::ilist_base_hook<0, false>
{
  WM &wm;
public:
  WColumn(WM &wm, WView *view)
    : wm(wm), selected_frame(0), view(view) {}
  
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<0, false>::value_traits<WFrame>,
    true /* constant-time size */
    > FrameList;
  FrameList frames;

  WFrame *selected_frame;

  WRect bounds;
  WView *view;

  WFrame *add_client(WClient *client, FrameList::iterator position);

  /* Computes the positions of frames*/
  void update_positions();

  int available_frame_height() const;
};

class WView
{
private:
  WM &wm;
public:

  WView(WM &wm, const utf8_string &name);
  ~WView();
  
  typedef boost::intrusive::ilist<
    boost::intrusive::ilist_base_hook<0, false>::value_traits<WColumn>,
    true /* constant-time size */
    > ColumnList;

  ColumnList columns;
  
  WColumn *selected_column;

  utf8_string name;

  /* If fraction is 0, use an `equal' portion.  The new column is
     inserted before `position'. */
  WColumn *create_column(float fraction, ColumnList::iterator position);

  WRect bounds;

  void compute_bounds();

  void update_positions();

  int available_column_width() const;
};


#endif /* _WM_HPP */
