#ifndef _MENU_URL_COMPLETION_HPP
#define _MENU_URL_COMPLETION_HPP

#include <menu/menu.hpp>
#include <boost/filesystem/path.hpp>

WMenu::Completer url_completer(const boost::filesystem::path &mozilla_profile_dir);

#endif /* _MENU_URL_COMPLETION_HPP */
