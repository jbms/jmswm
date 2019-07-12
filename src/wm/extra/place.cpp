#include <wm/extra/place.hpp>
#include <wm/extra/view_columns.hpp>
#include <boost/next_prior.hpp>

void place_frame_in_smallest_column(WView *view, WFrame *frame)
{
  WView::iterator col;
  std::size_t desired_columns = get_desired_column_count(view);

  if (view->columns.size() < desired_columns)
    col = view->create_column(view->columns.end());
  else
  {
    col = view->columns.begin();
    for (WView::iterator it = boost::next(col); it != view->columns.end(); ++it)
    {
      if (col->frames.size() > it->frames.size())
        col = it;
    }
  }

  col->add_frame(frame, col->frames.end());
}

WFrame *place_client_in_smallest_column(WView *view, WClient *client)
{
  WFrame *frame = new WFrame(*client);
  place_frame_in_smallest_column(view, frame);
  return frame;
}
