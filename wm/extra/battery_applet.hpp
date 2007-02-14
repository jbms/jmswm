#ifndef _WM_EXTRA_BATTERY_APPLET_HPP
#define _WM_EXTRA_BATTERY_APPLET_HPP

#include <wm/all.hpp>

class BatteryApplet
{
  WM &wm;
  WBarCellStyle charging_style, discharging_style, inactive_style;
  TimerEvent ev;

  WBar::CellRef cell;

  void event_handler();

public:

  BatteryApplet(WM &wm,
                const WBarCellStyle::Spec &charging_style_spec,
                const WBarCellStyle::Spec &discharging_style_spec,
                const WBarCellStyle::Spec &inactive_style_spec,
                WBar::InsertPosition position)
    : wm(wm),
      charging_style(wm.dc, charging_style_spec),
      discharging_style(wm.dc, discharging_style_spec),
      inactive_style(wm.dc, inactive_style_spec),
      ev(wm.event_service(), boost::bind(&BatteryApplet::event_handler, this))
  {
    cell = wm.bar.insert(position, inactive_style);
    event_handler();
  }
  
};

#endif /* _WM_EXTRA_BATTERY_APPLET_HPP */
