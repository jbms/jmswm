
#include <style/db.hpp>

#include <map>
#include <vector>
#include <stack>
#include <boost/variant.hpp>

#include <util/string.hpp>

#include <boost/spirit/include/classic_file_iterator.hpp>
#include <boost/spirit/include/classic_position_iterator.hpp>

#include <boost/utility.hpp>

#include <style/scanner.hpp>

namespace style
{

  class StyleSpecData;

  typedef boost::variant<int,
                         ascii_string,
                         boost::recursive_wrapper<StyleSpecData> >
    StyleEntryValue;
  class StyleSpecData
  {
  public:
    StyleSpecData()
      : context(0), parent(0)
    {}
    StyleSpecData(const StyleSpecData *context, const StyleSpecData *parent)
      : context(context), parent(parent)
    {}
    const StyleSpecData *context;
    const StyleSpecData *parent;
    typedef std::map<ascii_string, StyleEntryValue> PropertyMap;
    PropertyMap properties;
  };

  class DBState
  {
  public:

    StyleSpecData root;
    void load(const boost::filesystem::path &path);

    class Path : public std::vector<ascii_string>
    {
    public:
      bool absolute;
    };

    const StyleEntryValue *lookup(const StyleSpecData *context, const ascii_string &identifier) const;
    const StyleEntryValue *lookup(const StyleSpecData *context, const Path &path) const;
  };

  const StyleEntryValue *DBState::lookup(const StyleSpecData *context,
                                         const ascii_string &identifier) const
  {
    if (!context)
      context = &root;

    do
    {
      StyleSpecData::PropertyMap::const_iterator it = context->properties.find(identifier);
      if (it != context->properties.end())
        return &it->second;
      context = context->parent;
    } while (context);
    return 0;
  }

  const StyleEntryValue *DBState::lookup(const StyleSpecData *context,
                                         const DBState::Path &path) const
  {
    if (path.empty())
      return 0;

    if (!context || path.absolute)
      context = &root;

    const StyleEntryValue *entry;
    do
    {
      if ((entry = lookup(context, path.front())))
        break;
      context = context->context;
      if (!context)
        return 0;
    } while (1);

    for (Path::const_iterator it = boost::next(path.begin()); it != path.end(); ++it)
    {
      try
      {
        const StyleSpecData &spec = boost::get<StyleSpecData>(*entry);
        entry = lookup(&spec, *it);
      } catch (boost::bad_get &)
      {
        return 0;
      }
    }
    return entry;
  }

  class Parser
  {
    typedef boost::spirit::classic::position_iterator2<boost::spirit::classic::file_iterator<> > iterator;
    typedef Scanner<iterator> ScannerT;
  public:
    typedef ScannerT scanner_type;
  private:
    DBState &db_state;
    ScannerT scanner;
    std::stack<StyleSpecData *> context;
    int include_depth;
    boost::filesystem::path path;
  public:
    class FileOpenError : public std::exception
    {
      std::string filename_;
    public:
      FileOpenError(const std::string &filename_)
        : filename_(filename_)
      {}
      virtual ~FileOpenError() throw () {}
      const std::string &filename() const { return filename_; }
      virtual const char *what() const throw() { return "FileOpenError"; }
    };
  private:
    static const int max_include_depth = 255;

    static iterator get_begin_iterator(const std::string &filename)
    {
      boost::spirit::classic::file_iterator<> file_begin(filename);
      if (!file_begin)
        throw FileOpenError(filename);
      return iterator(file_begin, file_begin.make_end(), filename);
    }

  public:
    class Error : public std::exception
    {
      std::string message_;
      iterator error_pos;
    public:
      Error(const std::string &message_, iterator error_pos)
        : message_(message_), error_pos(error_pos)
      {}
      virtual ~Error() throw () {}
      iterator position() const { return error_pos; }
      const std::string &message() const { return message_; }
      virtual const char *what() const throw() { return "Parser::Error"; }
    };

    Parser(DBState &db_state,
           const std::string &filename,
           StyleSpecData *initial_context,
           int include_depth)
      : db_state(db_state),
        scanner(get_begin_iterator(filename), iterator()),
        include_depth(include_depth),
        path(filename)
    {
      context.push(initial_context);
    }

    void parse();
  private:
    void expect(ScannerT::token_type type)
    {
      if (scanner.current_token() != type)
        unexpected_token();
    }
    void unexpected_token()
    {
      if (scanner.current_token() == ScannerT::EOF_TOKEN)
        throw Error("unexpected end-of-file", scanner.token_start());
      else
        throw Error("unexpected token", scanner.token_start());
    }

    void parse_include();
    void parse_definition();
    void parse_path(DBState::Path &p);
  };

  void Parser::parse()
  {
    while (scanner.current_token() != ScannerT::EOF_TOKEN)
    {
      switch (scanner.current_token())
      {
      case ScannerT::POUND:
        scanner.next_token();
        parse_include();
        break;
      case ScannerT::IDENTIFIER:
        parse_definition();
        break;
      case ScannerT::RBRACE:
        if (context.size() > 1)
        {
          scanner.next_token();
          context.pop();
          break;
        }
        else
          unexpected_token();
      default:
        unexpected_token();
      }
    }
    if (context.size() > 1)
      unexpected_token();
  }

