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
    if (str == home_str || boost::algorithm::starts_with(str, home_str + "/"))
      return "~" + str.substr(home_str.size());
  }
  return str;
}

/**
 * Note: this doesn't work correctly on non-POSIX, because it is not
 * clear what to return if passed, e.g.:
 *
 * base =  c:/whatever
 * other = d:blah
 */
boost::filesystem::path interpret_path(const boost::filesystem::path &base,
                                       const boost::filesystem::path &other)
{
  if (other.is_complete())
    return other;
  return base / other;
}
