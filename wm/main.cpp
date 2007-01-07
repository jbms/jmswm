
#include <wm/wm.hpp>
#include <draw/draw.hpp>
#include <util/log.hpp>
#include <util/close_on_exec.hpp>

#include <event.h>

#include <getopt.h>
#include <stdlib.h>

#include <unistd.h>
#include <signal.h>

#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>

#include <time.h>

/* NOTE: this should perhaps be moved */
#include <wm/volume.hpp>

#include <boost/algorithm/string.hpp>

void select_left(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    WView::iterator it = view->prior_column(view->selected_position(), true);
    view->select_column(it, true);
  }
}

void select_right(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    WView::iterator it = view->next_column(view->selected_position(), true);
    view->select_column(it, true);
  }
}

void select_up(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    if (WColumn *column = view->selected_column())
    {
      WColumn::iterator it = column->prior_frame(column->selected_position(), true);
      column->select_frame(it, true);
    }
  }
}

void select_down(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    if (WColumn *column = view->selected_column())
    {
      WColumn::iterator it = column->next_frame(column->selected_position(), true);
      column->select_frame(it, true);
    }
  }
}


void move_left(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WView::iterator cur = frame->view()->make_iterator(frame->column());
    WView::iterator it = frame->view()->prior_column(cur, false);
    if (it == cur)
      it = frame->view()->create_column(frame->view()->columns.begin());
    frame->remove();
    it->add_frame(frame);
    frame->view()->select_frame(frame, true);
  }
}

void move_right(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WView::iterator cur = frame->view()->make_iterator(frame->column());
    WView::iterator it = frame->view()->next_column(cur, false);
    if (it == cur)
      it = frame->view()->create_column(frame->view()->columns.end());
    frame->remove();
    it->add_frame(frame);
    frame->view()->select_frame(frame, true);
  }
}

void move_up(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WColumn::iterator cur = frame->column()->make_iterator(frame);
    WColumn::iterator it;
    if (cur == frame->column()->frames.begin())
      it = frame->column()->frames.end();
    else
      it = frame->column()->prior_frame(cur, false);
    if (it != cur)
    {
      frame->column()->frames.splice(it, frame->column()->frames, cur);
      frame->column()->schedule_update_positions();
    }
  }
}

void move_down(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WColumn::iterator cur = frame->column()->make_iterator(frame);
    WColumn::iterator it;

    if (boost::next(cur) == frame->column()->frames.end())
      it = frame->column()->frames.begin();
    else
      it = boost::next(boost::next(cur));
    
    if (it != cur)
    {
      frame->column()->frames.splice(it, frame->column()->frames, cur);
      frame->column()->schedule_update_positions();
    }
  }
}

void toggle_shaded(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    frame->set_shaded(!frame->shaded());
  }
}

void toggle_decorated(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    frame->set_decorated(!frame->decorated());
  }
}

void toggle_bar_visible(WM &wm)
{
  if (WView *view = wm.selected_view())
  {
    view->set_bar_visible(!view->bar_visible());
  }
}

void increase_priority(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    float priority = frame->priority();

    frame->set_priority(priority * 1.2);
  }
}

void decrease_priority(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    float priority = frame->priority();

    frame->set_priority(priority / 1.2);
  }
}

void increase_column_priority(WM &wm)
{
  if (WColumn *column = wm.selected_column())
  {
    float priority = column->priority();

    column->set_priority(priority * 1.2);
  }
}

void decrease_column_priority(WM &wm)
{
  if (WColumn *column = wm.selected_column())
  {
    float priority = column->priority();

    column->set_priority(priority / 1.2);
  }
}

void execute_command(const ascii_string &command)
{
  if (fork() == 0)
  {
    setsid();
    close(STDIN_FILENO);
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), 0);
    exit(-1);
  }
}

utf8_string get_selected_cwd(WM &wm)
{
  utf8_string cwd;
  if (WFrame *frame = wm.selected_frame())
    cwd = frame->client().context_info();
  return cwd;
}

utf8_string resolve_cwd(const utf8_string &path)
{
  utf8_string cwd(path);
  if (!cwd.empty() && cwd[0] == '~')
  {
    cwd = "/home/jbms" + cwd.substr(1);
  }
  return cwd;
}

