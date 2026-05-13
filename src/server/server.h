#ifndef SERVER_H
#define SERVER_H

#include "../config/config.h"

/* Maximum queued commands in a MULTI transaction */
#define TX_MAX_QUEUED 128

/* Per-client state (used by server.c handle_client) */
typedef struct {
    int   fd;
    /* MULTI/EXEC transaction state */
    int   in_multi;
    char *tx_queue[TX_MAX_QUEUED];
    int   tx_count;
    /* Pub/Sub state */
    int   is_subscriber;
    /* Replication: set when this connection sent REPLCONF */
    int   is_replica;
} ClientState;

void start_server(const MiniRedisConfig *cfg);

#endif
