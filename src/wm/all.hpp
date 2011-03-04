#ifndef _WM_ALL_HPP
#define _WM_ALL_HPP

// C library headers
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

// C++ standard library headers
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <vector>
#include <functional>
#include <stdexcept>


// Boost headers
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/archive_exception.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/algorithm/string.hpp>

#include <boost/bind.hpp>
#include <boost/bind/protect.hpp>

#include <boost/utility.hpp>


#include <util/range.hpp>


#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>

#include <boost/shared_ptr.hpp>


// X headers
#include <X11/Xatom.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrandr.h>

// Library headers
#include <util/log.hpp>
#include <util/close_on_exec.hpp>

// WM headers
#include <wm/wm.hpp>
#include <wm/xwindow.hpp>

#endif /* _WM_ALL_HPP */
