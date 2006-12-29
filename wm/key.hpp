#ifndef _WM_KEY_HPP
#define _WM_KEY_HPP

#include <X11/Xlib.h>
#include <util/string.hpp>
#include <vector>

class WKeySpec
{
private:
  KeySym keysym_;
  unsigned int modifiers_;
public:

  WKeySpec() {}

  WKeySpec(KeySym ksym,
           unsigned int mods);

  WKeySpec(const ascii_string &str);

  KeySym keysym() const
  {
    return keysym_;
  }

  unsigned int modifiers() const
  {
    return modifiers_;
  }
};

ascii_string to_string(const WKeySpec &spec);

class WKeySequence : public std::vector<WKeySpec>
{
public:
  WKeySequence(const ascii_string &str);
  WKeySequence() {}
};

ascii_string to_string(const WKeySequence &seq);

class WKeyBindingState;

#endif /* _WM_KEY_HPP */
