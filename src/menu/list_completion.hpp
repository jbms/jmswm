#ifndef _MENU_LIST_COMPLETION_HPP
#define _MENU_LIST_COMPLETION_HPP

#include <util/range.hpp>
#include <menu/menu.hpp>
#include <style/common.hpp>

namespace menu
{
  namespace list_completion
  {

    STYLE_DEFINITION(EntryStyle,
                     ((normal, style::TextColor, style::Spec),
                      (selected, style::TextColor, style::Spec)))
    
    typedef std::pair<utf8_string, const EntryStyle *> Entry;

    typedef boost::function<void (InputState &, const utf8_string &)> StringCompletionApplicator;

    void apply_completion_simple(InputState &state, const utf8_string &completion);

    Menu::CompletionsPtr completion_list(const InputState &initial_input,
                                         const std::vector<Entry> &list,
                                         const StringCompletionApplicator &applicator = apply_completion_simple,
                                         bool complete_common_prefix = true);

    Menu::Completer prefix_completer(const std::vector<utf8_string> &list, const EntryStyle &style);

    template <class Range>
    Menu::Completer prefix_completer(const Range &rng, const EntryStyle &style)
    {
      return prefix_completer(boost::copy_range<std::vector<utf8_string> >(rng), style);
    }
    
  } // namespace menu::list_completion
} // namespace menu

#endif /* _MENU_LIST_COMPLETION_HPP */
