#ifndef _STYLE_SCANNER_HPP
#define _STYLE_SCANNER_HPP

#include <string>

#include <exception>

namespace style
{

  template <class Iterator>
  class Scanner
  {
  public:

    typedef enum { POUND, STRING, INTEGER, COLON, LBRACE, RBRACE,
                   SEMICOLON, IDENTIFIER, DOT, EQUALS, EOF_TOKEN } token_type;

    class Error : public std::exception
    {
      Iterator error_pos;
    public:
      Error(Iterator error_pos)
        : error_pos(error_pos)
      {}
      Iterator position() const { return error_pos; }
      virtual const char *what() const throw() { return "Scanner::Error"; }
      virtual ~Error() throw() {}
    };
    
  private:
    token_type current_token_;
    Iterator token_start_;
    std::string text_data_;
    int number_data_;
    char current_char;
    Iterator start, end;
    void error();
    bool at_end()
    {
      return start == end;
    }
    void set_current_char()
    {
      if (start == end)
        return;
      current_char = *start;
    }
    void next_char()
    {
      ++start;
      set_current_char();
    }
    bool skip();
    void read_escape();
    void read_hex_escape();
    void read_octal_escape();
    void read_number();
    void read_decimal_number();
    void read_octal_number();
    void read_hex_number();
  public:
    Scanner(Iterator start, Iterator end);
    token_type current_token() const { return current_token_; }
    Iterator token_start() const { return token_start_; }
    const std::string &text_data() const
    {
      return text_data_;
    }

    int number_data() const
    {
      return number_data_;
    }

    void next_token();
  };

  template <class Iterator>
  Scanner<Iterator>::Scanner(Iterator start, Iterator end)
    : start(start), end(end)
  {
    set_current_char();
    next_token();
  }

  template <class Iterator>
  void Scanner<Iterator>::error()
  {
    throw Error(start);
  }

  template <class Iterator>
  bool Scanner<Iterator>::skip()
  {
    while (1)
    {
      if (at_end())
        return false;
      switch (current_char)
      {
      case '/':
        next_char();
        if (at_end())
          error();
        if (current_char == '*')
        {
          bool has_star = false;
          while (1)
          {
            next_char();
            if (at_end())
              error();
            if (current_char == '*')
              has_star = true;
            else if (has_star && current_char == '/')
              break;
            else
              has_star = false;
          }
        } else if (current_char == '/')
        {
          while (1)
          {
            next_char();
            if (at_end())
              return false;
            if (current_char == '\n')
              break;
          }
        } else
          error();
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        next_char();
        break;
      default:
        return true;
      }
    }
  }

  static int digit_value(char c, int base)
  {
    int value;
    if (c >= '0' && c <= '9')
      value = c - '0';
    else if (c >= 'a' && c <= 'z')
      value = 10 + c - 'a';
    else if (c >= 'A' && c <= 'Z')
      value = 10 + c - 'A';
    else
      return -1;
    if (value >= base)
      return -1;
    return value;
  }

  static bool is_identifier_start(char c)
  {
    return (c >= 'a' && c <= 'z')
      || (c >= 'A' && c <= 'Z')
      || (c == '_');
  }

  static bool is_identifier(char c)
  {
    return is_identifier_start(c)
      || (c >= '0' && c <= '9');
  }

  template <class Iterator>
  void Scanner<Iterator>::read_number()
  {
    bool negative = false;
    int number = 0;
    int base = 10;
    bool at_least_one = false;
    if (current_char == '-')
    {
      negative = true;
      next_char();
      if (at_end())
        error();
    }
    if (current_char == '0')
    {
      next_char();
      if (!at_end() && current_char == 'x')
      {
        next_char();
        base = 16;
      } else
      {
        base = 8;
        at_least_one = true;
      }
    }
    int value;
    while (!at_end() && (value = digit_value(current_char, base)) != -1)
    {
      number = number * base + value;
      next_char();
      at_least_one = true;
    }
    if (!at_least_one)
      error();
    if (negative)
      number_data_ = -number;
    else
      number_data_ = number;
    current_token_ = INTEGER;
  }

  template <class Iterator>
  void Scanner<Iterator>::read_escape()
  {
    if (at_end())
      error();
    switch (current_char)
    {
    case '\\':
    case '"':
    case '\'':
    case '?':
      text_data_ += current_char;
      next_char();
      return;
    case 'a':
      text_data_ += '\a';
      next_char();
      return;
    case 'b':
      text_data_ += '\b';
      next_char();
      return;
    case 'f':
      text_data_ += '\f';
      next_char();
      return;
    case 'n':
      text_data_ += '\n';
      next_char();
      return;
    case 'r':
      text_data_ += '\r';
      next_char();
      return;
    case 't':
      text_data_ += '\t';
      next_char();
      return;
    case 'v':
      text_data_ += '\v';
      next_char();
      return;
    case 'x':
      next_char();
      read_hex_escape();
      return;
    default:
      read_octal_escape();
      return;
    }
  }

  template <class Iterator>
  void Scanner<Iterator>::read_hex_escape()
  {
    int number = 0;
    if (at_end())
      error();
    bool at_least_one = false;
    int value;
    while ((value = digit_value(current_char, 16)) != -1)
    {
      number = number * 16 + value;
      if (number > 255)
        error();
      next_char();
      at_least_one = true;
    }
    if (!at_least_one)
      error();
    text_data_ += (char)((unsigned char)number);
  }

  template <class Iterator>
  void Scanner<Iterator>::read_octal_escape()
  {
    int number = 0;
    if (at_end())
      error();
    int digit_count = 0;
    int value;
    while ((value = digit_value(current_char, 8)) != -1)
    {
      number = number * 8 + value;
      if (number > 255)
        error();
      next_char();
      if (++digit_count == 3)
        break;
    }
    if (digit_count == 0)
      error();
    text_data_ += (char)((unsigned char)number);
  }  
  
  template <class Iterator>
  void Scanner<Iterator>::next_token()
  {
    token_start_ = start;
    if (!skip())
    {
      current_token_ = EOF_TOKEN;
      return;
    }
    
    switch (current_char)
    {
    case '.':
      current_token_ = DOT;
      next_char();
      return;
    case ':':
      current_token_ = COLON;
      next_char();
      return;
    case '{':
      current_token_ = LBRACE;
      next_char();      
      return;
    case '}':
      current_token_ = RBRACE;
      next_char();
      return;
    case ';':
      current_token_ = SEMICOLON;
      next_char();      
      return;
    case '#':
      current_token_ = POUND;
      next_char();
      return;
    case '=':
      current_token_ = EQUALS;
      next_char();
      return;
    case '"':
      text_data_.clear();
      next_char();
      while (1)
      {
        if (at_end() || current_char == '\n')
          error();
        if (current_char == '"')
        {
          next_char();
          break;
        }
        if (current_char == '\\')
        {
          next_char();
          read_escape();
        }
        else
        {
          text_data_ += current_char;
          next_char();
        }
      }
      current_token_ = STRING;
      break;
    default:
      if (is_identifier_start(current_char))
      {
        text_data_.clear();
        do
        {
          text_data_ += current_char;
          next_char();
        } while (!at_end() && is_identifier(current_char));
        current_token_ = IDENTIFIER;
      } else
        read_number();
      return;
    }
  }
}

#endif /* _STYLE_SCANNER_HPP */
