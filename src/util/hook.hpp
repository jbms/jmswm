#ifndef _UTIL_HOOK_HPP
#define _UTIL_HOOK_HPP

#include <boost/signal.hpp>
#include <boost/signals/connection.hpp>

namespace util
{
  struct RunUntilSuccess
  {
    typedef bool result_type;
    template <typename InputIterator>
    bool operator()(InputIterator first, InputIterator last) const
    {
      while (first != last)
      {
        if (*first)
          return true;
        ++first;
      }
      return false;
    }
  };

  struct RunUntilFailure
  {
    typedef bool result_type;
    template <typename InputIterator>
    bool operator()(InputIterator first, InputIterator last) const
    {
      while (first != last)
      {
        if (!*first)
          return false;
        ++first;
      }
      return true;
    }
  };
}

#endif /* _UTIL_HOOK_HPP */ 
