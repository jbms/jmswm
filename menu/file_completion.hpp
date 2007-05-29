#ifndef _MENU_FILE_COMPLETION_HPP
#define _MENU_FILE_COMPLETION_HPP

#include <menu/menu.hpp>
#include <boost/filesystem/path.hpp>

WMenu::Completer file_completer(const boost::filesystem::path &default_dir);

#endif /* _MENU_FILE_COMPLETION_HPP */
