/*
 * server.c — Multi-threaded TCP server
 *
 * Features integrated:
 *   • AOF + RDB persistence
 *   • Active expiry background thread
 *   • Pipelining (multiple commands per recv)
 *   • MULTI/EXEC transactions (per-client command queue)
 *   • Pub/Sub messaging (SUBSCRIBE/PUBLISH/UNSUBSCRIBE)
 *   • Replication hooks (broadcast writes to replicas)
 *   • Cluster state (hash slot awareness)
 *   • Lua scripting (EVAL)
 *
 * Protocol:
 *   Client sends : <command>\n
 *   Server replies: <result>\r\nEND\n
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
#include <ctype.h>

#include "server.h"
#include "../engine/engine.h"
#include "../core/store/store.h"
#include "../commands/result.h"
#include "../commands/parser.h"
#include "../persistence/aof.h"
#include "../persistence/rdb.h"
#include "../config/config.h"
#include "../pubsub/pubsub.h"
#include "../cluster/cluster.h"
#include "../replication/replication.h"
#include "../scripting/scripting.h"

#define RECV_BUF_SIZE    4096
#define SEND_BUF_SIZE    8192
#define BACKLOG          10

/* Write commands that should be persisted + replicated */
static const char *WRITE_CMDS[] = {
    "SET","LPUSH","RPUSH","SADD","HSET","DEL",
    "ZADD","ZREM","EXPIRE","PERSIST",
    "INCR","DECR","INCRBY","DECRBY","APPEND","MSET",
    NULL
};

