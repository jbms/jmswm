#ifndef _UTIL_SPAWN_HPP
#define _UTIL_SPAWN_HPP

/**
 * Spawns a process asynchronously.
 *
 * Signal handlers are reset in the new process to normal defaults so
 * that the spawned program runs normally.  (SA_NOCLDWAIT breaks Flash
 * plugin for firefox)
 *
 * The new process is created in a new process group.
 *
 * The standard input file descriptor is closed in the new process.
 */
int spawnl(const char *path, ...);

#endif /* _UTIL_SPAWN_HPP */