utf8_string make_path_pretty(const utf8_string &path)
{
  utf8_string context = path;
  if (context.find("/home/jbms") == 0)
    context = "~" + context.substr(10);
  return context;
}

void execute_command_cwd(const ascii_string &command,
                         const utf8_string &cwd)
{
  if (fork() == 0)
  {
    if (!cwd.empty())
      chdir(resolve_cwd(cwd).c_str());
    setsid();
    close(STDIN_FILENO);
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), 0);
    exit(-1);
  }
}

void execute_command_selected_cwd(WM &wm, const ascii_string &command)
{
  return execute_command_cwd(command, get_selected_cwd(wm));
}


void execute_action(WM &wm, const ascii_string &command)
{
  if (command == "quit")
  {
    wm.quit();
  } else if (command == "restart")
  {
    wm.restart();
  }
}

void get_action_completions(WMenu::CompletionList &list,
                            const WMenu::InputState &s)
{
  WMenu::InputState state;

  state.text = "quit";
  state.cursor_position = state.text.size();
  list.push_back(std::make_pair(state.text, state));

  state.text = "restart";
  state.cursor_position = state.text.size();
  list.push_back(std::make_pair(state.text, state));
}

void execute_action_interactive(WM &wm)
{
  wm.menu.read_string("Action: ",
                      boost::bind(&execute_action, boost::ref(wm), _1),
                      boost::function<void (void)>(),
                      &get_action_completions);
}


void close_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
    frame->client().request_close();
}

void kill_current_client(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
    frame->client().kill();
}

void execute_command_interactive(WM &wm)
{
  wm.menu.read_string("Command: ", &execute_command,
                      boost::function<void (void)>());
}

void execute_command_cwd_interactive(WM &wm)
{
  utf8_string cwd = get_selected_cwd(wm);
  if (cwd.empty())
  {
    char buf[256];
    buf[255] = 0;
    getcwd(buf, 256);
    cwd = make_path_pretty(buf);
  }
  
  wm.menu.read_string(cwd + " $ ",
                      boost::bind(&execute_command_cwd, _1, cwd));
}


void switch_to_view(WM &wm, const utf8_string &name)
{
  WView *view = wm.view_by_name(name);
  if (!view)
  {
    if (!wm.valid_view_name(name))
      return;
    view = new WView(wm, name);
  }
  wm.select_view(view);
}

void switch_to_view_interactive(WM &wm)
{
  wm.menu.read_string("Switch to tag: ",
                      boost::bind(&switch_to_view, boost::ref(wm), _1));
}

void switch_to_view_by_letter(WM &wm, char c)
{
  for (WM::ViewMap::const_iterator it = wm.views().begin();
       it != wm.views().end();
       ++it)
  {
    if (it->first[0] == c)
    {
      wm.select_view(it->second);
      break;
    }
  }
}

class PreviousViewInfo
{
  utf8_string previous_view_;
  WM &wm_;
  boost::signals::connection conn_;

  void select_view(WView *new_view, WView *old_view)
  {
    if (old_view)
      previous_view_ = old_view->name();
  }
  
public:
  PreviousViewInfo(WM &wm)
    : wm_(wm),
      conn_(wm_.select_view_hook.connect
            (boost::bind(&PreviousViewInfo::select_view, this, _1, _2)))
  {}

  ~PreviousViewInfo()
  {
    conn_.disconnect();
  }
  
  const utf8_string &name() const { return previous_view_; }
  void switch_to()
  {
    switch_to_view(wm_, previous_view_);
  }
};

static utf8_string fullscreen_client_suffix(WClient *client)
{
  return client->instance_name();
}

static WFrame *is_fullscreen_frame(WFrame *frame)
{
  WClient *client = &frame->client();
  const utf8_string &view_name = frame->view()->name();

  utf8_string::size_type sep_pos = view_name.rfind('/');
  if (sep_pos != utf8_string::npos)
  {
    utf8_string original_name = view_name.substr(0, sep_pos);

    if (WView *orig_view = frame->wm().view_by_name(original_name))
    {
      if (WFrame *orig_frame = client->frame_by_view(orig_view))
        return orig_frame;
    }
  }
  return 0;
}

