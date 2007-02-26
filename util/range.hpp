#ifndef _UTIL_RANGE_HPP
#define _UTIL_RANGE_HPP

#include <boost/foreach.hpp>
#include <boost/range.hpp>
#include <boost/range_ex/algorithm.hpp>
#include <boost/range_ex/filter.hpp>
#include <boost/range_ex/indirect.hpp>
#include <boost/range_ex/reverse.hpp>
#include <boost/range_ex/transform.hpp>

#include <boost/type_traits/add_const.hpp>
#include <boost/type_traits/add_reference.hpp>

// Disable warnings about unused select1st and select2nd
#if defined __GNUC__
#pragma GCC system_header
#endif

static struct select1st_type
{

  template <class T>
  struct result;

  /*

  template <class T>
  struct result<select1st_type(T)>
    : boost::add_reference<typename T::first_type> {};

  */

  template <class T>
  struct result<select1st_type(T &)>
    : boost::add_reference<typename T::first_type> {};

  template <class T>
  struct result<select1st_type(T const &)>
    : boost::add_reference<typename boost::add_const<typename T::first_type>::type> {};

  template <class T>
  struct result<select1st_type(T const)>
    : boost::add_reference<typename boost::add_const<typename T::first_type>::type> {};

  template <class T>
  typename boost::add_reference<typename T::first_type>::type
  operator()(T &x) const
  {
    return x.first;
  }
  
  template <class T>
  typename boost::add_reference<typename boost::add_const<typename T::first_type>::type>::type
  operator()(const T &x) const
  {
    return x.first;
  }
} select1st;


static struct select2nd_type
{

  template <class T>
  struct result;

  template <class T>
  struct result<select2nd_type(T &)>
    : boost::add_reference<typename T::second_type> {};

  /*
  template <class T>
  struct result<select2nd_type(T)>
    : boost::add_reference<typename T::second_type> {};
  */

  template <class T>
  struct result<select2nd_type(T const &)>
    : boost::add_reference<typename boost::add_const<typename T::second_type>::type> {};

  /*
  template <class T>
  struct result<select2nd_type(T const)>
    : boost::add_reference<typename boost::add_const<typename T::second_type>::type> {};
  */
  
  template <class T>
  typename boost::add_reference<typename T::second_type>::type
  operator()(T &x) const
  {
    return x.second;
  }

  template <class T>
  typename boost::add_reference<typename boost::add_const<typename T::second_type>::type>::type
  operator()(const T &x) const
  {
    return x.second;
  }
} select2nd;

#endif /* _UTIL_RANGE_HPP */
