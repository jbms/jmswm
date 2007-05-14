#include <util/path.hpp>
#include <boost/algorithm/string.hpp>

static const char *home_directory = 0;

static const char *get_home()
{
  if (!home_directory)
    home_directory = getenv("HOME");
  return home_directory;
}

std::string expand_path_home(const std::string &str)
{
  const char *home = get_home();
  if (home && str.size() > 0 && str[0] == '~')
    return home + str.substr(1);
  return str;
}

const std::string compact_path_home(const std::string &str)
{
  const char *home = get_home();
  if (home)
  {
    std::string home_str(home);
    if (boost::algorithm::starts_with(str, home_str))
      return "~" + str.substr(home_str.size());
  }
  return str;
}
