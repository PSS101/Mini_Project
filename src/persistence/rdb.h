#ifndef RDB_H
#define RDB_H

#include "../core/store/store.h"

/*
 * RDB Snapshot Persistence
 * ------------------------
 * Binary point-in-time snapshot of the entire database.
 *
 * File format:
 *   "MINIRDB\0" (8 bytes magic)
 *   uint32_t version (1)
 *   For each key:
 *     uint8_t  type   (0=string, 1=list, 2=set, 3=hash, 4=zset)
 *     int64_t  expires_at (-1 = no TTL)
 *     uint32_t key_len
 *     char     key[key_len]
 *     <type-specific data>
 *   uint8_t  EOF_MARKER (0xFF)
 */

#define RDB_TYPE_STRING 0
#define RDB_TYPE_LIST   1
#define RDB_TYPE_SET    2
#define RDB_TYPE_HASH   3
#define RDB_TYPE_ZSET   4
#define RDB_EOF_MARKER  0xFF

/* Synchronous save — blocks until complete.
   Returns 0 on success, -1 on error. */
int rdb_save(Store *store, const char *filepath);

/* Load RDB file into store.
   Returns number of keys loaded, -1 on error, 0 if no file. */
int rdb_load(Store *store, const char *filepath);

/* Background save — runs rdb_save in a separate thread.
   Returns 0 if save was started, -1 on error. */
int rdb_bgsave(Store *store, const char *filepath);

/* Check if a background save is still running. */
int rdb_bgsave_in_progress(void);

#endif
