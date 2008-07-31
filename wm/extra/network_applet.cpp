
#include <wm/extra/network_applet.hpp>

#include <boost/optional.hpp>
#include <iwlib.h>
#include <linux/sockios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/inotify.h>

static char const *network_status_filename = "/tmp/network-status";

class NetworkAppletState
{
  WM &wm;
  WBarCellStyle style;
  TimerEvent ev;
  InotifyEvent inotify;
  int wd;
  WBar::CellRef placeholder;
  boost::optional<WBar::CellRef> cell;

  ifreq req_wlan, req_eth;
  int skfd;

  void event_handler();
  void inotify_handler(int wd, uint32_t mask, uint32_t cookie,
                       const char *pathname);
public:
  NetworkAppletState(WM &wm,
                     const style::Spec &spec,
                     const WBar::InsertPosition &position);
  ~NetworkAppletState();
};

NetworkAppletState::NetworkAppletState(WM &wm,
                     const style::Spec &spec,
                     const WBar::InsertPosition &position)
  : wm(wm),
    style(wm.dc, spec),
    ev(wm.event_service(), boost::bind(&NetworkAppletState::event_handler, this)),
    inotify(wm.event_service(), boost::bind(&NetworkAppletState::inotify_handler, this, _1, _2, _3, _4))
{
  placeholder = wm.bar.placeholder(position);
  skfd = iw_sockets_open();

  // Initialize req structs
  strncpy(req_eth.ifr_name, "eth", IFNAMSIZ);
  strncpy(req_wlan.ifr_name, "wlan", IFNAMSIZ);

  wd = inotify.add_watch(network_status_filename, IN_ATTRIB);

  event_handler();
}

void NetworkAppletState::inotify_handler(int wd, uint32_t mask, uint32_t cookie,
                                         const char *pathname)
{
  if (wd == this->wd && mask & IN_ATTRIB)
    event_handler();
}

static sockaddr_in *get_iface_address(ifreq &req, int skfd)
{
  if ((ioctl(skfd, SIOCGIFFLAGS, &req) == 0)
      && req.ifr_flags & IFF_UP)
  {
    if (ioctl(skfd, SIOCGIFADDR, &req) == 0)
    {
      if (req.ifr_addr.sa_family == AF_INET)
      {
        return (sockaddr_in *)&req.ifr_addr;
      }
    }
  }
  return 0;
}

void NetworkAppletState::event_handler()
{
  ev.wait(time_duration::seconds(10));

  ascii_string str;

  if (get_iface_address(req_eth, skfd))
  {
    str = "eth";
  } else if (get_iface_address(req_wlan, skfd))
  {
    std::ostringstream ostr;
    
    iwreq wrq;
    
    /* Get ESSID */
    char essid[IW_ESSID_MAX_SIZE + 1];
    bool essidOn = false;
    wrq.u.essid.pointer = (caddr_t) essid;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    wrq.u.essid.flags = 0;
    if(iw_get_ext(skfd, "wlan", SIOCGIWESSID, &wrq) >= 0)
    {
      if (wrq.u.data.flags)
        essidOn = true;
      essid[wrq.u.essid.length] = 0;

      if (essidOn)
      {
      
        // Get link quality

        int linkFd = open("/sys/class/net/wlan/wireless/link", O_RDONLY);
        char linkQualityBuffer[10];
        int linkQuality = -1;
        if (linkFd >= 0) {
          int nread = read(linkFd, linkQualityBuffer, 9);
          if (nread > 0) {
            linkQualityBuffer[nread] = 0;
            sscanf(linkQualityBuffer, "%d", &linkQuality);
          }
          close(linkFd);
        }

        if (linkQuality >= 0)
        {
          ostr << "wlan: " << essid << " " << (linkQuality + 5) / 10;
          str = ostr.str();
        }
      }
    }
  }

  if (str.empty())
  {
    cell.reset();
  } else
  {
    if (!cell)
      cell.reset(wm.bar.insert(WBar::after(placeholder), style));
    if (cell->text() != str)
      cell->set_text(str);
  }
}

NetworkAppletState::~NetworkAppletState()
{
  iw_sockets_close(skfd);
}

NetworkApplet::NetworkApplet(WM &wm,
                             const style::Spec &spec,
                             const WBar::InsertPosition &position)
  : state(new NetworkAppletState(wm, spec, position))
{}


NetworkApplet::~NetworkApplet()
{
}

