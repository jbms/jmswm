
#include <boost/foreach.hpp>
#include <wm/wm.hpp>

#include <wm/xwindow.hpp>

#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>



static bool get_persistent_state_data(WM &wm, Window w, std::string &state)
{
  unsigned char *data;
  unsigned long len
    = xwindow_get_property(wm.display(), w,
                           wm.atom_persistence,
                           wm.atom_persistence,
                           10, /* "expected" number of 32-bit
                                  quantities; since more is set to
                                  true, this is arbitrary */
                           true, /* more */
                           &data);
  if (len == 0)
    return false;
  state.assign((char *)data, len);
  XFree(data);
  return true;
}

static void remove_persistent_state_data(WM &wm, Window w)
{
  XDeleteProperty(wm.display(), w, wm.atom_persistence);
}

static void set_persistent_state_data(WM &wm, Window w,
                                      const std::string &state)
{
  XChangeProperty(wm.display(), w, wm.atom_persistence,
                  wm.atom_persistence, 8, PropModeReplace,
                  (unsigned char *)(&state[0]), state.size());
}

static int column_index(WColumn *column)
{
  return std::distance(column->view()->columns.begin(),
                       column->view()->make_iterator(column));
}

static int frame_index(WFrame *frame)
{
  return std::distance(frame->column()->frames.begin(),
                       frame->column()->make_iterator(frame));
}

static WColumn *get_column(WView *view, int index)
{
  if (index < 0 || (unsigned int)index >= view->columns.size())
    return false;

  WView::iterator it = view->columns.begin();
  std::advance(it, index);

  return view->get_column(it);
}

static WColumn::iterator get_frame_insert_position(WColumn *column, int index)
{
  WColumn::iterator it = column->frames.begin();

  while (it != column->frames.end()
         && it->initial_position < index)
    ++it;

  return it;
}

struct PersistentColumnInfo
{
  
  bool selected;
  float priority;
  
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & selected;
    ar & priority;
  }
  PersistentColumnInfo() {}
  PersistentColumnInfo(WColumn *column)
    : selected(column == column->view()->selected_column()),
      priority(column->priority())
  {}
};

struct PersistentViewInfo
{
  utf8_string name;
  bool selected;
  bool bar_visible;

  std::vector<PersistentColumnInfo> columns;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & name;
    ar & selected;
    ar & bar_visible;
    ar & columns;
  }

  PersistentViewInfo() {}
  PersistentViewInfo(WView *view)
    : name(view->name()),
      selected(view == view->wm().selected_view()),
      bar_visible(view->bar_visible())
  {
    BOOST_FOREACH (WColumn &c, view->columns)
      columns.push_back(PersistentColumnInfo(&c));
  }
};

struct PersistentWMInfo
{
  std::vector<PersistentViewInfo> views;
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & views;
  }

  PersistentWMInfo() {}
  PersistentWMInfo(WM &wm)
  {
    for (WM::ViewMap::const_iterator it = wm.views().begin();
         it != wm.views().end();
         ++it)
    {
      views.push_back(PersistentViewInfo(it->second));
    }
  }
};

struct PersistentFrameInfo
{
  utf8_string view;
  bool selected;
  int column;
  int position;
  float priority;
  bool shaded;
  bool decorated;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & view;
    ar & selected;
    ar & column;
    ar & position;
    ar & priority;
    ar & shaded;
    ar & decorated;
  }

  PersistentFrameInfo() {}
  PersistentFrameInfo(WFrame *frame)
    : view(frame->view()->name()),
      selected(frame->column()->selected_frame() == frame),
      column(column_index(frame->column())),
      position(frame_index(frame)),
      priority(frame->priority()),
      shaded(frame->shaded()),
      decorated(frame->decorated())
  {}
};

struct PersistentClientInfo
{
  std::vector<PersistentFrameInfo> frames;

  template <class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & frames;
  }

  PersistentClientInfo() {}
  PersistentClientInfo(WClient *client)
  {
    for (WClient::ViewFrameMap::const_iterator it = client->view_frames().begin();
         it != client->view_frames().end();
         ++it)
      frames.push_back(PersistentFrameInfo(it->second));
  }
};

