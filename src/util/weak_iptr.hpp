#ifndef _UTIL_WEAK_IPTR_HPP
#define _UTIL_WEAK_IPTR_HPP

#include <boost/utility.hpp>

template <class T>
class weak_iptr
{
  class info
  {
  public:
    unsigned int count;
    bool valid;
    info()
      : count(1), valid(true)
    {}
  };
  
  static void release_info(info *i)
  {
    if (--i->count == 0)
      delete i;
  }
  
public:
  class base : boost::noncopyable
  {
  private:
    info *weak_iptr_info_;
    friend class weak_iptr<T>;
  public:
    base() : weak_iptr_info_(new info)
    {}
    ~base()
    {
      weak_iptr_info_->valid = false;
      release_info(weak_iptr_info_);
    }
  };
  
private:
  T *ptr_;
  info *info_;
  
public:
  weak_iptr()
    : ptr_(0)
  {}
  
  weak_iptr(T *p)
    : ptr_(p)
  {
    if (ptr_)
    {
      info_ = static_cast<base *>(ptr_)->weak_iptr_info_;
      info_->count++;
    }
  }

  weak_iptr(const weak_iptr<T> &p)
    : ptr_(p.ptr_)
  {
    if (ptr_)
    {
      info_ = p.info_;
      info_->count++;
    }
  }

  void reset(T *p)
  {
    if (ptr_)
      release_info(info_);
    ptr_ = p;
    if (ptr_)
    {
      info_ = static_cast<base *>(ptr_)->weak_iptr_info_;
      info_->count++;
    }
  }

  weak_iptr<T> &operator=(const weak_iptr<T> &p)
  {
    reset(p);
  }

  ~weak_iptr()
  {
    if (ptr_)
      release_info(info_);
  }

  T *get() const
  {
    if (ptr_ && info_->valid)
      return ptr_;
    return 0;
  }
};

#endif /* _UTIL_WEAK_IPTR_HPP */
