#ifndef _WM_EXTRA_BATTERY_APPLET_HPP
#define _WM_EXTRA_BATTERY_APPLET_HPP

#include <wm/all.hpp>

STYLE_DEFINITION(BatteryAppletStyle,
                 ((charging, WBarCellStyle, style::Spec),
                  (discharging, WBarCellStyle, style::Spec),
                  (inactive, WBarCellStyle, style::Spec)))

class BatteryApplet
{
  WM &wm;
  BatteryAppletStyle style;
  TimerEvent ev;

  WBar::CellRef cell;

  void event_handler();

public:

  BatteryApplet(WM &wm,
                const style::Spec &style_spec,
                WBar::InsertPosition position);  
};

#endif /* _WM_EXTRA_BATTERY_APPLET_HPP */