void WM::save_state_to_server()
{

  /**
   * Save view and column information to the root window.
   */
  {
    std::ostringstream ostr;
    try
    {
      {
        boost::archive::binary_oarchive ar(ostr, boost::archive::no_header);
        PersistentWMInfo info(*this);
        ar & info;
      }

      set_persistent_state_data(*this, root_window(), ostr.str());
    } catch (boost::archive::archive_exception &e)
    {
      ERROR("archive_exception occurred: %s", e.what());
    }
  }

  /**
   * Save client information to each client
   */
  for (ClientMap::const_iterator it = managed_clients.begin();
       it != managed_clients.end();
       ++it)
  {
    std::ostringstream ostr;
    try
    {
      {
        boost::archive::binary_oarchive ar(ostr, boost::archive::no_header);
        PersistentClientInfo info(it->second);
        ar & info;
      }
      set_persistent_state_data(*this, it->second->xwin(), ostr.str());
    } catch (boost::archive::archive_exception &e)
    {
      ERROR("archive_exception occurred: %s", e.what());
    }
  }
}

void WM::load_state_from_server()
{
  /**
   * Attempt to load view and column information
   */
  {
    std::string str;
    if (get_persistent_state_data(*this, root_window(), str))
    {
      try {
        PersistentWMInfo info;
        {
          std::istringstream istr(str);
          boost::archive::binary_iarchive ar(istr, boost::archive::no_header);
          ar & info;
        }

        BOOST_FOREACH (PersistentViewInfo &v, info.views)
        {
          if (view_by_name(v.name))
            continue;
          WView *view = new WView(*this, v.name);

          view->set_bar_visible(v.bar_visible);

          if (v.selected)
            select_view(view);

          BOOST_FOREACH (PersistentColumnInfo &c, v.columns)
          {
            /* TODO: maybe handle priority here */
            WView::iterator it = view->create_column(view->columns.end());
            it->set_priority(c.priority);
            if (c.selected)
              view->select_column(it);
          }
        }
      } catch (boost::archive::archive_exception &e)
      {
        WARN("archive_exception occurred: %s", e.what());
      }
    }
  }

  /**
   * Manage existing clients
   */
  {
    Window junk1, junk2;
    Window *top_level_windows = 0;
    unsigned int top_level_window_count = 0;
    XQueryTree(display(), root_window(), &junk1, &junk2,
               &top_level_windows, &top_level_window_count);
    for (unsigned int i = 0; i < top_level_window_count; ++i)
    {
      manage_client(top_level_windows[i], false);
    }
    XFree(top_level_windows);
  }

  for (ViewMap::iterator it = views_.begin(), next;
       it != views_.end();
       it = next)
  {
    next = boost::next(it);
    WView *view = it->second;
    
    for (WView::iterator it = view->columns.begin();
         it != view->columns.end();
         ++it)
    {
      if (it->frames.empty())
        delete &*it;
    }

    /*

    if (view->columns.empty() && selected_view() != view)
      delete view;

    */
  }
}

bool WM::place_existing_client(WClient *client)
{
  std::string str;
  if (!get_persistent_state_data(*this, client->xwin(), str))
    return false;
  
  remove_persistent_state_data(*this, client->xwin());

  PersistentClientInfo info;
  try
  {
    {
      std::istringstream istr(str);
      boost::archive::binary_iarchive ar(istr, boost::archive::no_header);
      ar & info;
    }
  } catch (boost::archive::archive_exception &e)
  {
    WARN("archive_exception occurred: %s", e.what());
    return false;
  }

  bool placed = false;

  BOOST_FOREACH (PersistentFrameInfo &f, info.frames)
  {
    WView *view = view_by_name(f.view);
    if (!view)
    {
      WARN("client with invalid saved state");
      break;
    }
    WColumn *column = get_column(view, f.column);
    if (!column)
    {
      WARN("client with invalid saved state");
      break;
    }
    WFrame *frame = new WFrame(*client);
    frame->set_priority(f.priority);
    frame->set_shaded(f.shaded);
    frame->set_decorated(f.decorated);
    
    WColumn::iterator pos = get_frame_insert_position(column, f.position);
    
    column->add_frame(frame, pos);
    if (f.selected)
      column->select_frame(frame);
    placed = true;
  }

  return placed;
}
