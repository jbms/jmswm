#ifndef _MENU_FILE_COMPLETION_HPP
#define _MENU_FILE_COMPLETION_HPP

#include <menu/menu.hpp>
#include <boost/filesystem/path.hpp>
#include <menu/list_completion.hpp>
#include <sys/types.h>
#include <map>

namespace menu
{
  namespace file_completion
  {

    class Style;

    class EntryStyler
    {
      std::unique_ptr<Style> style;
      typedef std::map<std::string, const menu::list_completion::EntryStyle *> ExtMap;
      ExtMap ext_map;
    public:
      EntryStyler(WDrawContext &dc, const style::Spec &style_spec);
      ~EntryStyler();
      const menu::list_completion::EntryStyle &operator()(const boost::filesystem::path &path,
                                                          struct stat const &stat_info) const;
    };

    Menu::Completer file_completer(const boost::filesystem::path &default_dir, const EntryStyler &styler);

  } // namespace menu::file_completion
} // namespace menu

#endif /* _MENU_FILE_COMPLETION_HPP */