void toggle_fullscreen(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    WClient *client = &frame->client();

    if (WFrame *orig_frame = is_fullscreen_frame(frame))
    {
      delete frame;
      orig_frame->view()->select_frame(orig_frame);
      wm.select_view(orig_frame->view());
      return;
    }

    // Frame is assumed to not be a fullscreen frame.

    // Check if frame already has a fullscreen frame
    for (WClient::ViewFrameMap::const_iterator it = client->view_frames().begin();
         it != client->view_frames().end();
         ++it)
    {
      if (is_fullscreen_frame(it->second))
      {
        it->first->select_frame(it->second);
        wm.select_view(it->first);
        return;
      }
    }

    // Otherwise, a fullscreen frame must be created
    utf8_string new_name = frame->view()->name();
    new_name += '/';
    new_name += fullscreen_client_suffix(&frame->client());
    utf8_string extra_suffix;
    int count = 1;
    while (wm.view_by_name(new_name + extra_suffix))
    {
      char buffer[32];
      sprintf(buffer, "<%d>", count);
      extra_suffix = buffer;
      ++count;
    }
    WView *view = new WView(wm, new_name + extra_suffix);
    view->set_bar_visible(false);
    WFrame *new_frame = new WFrame(*client);
    new_frame->set_decorated(false);
    view->create_column()->add_frame(new_frame);
    wm.select_view(view);
  }
}

void check_fullscreen_on_unmanage_client(WClient *client)
{
  if (WFrame *frame = client->visible_frame())
  {
    if (frame->column()->frames.size() == 1
        && frame->view()->columns.size() == 1)
    {
    
      if (WFrame *orig_frame = is_fullscreen_frame(frame))
      {
        client->wm().select_view(orig_frame->view());
      }
    }
  }
}

void update_client_visible_name_and_context(WClient *client)
{
  /* Handle xterm */
  if (client->class_name() == "XTerm")
  {
    using boost::algorithm::split_iterator;
    using boost::algorithm::make_split_iterator;
    using boost::algorithm::token_finder;
    typedef split_iterator<utf8_string::const_iterator> string_split_iterator;
    std::vector<utf8_string> parts;
    utf8_string sep = "<#!#!#>";
    for (utf8_string::size_type x = 0, next; x != utf8_string::npos; x = next)
    {
      utf8_string::size_type end = client->name().find(sep, x);
      if (end == utf8_string::npos)
        next = end;
      else
        next = end + sep.size();

      parts.push_back(client->name().substr(x, end - x));
    }
        
    if (parts.size() >= 2 && parts.size() <= 4)
    {
      client->set_context_info(parts[1]);

      if (parts.size() == 4)
        client->set_visible_name("[" + parts[0] + "]" + " " + parts[3]);
      else if (parts.size() == 3) 
        client->set_visible_name("[" + parts[0] + "] [" + parts[2] + "]");
      else
        client->set_visible_name("[" + parts[0] + "] xterm");
    }
    else
      client->set_visible_name(client->name());

  }
  /* Handle emacs */
  else if (client->class_name() == "Emacs")
  {
    const utf8_string &name = client->name();

    if (name != client->instance_name() + "@jlaptop")
    {

      utf8_string::size_type dir_start = name.find(" [");
      if (dir_start != utf8_string::npos && name[name.size() - 1] == ']')
      {
        utf8_string context = name.substr(dir_start + 2,
                                          name.size() - dir_start - 2 - 1);
        context = make_path_pretty(context);
        if (context.size() > 1 && context[context.size() - 1] == '/')
          context.resize(context.size() - 1);
        client->set_context_info(context);
        client->set_visible_name(name.substr(0, dir_start));
      } else
      {
        client->set_visible_name(name);
        client->set_context_info(utf8_string());
      }
    }
  }
  /* Handle firefox */
  else if (client->class_name() == "Firefox-bin")
  {
    utf8_string suffix = " - Conkeror";
    utf8_string::size_type pos = client->name().rfind(suffix);
    if (pos + suffix.length() == client->name().length())
    {
      if (pos == 0)
        client->set_visible_name("Conkeror");
      else
        client->set_visible_name(client->name().substr(0, pos));
    } else
      client->set_visible_name(client->name());
  }
}

