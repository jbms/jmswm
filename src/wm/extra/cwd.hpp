#ifndef _WM_EXTRA_CWD_HPP
#define _WM_EXTRA_CWD_HPP

#include <util/property.hpp>

class WClient;

bool update_client_visible_name_and_context(WClient *client);

PROPERTY_ACCESSOR(WClient, ascii_string, client_cwd)

#endif /* _WM_EXTRA_CWD_HPP */
