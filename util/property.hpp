#ifndef _UTIL_PROPERTY_HPP
#define _UTIL_PROPERTY_HPP

#include <boost/any.hpp>
#include <string>
#include <map>

template <class T>
class Property;

template <class T>
class ConstProperty;

class PropertyContainer
{
private:
  typedef std::map<std::string, boost::any> PropContainerType;
  
  template <class T>
  friend class Property;

  template <class T>
  friend class ConstProperty;
  
  PropContainerType properties_;
  
public:
  
  template <class T>
  Property<T> property(const std::string &name)
  {
    return Property<T>(*this, name);
  }

  template <class T>
  ConstProperty<T> property(const std::string &name) const
  {
    return ConstProperty<T>(*this, name);
  }
};

template <class T>
class Property
{
  friend class PropertyContainer;
  PropertyContainer &container;
  typedef bool (Property::*unspecified_bool_type)() const;
  std::string name;
public:
  Property(PropertyContainer &container, const std::string &name)
    : container(container), name(name)
  {}

  T &operator=(const T &x) const
  {
    container.properties_[name] = x;
    return get();
  }

  void remove() const
  {
    container.properties_.erase(name);
  }

  T &operator*() const
  {
    return get();
  }

  T *operator->() const
  {
    PropertyContainer::PropContainerType::iterator it = container.properties_.find(name);
    if (it == container.properties_.end())
      return 0;
    return boost::any_cast<T>(&it->second);
  }

  T &get() const
  {
    return *this->operator->();
  }

  bool empty() const
  {
    PropertyContainer::PropContainerType::const_iterator it = container.properties_.find(name);
    if (it == container.properties_.end())
      return true;
    return (it->second.type() != typeid(T));
  }
    
  operator unspecified_bool_type() const
  {
    if (empty())
      return 0;
    else
      return &Property<T>::empty;
  }

  bool operator!() const
  {
    return empty();
  }
};

template <class T>
class ConstProperty
{
  friend class PropertyContainer;
  const PropertyContainer &container;
  typedef bool (ConstProperty::*unspecified_bool_type)() const;
  std::string name;
public:
  ConstProperty(const PropertyContainer &container, const std::string &name)
    : container(container), name(name)
  {}
    
  const T &operator*() const
  {
    return get();
  }

  const T *operator->() const
  {
    PropertyContainer::PropContainerType::const_iterator it = container.properties_.find(name);
    if (it == container.properties_.end())
      return 0;
    return boost::any_cast<T>(&it->second);
  }

  const T &get() const
  {
    return *this->operator->();
  }

  bool empty() const
  {
    PropertyContainer::PropContainerType::const_iterator it = container.properties_.find(name);
    if (it == container.properties_.end())
      return false;
    return (it->second.type() == typeid(T));
  }
    
  operator unspecified_bool_type() const
  {
    if (empty())
      return 0;
    else
      return &Property<T>::empty;
  }

  bool operator!() const
  {
    return !empty();
  }
};

#define PROPERTY_ACCESSOR(Container, T, name)         \
  inline Property<T> name(Container *c) { return c->property<T>(#name); } \
  inline Property<T> name(Container &c) { return c.property<T>(#name); } \
  inline ConstProperty<T> name(const Container *c) { return c->property<T>(#name); } \
  inline ConstProperty<T> name(const Container &c) { return c.property<T>(#name); }

#endif /* _UTIL_PROPERTY_HPP */
