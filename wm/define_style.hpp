#ifndef _WM_DEFINE_STYLE_HPP
#define _WM_DEFINE_STYLE_HPP

#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/list.hpp>
#include <boost/preprocessor/tuple.hpp>

#define WM_DEFINE_STYLE_TYPE_HELPER1(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(2, 0, elem) BOOST_PP_TUPLE_ELEM(2, 1, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER2(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(3, 1, elem) BOOST_PP_TUPLE_ELEM(3, 2, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER3(r, _, elem) \
  BOOST_PP_TUPLE_ELEM(3, 0, elem) BOOST_PP_TUPLE_ELEM(3, 2, elem);

#define WM_DEFINE_STYLE_TYPE_HELPER4(r, _, elem)                  \
  BOOST_PP_TUPLE_ELEM(3, 2, elem)                           \
    (dc_, spec_.BOOST_PP_TUPLE_ELEM(3, 2, elem))            \

#define WM_DEFINE_STYLE_TYPE_HELPER5(r, _, elem)      \
  BOOST_PP_TUPLE_ELEM(2, 1, elem)               \
    (spec_.BOOST_PP_TUPLE_ELEM(2, 1, elem))

#define WM_DEFINE_STYLE_TYPE_HELPER6(seq)                                     \
  BOOST_PP_LIST_REST(BOOST_PP_TUPLE_TO_LIST                                   \
                     BOOST_PP_SEQ_TO_ARRAY(seq))

#define WM_DEFINE_STYLE_TYPE(name, features1, features2) \
  class name                                         \
  {                                                      \
  public:                                                \
    class Spec                                                          \
    {                                                                   \
    public:                                                             \
      BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER2, _,           \
                             WM_DEFINE_STYLE_TYPE_HELPER6(features1))   \
      BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER1, _,           \
                             WM_DEFINE_STYLE_TYPE_HELPER6(features2))   \
    };                                                                  \
    BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER3, _,                       \
                           WM_DEFINE_STYLE_TYPE_HELPER6(features1))               \
    BOOST_PP_LIST_FOR_EACH(WM_DEFINE_STYLE_TYPE_HELPER1, _,                        \
                           WM_DEFINE_STYLE_TYPE_HELPER6(features2))                \
    name (WDrawContext &dc_, const Spec &spec_)                              \
      : BOOST_PP_LIST_ENUM                                                      \
      (BOOST_PP_LIST_APPEND                                                  \
       (BOOST_PP_LIST_TRANSFORM(WM_DEFINE_STYLE_TYPE_HELPER4, _,             \
                                WM_DEFINE_STYLE_TYPE_HELPER6(features1)),     \
        BOOST_PP_LIST_TRANSFORM(WM_DEFINE_STYLE_TYPE_HELPER5, _,             \
                                WM_DEFINE_STYLE_TYPE_HELPER6(features2))))   \
    {}                                                                        \
  };

#endif /* _WM_DEFINE_STYLE_HPP */

