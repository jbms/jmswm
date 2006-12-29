
#include "log.hpp"
#include "string.hpp"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static ascii_string clean_function_name(const ascii_string &name)
{
  ascii_string out = name.substr(0, name.find('('));
  ascii_string::size_type pos = out.rfind(' ');
  if (pos != std::string::npos)
    out = out.substr(pos + 1);
  return out;
}

#ifndef NDEBUG

void wm_print_debug(char const *function, int line, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s:%d: ", clean_function_name(function).c_str(), line);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

#endif 

void wm_print_warn(char const *function, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s: ", clean_function_name(function).c_str());
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

void wm_print_warn_sys(char const *function, char const *format, ...)
{
  va_list args;
  int syserr = errno;
  va_start(args, format);

  fprintf(stderr, "%s: ", clean_function_name(function).c_str());
  vfprintf(stderr, format, args);
  fprintf(stderr, ": %s\n", strerror(syserr));
  va_end(args);
}

void wm_print_error(char const *function, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s: ", clean_function_name(function).c_str());
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(EXIT_FAILURE);
}

void wm_print_error_sys(char const *function, char const *format, ...)
{
  va_list args;
  int syserr = errno;
  va_start(args, format);

  fprintf(stderr, "%s: ", clean_function_name(function).c_str());
  vfprintf(stderr, format, args);
  fprintf(stderr, ": %s\n", strerror(syserr));
  va_end(args);
  exit(EXIT_FAILURE);
}
