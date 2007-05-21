#ifndef _WM_EXTRA_VOLUME_APPLET_HPP
#define _WM_EXTRA_VOLUME_APPLET_HPP

#include <wm/all.hpp>
#include <wm/volume.hpp>
#include <wm/commands.hpp>

STYLE_DEFINITION(VolumeAppletStyle,
                 ((muted, WBarCellStyle, style::Spec),
                  (unmuted, WBarCellStyle, style::Spec)))

class VolumeApplet
{
  WM &wm;
  VolumeAppletStyle style;
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
      cell.set_style(style.muted);
    else
      cell.set_style(style.unmuted);
  }
public:

  VolumeApplet(WM &wm,
               const style::Spec &style_spec,
               WBar::InsertPosition position)
    : wm(wm), style(wm.dc, style_spec),
      ev(wm.event_service(), boost::bind(&VolumeApplet::event_handler, this))
  {
    cell = wm.bar.insert(position, style.unmuted);
    event_handler();
  }

  void lower_volume()
  {
    execute_shell_command("amixer set Master 1-");
    ev.wait_for(0, 100000);
   }
  void raise_volume()
  {
    execute_shell_command("amixer set Master 1+");
    ev.wait_for(0, 100000);
  }
  void toggle_mute()
  {
    execute_shell_command("amixer set Master toggle");
    ev.wait_for(0, 100000);
  }
};


#endif /* _WM_EXTRA_VOLUME_APPLET_HPP */
