#ifndef _LOG_HPP
#define _LOG_HPP

/* Error messages */
#ifndef NDEBUG
#define DEBUG(...) wm_print_debug(__PRETTY_FUNCTION__, __LINE__, __VA_ARGS__)
void wm_print_debug(char const *function, int line, char const *format, ...);
#else
#define DEBUG(...) do { } while(0)
#endif

#define WARN(...) wm_print_warn(__PRETTY_FUNCTION__, __VA_ARGS__)
#define WARN_SYS(...) wm_print_warn_sys(__PRETTY_FUNCTION__, __VA_ARGS__)
#define ERROR(...) wm_print_error(__PRETTY_FUNCTION__, __VA_ARGS__)
#define ERROR_SYS(...) wm_print_error_sys(__PRETTY_FUNCTION__, __VA_ARGS__)

void wm_print_warn(char const *function, char const *format, ...);
void wm_print_warn_sys(char const *function, char const *format, ...);
void wm_print_error(char const *function, char const *format, ...);
void wm_print_error_sys(char const *function, char const *format, ...);

#endif /* _LOG_HPP */
