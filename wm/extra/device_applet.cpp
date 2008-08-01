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

  inotify.add_watch("/dev/disk/by-path/", IN_CREATE | IN_DELETE);

  ignore_present.insert("cdrom");

  update_cells();
  // FIXME: make this support this applet being unloaded
  boost::thread(boost::bind(&DeviceAppletState::monitor_mounts, this));
}

void DeviceAppletState::update_cells()
{
  typedef std::map<utf8_string, bool> DevMountedMap;
  DevMountedMap dev_mounted_map;

  {
    std::ifstream ifs(mount_list);
    std::string line;
    while (getline(ifs, line))
    {
      std::string::size_type pos, pos2, pos3, mount_sep;
      if ((pos = line.find(' ')) == std::string::npos)
        continue;
      if ((pos2 = line.find(' ', pos + 1)) == std::string::npos)
        continue;
      if ((pos3 = line.find(' ', pos2 + 1)) == std::string::npos)
        continue;
      std::string mount_point = line.substr(pos + 1, pos2 - pos - 1);
      std::string fs_type = line.substr(pos2 + 1, pos3 - pos2 - 1);
      mount_sep = mount_point.find('/', 1);
      if (mount_sep == std::string::npos)
        continue;
      std::string mount_name = mount_point.substr(mount_sep + 1);
      if (fs_type == "autofs")
        dev_mounted_map[mount_name] = false;
      else {
        DevMountedMap::iterator it = dev_mounted_map.find(mount_name);
        if (it != dev_mounted_map.end())
          it->second = true;
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

