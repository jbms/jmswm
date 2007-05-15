#ifndef _WM_EXTRA_CWD_HPP
#define _WM_EXTRA_CWD_HPP

#include <util/property.hpp>

class WClient;

void update_client_visible_name_and_context(WClient *client);

PROPERTY_ACCESSOR(WClient, ascii_string, client_cwd)
PROPERTY_ACCESSOR(WClient, utf8_string, web_browser_title)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_url)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_frame_id)
PROPERTY_ACCESSOR(WClient, ascii_string, web_browser_frame_tag)

#endif /* _WM_EXTRA_CWD_HPP */
