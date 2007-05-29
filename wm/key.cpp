#include <wm/all.hpp>

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
  mutable KeyAction action;
  
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

class WKeyBindingState
{
public:
  
  WKeyMap kmap;

  /* If this is non-NULL, the keyboard is grabbed. */
  boost::shared_ptr<WKeyMap> current_kmap;
  TimerEvent kmap_reset_event;
};

WKeyBindingContext::WKeyBindingContext(WM &wm, const WModifierInfo &mod_info,
                                       Window event_window,
                                       bool grab_required)
  : wm(wm), mod_info(mod_info), event_window(event_window),
    grab_required(grab_required)
{
  state = new WKeyBindingState;

  state->kmap_reset_event.initialize
    (wm.event_service(),
     boost::bind(&WKeyBindingContext::reset_current_key_sequence,
                 this));
}

WKeyBindingContext::~WKeyBindingContext()
{
  delete state;
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
void WModifierInfo::update(Display *dpy)
{
  /* Always assume caps lock is a locking modifier */
  locking_modifiers[2] = LockMask;
  locking_modifiers[0] = 0;
  locking_modifiers[1] = 0;

  locking_mask = LockMask;

  modifier_keycodes.clear();

  const int num_lookup = 2;

  static const KeySym keysyms[num_lookup] = {XK_Num_Lock, XK_Scroll_Lock};

  XModifierKeymap *modmap = XGetModifierMapping(dpy);
  assert(modmap != 0);

  unsigned int keycodes[num_lookup];

  std::transform(keysyms, keysyms + num_lookup, keycodes,
                 boost::bind(XKeysymToKeycode, dpy, _1));

  for (int j=0; j < N_MODS*modmap->max_keypermod; j++)
  {
    if (modmap->modifiermap[j] != 0)
    {
      modifier_keycodes.insert(modmap->modifiermap[j]);
    }
    
    for (int i=0; i < num_lookup; i++)
    {
      if (keycodes[i] == 0)
        continue;
      
      if (modmap->modifiermap[j] == keycodes[i])
      {
        locking_modifiers[i] = modmasks[j/modmap->max_keypermod];
        locking_mask |= locking_modifiers[i];
      }
    }
  }

  XFreeModifiermap(modmap);
}

template <class Function>
static void for_each_mod_combo(const WModifierInfo &info,
                               unsigned int modifiers,
                               const Function &f)
{
  assert((modifiers & info.locking_mask) == 0);

  f(modifiers);

  if (modifiers == AnyModifier)
    return;

  for (int i = 0; i < info.locking_modifier_count; ++i)
  {
    if (info.locking_modifiers[i] == 0)
      continue;
    unsigned int mods = modifiers | info.locking_modifiers[i];
    f(mods);
    
    for (int j = i+1; j < info.locking_modifier_count; ++j)
    {
      if (info.locking_modifiers[j] == 0)
        continue;
      mods |= info.locking_modifiers[j];
      f(mods);

      for (int k = j+1; k < info.locking_modifier_count; ++k)
      {
        if (info.locking_modifiers[k] == 0)
          continue;
        mods |= info.locking_modifiers[k];
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
  for_each_mod_combo(mod_info, modifiers,
                     boost::bind(XGrabKey, display(), keycode, _1,
                                 grab_window, owner_events, pointer_mode,
                                 keyboard_mode));
}

void WM::ungrab_key(int keycode, unsigned int modifiers,
                    Window grab_window)
{
  if (grab_window == None)
    grab_window = root_window();  
  for_each_mod_combo(mod_info, modifiers,
                     boost::bind(XUngrabKey, display(), keycode, _1,
                                 grab_window));
}

/**
 * }}}
 */


typedef std::pair<WKeySequence, KeyAction> KeyBindingSpec;
typedef std::vector<KeyBindingSpec> KeyBindingSpecList;

static void keymap_to_binding_list(const WKeyMap &kmap,
                                   KeyBindingSpecList &out,
                                   const WKeySequence &partial = WKeySequence())
{
  BOOST_FOREACH (const WKeyBinding &b, kmap.keysym_map)
  {
    WKeySequence seq(partial);
    seq.push_back(b);    
    if (b.is_submap())
      keymap_to_binding_list(*b.submap, out, seq);
    else
      out.push_back(KeyBindingSpec(seq, b.action));
  }
}

bool WKeyBindingContext::bind(const WKeySequence &seq,
                              const KeyAction &action)
{
  /* TODO: maybe throw or ERROR here */
  if (seq.empty())
    return false;

  WKeyMap *cur = &state->kmap;

  std::vector<int> keycodes;
  std::transform(seq.begin(), seq.end(),
                 std::back_inserter(keycodes),
                 boost::bind(XKeysymToKeycode,
                             wm.display(),
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

    bool inserted = false;
    
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
      inserted = true;
    }
    
    const WKeyBinding *binding = &*it;
    int clean_modifiers = spec.modifiers()
      & ~mod_info.locking_mask;
    int keycode = *keycode_it;

    if (spec.modifiers() & mod_info.locking_mask)
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
        if (grab_required && cur == &state->kmap)
          wm.grab_key(binding->keycode, binding->clean_modifiers,
                      event_window);
        
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
      if (inserted)
        binding->submap.reset(new WKeyMap);
      cur = binding->submap.get();
    }
  } while (1);
}

bool WKeyBindingContext::unbind(const WKeySequence &seq)
{
  if (seq.empty())
    return false;

  WKeyMap *cur = &state->kmap;

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
        if (grab_required && it->first == &state->kmap)
          wm.ungrab_key(keycode, it->second->modifiers(), event_window);
      }
    }
    
    it->first->keysym_map.erase(it->second);
  }

  return true;
}