  void Parser::parse_include()
  {
    iterator include_pos = scanner.token_start();

    expect(ScannerT::IDENTIFIER);
    if (scanner.text_data() != "include")
      unexpected_token();
    scanner.next_token();
    expect(ScannerT::STRING);
    std::string include_filename = scanner.text_data();
    scanner.next_token();
    expect(ScannerT::SEMICOLON);

    if (include_depth == max_include_depth)
      throw Error("maximum include depth exceeded", include_pos);

    boost::filesystem::path include_path = path / include_filename;

    {
      Parser parser(db_state, include_path.string(), context.top(), include_depth + 1);
      parser.parse();
    }
  }

  void Parser::parse_path(DBState::Path &p)
  {
    p.clear();
    if (scanner.current_token() == ScannerT::DOT)
    {
      p.absolute = true;
      scanner.next_token();
    } else
      p.absolute = false;
    while (1)
    {
      expect(ScannerT::IDENTIFIER);
      p.push_back(scanner.text_data());
      scanner.next_token();
      if (scanner.current_token() != ScannerT::DOT)
        break;
      scanner.next_token();
    }
  }

  void Parser::parse_definition()
  {
    std::string name = scanner.text_data();

    if (context.top()->properties.count(name))
      throw Error("duplicate definition", scanner.token_start());

    scanner.next_token();

    StyleEntryValue value;
    const StyleSpecData *parent = 0;
    bool style_spec = false;

    switch (scanner.current_token())
    {
    case ScannerT::EQUALS:
      scanner.next_token();
      if (scanner.current_token() == ScannerT::INTEGER)
      {
        value = scanner.number_data();
        scanner.next_token();
      }
      else if (scanner.current_token() == ScannerT::STRING)
      {
        value = scanner.text_data();
        scanner.next_token();
      }
      else
      {
        DBState::Path p;
        iterator path_start = scanner.token_start();
        parse_path(p);
        if (const StyleEntryValue *value_ptr = db_state.lookup(context.top(), p))
          value = *value_ptr;
        else
          throw Error("invalid path", path_start);
      }
      expect(ScannerT::SEMICOLON);
      scanner.next_token();
      break;
    case ScannerT::COLON:
      {
        scanner.next_token();
        DBState::Path p;
        iterator path_start = scanner.token_start();
        parse_path(p);
        if (const StyleEntryValue *value_ptr = db_state.lookup(context.top(), p))
        {
          const StyleSpecData *spec_data = boost::get<const StyleSpecData>(value_ptr);
          if (spec_data)
          {
            parent = spec_data;
          } else
            throw Error("invalid path", path_start);
        } else
          throw Error("invalid path", path_start);
      }
    default:
      expect(ScannerT::LBRACE);
      value = StyleSpecData(context.top(), parent);
      scanner.next_token();
      style_spec = true;
    }
    std::pair<StyleSpecData::PropertyMap::iterator, bool> pair
      = context.top()->properties.insert(std::make_pair(name, value));
    if (style_spec)
      context.push(boost::get<StyleSpecData>(&pair.first->second));
  }

  void DBState::load(const boost::filesystem::path &path)
  {
    Parser p(*this, path.string(), &root, 0);
    p.parse();
  }

  template<>
  int Spec::get<int>(const ascii_string &identifier) const
  {
    const StyleEntryValue *entry_ptr = db_state.lookup(data, identifier);
    if (!entry_ptr)
      throw PropertyAccessError("undefined property", identifier);
    const int *value_ptr = boost::get<int>(entry_ptr);
    if (!value_ptr)
      throw PropertyAccessError("invalid type", identifier);
    return *value_ptr;
  }

  template<>
  ascii_string Spec::get<ascii_string>(const ascii_string &identifier) const
  {
    const StyleEntryValue *entry_ptr = db_state.lookup(data, identifier);
    if (!entry_ptr)
      throw PropertyAccessError("undefined property", identifier);
    const ascii_string *value_ptr = boost::get<const ascii_string>(entry_ptr);
    if (!value_ptr)
      throw PropertyAccessError("invalid type", identifier);
    return *value_ptr;
  }

  template<>
  Spec Spec::get<Spec>(const ascii_string &identifier) const
  {
    const StyleEntryValue *entry_ptr = db_state.lookup(data, identifier);
    if (!entry_ptr)
      throw PropertyAccessError("undefined property", identifier);
    const StyleSpecData *data_ptr = boost::get<StyleSpecData>(entry_ptr);
    if (!data_ptr)
      throw PropertyAccessError("invalid type", identifier);
    return Spec(db_state, data_ptr);
  }

  DB::DB()
    : state(new DBState)
  {}

  DB::~DB() {}

  void DB::load(const boost::filesystem::path &path)
  {
    try
    {
      state->load(path);
    } catch (Parser::FileOpenError &e)
    {
      throw LoadError("error opening file", e.filename());
    } catch (Parser::Error &e)
    {
      throw LoadError(e.message(), e.position().get_position().file,
                      e.position().get_currentline(),
                      e.position().get_position().line,
                      e.position().get_position().column);
    } catch (Parser::scanner_type::Error &e)
    {
      throw LoadError("scanner error", e.position().get_position().file,
                      e.position().get_currentline(),
                      e.position().get_position().line,
                      e.position().get_position().column);
    }
  }

  Spec DB::get(const ascii_string &identifier) const
  {
    return Spec(*state, 0).get<Spec>(identifier);
  }

} // namespace style