void move_frame_to_view(const weak_iptr<WFrame> &weak_frame,
                        const utf8_string &view_name)
{
  if (WFrame *frame = weak_frame.get())
  {
    // TODO: make this logic a separate function, possibly of class WM
    WView *view = frame->wm().view_by_name(view_name);
    if (!view)
    {
      if (!frame->wm().valid_view_name(view_name))
        return;
      view = new WView(frame->wm(), view_name);
    }
    if (frame->client().frame_by_view(view))
      return;
    
    frame->remove();
    view->place_frame_in_smallest_column(frame);
  }
}


void move_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Move to view: ",
                        boost::bind(&move_frame_to_view, weak_iptr<WFrame>(frame),
                                    _1));
  }
}

void copy_frame_to_view(const weak_iptr<WFrame> &weak_frame,
                        const utf8_string &view_name)
{
  if (WFrame *frame = weak_frame.get())
  {
    // TODO: make this logic a separate function, possibly of class WM
    WView *view = frame->wm().view_by_name(view_name);
    if (!view)
    {
      if (!frame->wm().valid_view_name(view_name))
        return;
      view = new WView(frame->wm(), view_name);
    }
    if (frame->client().frame_by_view(view))
      return;

    WFrame *new_frame = new WFrame(frame->client());
    view->place_frame_in_smallest_column(new_frame);
  }
}

void copy_current_frame_to_other_view_interactive(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    wm.menu.read_string("Duplicate to view: ",
                        boost::bind(&copy_frame_to_view, weak_iptr<WFrame>(frame),
                                    _1));
  }
}

void remove_current_frame(WM &wm)
{
  if (WFrame *frame = wm.selected_frame())
  {
    if (frame->client().view_frames().size() > 1)
    {
      delete frame;
    }
  }
}

WM_DEFINE_STYLE_TYPE(BarViewInfoStyle,
                     /* style type features */
                     ()
                     ((WBarCellStyle, WBarCellStyle::Spec, selected))
                     ((WBarCellStyle, WBarCellStyle::Spec, normal)),

                     /* regular features */
                     ()
                     )

class BarViewInfo
{

  WM &wm;
  BarViewInfoStyle style;
  struct CompareViewByName
  {
    bool operator()(WView *a, WView *b)
    {
      return a->name() < b->name();
    }
    
  };
  typedef WBar::CellRef CellRef;
  typedef std::map<WView *, CellRef, CompareViewByName> ViewMap;
  ViewMap views;


  void create_view(WView *view)
  {
    ViewMap::iterator it = views.upper_bound(view);
    
    if (it == views.end())
    {
      CellRef ref = wm.bar.insert_end(WBar::LEFT, style.normal.foreground_color,
                                      style.normal.background_color,
                                      view->name());
      views.insert(std::make_pair(view, ref));
    } else
    {
      CellRef ref = wm.bar.insert_before(it->second,
                                         WBar::LEFT, style.normal.foreground_color,
                                         style.normal.background_color,
                                         view->name());
      views.insert(std::make_pair(view, ref));
    }
  }

  void destroy_view(WView *view)
  {
    views.erase(view);
  }

  void select_view(WView *new_view, WView *old_view)
  {
    ViewMap::iterator it;

    if (new_view)
    {
      it = views.find(new_view);

      if (it != views.end())
      {
        it->second.set_foreground(style.selected.foreground_color);
        it->second.set_background(style.selected.background_color);
      }
    }

    if (old_view)
    {
      it = views.find(old_view);

      if (it != views.end())
      {
        it->second.set_foreground(style.normal.foreground_color);
        it->second.set_background(style.normal.background_color);
      }
    }
  }
public:
  void select_next()
  {
    WView *view = wm.selected_view();

    if (view)
    {
      ViewMap::iterator it = views.find(view);
      if (it != views.end())
      {
        ++it;
        if (it == views.end())
          it = views.begin();

        wm.select_view(it->first);
      }
    }
  }

  void select_prev()
  {
    WView *view = wm.selected_view();

    if (view)
    {
      ViewMap::iterator it = views.find(view);
      if (it != views.end())
      {
        if (it == views.begin())
          it = boost::prior(views.end());
        else
          --it;

        wm.select_view(it->first);
      }
    }
  }
  
public:

