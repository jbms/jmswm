#ifndef _WM_EXTRA_NETWORK_APPLET_HPP
#define _WM_EXTRA_NETWORK_APPLET_HPP

#include <wm/all.hpp>

class NetworkAppletState;

class NetworkApplet
{
  std::auto_ptr<NetworkAppletState> state;
  
public:

  NetworkApplet(WM &wm,
                const style::Spec &spec,
                const WBar::InsertPosition &position);
  ~NetworkApplet();
};

#endif /* _WM_EXTRA_NETWORK_APPLET_HPP */
