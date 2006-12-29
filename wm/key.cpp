
#include <boost/foreach.hpp>
#include <util/log.hpp>

#include <assert.h>

#include <wm/key.hpp>
#include <wm/wm.hpp>

#include <boost/utility.hpp>
#include <boost/bind.hpp>

#include <boost/algorithm/string.hpp>
#include <functional>
#include <stdexcept>

#include <algorithm>
#include <iterator>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/shared_ptr.hpp>

#include <X11/keysymdef.h>

#include <assert.h>

class WKeyMap;

class WKeyBinding : public WKeySpec
{
public:
  /* If this is 0, it means no corresponding mapping */
  mutable int keycode;

  /* This is the actual set of modifiers used; it is equal to
     modifiers() with locking modifiers removed. */
  mutable unsigned int clean_modifiers;
  
  boost::tuple<KeySym, unsigned int> keysym_map_key() const
  {
    return boost::make_tuple(keysym(), modifiers());
  }

  boost::tuple<int, unsigned int> keycode_map_key() const
  {
    return boost::make_tuple(keycode, clean_modifiers);
  }

  mutable boost::shared_ptr<WKeyMap> submap;
  mutable boost::function<void ()> action;
  
  bool is_submap() const
  {
    return !!submap;
  }
  
  WKeyBinding(const WKeySpec &spec)
    : WKeySpec(spec)
  {}
};

class WKeyMap
{
public:

  typedef boost::multi_index_container<
    WKeyBinding,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
        /*        
        boost::multi_index::composite_key<
          boost::multi_index::const_mem_fun<WKeySpec, KeySym,
                                            &WKeySpec::keysym>,
          boost::multi_index::const_mem_fun<WKeySpec, unsigned int,
                                            &WKeySpec::modifiers>
        >*/
        boost::multi_index::const_mem_fun<
          WKeyBinding,
          boost::tuple<KeySym, unsigned int>,
          &WKeyBinding::keysym_map_key
        >
      >
    >
  > KeysymMap;

  /* This is a map of the actual bindings that have been grabbed */
  typedef boost::multi_index_container<
    const WKeyBinding *,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
        /*
        boost::multi_index::composite_key<
          boost::multi_index::member<WKeyBinding, int, &WKeyBinding::keycode>,
          boost::multi_index::member<WKeyBinding, unsigned int,
                                     &WKeyBinding::clean_modifiers>
        >*/
        boost::multi_index::const_mem_fun<
          WKeyBinding,
          boost::tuple<int, unsigned int>,
          &WKeyBinding::keycode_map_key
        >
      >
    >
  > KeycodeMap;

  mutable KeysymMap keysym_map;
  mutable KeycodeMap keycode_map;

  bool empty() const
  {
    return keysym_map.empty();
  }

  void clear() const
  {
    /* Clear keycode_map first, because it contains pointers to
       keysym_map */
    keycode_map.clear();
    keysym_map.clear();
  }

  ~WKeyMap()
  {
    /* Clear before destruction to avoid dangling pointer problems */
    clear();
  }
};

/* Note: update must be called at least once after construction before
   modifiers and mask are valid. */
class WLockingModifierInfo
{
public:

  const static int modifier_count = 3;

  unsigned int modifiers[modifier_count];

  unsigned int mask;

  void update(Display *dpy);
};


class WKeyBindingState
{
public:
  
  WKeyMap kmap;

  /* If this is non-NULL, the keyboard is grabbed. */
  boost::shared_ptr<WKeyMap> current_kmap;
  struct event kmap_reset_event;

  WLockingModifierInfo locking_mod_info;
};

void WM::key_map_reset_timeout_handler(int, short, void *wm_ptr)
{
  WM &wm = *(WM *)wm_ptr;
  wm.reset_current_key_sequence();
}

void WM::initialize_key_handler()
{
  if (key_binding_state)
    ERROR("key handler already initialized");
  
  key_binding_state = new WKeyBindingState;

  WKeyBindingState &kbs = *key_binding_state;

  evtimer_set(&kbs.kmap_reset_event, &WM::key_map_reset_timeout_handler, this);
  if (event_base_set(eb, &kbs.kmap_reset_event) != 0)
    ERROR_SYS("event_base_set");

  kbs.locking_mod_info.update(display());

  key_sequence_timeout.tv_sec = 5;
  key_sequence_timeout.tv_usec = 0;
}

void WM::deinitialize_key_handler()
{
  if (!key_binding_state)
    ERROR("key handler not yet initialized");

  /* TODO: remove key grabs */
  event_del(&key_binding_state->kmap_reset_event);
  delete key_binding_state;
  key_binding_state = 0;
}