static int is_write_command(const char *cmd) {
    for (int i = 0; WRITE_CMDS[i]; i++)
        if (strcmp(cmd, WRITE_CMDS[i]) == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Shared state                                                        */
/* ------------------------------------------------------------------ */
static Store          *g_db;
static pthread_mutex_t g_db_lock  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_aof_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_server_fd = -1;
static volatile int    g_running   = 1;
static MiniRedisConfig g_cfg;
static PubSubRegistry  g_pubsub;
static ClusterState    g_cluster;
static ReplState       g_repl;

/* ------------------------------------------------------------------ */
/*  Signal handler                                                      */
/* ------------------------------------------------------------------ */
static void handle_signal(int sig) {
    (void)sig;
    printf("\n[server] Shutting down...\n");
    g_running = 0;
    pthread_mutex_lock(&g_aof_lock);
    aof_close();
    pthread_mutex_unlock(&g_aof_lock);
    scripting_close();
    pubsub_free(&g_pubsub);
    repl_free(&g_repl);
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
/*  Helper: upper-case a command name in-place                          */
/* ------------------------------------------------------------------ */
static void upper_cmd(char *s) {
    for (int i = 0; s[i]; i++) s[i] = (char)toupper((unsigned char)s[i]);
}

/* ------------------------------------------------------------------ */
/*  Process one command for a client (handles meta-commands)            */
/* ------------------------------------------------------------------ */
static void process_command(ClientState *cs, char *raw_line,
                            char *resp_buf, size_t resp_cap, size_t *resp_pos) {
    /* Save the original line BEFORE any destructive parsing.
     * parse() replaces spaces/quotes with NUL bytes in-place, so both
     * parse_copy and raw_line are destroyed after their respective parse()
     * calls.  broadcast_copy preserves the original for AOF + replication. */
    char broadcast_copy[RECV_BUF_SIZE];
    strncpy(broadcast_copy, raw_line, sizeof(broadcast_copy) - 1);
    broadcast_copy[sizeof(broadcast_copy) - 1] = '\0';

    /* Parse a copy for token inspection */
    char parse_copy[RECV_BUF_SIZE];
    strncpy(parse_copy, raw_line, sizeof(parse_copy) - 1);
    parse_copy[sizeof(parse_copy) - 1] = '\0';

    char *tokens[MAX_TOKENS];
    int argc = parse(parse_copy, tokens);
    if (argc <= 0) return;

    /* Upper-case the command name for comparison */
    char cmd_upper[64];
    strncpy(cmd_upper, tokens[0], sizeof(cmd_upper) - 1);
    cmd_upper[sizeof(cmd_upper) - 1] = '\0';
    upper_cmd(cmd_upper);

    const char *reply = NULL;
    int should_free_reply = 0;

    /* ---- REPLCONF: replica identifying itself ---- */
    if (strcmp(cmd_upper, "REPLCONF") == 0) {
        repl_add_replica(&g_repl, cs->fd);
        cs->is_replica = 1;
        /* Send +OK immediately (don't buffer — replica is waiting for this
         * before it starts receiving the replication stream) */
        const char *ack = "+OK\r\nEND\n";
        send(cs->fd, ack, strlen(ack), 0);
        return;  /* skip the normal send_reply accumulation */
    }

    /* ---- SUBSCRIBE ---- */
    if (strcmp(cmd_upper, "SUBSCRIBE") == 0) {
        for (int i = 1; i < argc; i++)
            pubsub_subscribe(&g_pubsub, cs->fd, tokens[i]);
        cs->is_subscriber = 1;
        return;  /* subscribe sends its own response */
    }

    /* ---- UNSUBSCRIBE ---- */
    if (strcmp(cmd_upper, "UNSUBSCRIBE") == 0) {
        if (argc > 1) {
            for (int i = 1; i < argc; i++)
                pubsub_unsubscribe(&g_pubsub, cs->fd, tokens[i]);
        } else {
            pubsub_unsubscribe_all(&g_pubsub, cs->fd);
        }
        cs->is_subscriber = 0;
        return;
    }

    /* ---- PUBLISH ---- */
    if (strcmp(cmd_upper, "PUBLISH") == 0) {
        if (argc < 3) { reply = "ERR wrong number of arguments for PUBLISH"; goto send_reply; }
        int delivered = pubsub_publish(&g_pubsub, tokens[1], tokens[2]);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", delivered);
        reply = buf;
        goto send_reply;
    }

    /* ---- MULTI ---- */
    if (strcmp(cmd_upper, "MULTI") == 0) {
        if (cs->in_multi) { reply = "ERR MULTI calls can not be nested"; goto send_reply; }
        cs->in_multi = 1;
        cs->tx_count = 0;
        reply = "OK";
        goto send_reply;
    }

    /* ---- DISCARD ---- */
    if (strcmp(cmd_upper, "DISCARD") == 0) {
        if (!cs->in_multi) { reply = "ERR DISCARD without MULTI"; goto send_reply; }
        for (int i = 0; i < cs->tx_count; i++) free(cs->tx_queue[i]);
        cs->tx_count = 0;
        cs->in_multi = 0;
        reply = "OK";
        goto send_reply;
    }

    /* ---- EXEC ---- */
    if (strcmp(cmd_upper, "EXEC") == 0) {
        if (!cs->in_multi) { reply = "ERR EXEC without MULTI"; goto send_reply; }
        cs->in_multi = 0;

        /* Execute all queued commands atomically */
        pthread_mutex_lock(&g_db_lock);
        for (int i = 0; i < cs->tx_count; i++) {
            /* Parse for AOF/replication */
            char aof_copy[RECV_BUF_SIZE];
            strncpy(aof_copy, cs->tx_queue[i], sizeof(aof_copy) - 1);
            aof_copy[sizeof(aof_copy) - 1] = '\0';
            char *aof_tokens[MAX_TOKENS];
            int aof_argc = parse(aof_copy, aof_tokens);

            CommandResult res = run_command(g_db, cs->tx_queue[i]);

            /* Append each result */
            const char *msg = (res.message && res.message[0]) ? res.message : "";
            size_t mlen = strlen(msg);
            if (*resp_pos + mlen + 16 < resp_cap) {
                memcpy(resp_buf + *resp_pos, msg, mlen);
                *resp_pos += mlen;
                resp_buf[*resp_pos] = '\n';
                (*resp_pos)++;
            }

            /* AOF + replication for write commands */
            if (res.status == 1 && aof_argc > 0) {
                char cmd_name[64];
                strncpy(cmd_name, aof_tokens[0], sizeof(cmd_name) - 1);
                cmd_name[sizeof(cmd_name) - 1] = '\0';
                upper_cmd(cmd_name);
                if (is_write_command(cmd_name)) {
                    if (g_cfg.aof_enabled) {
                        pthread_mutex_lock(&g_aof_lock);
                        aof_append(aof_argc, aof_tokens);
                        pthread_mutex_unlock(&g_aof_lock);
                    }
                    repl_broadcast(&g_repl, cs->tx_queue[i]);
                }
            }
            freeResult(res);
            free(cs->tx_queue[i]);
        }
        pthread_mutex_unlock(&g_db_lock);
        cs->tx_count = 0;

        /* Add END sentinel and return (response already built) */
        if (*resp_pos + 5 < resp_cap) {
            memcpy(resp_buf + *resp_pos, "END\n", 4);
            *resp_pos += 4;
        }
        resp_buf[*resp_pos] = '\0';
        send(cs->fd, resp_buf, *resp_pos, 0);
        *resp_pos = 0;
        return;
    }

    /* ---- Queue if inside MULTI ---- */
    if (cs->in_multi) {
        if (cs->tx_count >= TX_MAX_QUEUED) {
            reply = "ERR transaction queue full";
            goto send_reply;
        }
        cs->tx_queue[cs->tx_count++] = strdup(raw_line);
        reply = "QUEUED";
        goto send_reply;
    }

    /* ---- CLUSTER commands ---- */
    if (strcmp(cmd_upper, "CLUSTER") == 0) {
        if (argc < 2) { reply = "ERR wrong number of arguments for CLUSTER"; goto send_reply; }
        char sub[64];
        strncpy(sub, tokens[1], sizeof(sub) - 1); sub[sizeof(sub) - 1] = '\0';
        upper_cmd(sub);

        static char cluster_buf[2048];
        if (strcmp(sub, "INFO") == 0) {
            cluster_info(&g_cluster, cluster_buf, sizeof(cluster_buf));
            reply = cluster_buf;
        } else if (strcmp(sub, "MYID") == 0) {
            reply = g_cluster.node_id;
        } else if (strcmp(sub, "KEYSLOT") == 0) {
            if (argc < 3) { reply = "ERR need key"; goto send_reply; }
            unsigned int slot = cluster_keyslot(tokens[2]);
            snprintf(cluster_buf, sizeof(cluster_buf), "%u", slot);
            reply = cluster_buf;
        } else if (strcmp(sub, "SLOTS") == 0) {
            cluster_slots_str(&g_cluster, cluster_buf, sizeof(cluster_buf));
            reply = cluster_buf;
        } else {
            reply = "ERR unknown CLUSTER subcommand";
        }
        goto send_reply;
    }

    /* ---- REPLICAOF ---- */
    if (strcmp(cmd_upper, "REPLICAOF") == 0) {
        if (argc < 3) { reply = "ERR wrong number of arguments for REPLICAOF"; goto send_reply; }
        if (strcmp(tokens[1], "NO") == 0 && strcmp(tokens[2], "ONE") == 0) {
            g_repl.is_replica = 0;
            reply = "OK - stopped replication";
        } else {
            int port = atoi(tokens[2]);
            repl_set_master(&g_repl, tokens[1], port);
            reply = "OK - replicating";
        }
        goto send_reply;
    }

    /* ---- INFO ---- */
    if (strcmp(cmd_upper, "INFO") == 0) {
        static char info_buf[4096];
        char repl_info_buf[1024];
        repl_info(&g_repl, repl_info_buf, sizeof(repl_info_buf));
        snprintf(info_buf, sizeof(info_buf),
            "# Server\r\nmini_redis_version:2.0\r\n"
            "# Replication\r\n%s\r\n"
            "# Cluster\r\ncluster_enabled:%d",
            repl_info_buf, g_cluster.cluster_enabled);
        reply = info_buf;
        goto send_reply;
    }

    /* ---- EVAL (Lua scripting) ---- */
    if (strcmp(cmd_upper, "EVAL") == 0) {
        if (argc < 3) { reply = "ERR wrong number of arguments for EVAL"; goto send_reply; }
        const char *script = tokens[1];
        int numkeys = atoi(tokens[2]);
        char **keys = (argc > 3) ? &tokens[3] : NULL;
        int numargs = argc - 3 - numkeys;
        char **args = (numargs > 0) ? &tokens[3 + numkeys] : NULL;
        if (numargs < 0) numargs = 0;

        pthread_mutex_lock(&g_db_lock);
        CommandResult res = scripting_eval(g_db, script, numkeys, keys, args, numargs);
        pthread_mutex_unlock(&g_db_lock);

        const char *msg = (res.message && res.message[0]) ? res.message : "";
        size_t mlen = strlen(msg);
        if (*resp_pos + mlen + 16 < resp_cap) {
            memcpy(resp_buf + *resp_pos, msg, mlen);
            *resp_pos += mlen;
            memcpy(resp_buf + *resp_pos, "\r\nEND\n", 6);
            *resp_pos += 6;
        }
        resp_buf[*resp_pos] = '\0';
        send(cs->fd, resp_buf, *resp_pos, 0);
        *resp_pos = 0;
        freeResult(res);
        return;
    }

    /* ---- Normal command execution ---- */
    {
        pthread_mutex_lock(&g_db_lock);
        CommandResult res = run_command(g_db, raw_line);
        pthread_mutex_unlock(&g_db_lock);

        /* AOF + replication for write commands */
        if (res.status == 1 && argc > 0 && is_write_command(cmd_upper)) {
            if (g_cfg.aof_enabled) {
                /* Re-parse from the pristine copy for AOF */
                char aof_copy[RECV_BUF_SIZE];
                strncpy(aof_copy, broadcast_copy, sizeof(aof_copy) - 1);
                aof_copy[sizeof(aof_copy) - 1] = '\0';
                char *aof_toks[MAX_TOKENS];
                int aof_argc = parse(aof_copy, aof_toks);
                if (aof_argc > 0) {
                    pthread_mutex_lock(&g_aof_lock);
                    aof_append(aof_argc, aof_toks);
                    pthread_mutex_unlock(&g_aof_lock);
                }
            }
            /* Broadcast the full original command to replicas */
            repl_broadcast(&g_repl, broadcast_copy);
        }

        reply = (res.message && res.message[0]) ? res.message : "";
        /* Build response into pipeline buffer */
        size_t mlen = strlen(reply);
        if (*resp_pos + mlen + 16 < resp_cap) {
            memcpy(resp_buf + *resp_pos, reply, mlen);
            *resp_pos += mlen;
            memcpy(resp_buf + *resp_pos, "\r\nEND\n", 6);
            *resp_pos += 6;
        }
        resp_buf[*resp_pos] = '\0';
        freeResult(res);
        return;
    }

send_reply:
    {
        const char *msg = reply ? reply : "";
        size_t mlen = strlen(msg);
        if (*resp_pos + mlen + 16 < resp_cap) {
            memcpy(resp_buf + *resp_pos, msg, mlen);
            *resp_pos += mlen;
            memcpy(resp_buf + *resp_pos, "\r\nEND\n", 6);
            *resp_pos += 6;
        }
        resp_buf[*resp_pos] = '\0';
        if (should_free_reply) free((char*)reply);
    }
}

/* ------------------------------------------------------------------ */
/*  Per-client thread (with pipelining support)                        */
/* ------------------------------------------------------------------ */
static void* handle_client(void *arg) {
    ClientState *cs = (ClientState*)arg;
    char recv_buf[RECV_BUF_SIZE];
    char resp_buf[SEND_BUF_SIZE];

    while (1) {
        int n = recv(cs->fd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (n <= 0) break;
        recv_buf[n] = '\0';

        /* ---- Pipelining: split on newlines, process each command ---- */
        size_t resp_pos = 0;
        resp_buf[0] = '\0';

        char *saveptr = NULL;
        char *line = strtok_r(recv_buf, "\n", &saveptr);
        while (line) {
            /* Trim \r */
            line[strcspn(line, "\r")] = '\0';
            if (strlen(line) > 0) {
                process_command(cs, line, resp_buf, sizeof(resp_buf), &resp_pos);
            }
            line = strtok_r(NULL, "\n", &saveptr);
        }

        /* Flush all pipelined responses at once */
        if (resp_pos > 0) {
            send(cs->fd, resp_buf, resp_pos, 0);
        }

        /* If this connection has become a replica sink, stop reading.
         * repl_broadcast() now owns writes to this fd; handle_client
         * must not call recv() again or it will steal broadcast data. */
        if (cs->is_replica) break;
    }

    /* Clean up client state.
     * For replica connections: the fd stays alive and is owned by
     * repl_broadcast().  Don't close it or remove it from the replica list
     * here — repl_broadcast will detect the disconnect on send failure. */
    pubsub_unsubscribe_all(&g_pubsub, cs->fd);
    if (!cs->is_replica) {
        repl_remove_replica(&g_repl, cs->fd);
        close(cs->fd);
    }
    for (int i = 0; i < cs->tx_count; i++) free(cs->tx_queue[i]);
    free(cs);
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
    if (!g_db) { fprintf(stderr, "[server] Failed to initialise store\n"); exit(1); }

    /* Step 2: load RDB snapshot if enabled */
    if (g_cfg.rdb_enabled) {
        int loaded = rdb_load(g_db, g_cfg.rdb_file);
        if (loaded > 0) printf("[server] Restored %d keys from RDB %s\n", loaded, g_cfg.rdb_file);
        else            printf("[server] No existing RDB found\n");
    }

    /* Step 3: replay AOF */
    if (g_cfg.aof_enabled) {
        int replayed = aof_load(g_db, g_cfg.aof_file);
        if (replayed > 0) printf("[server] Restored %d commands from AOF %s\n", replayed, g_cfg.aof_file);
        else              printf("[server] No existing AOF found — starting fresh\n");
        if (aof_open(g_cfg.aof_file) != 0)
            fprintf(stderr, "[server] WARNING: could not open AOF for writing\n");
    }

    /* Step 4: init subsystems */
    pubsub_init(&g_pubsub);
    cluster_init(&g_cluster, "127.0.0.1", g_cfg.port);
    repl_init(&g_repl, g_db, &g_db_lock);
    scripting_init();

    /* Step 4b: start replication if configured */
    if (g_cfg.replicaof_host[0] != '\0' && g_cfg.replicaof_port > 0) {
        printf("[server] Starting replication from %s:%d (from config)\n",
               g_cfg.replicaof_host, g_cfg.replicaof_port);
        repl_set_master(&g_repl, g_cfg.replicaof_host, g_cfg.replicaof_port);
    }

    /* Step 5: signal handlers */
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* Step 6: active expiry thread */
    {
        pthread_t expiry_tid;
        if (pthread_create(&expiry_tid, NULL, active_expiry_thread, NULL) == 0) {
            pthread_detach(expiry_tid);
            printf("[server] Active expiry running at %d Hz\n", g_cfg.active_expiry_hz);
        }
    }

    /* Step 7: bind and listen */
    g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_server_fd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(g_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)g_cfg.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        { perror("bind"); close(g_server_fd); exit(1); }
    if (listen(g_server_fd, BACKLOG) < 0)
        { perror("listen"); close(g_server_fd); exit(1); }

    printf("[server] Listening on port %d\n", g_cfg.port);
    printf("[server] Cluster node ID: %s\n", g_cluster.node_id);

    /* Step 8: accept loop */
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(g_server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("[server] Client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        ClientState *cs = (ClientState*)calloc(1, sizeof(ClientState));
        if (!cs) { close(client_fd); continue; }
        cs->fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, cs) != 0) {
            perror("pthread_create");
            free(cs);
            close(client_fd);
            continue;
        }
        pthread_detach(tid);
    }
}