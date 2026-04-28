#ifndef CONFIG_H
#define CONFIG_H

/*
 * Configuration Loader
 * --------------------
 * Reads a simple key = value text config file.
 * All fields have sensible defaults.
 *
 * Example miniredis.conf:
 *   port 6379
 *   store-size 256
 *   aof-enabled yes
 *   aof-file miniredis.aof
 *   rdb-enabled yes
 *   rdb-file miniredis.rdb
 *   active-expiry-hz 10
 */

typedef struct {
    int   port;
    int   store_size;
    int   aof_enabled;
    char  aof_file[256];
    int   rdb_enabled;
    char  rdb_file[256];
    int   active_expiry_hz;   /* expiry cycles per second */
} MiniRedisConfig;

/* Return a config with all default values */
MiniRedisConfig config_defaults(void);

/* Load config from file, overriding defaults for keys present.
   Returns 0 on success, -1 on error (file not found is OK — uses defaults). */
int config_load(MiniRedisConfig *cfg, const char *filepath);

/* Print the current config to stdout */
void config_print(const MiniRedisConfig *cfg);

#endif