/**
 * {{{ Modifier handling
 */
const static int N_MODS = 8;

static const unsigned int modmasks[N_MODS]={
  ShiftMask, LockMask, ControlMask, Mod1Mask, Mod2Mask, Mod3Mask,
  Mod4Mask, Mod5Mask
};

static const char *modnames[N_MODS]={"Shift","Lock","Control","Mod1",
                                     "Mod2","Mod3","Mod4","Mod5"};

static unsigned int modname_to_modmask(const ascii_string &name)
{
  for (int i = 0; i < N_MODS; ++i)
  {
    if (boost::algorithm::iequals(name, ascii_string(modnames[i])))
      return modmasks[i];
  }

  return 0;
}

/* This function is based on code from Ion.
   Copyright (c) Tuomo Valkonen 1999-2006.  */
void WLockingModifierInfo::update(Display *dpy)
{
  /* Always assume caps lock is a locking modifier */
  modifiers[2] = LockMask;

  mask = LockMask;

  const int num_lookup = 2;

  static const KeySym keysyms[num_lookup] = {XK_Num_Lock, XK_Scroll_Lock};

  XModifierKeymap *modmap = XGetModifierMapping(dpy);
  assert(modmap != 0);

  unsigned int keycodes[num_lookup];

  std::transform(keysyms, keysyms + num_lookup, keycodes,
                 boost::bind(XKeysymToKeycode, dpy, _1));

  for (int j=0; j < N_MODS*modmap->max_keypermod; j++)
  {
    for (int i=0; i < num_lookup; i++)
    {
      if (keycodes[i] == 0)
        continue;
      
      if (modmap->modifiermap[j] == keycodes[i])
      {
        modifiers[i] = modmasks[j/modmap->max_keypermod];
        mask |= modifiers[i];
      }
    }
  }

  XFreeModifiermap(modmap);
}

template <class Function>
static void for_each_mod_combo(const WLockingModifierInfo &info,
                               unsigned int modifiers,
                               const Function &f)
{
  assert((modifiers & info.mask) == 0);

  f(modifiers);

  if (modifiers == AnyModifier)
    return;

  for (int i = 0; i < info.modifier_count; ++i)
  {
    if (info.modifiers[i] == 0)
      continue;
    unsigned int mods = modifiers | info.modifiers[i];
    f(mods);
    
    for (int j = i+1; j < info.modifier_count; ++j)
    {
      if (info.modifiers[j] == 0)
        continue;
      mods |= info.modifiers[j];
      f(mods);

      for (int k = j+1; k < info.modifier_count; ++k)
      {
        if (info.modifiers[k] == 0)
          continue;
        mods |= info.modifiers[k];
        
        f(mods);
      }
    }
  }
}

/**
 * }}}
 */

/**
 * {{{ Key grabbing
 */

void WM::grab_key(int keycode, unsigned int modifiers,
                  Window grab_window,
                  bool owner_events,
                  int pointer_mode,
                  int keyboard_mode)
{
  if (grab_window == None)
    grab_window = root_window();
  for_each_mod_combo(key_binding_state->locking_mod_info, modifiers,
                     boost::bind(XGrabKey, display(), keycode, _1,
                                 grab_window, owner_events, pointer_mode,
                                 keyboard_mode));
}

void WM::ungrab_key(int keycode, unsigned int modifiers,
                    Window grab_window)
{
  if (grab_window == None)
    grab_window = root_window();  
  for_each_mod_combo(key_binding_state->locking_mod_info, modifiers,
                     boost::bind(XUngrabKey, display(), keycode, _1,
                                 grab_window));
}

/**
 * }}}
 */

