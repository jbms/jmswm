#ifndef _STYLE_DB_HPP
#define _STYLE_DB_HPP

#include <util/string.hpp>
#include <exception>
#include <memory>
#include <boost/filesystem/path.hpp>

namespace style
{
  class DBState;
  class StyleSpecData;

  class LoadError : public std::exception
  {
    std::string filename_;
    std::string message_;
    std::string line_;
    int line_number_, column_number_;
  public:
    LoadError(std::string message_, std::string filename_,
              std::string line_ = std::string(),
              int line_number_ = 0, int column_number_ = 0)
      : filename_(filename_), message_(message_),
        line_(line_),
        line_number_(line_number_), column_number_(column_number_)
    {}
    virtual ~LoadError() throw () {}
    const std::string &filename() const { return filename_; }
    const std::string &message() const { return message_; }
    const std::string &line() const { return line_; }
    int line_number () const { return line_number_; }
    int column_number() const { return column_number_; }
    virtual const char *what() const throw() { return "style::LoadError"; }
  };

  class PropertyAccessError : public std::exception
  {
    std::string message_, identifier_;
  public:
    PropertyAccessError(const std::string &message_,
                        const std::string &identifier_)
      : message_(message_), identifier_(identifier_)
    {}
    virtual const char *what() const throw() { return "style::PropertyAccessError"; }
    virtual ~PropertyAccessError() throw () {}
    const std::string &message() const { return message_; }
    const std::string &identifier() const { return identifier_; }
  };

  class DB;

  class Spec
  {
    friend class DB;
    const DBState &db_state;
    const StyleSpecData *data;
    Spec(const DBState &db_state, const StyleSpecData *data)
      : db_state(db_state), data(data)
    {}
  public:
    template<class T>
    T get(const ascii_string &identifier) const;
  };


  class DB
  {
    std::auto_ptr<DBState> state;

  public:
    DB();
    ~DB();

    void load(const boost::filesystem::path &path);

    Spec get(const ascii_string &identifier) const;

    Spec operator[](const ascii_string &identifier) const { return get(identifier); }
  };

} // namespace style

#endif /* _STYLE_DB_HPP */
