
#include "util/spawn.hpp"

#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

static void reset_signal_handling()
{
  struct sigaction act;
  act.sa_flags = 0;
  act.sa_handler = SIG_DFL;
  while (sigaction(SIGCHLD, &act, 0) != 0 && errno == EINTR);
  while (sigaction(SIGINT, &act, 0) != 0 && errno == EINTR);
}

int spawnl(const char *path, ...)
{
  const int max_args = 31;
  va_list argp;
  char *argv[max_args];
  int argc = 0;
  va_start (argp, path);
  while (argc < max_args
         && (argv[argc++] = va_arg(argp, char *)) != (char*)0)
    ;
  va_end(argp);
  int val = fork();
  if (val == 0)
  {
    reset_signal_handling();
    setsid();
    close(STDIN_FILENO);
    execv(path, argv);
    exit(-1);
  }
  return val;
}