bool WM::bind_key(const WKeySequence &seq,
                  const boost::function<void ()> &action)
{
  /* TODO: maybe throw or ERROR here */
  if (seq.empty())
    return false;

  WKeyMap *cur = &key_binding_state->kmap;

  std::vector<int> keycodes;
  std::transform(seq.begin(), seq.end(),
                 std::back_inserter(keycodes),
                 boost::bind(XKeysymToKeycode,
                             display(),
                             boost::bind(&WKeySpec::keysym, _1)));

  bool keycode_mapping_exists
    = (std::find(keycodes.begin(), keycodes.end(), 0) == keycodes.end());
                            
  if (!keycode_mapping_exists)
    WARN("No keycode mapping for key sequence: %s",
         to_string(seq).c_str());

  std::vector<int>::const_iterator keycode_it = keycodes.begin();

  WKeySequence::const_iterator seq_it = seq.begin(), seq_end = seq.end();  

  do
  {
    const WKeySpec &spec = *seq_it;
    
    WKeyMap::KeysymMap::iterator it
      = cur->keysym_map.find(boost::make_tuple(spec.keysym(), spec.modifiers()));

    if (it != cur->keysym_map.end())
    {
      if (boost::next(seq_it) == seq_end
          || !it->is_submap())
      {
        // Key sequence is already mapped
        return false;
      }
    } else
    {
      boost::tie(it, boost::tuples::ignore) = cur->keysym_map.insert(spec);
      it->keycode = *keycode_it;
    }
    
    const WKeyBinding *binding = &*it;
    int clean_modifiers = spec.modifiers()
      & ~key_binding_state->locking_mod_info.mask;
    int keycode = *keycode_it;

    if (spec.modifiers() & key_binding_state->locking_mod_info.mask)
      WARN("Locking modifiers ignored in binding key %d with modifiers 0x%x",
           keycode, spec.modifiers());
    
    if (keycode_mapping_exists)
    {
      WKeyMap::KeycodeMap::iterator it2
        = cur->keycode_map.find(boost::make_tuple(keycode,
                                                  clean_modifiers));
      if (it2 != cur->keycode_map.end())
      {
        if (*it2 != binding)
        {
          WARN("Conflicting keycode mapping for key sequence: %s",
               to_string(seq).c_str());
          
          keycode_mapping_exists = false;
        }
      } else
      {
        binding->keycode = keycode;
        binding->clean_modifiers = clean_modifiers;
        
        /* If this is a top-level keycode mapping that is being added,
           grab the new key combination */
        if (cur == &key_binding_state->kmap)
          grab_key(binding->keycode, binding->clean_modifiers);
        
        cur->keycode_map.insert(binding);
      }
    }

    ++seq_it;
    ++keycode_it;
    if (seq_it == seq_end)
    {
      binding->action = action;
      return true;
    }
    else
    {
      binding->submap.reset(new WKeyMap);
      cur = binding->submap.get();
    }
  } while (1);
}

bool WM::unbind_key(const WKeySequence &seq)
{
  if (seq.empty())
    return false;

  WKeyMap *cur = &key_binding_state->kmap;

  WKeySequence::const_iterator seq_it = seq.begin(), seq_end = seq.end();
  
  typedef WKeyMap::KeysymMap::iterator KeysymMapIterator;
  typedef std::vector<std::pair<WKeyMap *, KeysymMapIterator> > BindingList;
  
  BindingList bindings;

  do
  {
    const WKeySpec &spec = *seq_it;
    
    WKeyMap::KeysymMap::iterator it
      = cur->keysym_map.find(boost::make_tuple(spec.keysym(), spec.modifiers()));

    if (it == cur->keysym_map.end())
      return false; // Key sequence is not mapped

    bindings.push_back(std::make_pair(cur, it));

    ++seq_it;
    if (seq_it == seq_end)
    {
      if (it->is_submap())
        return false; // Incomplete sequence
      else
        break;
    }
  } while (1);

  for (BindingList::iterator it = bindings.end();
       it-- != bindings.begin(); )
  {
    if (it->second->is_submap() && !it->second->submap->empty())
      break;

    if (int keycode = it->second->keycode)
    {
      WKeyMap::KeycodeMap::iterator kc_it
        = it->first->keycode_map.find
        (boost::make_tuple(keycode, it->second->clean_modifiers));
      if (kc_it != it->first->keycode_map.end())
      {
        it->first->keycode_map.erase(kc_it);
        if (it->first == &key_binding_state->kmap)
          ungrab_key(keycode, it->second->modifiers());
      }
    }
    
    it->first->keysym_map.erase(it->second);
  }

  return true;
}

typedef std::pair<WKeySequence, boost::function<void ()> > KeyBindingSpec;
typedef std::vector<KeyBindingSpec> KeyBindingSpecList;

static void keymap_to_binding_list(const WKeyMap &kmap,
                                   const WKeySequence &partial,
                                   KeyBindingSpecList &out)
{
  BOOST_FOREACH (const WKeyBinding &b, kmap.keysym_map)
  {
    WKeySequence seq(partial);
    seq.push_back(b);    
    if (b.is_submap())
      keymap_to_binding_list(*b.submap, seq, out);
    else
      out.push_back(KeyBindingSpec(seq, b.action));
  }
}