  BarViewInfo(WM &wm, const BarViewInfoStyle::Spec &style_spec)
    : wm(wm), style(wm.dc, style_spec)
  {
    wm.create_view_hook.connect(bind(&BarViewInfo::create_view, this, _1));
    wm.destroy_view_hook.connect(bind(&BarViewInfo::destroy_view, this, _1));
    wm.select_view_hook.connect(bind(&BarViewInfo::select_view,
                                     this, _1, _2));

    for (WM::ViewMap::const_iterator it = wm.views().begin();
         it != wm.views().end();
         ++it)
    {
      create_view(it->second);
    }

    select_view(wm.selected_view(), 0);
  }
};

class TimeApplet
{
  WM &wm;
  WBarCellStyle style;

  TimerEvent ev;
  
  WBar::CellRef cell;

  void event_handler()
  {
    time_t t1 = time(0);

    tm m1, m2;
    localtime_r(&t1, &m1);

    m2 = m1;

    m2.tm_sec = 0;
    m2.tm_min++;

    time_t t2 = mktime(&m2);

    ev.wait_for(t2 - t1, 0);

    char buffer[50];

    strftime(buffer, 50, "%04Y-%02m-%02d %02H:%02M", &m1);

    cell.set_text(buffer);
  }
public:

  TimeApplet(WM &wm, const WBarCellStyle::Spec &style_spec,
             WBar::InsertPosition position)
    : wm(wm), style(wm.dc, style_spec),
      ev(wm.event_service(), boost::bind(&TimeApplet::event_handler, this))
  {
    cell = wm.bar.insert(position, style);
    event_handler();
  }
};

class VolumeApplet
{
  WM &wm;
  WBarCellStyle unmuted_style, muted_style;
  TimerEvent ev;
  
  WBar::CellRef cell;

  void event_handler()
  {
    ev.wait_for(10, 0);

    bool muted = false;
    int volume_percent = 0;
    try
    {
      ALSAMixerInfo info;
      ALSAMixerInfo::Channel channel(info, "Master");

      if (!info.get_channel_volume(channel, volume_percent, muted))
      {
        WARN("failed to get volume");
      }
    } catch (std::runtime_error &e)
    {
      WARN("error: %s", e.what());
    }

    char buffer[50];
    sprintf(buffer, "vol: %d%%", volume_percent);
    cell.set_text(buffer);
    if (muted)
    {
      cell.set_foreground(muted_style.foreground_color);
      cell.set_background(muted_style.background_color);
    } else
    {
      cell.set_foreground(unmuted_style.foreground_color);
      cell.set_background(unmuted_style.background_color);
    }
  }
public:

  VolumeApplet(WM &wm,
               const WBarCellStyle::Spec &unmuted_style_spec,
               const WBarCellStyle::Spec &muted_style_spec,               
               WBar::InsertPosition position)
    : wm(wm), unmuted_style(wm.dc, unmuted_style_spec),
      muted_style(wm.dc, muted_style_spec),
      ev(wm.event_service(), boost::bind(&VolumeApplet::event_handler, this))
  {
    cell = wm.bar.insert(position, unmuted_style);
    event_handler();
  }

  void lower_volume()
  {
    execute_command("amixer set Master 1-");
    ev.wait_for(0, 100000);
   }
  void raise_volume()
  {
    execute_command("amixer set Master 1+");
    ev.wait_for(0, 100000);
  }
  void toggle_mute()
  {
    execute_command("amixer set Master toggle");
    ev.wait_for(0, 100000);
  }
};

