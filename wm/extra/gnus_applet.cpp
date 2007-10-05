
#include <wm/extra/gnus_applet.hpp>
#include <fstream>
#include <sys/inotify.h>
#include <boost/algorithm/string.hpp>
#include <wm/commands.hpp>

static const char *mail_status_filename = "/tmp/jbms/mail-status";

void GnusApplet::handle_inotify_event(int wd, uint32_t mask, uint32_t cookie,
                                      const char *pathname)
{
  if (wd == this->wd && mask & IN_CLOSE_WRITE)
    update();
}

void GnusApplet::update()
{

  typedef std::map<ascii_string, ascii_string> NewStatus;

  NewStatus new_status;

  {
    std::ifstream ifs(mail_status_filename);
    do
    {
      ascii_string group_name, count;
      if (ifs >> group_name >> count)
      {
        if (abbrevs.count(group_name))
          new_status.insert(std::make_pair(group_name, count));
      }
      else
        break;
    } while (1);
  }

  for (NewStatus::iterator it = new_status.begin(); it != new_status.end(); ++it)
  {
    GroupMap::iterator it2 = groups.find(it->first);
    ascii_string str = abbrevs[it->first] + " " + it->second;
    if (it2 == groups.end())
    {
      // Insert
      GroupMap::iterator it3 = groups.upper_bound(it->first);
      WBar::InsertPosition pos;
      if (it3 == groups.end())
        pos = WBar::before(placeholder);
      else
        pos = WBar::before(it3->second);
      CellRef ref = wm.bar.insert(pos, style, str);
      groups.insert(std::make_pair(it->first, ref));
    } else
      it2->second.set_text(str);
  }

  for (GroupMap::iterator it = groups.begin(), next; it != groups.end(); it = next)
  {
    next = boost::next(it);
    if (new_status.find(it->first) == new_status.end())
      groups.erase(it);
  }
}

GnusApplet::GnusApplet(WM &wm, const style::Spec &style_spec,
                       const WBar::InsertPosition &pos)
  : wm(wm), style(wm.dc, style_spec),
    inotify(wm.event_service(),
            boost::bind(&GnusApplet::handle_inotify_event, this, _1, _2, _3, _4))
{
  {
    // Attempt to create file if it doesn't exist.
    std::ofstream ofs(mail_status_filename, std::ios_base::app | std::ios_base::out);
  }
  abbrevs["mail.misc"] = "misc";
  abbrevs["list.boost-dev"] = "boost";
  //abbrevs["list.cmu-misc-market"] = "misc.market";
  abbrevs["list.erc-help"] = "erc";
  abbrevs["list.linux-kernel"] = "linux";
  abbrevs["list.wmii"] = "wmii";
  abbrevs["list.wmii-hackers"] = "wmii-hackers";
  abbrevs["list.ipw2100-devel"] = "ipw";
  abbrevs["list.ion"] = "ion";
  abbrevs["list.emacs-devel"] = "emacs";
  abbrevs["nnrss:slashdot"] = "slashdot";
  abbrevs["nnrss:boingboing"] = "boingboing";
  abbrevs["nnrss:bbc"] = "bbc";
  abbrevs["nnrss:nytimes"] = "nytimes";
  
  wd = inotify.add_watch(mail_status_filename, IN_CLOSE_WRITE);
  placeholder = wm.bar.placeholder(pos);
  update();
}

void GnusApplet::switch_to_mail()
{
  WView *view = wm.view_by_name("plan");
  WFrame *matching_frame = 0;

  ascii_string group_name;
  if (!groups.empty())
    group_name = groups.begin()->first;

  if (view)
  {
    BOOST_FOREACH (WFrame &f, view->frames_by_activity())
    {
      const utf8_string &name = f.client().name();
      if ((f.client().class_name() == "Emacs")
          && (name == "*Group*" || name == "*Article*" || boost::algorithm::starts_with(name, "*Summary")
              || name == "*Server*"))
      {
        matching_frame = &f;
        break;
      }
    }
    if (matching_frame)
    {
      view->select_frame(matching_frame, true);
    }
    wm.select_view(view);
  } else
  {
    switch_to_view(wm, "plan");
  }

  std::string command("~/bin/gnus-select-group ");
  command += group_name;
  execute_shell_command(command.c_str());
}
