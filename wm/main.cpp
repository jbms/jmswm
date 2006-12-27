
#include <wm/wm.hpp>
#include <draw/draw.hpp>
#include <util/log.hpp>
#include <util/close_on_exec.hpp>

#include <event.h>

#include <getopt.h>
#include <stdlib.h>

int main(int argc, char **argv)
{

  char const *display_name = NULL;

  {
    static struct option long_options[] =
      {
        {"display", 1, 0, 'd'},
        {0, 0, 0, 0}
      };
    
    while (1)
    {
      char c = getopt_long_only(argc, argv, "d:", long_options, NULL);
      if (c == -1)
        break;
      switch (c)
      {
      case 'd':
        display_name = optarg;
        break;
      case '?':
        exit(EXIT_FAILURE);
      }
    }

    if (optind < argc)
      ERROR("extra command-line arguments specified");
  }

  
  WXDisplay xdisplay(display_name);
  set_close_on_exec_flag(ConnectionNumber(xdisplay.display()), true);
  
  event_base *eb = static_cast<event_base *>(event_init());

  // Style

  WFrameStyle::Spec style_spec;
  
  style_spec.label_font = "fixed 11";
  style_spec.client_background_color = "black";

  style_spec.active_selected.highlight_color = "gold1";
  style_spec.active_selected.shadow_color = "gold1";
  style_spec.active_selected.padding_color = "gold3";
  style_spec.active_selected.background_color = "black";
  style_spec.active_selected.label_foreground_color = "black";
  style_spec.active_selected.label_background_color = "gold1";

  style_spec.inactive_selected.highlight_color = "grey20";
  style_spec.inactive_selected.shadow_color = "grey20";
  style_spec.inactive_selected.padding_color = "black";
  style_spec.inactive_selected.background_color = "black";
  style_spec.inactive_selected.label_foreground_color = "black";
  style_spec.inactive_selected.label_background_color = "gold3";

  style_spec.inactive.highlight_color = "grey20";
  style_spec.inactive.shadow_color = "grey20";
  style_spec.inactive.padding_color = "black";
  style_spec.inactive.background_color = "black";
  style_spec.inactive.label_foreground_color = "black";
  style_spec.inactive.label_background_color = "grey85";

  style_spec.highlight_pixels = 1;
  style_spec.shadow_pixels = 1;
  style_spec.padding_pixels = 1;
  style_spec.spacing = 3;
  style_spec.label_horizontal_spacing = 2;
  style_spec.label_vertical_spacing = 2;
  
  WM wm(xdisplay.display(), eb, style_spec);

  event_base_dispatch(eb);

  return EXIT_FAILURE;
}
