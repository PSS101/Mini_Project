/*
 * server_main.c  —  TCP server entry point
 *
 * Usage:
 *   ./miniredis-server                     (uses miniredis.conf or defaults)
 *   ./miniredis-server <port>              (override port only)
 *   ./miniredis-server --config <file>     (load from specified config file)
 *
 * To start a replica:
 *   ./miniredis-server --config replica.conf
 *   where replica.conf contains:
 *     port 6380
 *     replicaof 127.0.0.1 6379
 *
 * All persistence (AOF + RDB), active expiry, and replication are
 * started inside start_server() based on the loaded configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "server/server.h"
#include "config/config.h"

int main(int argc, char *argv[]) {
    MiniRedisConfig cfg;
    const char *config_file = "miniredis.conf";  /* default config path */

    /* Parse command line */
    if (argc >= 3 && strcmp(argv[1], "--config") == 0) {
        config_file = argv[2];
    }

    /* Load config (falls back to defaults if file not found) */
    config_load(&cfg, config_file);

    /* Allow port override from command line */
    if (argc >= 2 && argv[1][0] != '-') {
        int port = atoi(argv[1]);
        if (port > 0 && port <= 65535) {
            cfg.port = port;
        } else {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    start_server(&cfg);   /* blocks; returns only on fatal error */
    return 0;
}