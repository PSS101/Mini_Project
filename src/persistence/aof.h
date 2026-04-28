#ifndef AOF_H
#define AOF_H

#include "../core/store/store.h"

/*
 * Append-Only File persistence
 * ----------------------------
 * Every write command is appended to a plain-text log file.
 * On restart the log is replayed through the engine to rebuild state.
 *
 * Format (one command per line):
 *   SET key value
 *   LPUSH key value
 *   ... etc.
 */

/* Open (or create) the AOF file.  Must be called before aof_append.
   Returns 0 on success, -1 on failure. */
int  aof_open(const char *filepath);

/* Append a write command to the AOF.
   argc/argv are exactly what was parsed by the engine.
   Read-only commands (GET, TYPE, EXISTS, …) are not appended. */
void aof_append(int argc, char *argv[]);

/* Replay the AOF into `store`, rebuilding all keys.
   Returns the number of commands replayed, or -1 on error. */
int  aof_load(Store *store, const char *filepath);

/* Flush and close the AOF file descriptor. */
void aof_close(void);

#endif
