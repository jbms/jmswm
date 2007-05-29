#ifndef _UTIL_PATH_HPP
#define _UTIL_PATH_HPP

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>

/**
 * If str is of the form ~/path it is expanded to ${HOME}/path.
 * Otherwise, it is used as is.
 */
std::string expand_path_home(const std::string &str);

const std::string compact_path_home(const std::string &str);

/**
 * Note: this doesn't work correctly on non-POSIX, because it is not
 * clear what to return if passed, e.g.:
 *
 * base =  c:/whatever
 * other = d:blah
 */
boost::filesystem::path interpret_path(const boost::filesystem::path &base,
                                       const boost::filesystem::path &other);

#endif /* _UTIL_PATH_HPP */
