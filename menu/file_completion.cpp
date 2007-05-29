#include <menu/list_completion.hpp>
#include <menu/file_completion.hpp>
#include <vector>
#include <util/string.hpp>
#include <util/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

static void apply_path_completion(const std::string &base,
                                  WMenu::InputState &state,
                                  const utf8_string &completion)
{
  state.text = base + completion;
  state.cursor_position = state.text.size();
}

static WMenu::Completions file_completions(const boost::filesystem::path &default_dir,
                                           const WMenu::InputState &state)
{
  // No completions for the special input of ~
  if (state.text == "~")
    return WMenu::Completions();

  using boost::filesystem::path;
  using boost::filesystem::directory_iterator;
  using boost::algorithm::starts_with;
  
  std::string prefix;
  std::string base;
  {
    std::string::size_type pos = state.text.rfind('/');
    if (pos != std::string::npos)
    {
      prefix = state.text.substr(pos + 1);
      base = state.text.substr(0, pos + 1);
    }
    else
    {
      prefix = state.text;
      base = "";
    }
  }
  
  std::vector<utf8_string> list;
  try
  {
    for (directory_iterator it(interpret_path(default_dir, expand_path_home(base))), end;
         it != end; ++it)
    {
      std::string leaf = it->path().leaf();
      if (starts_with(leaf, ".") && !starts_with(prefix, "."))
        continue;
      if (starts_with(leaf, prefix))
      {
        try
        {
          if (is_directory(it->status()))
            list.push_back(it->path().leaf() + "/");
          else
            list.push_back(it->path().leaf());
        } catch (boost::filesystem::filesystem_error &e)
        {}
      }
    }
  } catch (boost::filesystem::filesystem_error &e)
  {}
  return completion_list(state, list, boost::bind(&apply_path_completion,
                                                  base, _1, _2));
}

WMenu::Completer file_completer(const boost::filesystem::path &default_dir)
{
  return boost::bind(&file_completions, default_dir, _1);
}
