
#include <wm/extra/bar_view_applet.hpp>

void BarViewApplet::create_view(WView *view)
{
  ViewMap::iterator it = views.upper_bound(view);
    
  if (it == views.end())
  {
    CellRef ref = wm.bar.insert(WBar::end(WBar::LEFT),
                                style.normal.foreground_color,
                                style.normal.background_color,
                                view->name());
    views.insert(std::make_pair(view, ref));
  } else
  {
    CellRef ref = wm.bar.insert(WBar::before(it->second),
                                style.normal.foreground_color,
                                style.normal.background_color,
                                view->name());
    views.insert(std::make_pair(view, ref));
  }
}

void BarViewApplet::destroy_view(WView *view)
{
  views.erase(view);
}

void BarViewApplet::select_view(WView *new_view, WView *old_view)
{
  ViewMap::iterator it;

  if (new_view)
  {
    it = views.find(new_view);

    if (it != views.end())
    {
      it->second.set_foreground(style.selected.foreground_color);
      it->second.set_background(style.selected.background_color);
    }
  }

  if (old_view)
  {
    it = views.find(old_view);

    if (it != views.end())
    {
      it->second.set_foreground(style.normal.foreground_color);
      it->second.set_background(style.normal.background_color);
    }
  }
}


void BarViewApplet::select_next()
{
  WView *view = wm.selected_view();

  if (view)
  {
    ViewMap::iterator it = views.find(view);
    if (it != views.end())
    {
      ++it;
      if (it == views.end())
        it = views.begin();

      wm.select_view(it->first);
    }
  }
}


void BarViewApplet::select_prev()
{
  WView *view = wm.selected_view();

  if (view)
  {
    ViewMap::iterator it = views.find(view);
    if (it != views.end())
    {
      if (it == views.begin())
        it = boost::prior(views.end());
      else
        --it;

      wm.select_view(it->first);
    }
  }
}


BarViewApplet::BarViewApplet(WM &wm, const BarViewAppletStyle::Spec &style_spec)
  : wm(wm), style(wm.dc, style_spec)
{
  wm.create_view_hook.connect(bind(&BarViewApplet::create_view, this, _1));
  wm.destroy_view_hook.connect(bind(&BarViewApplet::destroy_view, this, _1));
  wm.select_view_hook.connect(bind(&BarViewApplet::select_view,
                                   this, _1, _2));

  for (WM::ViewMap::const_iterator it = wm.views().begin();
       it != wm.views().end();
       ++it)
  {
    create_view(it->second);
  }

  select_view(wm.selected_view(), 0);
}