void WM::handle_mapping_notify(const XMappingEvent &ev)
{
  /* Get the list of bindings from the keymap tree */
  KeyBindingSpecList bindings;
  keymap_to_binding_list(key_binding_state->kmap, WKeySequence(), bindings);

  /* Remove current bindings */
  std::for_each(bindings.begin(), bindings.end(),
                boost::bind(&WM::unbind_key, this,
                            bind(&KeyBindingSpec::first, _1)));
  
  /* Update locking modifier info */
  key_binding_state->locking_mod_info.update(display());

  /* Remap all bindings */
  std::for_each(bindings.begin(), bindings.end(),
                boost::bind(&WM::bind_key, this,
                            bind(&KeyBindingSpec::first, _1),
                            bind(&KeyBindingSpec::second, _1)));
}

void WM::handle_keypress(const XKeyEvent &ev)
{
  WKeyBindingState &kbs = *key_binding_state;
  WKeyMap &km = kbs.current_kmap ? *kbs.current_kmap : kbs.kmap;

  /* All events for key bindings should be received for the root
     window */
  if (ev.window != root_window())
    return;

  int clean_state = ev.state & ~kbs.locking_mod_info.mask;

  WKeyMap::KeycodeMap::const_iterator it
    = km.keycode_map.find(boost::make_tuple(ev.keycode, clean_state));

  if (it == km.keycode_map.end())
  {
    /* Invalid key sequence */
    reset_current_key_sequence();
    return;
  }

  if ((*it)->is_submap())
  {
    if (key_binding_state->current_kmap)
      evtimer_del(&kbs.kmap_reset_event);
    else
      XGrabKeyboard(display(), root_window(), false,
                    GrabModeAsync, GrabModeAsync, CurrentTime);
    
    struct timeval tv = key_sequence_timeout;
    evtimer_add(&kbs.kmap_reset_event, &tv);

    kbs.current_kmap = (*it)->submap;
  } else
  {
    reset_current_key_sequence();
    (*it)->action();
  }
}

void WM::reset_current_key_sequence()
{
  evtimer_del(&key_binding_state->kmap_reset_event);
  if (key_binding_state->current_kmap)
  {
    XUngrabKeyboard(display(), CurrentTime);
    key_binding_state->current_kmap.reset();
  }
}

/**
 * {{{ String conversion
 */

WKeySpec::WKeySpec(KeySym ksym, unsigned int mods)
  : keysym_(ksym), modifiers_(mods)
{}

WKeySpec::WKeySpec(const ascii_string &str)
{
  std::vector<ascii_string> parts;
  boost::algorithm::split(parts, str, boost::bind(std::equal_to<char>(), _1, '-'));

  if (parts.empty())
    throw std::invalid_argument(str);

  /* Check for an empty part, which indicates an invalid string. */
  BOOST_FOREACH (ascii_string &s, parts)
  {
    if (s.empty())
      throw std::invalid_argument(str);
  }

  ascii_string keysym_name = parts.back();
  parts.pop_back();

  keysym_ = XStringToKeysym(keysym_name.c_str());
  if (keysym_ == NoSymbol)
    throw std::invalid_argument(str);

  modifiers_ = 0;

  if (parts.size() == 1
      && boost::algorithm::iequals(parts.front(), ascii_string("any")))
    modifiers_ = AnyModifier;
  else
  {
    BOOST_FOREACH(ascii_string &s, parts)
    {
      unsigned int current_mask = modname_to_modmask(s);
      if (current_mask == 0 || modifiers_ & current_mask)
        throw std::invalid_argument(str);
      modifiers_ |= current_mask;
    }
  }
}

ascii_string to_string(const WKeySpec &spec)
{
  ascii_string out;

  if (spec.modifiers() == AnyModifier)
    out += "Any-";

  else
  {
    for (int i = 0; i < N_MODS; ++i)
    {
      if (spec.modifiers() & modmasks[i])
      {
        out += modnames[i];
        out += '-';
      }
    }
  }

  out += XKeysymToString(spec.keysym());

  return out;
}

WKeySequence::WKeySequence(const ascii_string &str)
{
  std::vector<ascii_string> parts;
  boost::algorithm::split(parts, str,
                          boost::bind(std::equal_to<char>(), _1, ' '));

  if (parts.empty())
    throw std::invalid_argument(str);

  assign(parts.begin(), parts.end());
}

ascii_string to_string(const WKeySequence &seq)
{
  ascii_string out;

  for (WKeySequence::const_iterator it = seq.begin();
       it != seq.end();
       ++it)
  {
    if (it != seq.begin())
      out += ' ';
    out += to_string(*it);
  }

  return out;
}

/**
 * }}}
 */
