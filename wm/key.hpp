#ifndef _WM_KEY_HPP
#define _WM_KEY_HPP

#include <X11/Xlib.h>
#include <util/string.hpp>
#include <vector>
#include <boost/function.hpp>

#include <set>

class WKeySpec
{
private:
  KeySym keysym_;
  unsigned int modifiers_;
  void initialize(const ascii_string &str);
public:

  WKeySpec() {}

  WKeySpec(KeySym ksym,
           unsigned int mods);

  WKeySpec(const ascii_string &str);

  WKeySpec(const char *str);

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
  void initialize(const ascii_string &str);
public:
  WKeySequence(const ascii_string &str);
  WKeySequence(const char *str);
  WKeySequence() {}
};

ascii_string to_string(const WKeySequence &seq);

class WKeyBindingState;


class WM;
class WModifierInfo;

class WKeyMap;

typedef boost::function<void (WM &)> KeyAction;

/* Note: update must be called at least once after construction before
   modifiers and mask are valid. */
class WModifierInfo
{
public:

  const static int locking_modifier_count = 3;

  unsigned int locking_modifiers[locking_modifier_count];

  unsigned int locking_mask;

  std::set<int> modifier_keycodes;  

  void update(Display *dpy);
};

class WKeyBindingContext
{

public:
  /* Note: these members are not part of the external interface, and
     are declared public for technical reasons. */
  WM &wm;
  const WModifierInfo &mod_info;

  /* If event_window is None, the root window is used; only relevant
     if grab_required is true. */
  Window event_window;
  bool grab_required;

  WKeyBindingState *state;

  static void sequence_timeout_handler(int, short, void *ctx);

  void reset_current_key_sequence();
  
public:
  WKeyBindingContext(WM &wm, const WModifierInfo &mod_info,
                     Window event_window,
                     bool grab_required);

  ~WKeyBindingContext();

  bool bind(const WKeySequence &seq, const KeyAction &action);
  bool unbind(const WKeySequence &seq);

  bool process_keypress(const XKeyEvent &ev);
};

#endif /* _WM_KEY_HPP */
