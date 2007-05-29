#ifndef _MENU_LIST_COMPLETION_HPP
#define _MENU_LIST_COMPLETION_HPP

#include <util/range.hpp>
#include <menu/menu.hpp>

typedef boost::function<void (WMenu::InputState &, const utf8_string &)> StringCompletionApplicator;

void apply_completion_simple(WMenu::InputState &state, const utf8_string &completion);

WMenu::Completions completion_list(const WMenu::InputState &initial_input,
                                   const std::vector<utf8_string> &list,
                                   const StringCompletionApplicator &applicator
                                   = apply_completion_simple,
                                   bool complete_common_prefix = true);

WMenu::Completer prefix_completer(const std::vector<utf8_string> &list);

template <class Range>
WMenu::Completer prefix_completer(const Range &rng)
{
  return prefix_completer(boost::copy_range<std::vector<utf8_string> >(rng));
}

#endif /* _MENU_LIST_COMPLETION_HPP */
