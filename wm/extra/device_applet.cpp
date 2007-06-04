#include <wm/extra/device_applet.hpp>
#include <boost/regex.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>
#include <util/close_on_exec.hpp>
#include <boost/thread.hpp>
#include <sys/inotify.h>
#include <fstream>
#include <fcntl.h>

static const char *autofs_map_file = "/etc/autofs/auto.auto";
static const char *mount_list = "/proc/mounts";

STYLE_DEFINITION(DeviceAppletStyle,
                 ((present, WBarCellStyle, style::Spec),
                  (mounted, WBarCellStyle, style::Spec)))

class DeviceAppletState
{
  WM &wm;
  DeviceAppletStyle style;
  InotifyEvent inotify;
  WBar::CellRef placeholder;
  std::set<utf8_string> ignore_present;
  typedef std::vector<std::pair<utf8_string, std::string> > DeviceList;
  DeviceList all_devices;
  std::vector<WBar::CellRef> cells;

  void inotify_handler(int wd, uint32_t mask, uint32_t cookie,
                       const char *pathname);
  void update_all_devices();
  void update_cells();
  void monitor_mounts();
public:
  DeviceAppletState(WM &wm,
                    const style::Spec &style_spec,
                    const WBar::InsertPosition &position);
};

DeviceAppletState::DeviceAppletState(WM &wm,
                                     const style::Spec &style_spec,
                                     const WBar::InsertPosition &position)
  : wm(wm),
    style(wm.dc, style_spec),
    inotify(wm.event_service(), boost::bind(&DeviceAppletState::inotify_handler, this, _1, _2, _3, _4))
{
  placeholder = wm.bar.placeholder(position);

  inotify.add_watch("/dev/disk/by-uuid/", IN_CREATE | IN_DELETE);

  ignore_present.insert("cdrom");

  update_all_devices();
  update_cells();
  // FIXME: make this support this applet being unloaded
  boost::thread(boost::bind(&DeviceAppletState::monitor_mounts, this));
}

void DeviceAppletState::update_all_devices()
{
  static const boost::regex map_regex(" *([^ ]*) *[^ ]* *:([^ ]*) *");
  std::ifstream ifs(autofs_map_file);
  std::string line;
  boost::smatch match;
  all_devices.clear();
  while (getline(ifs, line))
  {
    if (regex_match(line, match, map_regex))
      all_devices.push_back(std::make_pair(match[1], match[2]));
  }
}

void DeviceAppletState::update_cells()
{
  typedef std::map<dev_t, utf8_string> DevNumMap;
  typedef std::map<utf8_string, bool> DevMountedMap;
  DevNumMap dev_num_map;
  DevMountedMap dev_mounted_map;
  struct stat st;
  for (DeviceList::const_iterator it = all_devices.begin(), end = all_devices.end();
       it != end;
       ++it)
  {
    if (stat(it->second.c_str(), &st) == 0
        && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)))
    {
      dev_num_map.insert(std::make_pair(st.st_rdev, it->first));
      dev_mounted_map.insert(std::make_pair(it->first, false));
    }
  }


  {
    std::ifstream ifs(mount_list);
    std::string line;
    while (getline(ifs, line))
    {
      std::string::size_type pos = line.find(' ');
      if (pos != std::string::npos)
      {
        std::string dev = line.substr(0, pos);
        if (stat(dev.c_str(), &st) == 0
            && (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode)))
        {
          DevNumMap::const_iterator it = dev_num_map.find(st.st_rdev);
          if (it != dev_num_map.end())
          {
            dev_mounted_map[it->second] = true;
          }
        }
      }
    }
  }
  
  cells.clear();

  for (DevMountedMap::const_iterator it = dev_mounted_map.begin(), end = dev_mounted_map.end();
       it != end;
       ++it)
  {
    if (it->second || ignore_present.find(it->first) == ignore_present.end())
    {
      cells.push_back(wm.bar.insert(WBar::before(placeholder), it->second ? style.mounted : style.present,
                                    it->first));
    }
  }

}

void DeviceAppletState::inotify_handler(int wd, uint32_t mask, uint32_t cookie,
                                        const char *pathname)
{
  if (mask & (IN_CREATE | IN_DELETE))
    update_cells();
}

void DeviceAppletState::monitor_mounts()
{
  int mount_fd = open(mount_list, O_RDONLY);
  if (mount_fd == -1)
    return;
  set_close_on_exec_flag(mount_fd, true);
  fd_set rfds;
  while (1)
  {
    FD_ZERO(&rfds);
    FD_SET(mount_fd, &rfds);

    
    int retval;
    while ((retval = select(mount_fd + 1, &rfds, NULL, NULL, NULL)) == -1 && errno == EINTR)
      continue;
    if (retval == 1)
      wm.event_service().post(boost::bind(&DeviceAppletState::update_cells, boost::ref(this)));
  }
}

DeviceApplet::DeviceApplet(WM &wm,
                           const style::Spec &spec,
                           const WBar::InsertPosition &position)
  : state(new DeviceAppletState(wm, spec, position))
{}

DeviceApplet::~DeviceApplet() {}

