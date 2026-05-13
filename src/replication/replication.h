#ifndef REPLICATION_H
#define REPLICATION_H

#include <pthread.h>
#include "../core/store/store.h"

typedef struct ReplicaInfo {
    int                  fd;
    struct ReplicaInfo  *next;
} ReplicaInfo;

typedef struct {
    int              is_replica;
    char             master_host[256];
    int              master_port;
    int              master_fd;
    pthread_t        repl_thread;
    Store           *store;
    pthread_mutex_t *store_lock;   /* points to g_db_lock in server.c */
    ReplicaInfo     *replicas;
    pthread_mutex_t  replica_lock;
    int              running;
} ReplState;

void repl_init(ReplState *rs, Store *store, pthread_mutex_t *store_lock);
int  repl_set_master(ReplState *rs, const char *host, int port);
void repl_broadcast(ReplState *rs, const char *cmd);
void repl_add_replica(ReplState *rs, int fd);
void repl_remove_replica(ReplState *rs, int fd);
void repl_free(ReplState *rs);
void repl_info(const ReplState *rs, char *buf, size_t bufsize);

#endif
