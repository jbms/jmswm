#ifndef _CLOSE_ON_EXEC_HPP
#define _CLOSE_ON_EXEC_HPP

/**
 * If value is true, set the close-on-exec flag to true.  Returns
 * non-negative on success.
 */
int set_close_on_exec_flag(int fd, bool value);

#endif /* _CLOSE_ON_EXEC_HPP */
