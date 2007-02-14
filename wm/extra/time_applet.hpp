#ifndef _WM_EXTRA_TIME_APPLET_HPP
#define _WM_EXTRA_TIME_APPLET_HPP

#include <wm/all.hpp>

class TimeApplet
{
  WM &wm;
  WBarCellStyle style;

  TimerEvent ev;
  
  WBar::CellRef cell;

  void event_handler()
  {
    time_t t1 = time(0);

    tm m1, m2;
    localtime_r(&t1, &m1);

    m2 = m1;

    m2.tm_sec = 0;
    m2.tm_min++;

    time_t t2 = mktime(&m2);

    ev.wait_for(t2 - t1, 0);

    char buffer[50];

    strftime(buffer, 50, "%04Y-%02m-%02d %02H:%02M", &m1);

    cell.set_text(buffer);
  }
public:

  TimeApplet(WM &wm, const WBarCellStyle::Spec &style_spec,
             WBar::InsertPosition position)
    : wm(wm), style(wm.dc, style_spec),
      ev(wm.event_service(), boost::bind(&TimeApplet::event_handler, this))
  {
    cell = wm.bar.insert(position, style);
    event_handler();
  }
};


#endif /* _WM_EXTRA_TIME_APPLET_HPP */

