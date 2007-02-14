#ifndef _UTIL_TIME_HPP
#define _UTIL_TIME_HPP
#include <boost/cstdint.hpp>
#include <boost/operators.hpp>
#include <sys/time.h>

class time_duration : public boost::less_than_comparable<time_duration>,
                      public boost::equality_comparable<time_duration>,
                      public boost::addable<time_duration>,
                      public boost::subtractable<time_duration>,
                      public boost::multipliable<time_duration, long>,
                      public boost::dividable<time_duration, long>
{
public:
  typedef boost::int_fast64_t value_type;
private:
  value_type value;
public:
  time_duration()
    : value(0)
  {}

  explicit time_duration(value_type v)
    : value(v)
  {}

  explicit time_duration(timeval tv)
    : value(value_type(tv.tv_sec) * value_type(1000000) + value_type(tv.tv_usec))
  {}

  value_type total_microseconds() const
  {
    return value;
  }

  value_type total_milliseconds() const
  {
    return value / value_type(1000);
  }

  value_type total_seconds() const
  {
    return value / value_type(1000000);
  }

  value_type total_minutes() const
  {
    return value / value_type(60000000);
  }

  value_type total_hours() const
  {
    return value / value_type(3600000000ll);
  }

  long microseconds() const
  {
    return value % 1000000;
  }

  long seconds() const
  {
    return total_seconds() % 60;
  }

  long minutes() const
  {
    return total_minutes() % 60;
  }

  static time_duration microseconds(value_type x)
  {
    return time_duration(x);
  }

  static time_duration milliseconds(value_type x)
  {
    return time_duration(x * value_type(1000));
  }

  static time_duration seconds(value_type x)
  {
    return time_duration(x * value_type(1000000));
  }

  static time_duration minutes(value_type x)
  {
    return time_duration(x * value_type(60000000));
  }

  static time_duration hours(value_type x)
  {
    return time_duration(x * value_type(3600000000ll));
  }

  bool operator==(const time_duration &x) const
  {
    return value == x.value;
  }

  bool operator<(const time_duration &x) const
  {
    return value < x.value;
  }

  time_duration &operator+=(const time_duration &x)
  {
    value += x.value;
    return *this;
  }

  time_duration &operator-=(const time_duration &x)
  {
    value -= x.value;
    return *this;
  }

  time_duration operator-() const
  {
    return time_duration(-value);
  }

  time_duration &operator*=(long x)
  {
    value *= x;
    return *this;
  }

  time_duration &operator/=(long x)
  {
    value /= x;
    return *this;
  }
};

inline timeval to_timeval(const time_duration &x)
{
  timeval tv;
  tv.tv_sec = x.total_seconds();
  tv.tv_usec = x.microseconds();
  return tv;
}

class time_point : public boost::less_than_comparable<time_point>,
                   public boost::equality_comparable<time_point>,
                   public boost::addable<time_point, time_duration>
{
public:
  // This is the duration since the epoch.
  time_duration value;
public:
  time_point() {}
  
  time_point(time_duration v)
    : value(v)
  {}

  static time_point current()
  {
    // Linux-specific
    timeval tv;
    gettimeofday(&tv, 0);
    return time_point(time_duration(tv));
  }

  time_duration duration_since_epcoh() const
  {
    return value;
  }

  bool operator==(const time_point &x) const
  {
    return value == x.value;
  }

  bool operator<(const time_point &x) const
  {
    return value < x.value;
  }  

  time_duration operator-(const time_point &x) const
  {
    return value - x.value;
  }

  time_point &operator+=(const time_duration &x)
  {
    value += x;
    return *this;
  }

  time_point &operator-=(const time_duration &x)
  {
    value -= x;
    return *this;
  }
};

#endif /* _UTIL_TIME_HPP */