static void unbind_all(WKeyBindingContext &c, const KeyBindingSpecList &list)
{
  BOOST_FOREACH (const WKeySequence &seq, boost::make_transform_range(list, select1st))
    c.unbind(seq);
}

static void bind_all(WKeyBindingContext &c, const KeyBindingSpecList &list)
{
  BOOST_FOREACH (const KeyBindingSpec &spec, list)
    c.bind(spec.first, spec.second);
}

void WM::handle_mapping_notify(const XMappingEvent &ev)
{
  {
    XMappingEvent ev_tmp = ev;
    XRefreshKeyboardMapping(&ev_tmp);
  }
  
  /* Get the list of bindings from the keymap tree */
  KeyBindingSpecList global_bindings;
  keymap_to_binding_list(global_bindctx.state->kmap, global_bindings);

  KeyBindingSpecList menu_bindings;
  keymap_to_binding_list(menu.bindctx.state->kmap, menu_bindings);

  /* Remove current bindings */
  unbind_all(global_bindctx, global_bindings);
  unbind_all(menu.bindctx, menu_bindings);

  /* Update locking modifier info */
  mod_info.update(display());

  /* Remap all bindings */
  bind_all(global_bindctx, global_bindings);
  bind_all(menu.bindctx, menu_bindings);
}

void WM::handle_keypress(const XKeyEvent &ev)
{
  /* All events for key bindings should be received for the root
     window */
  // The global bindings apply to all key presses
  if (global_bindctx.process_keypress(ev))
    return;
  
  if (ev.window == menu.xwin())
    menu.handle_keypress(ev);
}


bool WKeyBindingContext::process_keypress(const XKeyEvent &ev)
{
  if (mod_info.modifier_keycodes.count(ev.keycode))
    return false;
  
  WKeyMap &km = state->current_kmap ? *state->current_kmap : state->kmap;

  int clean_state = ev.state & ~mod_info.locking_mask;

  WKeyMap::KeycodeMap::const_iterator it
    = km.keycode_map.find(boost::make_tuple(ev.keycode, clean_state));

  if (it == km.keycode_map.end())
    it = km.keycode_map.find(boost::make_tuple(ev.keycode, AnyModifier));

  if (it == km.keycode_map.end())
  {
    if (state->current_kmap)
    {
      /* Invalid key sequence */
      reset_current_key_sequence();
      return true;
    } else
      return false;
  }

  if ((*it)->is_submap())
  {
    if (state->current_kmap)
      state->kmap_reset_event.cancel();
    else if (grab_required)
      XGrabKeyboard(wm.display(), event_window, false,
                    GrabModeAsync, GrabModeAsync, CurrentTime);

    state->kmap_reset_event.wait_for(wm.key_sequence_timeout.tv_sec,
                                     wm.key_sequence_timeout.tv_usec);

    state->current_kmap = (*it)->submap;
  } else
  {
    reset_current_key_sequence();
    (*it)->action(wm);
  }

  return true;
}

void WKeyBindingContext::reset_current_key_sequence()
{
  state->kmap_reset_event.cancel();
  if (state->current_kmap)
  {
    if (grab_required)
      XUngrabKeyboard(wm.display(), CurrentTime);
    state->current_kmap.reset();
  }
}

/**
 * {{{ String conversion
 */

WKeySpec::WKeySpec(KeySym ksym, unsigned int mods)
  : keysym_(ksym), modifiers_(mods)
{}

void WKeySpec::initialize(const ascii_string &str)
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

WKeySpec::WKeySpec(const ascii_string &str)
{
  initialize(str);
}

WKeySpec::WKeySpec(const char *str)
{
  initialize(str);
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

void WKeySequence::initialize(const ascii_string &str)
{
  std::vector<ascii_string> parts;
  boost::algorithm::split(parts, str,
                          boost::bind(std::equal_to<char>(), _1, ' '));

  if (parts.empty())
    throw std::invalid_argument(str);

  assign(parts.begin(), parts.end());  
}

WKeySequence::WKeySequence(const ascii_string &str)
{
  initialize(str);
}

WKeySequence::WKeySequence(const char *str)
{
  initialize(str);
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
