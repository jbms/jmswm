#ifndef _WM_EXTRA_VOLUME_APPLET_HPP
#define _WM_EXTRA_VOLUME_APPLET_HPP

#include <wm/all.hpp>
#include <wm/commands.hpp>

class VolumeAppletState;

class VolumeApplet
{
  std::unique_ptr<VolumeAppletState> state;

public:
  VolumeApplet(WM &wm,
               const style::Spec &style_spec,
               const WBar::InsertPosition &position);
  ~VolumeApplet();
  void lower_volume();
  void raise_volume();
  void toggle_mute();
};

#endif /* _WM_EXTRA_VOLUME_APPLET_HPP */
