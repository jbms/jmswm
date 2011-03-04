
#include <util/close_on_exec.hpp>
#include <unistd.h>
#include <fcntl.h>

int set_close_on_exec_flag(int fd, bool value)
{
  int oldflags = fcntl(fd, F_GETFD, 0);
  
  if (oldflags < 0)
    return oldflags;
  
  if (value)
    oldflags |= FD_CLOEXEC;
  else
    oldflags &= ~FD_CLOEXEC;
  
  return fcntl(fd, F_SETFD, oldflags);
}
