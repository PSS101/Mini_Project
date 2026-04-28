/*
 * server.c  —  Person B
 *
 * Multi-threaded TCP server with AOF + RDB persistence and active expiry.
 *
 * On startup  : rdb_load() (if enabled), then aof_load() replays the log.
 * On each write command : aof_append() records the command before replying.
 * On SIGINT/SIGTERM     : aof_close() flushes and closes the log cleanly.
 *
 * Active expiry runs in a background thread at configurable Hz.
 *
 * Protocol framing
 * ----------------
 * Client sends : <command>\n
 * Server replies: <result>\r\nEND\n
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#include "server.h"
#include "../engine/engine.h"
#include "../core/store/store.h"
#include "../commands/result.h"
#include "../commands/parser.h"
#include "../persistence/aof.h"
#include "../persistence/rdb.h"
#include "../config/config.h"

#define RECV_BUF_SIZE    1024
#define SEND_BUF_SIZE    4096
#define BACKLOG          10

/* ------------------------------------------------------------------ */
/*  Shared state                                                        */
/* ------------------------------------------------------------------ */
static Store          *g_db;
static pthread_mutex_t g_db_lock  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_aof_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_server_fd = -1;
static volatile int    g_running   = 1;
static MiniRedisConfig g_cfg;

/* ------------------------------------------------------------------ */
/*  Signal handler — flush AOF and exit cleanly on Ctrl-C              */
/* ------------------------------------------------------------------ */
static void handle_signal(int sig) {
    (void)sig;
    printf("\n[server] Shutting down, flushing persistence...\n");
    g_running = 0;
    pthread_mutex_lock(&g_aof_lock);
    aof_close();
    pthread_mutex_unlock(&g_aof_lock);
    if (g_server_fd >= 0) close(g_server_fd);
    exit(0);
}

/* ------------------------------------------------------------------ */
/*  Active expiry background thread                                     */
/* ------------------------------------------------------------------ */
static void* active_expiry_thread(void *arg) {
    (void)arg;
    int interval_ms = 1000 / g_cfg.active_expiry_hz;
    struct timespec ts;
    ts.tv_sec  = interval_ms / 1000;
    ts.tv_nsec = (interval_ms % 1000) * 1000000L;

    while (g_running) {
        nanosleep(&ts, NULL);
        pthread_mutex_lock(&g_db_lock);
        activeExpiryCycle(g_db);
        pthread_mutex_unlock(&g_db_lock);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Per-client thread                                                   */
/* ------------------------------------------------------------------ */
typedef struct { int fd; } ClientArg;

static void* handle_client(void *arg) {
    ClientArg *ca = (ClientArg*)arg;
    int client_fd = ca->fd;
    free(ca);

    char recv_buf[RECV_BUF_SIZE];

    while (1) {
        int n = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) break;

        recv_buf[n] = '\0';
        recv_buf[strcspn(recv_buf, "\r\n")] = '\0';
        if (strlen(recv_buf) == 0) continue;

        /*
         * Parse a copy of the input so we can call aof_append()
         * with the token array BEFORE run_command() destroys the
         * buffer in-place.
         */
        char parse_buf[RECV_BUF_SIZE];
        strncpy(parse_buf, recv_buf, sizeof(parse_buf) - 1);
        parse_buf[sizeof(parse_buf) - 1] = '\0';

        char *tokens[MAX_TOKENS];
        int   argc = parse(parse_buf, tokens);

        /* Run the command under the DB lock */
        pthread_mutex_lock(&g_db_lock);
        CommandResult res = run_command(g_db, recv_buf);
        pthread_mutex_unlock(&g_db_lock);

        /*
         * Persist write commands AFTER successful execution.
         * Only append if the command succeeded (status == 1).
         */
        if (res.status == 1 && argc > 0 && g_cfg.aof_enabled) {
            pthread_mutex_lock(&g_aof_lock);
            aof_append(argc, tokens);
            pthread_mutex_unlock(&g_aof_lock);
        }

        /* Send response with sentinel */
        char send_buf[SEND_BUF_SIZE];
        const char *msg = (res.message && res.message[0]) ? res.message : "";
        snprintf(send_buf, sizeof(send_buf), "%s\r\nEND\n", msg);
        send(client_fd, send_buf, strlen(send_buf), 0);

        freeResult(res);
    }

    close(client_fd);
    return NULL;
}

/* ------------------------------------------------------------------ */
/*  Main server loop                                                    */
/* ------------------------------------------------------------------ */
void start_server(const MiniRedisConfig *cfg) {
    g_cfg = *cfg;

    config_print(&g_cfg);

    /* Step 1: initialise store */
    g_db = init_engine(g_cfg.store_size);
    if (!g_db) {
        fprintf(stderr, "[server] Failed to initialise store\n");
        exit(1);
    }

    /* Step 2: load RDB snapshot if enabled */
    if (g_cfg.rdb_enabled) {
        int loaded = rdb_load(g_db, g_cfg.rdb_file);
        if (loaded > 0)
            printf("[server] Restored %d keys from RDB %s\n", loaded, g_cfg.rdb_file);
        else
            printf("[server] No existing RDB found\n");
    }

    /* Step 3: replay AOF to restore latest state */
    if (g_cfg.aof_enabled) {
        int replayed = aof_load(g_db, g_cfg.aof_file);
        if (replayed > 0)
            printf("[server] Restored %d commands from AOF %s\n", replayed, g_cfg.aof_file);
        else
            printf("[server] No existing AOF found — starting fresh\n");

        /* Open AOF for appending new writes */
        if (aof_open(g_cfg.aof_file) != 0) {
            fprintf(stderr, "[server] WARNING: could not open AOF for writing. "
                            "Data will NOT be persisted.\n");
        }
    }

    /* Step 4: register signal handlers for clean shutdown */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* Step 5: start active expiry background thread */
    {
        pthread_t expiry_tid;
        if (pthread_create(&expiry_tid, NULL, active_expiry_thread, NULL) != 0) {
            perror("pthread_create(expiry)");
        } else {
            pthread_detach(expiry_tid);
            printf("[server] Active expiry running at %d Hz\n", g_cfg.active_expiry_hz);
        }
    }

    /* Step 6: bind and listen */
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)g_cfg.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(g_server_fd); exit(1);
    }
    if (listen(g_server_fd, BACKLOG) < 0) {
        perror("listen"); close(g_server_fd); exit(1);
    }

    printf("[server] Listening on port %d\n", g_cfg.port);

    /* Step 7: accept loop */
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(g_server_fd,
                               (struct sockaddr*)&client_addr,
                               &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("[server] Client connected: %s\n",
               inet_ntoa(client_addr.sin_addr));

        ClientArg *ca = (ClientArg*)malloc(sizeof(ClientArg));
        if (!ca) { close(client_fd); continue; }
        ca->fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, ca) != 0) {
            perror("pthread_create");
            free(ca);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }
}