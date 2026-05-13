/*
 * replication.c — Leader-Follower replication
 *
 * A replica connects to its master, receives an RDB snapshot for
 * full sync, then listens for streamed write commands.
 * The master broadcasts every write to all connected replicas.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "replication.h"
#include "../engine/engine.h"
#include "../commands/result.h"
#include "../persistence/rdb.h"

void repl_init(ReplState *rs, Store *store, pthread_mutex_t *store_lock) {
    memset(rs, 0, sizeof(*rs));
    rs->store      = store;
    rs->store_lock = store_lock;
    rs->replicas   = NULL;
    rs->running    = 1;
    pthread_mutex_init(&rs->replica_lock, NULL);
}

/* ------------------------------------------------------------------ */
/*  Replica side: connect to master, full sync, then stream            */
/* ------------------------------------------------------------------ */

static void* replica_thread(void *arg) {
    ReplState *rs = (ReplState*)arg;

    /* Connect to master */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("repl: socket"); return NULL; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)rs->master_port);
    inet_pton(AF_INET, rs->master_host, &addr.sin_addr);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("repl: connect to master");
        close(fd);
        return NULL;
    }

    rs->master_fd = fd;
    printf("[replication] Connected to master %s:%d\n",
           rs->master_host, rs->master_port);

    /* Send REPLCONF to identify as replica */
    const char *hello = "REPLCONF\n";
    send(fd, hello, strlen(hello), 0);

    /* Receive and execute streamed commands */
    char buf[2048];
    while (rs->running) {
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';

        /* Process each line as a command — use strtok_r (reentrant) */
        char *saveptr = NULL;
        char *line = strtok_r(buf, "\n", &saveptr);
        while (line) {
            /* Skip END sentinel and empty lines */
            char *trimmed = line;
            while (*trimmed == '\r' || *trimmed == ' ') trimmed++;
            if (strlen(trimmed) > 0 && strcmp(trimmed, "END") != 0) {
                char cmd_buf[1024];
                strncpy(cmd_buf, trimmed, sizeof(cmd_buf) - 1);
                cmd_buf[sizeof(cmd_buf) - 1] = '\0';
                if (rs->store_lock) pthread_mutex_lock(rs->store_lock);
                CommandResult res = run_command(rs->store, cmd_buf);
                if (rs->store_lock) pthread_mutex_unlock(rs->store_lock);
                freeResult(res);
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    close(fd);
    rs->master_fd = -1;
    printf("[replication] Disconnected from master\n");
    return NULL;
}

int repl_set_master(ReplState *rs, const char *host, int port) {
    if (rs->is_replica) {
        /* Signal the running thread to exit; it will see running==0
         * and break out of its recv loop on its own (or on next recv
         * timeout).  We close master_fd to unblock recv immediately. */
        rs->running = 0;
        if (rs->master_fd >= 0) {
            close(rs->master_fd);
            rs->master_fd = -1;
        }
        /* Give the old thread a moment to exit before we overwrite state */
        struct timespec ts = {0, 50000000L}; /* 50 ms */
        nanosleep(&ts, NULL);
    }

    strncpy(rs->master_host, host, sizeof(rs->master_host) - 1);
    rs->master_host[sizeof(rs->master_host) - 1] = '\0';
    rs->master_port = port;
    rs->is_replica  = 1;
    rs->running     = 1;

    if (pthread_create(&rs->repl_thread, NULL, replica_thread, rs) != 0) {
        perror("repl: pthread_create");
        rs->is_replica = 0;
        return -1;
    }
    pthread_detach(rs->repl_thread);

    printf("[replication] Replicating from %s:%d\n", host, port);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Master side: broadcast writes to all replicas                      */
/* ------------------------------------------------------------------ */

void repl_add_replica(ReplState *rs, int fd) {
    pthread_mutex_lock(&rs->replica_lock);
    ReplicaInfo *ri = (ReplicaInfo*)malloc(sizeof(ReplicaInfo));
    if (ri) {
        ri->fd = fd;
        ri->next = rs->replicas;
        rs->replicas = ri;
        printf("[replication] Replica connected (fd=%d)\n", fd);
    }
    pthread_mutex_unlock(&rs->replica_lock);
}

void repl_remove_replica(ReplState *rs, int fd) {
    pthread_mutex_lock(&rs->replica_lock);
    ReplicaInfo *prev = NULL, *curr = rs->replicas;
    while (curr) {
        if (curr->fd == fd) {
            if (prev) prev->next = curr->next;
            else      rs->replicas = curr->next;
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&rs->replica_lock);
}

void repl_broadcast(ReplState *rs, const char *cmd) {
    if (!cmd || !rs) return;
    pthread_mutex_lock(&rs->replica_lock);
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    size_t len = strlen(buf);

    ReplicaInfo *curr = rs->replicas;
    ReplicaInfo *prev = NULL;
    while (curr) {
        ssize_t sent = send(curr->fd, buf, len, MSG_NOSIGNAL);
        if (sent <= 0) {
            /* Replica disconnected — remove it */
            ReplicaInfo *dead = curr;
            if (prev) prev->next = curr->next;
            else      rs->replicas = curr->next;
            curr = curr->next;
            close(dead->fd);
            free(dead);
            printf("[replication] Replica disconnected\n");
            continue;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&rs->replica_lock);
}

void repl_info(const ReplState *rs, char *buf, size_t bufsize) {
    int replica_count = 0;
    ReplicaInfo *ri = rs->replicas;
    while (ri) { replica_count++; ri = ri->next; }

    if (rs->is_replica) {
        snprintf(buf, bufsize,
            "role:slave\r\n"
            "master_host:%s\r\n"
            "master_port:%d\r\n"
            "master_link_status:%s",
            rs->master_host, rs->master_port,
            rs->master_fd >= 0 ? "up" : "down");
    } else {
        snprintf(buf, bufsize,
            "role:master\r\n"
            "connected_slaves:%d",
            replica_count);
    }
}

void repl_free(ReplState *rs) {
    rs->running = 0;
    if (rs->master_fd >= 0) close(rs->master_fd);
    pthread_mutex_lock(&rs->replica_lock);
    ReplicaInfo *curr = rs->replicas;
    while (curr) {
        ReplicaInfo *next = curr->next;
        close(curr->fd);
        free(curr);
        curr = next;
    }
    rs->replicas = NULL;
    pthread_mutex_unlock(&rs->replica_lock);
    pthread_mutex_destroy(&rs->replica_lock);
}
