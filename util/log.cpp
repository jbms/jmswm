
#include "log.hpp"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef NDEBUG

void wm_print_debug(char const *function, int line, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s:%d: ", function, line);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

#endif 

void wm_print_warn(char const *function, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s: ", function);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
}

void wm_print_warn_sys(char const *function, char const *format, ...)
{
  va_list args;
  int syserr = errno;
  va_start(args, format);

  fprintf(stderr, "%s: ", function);
  vfprintf(stderr, format, args);
  fprintf(stderr, ": %s\n", strerror(syserr));
  va_end(args);
}

void wm_print_error(char const *function, char const *format, ...)
{
  va_list args;
  va_start(args, format);

  fprintf(stderr, "%s: ", function);
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

  fprintf(stderr, "%s: ", function);
  vfprintf(stderr, format, args);
  fprintf(stderr, ": %s\n", strerror(syserr));
  va_end(args);
  exit(EXIT_FAILURE);
}