int main(int argc, char **argv)
{

  char const *display_name = NULL;

#if 0
  {
    static struct option long_options[] =
      {
        {"display", 1, 0, 'd'},
        {0, 0, 0, 0}
      };
    
    while (1)
    {
      char c = getopt_long_only(argc, argv, "d:", long_options, NULL);
      if (c == -1)
        break;
      switch (c)
      {
      case 'd':
        display_name = optarg;
        break;
      case '?':
        exit(EXIT_FAILURE);
      }
    }

    if (optind < argc)
      ERROR("extra command-line arguments specified");
  }
#endif 

  /* Set up child handling */
  {
    struct sigaction act;
    act.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT;
    act.sa_handler = SIG_IGN;
    while (sigaction(SIGCHLD, &act, 0) != 0 && errno == EINTR);
  }
  
  WXDisplay xdisplay(display_name);
  set_close_on_exec_flag(ConnectionNumber(xdisplay.display()), true);

  EventService event_service;
  
  // Style

  WFrameStyle::Spec style_spec;

  ascii_string font_name = "fixed 10";
  
  style_spec.label_font = font_name;
  style_spec.client_background_color = "black";

  style_spec.active_selected.highlight_color = "gold1";
  style_spec.active_selected.shadow_color = "gold1";
  style_spec.active_selected.padding_color = "gold3";
  style_spec.active_selected.background_color = "black";
  style_spec.active_selected.label_foreground_color = "black";
  style_spec.active_selected.label_background_color = "gold1";
  style_spec.active_selected.label_extra_color = "gold3";

#if 1
  style_spec.inactive_selected.highlight_color = "grey20";
  style_spec.inactive_selected.shadow_color = "grey20";
  style_spec.inactive_selected.padding_color = "black";
  style_spec.inactive_selected.background_color = "black";
  style_spec.inactive_selected.label_foreground_color = "black";
  style_spec.inactive_selected.label_background_color = "gold3";
  style_spec.inactive_selected.label_extra_color = "gold4";

  style_spec.inactive.highlight_color = "grey20";
  style_spec.inactive.shadow_color = "grey20";
  style_spec.inactive.padding_color = "black";
  style_spec.inactive.background_color = "black";
  style_spec.inactive.label_foreground_color = "black";
  style_spec.inactive.label_background_color = "grey85";
  style_spec.inactive.label_extra_color = "grey70";
#else
  style_spec.inactive_selected.highlight_color = "grey20";
  style_spec.inactive_selected.shadow_color = "grey20";
  style_spec.inactive_selected.padding_color = "black";
  style_spec.inactive_selected.background_color = "black";
  style_spec.inactive_selected.label_foreground_color = "black";
  style_spec.inactive_selected.label_background_color = "grey85";
  style_spec.inactive_selected.label_extra_color = "grey70";

  style_spec.inactive.highlight_color = "grey20";
  style_spec.inactive.shadow_color = "grey20";
  style_spec.inactive.padding_color = "black";
  style_spec.inactive.background_color = "black";
  style_spec.inactive.label_foreground_color = "black";
  style_spec.inactive.label_background_color = "grey75";
  style_spec.inactive.label_extra_color = "grey60";
#endif 

  style_spec.highlight_pixels = 1;
  style_spec.shadow_pixels = 1;
  style_spec.padding_pixels = 1;
  style_spec.spacing = 2;
  style_spec.label_horizontal_padding = 5;
  style_spec.label_vertical_padding = 2;
  style_spec.label_component_spacing = 1;

  WBarStyle::Spec bar_style_spec;

  bar_style_spec.label_font = style_spec.label_font;
  bar_style_spec.highlight_pixels = 1;
  bar_style_spec.shadow_pixels = 1;
  bar_style_spec.padding_pixels = 1;
  bar_style_spec.spacing = 2;
  bar_style_spec.label_horizontal_padding = 2;
  bar_style_spec.label_vertical_padding = 2;
  bar_style_spec.cell_spacing = 1;
  bar_style_spec.highlight_color = "grey20";
  bar_style_spec.shadow_color = "grey20";
  bar_style_spec.padding_color = "black";
  bar_style_spec.background_color = "black";
  
  WM wm(argc, argv, xdisplay.display(), event_service,
        style_spec, bar_style_spec);

  wm.bind("mod4-f", select_right);
  wm.bind("mod4-b", select_left);
  wm.bind("mod4-p", select_up);
  wm.bind("mod4-n", select_down);

  wm.bind("mod4-k any-f", move_right);
  wm.bind("mod4-k any-b", move_left);
  wm.bind("mod4-k any-p", move_up);
  wm.bind("mod4-k any-n", move_down);
  
  wm.bind("mod4-d", toggle_shaded);
  /* JUST FOR TESTING */
  wm.bind("mod4-mod1-d", toggle_decorated);
  wm.bind("mod4-mod1-control-d", toggle_bar_visible);
  /* */
  wm.bind("mod4-u", decrease_priority);
  wm.bind("mod4-i", increase_priority);

  wm.bind("mod4-k any-u", decrease_column_priority);
  wm.bind("mod4-k any-i", increase_column_priority);

  wm.bind("mod4-x x",
          boost::bind(&execute_command_selected_cwd,
                      boost::ref(wm),
                      "xterm"));

  wm.bind("mod4-x mod4-x", execute_command_cwd_interactive);

  wm.bind("mod4-a", execute_action_interactive);

  wm.bind("mod4-t", switch_to_view_interactive);

  wm.bind("mod4-x e",
              boost::bind(&execute_command_selected_cwd,
                          boost::ref(wm),
                          "~/bin/emacs"));
  
  wm.bind("mod4-x b",
              boost::bind(&execute_command, "~/bin/browser http://google.com"));

  wm.bind("mod4-c", close_current_client);
  wm.bind("mod4-k c", kill_current_client);

  wm.bind("mod4-Return", toggle_fullscreen);

  wm.unmanage_client_hook.connect(&check_fullscreen_on_unmanage_client);

  wm.update_client_name_hook.connect(&update_client_visible_name_and_context);

  for (WM::ClientMap::iterator it = wm.managed_clients.begin();
       it != wm.managed_clients.end();
       ++it)
  {
    update_client_visible_name_and_context(it->second);
  }

  wm.menu.bind("BackSpace", menu_backspace);
  wm.menu.bind("any-Return", menu_enter);
  wm.menu.bind("control-c", menu_abort);
  wm.menu.bind("Escape", menu_abort);
  wm.menu.bind("control-f", menu_forward_char);
  wm.menu.bind("control-b", menu_backward_char);
  wm.menu.bind("Right", menu_forward_char);
  wm.menu.bind("Left", menu_backward_char);
  wm.menu.bind("control-a", menu_beginning_of_line);
  wm.menu.bind("control-e", menu_end_of_line);
  wm.menu.bind("Home", menu_beginning_of_line);
  wm.menu.bind("End", menu_end_of_line);
  wm.menu.bind("control-d", menu_delete);
  wm.menu.bind("Delete", menu_delete);
  wm.menu.bind("control-k", menu_kill_line);

  WBarCellStyle::Spec def_bar_style;
  def_bar_style.foreground_color = "black";
  def_bar_style.background_color = "grey85";

  BarViewInfoStyle::Spec bar_view_info_style;

  bar_view_info_style.normal = def_bar_style;
  
  bar_view_info_style.selected.foreground_color = "black";
  bar_view_info_style.selected.background_color = "gold1";

  BarViewInfo bar_view_info(wm, bar_view_info_style);

  wm.bind("mod4-comma", boost::bind(&BarViewInfo::select_prev,
                                boost::ref(bar_view_info)));
  
  wm.bind("mod4-period", boost::bind(&BarViewInfo::select_next,
                                boost::ref(bar_view_info)));


  WBarCellStyle::Spec muted_style, unmuted_style;

  muted_style.foreground_color = "white";
  muted_style.background_color = "red";
  
  unmuted_style.foreground_color = "white";
  unmuted_style.background_color = "blue";

  /* TODO: check for exceptions */
  VolumeApplet volume_applet(wm, unmuted_style, muted_style,
                             WBar::end(WBar::RIGHT));

  wm.bind("XF86AudioLowerVolume",
          boost::bind(&VolumeApplet::lower_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioRaiseVolume",
          boost::bind(&VolumeApplet::raise_volume,
                      boost::ref(volume_applet)));
  wm.bind("XF86AudioMute",
          boost::bind(&VolumeApplet::toggle_mute,
                      boost::ref(volume_applet)));

  TimeApplet time_applet(wm, def_bar_style,
                         WBar::end(WBar::RIGHT));

  for (char c = 'a'; c <= 'z'; ++c)
  {
    ascii_string str;
    str = "mod4-shift-";
    str += c;
    wm.bind(str, boost::bind(switch_to_view_by_letter, _1, c));
    str = "mod4-mod1-";
    str += c;
    wm.bind(str, boost::bind(switch_to_view_by_letter, _1, c));
  }

  PreviousViewInfo prev_info(wm);

  wm.bind("mod4-r", boost::bind(&PreviousViewInfo::switch_to, boost::ref(prev_info)));

  wm.bind("mod4-k m", move_current_frame_to_other_view_interactive);
  wm.bind("mod4-k n", copy_current_frame_to_other_view_interactive);
  wm.bind("mod4-k k", remove_current_frame);

  do
  {
    wm.flush();
    if (event_service.handle_events() != 0)
      ERROR_SYS("event service handle events");
  } while (1);
}
