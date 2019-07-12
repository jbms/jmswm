#ifndef _WM_EXTRA_DEVICE_APPLET_HPP
#define _WM_EXTRA_DEVICE_APPLET_HPP

#include <wm/all.hpp>

class DeviceAppletState;

class DeviceApplet
{
  std::unique_ptr<DeviceAppletState> state;

public:

  DeviceApplet(WM &wm,
               const style::Spec &spec,
               const WBar::InsertPosition &position);

  ~DeviceApplet();
};

#endif /* _WM_EXTRA_DEVICE_APPLET_HPP */
