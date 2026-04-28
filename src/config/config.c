/*
 * config.c  —  Configuration file loader
 *
 * Parses a simple text config file with one directive per line.
 * Format:  directive value
 *
 * Blank lines and lines starting with # are ignored.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

MiniRedisConfig config_defaults(void) {
    MiniRedisConfig cfg;
    cfg.port             = 6379;
    cfg.store_size       = 256;
    cfg.aof_enabled      = 1;
    strncpy(cfg.aof_file, "miniredis.aof", sizeof(cfg.aof_file));
    cfg.rdb_enabled      = 1;
    strncpy(cfg.rdb_file, "miniredis.rdb", sizeof(cfg.rdb_file));
    cfg.active_expiry_hz = 10;
    return cfg;
}

static int parse_bool(const char *val) {
    if (strcmp(val, "yes") == 0 || strcmp(val, "1") == 0 ||
        strcmp(val, "true") == 0 || strcmp(val, "on") == 0)
        return 1;
    return 0;
}

static void trim(char *s) {
    /* trim trailing whitespace */
    int len = (int)strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    /* trim leading whitespace */
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

int config_load(MiniRedisConfig *cfg, const char *filepath) {
    if (!cfg) return -1;

    *cfg = config_defaults();

    if (!filepath) return 0;

    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf("[config] No config file '%s' found, using defaults\n", filepath);
        return 0;   /* not an error — just use defaults */
    }

    char line[512];
    int lineno = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        line[strcspn(line, "\r\n")] = '\0';
        trim(line);

        /* skip blank lines and comments */
        if (line[0] == '\0' || line[0] == '#') continue;

        /* split into directive and value */
        char *space = strchr(line, ' ');
        if (!space) {
            fprintf(stderr, "[config] line %d: missing value for '%s'\n", lineno, line);
            continue;
        }
        *space = '\0';
        char *directive = line;
        char *value     = space + 1;
        trim(value);

        if (strcmp(directive, "port") == 0) {
            cfg->port = atoi(value);
        } else if (strcmp(directive, "store-size") == 0) {
            cfg->store_size = atoi(value);
        } else if (strcmp(directive, "aof-enabled") == 0) {
            cfg->aof_enabled = parse_bool(value);
        } else if (strcmp(directive, "aof-file") == 0) {
            strncpy(cfg->aof_file, value, sizeof(cfg->aof_file) - 1);
            cfg->aof_file[sizeof(cfg->aof_file) - 1] = '\0';
        } else if (strcmp(directive, "rdb-enabled") == 0) {
            cfg->rdb_enabled = parse_bool(value);
        } else if (strcmp(directive, "rdb-file") == 0) {
            strncpy(cfg->rdb_file, value, sizeof(cfg->rdb_file) - 1);
            cfg->rdb_file[sizeof(cfg->rdb_file) - 1] = '\0';
        } else if (strcmp(directive, "active-expiry-hz") == 0) {
            cfg->active_expiry_hz = atoi(value);
            if (cfg->active_expiry_hz < 1) cfg->active_expiry_hz = 1;
            if (cfg->active_expiry_hz > 500) cfg->active_expiry_hz = 500;
        } else {
            fprintf(stderr, "[config] line %d: unknown directive '%s'\n", lineno, directive);
        }
    }

    fclose(f);
    printf("[config] Loaded configuration from %s\n", filepath);
    return 0;
}

void config_print(const MiniRedisConfig *cfg) {
    printf("=== Mini Redis Configuration ===\n");
    printf("  port             : %d\n",  cfg->port);
    printf("  store-size       : %d\n",  cfg->store_size);
    printf("  aof-enabled      : %s\n",  cfg->aof_enabled ? "yes" : "no");
    printf("  aof-file         : %s\n",  cfg->aof_file);
    printf("  rdb-enabled      : %s\n",  cfg->rdb_enabled ? "yes" : "no");
    printf("  rdb-file         : %s\n",  cfg->rdb_file);
    printf("  active-expiry-hz : %d\n",  cfg->active_expiry_hz);
    printf("================================\n");
}
