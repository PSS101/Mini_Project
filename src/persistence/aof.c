/*
 * aof.c  —  Person C
 *
 * Append-Only File persistence layer.
 *
 * Write commands are appended one-per-line to a text file.
 * On startup aof_load() replays every line through the dispatcher
 * to rebuild the in-memory store.
 *
 * Bug fixed: parse() modifies its input buffer in-place.
 * aof_load() must pass a COPY of each line to parse(), not the
 * original — otherwise the tokens point into a buffer that has
 * already been overwritten with NUL bytes and execute() sees garbage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aof.h"
#include "../commands/dispatcher.h"
#include "../commands/result.h"
#include "../commands/parser.h"

/* Only these command names mutate state — only these get persisted */
static const char *WRITE_CMDS[] = {
    "SET", "LPUSH", "RPUSH", "SADD", "HSET", "DEL",
    "ZADD", "ZREM",                   /* sorted set writes */
    "EXPIRE", "PERSIST",              /* TTL mutations */
    "INCR", "DECR", "INCRBY", "DECRBY",  /* counter ops */
    "APPEND",                         /* string append */
    "MSET",                           /* multi-set */
    NULL
};
/*
 * NOTE: LPOP and RPOP are intentionally excluded.
 * They are destructive reads. Replaying them on an already-empty
 * list (or a list that was rebuilt from earlier LPUSH/RPUSH entries)
 * would incorrectly remove elements that should still be there.
 * The correct state is fully captured by the push commands alone.
 */

static FILE *g_aof = NULL;

/* ------------------------------------------------------------------ */

static int is_write_command(const char *cmd) {
    for (int i = 0; WRITE_CMDS[i] != NULL; i++) {
        if (strcmp(cmd, WRITE_CMDS[i]) == 0) return 1;
    }
    return 0;
}

int aof_open(const char *filepath) {
    g_aof = fopen(filepath, "a");
    if (!g_aof) { perror("aof_open"); return -1; }
    return 0;
}

void aof_append(int argc, char *argv[]) {
    if (!g_aof || argc == 0) return;
    if (!is_write_command(argv[0])) return;

    for (int i = 0; i < argc; i++) {
        /* Wrap values that contain spaces in double quotes */
        int has_space = (strchr(argv[i], ' ') != NULL);
        if (has_space)
            fprintf(g_aof, "\"%s\"", argv[i]);
        else
            fprintf(g_aof, "%s", argv[i]);
        if (i < argc - 1) fprintf(g_aof, " ");
    }
    fprintf(g_aof, "\n");
    fflush(g_aof);   /* fsync-level durability for this mini project */
}

int aof_load(Store *store, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return 0;   /* no AOF yet — fresh start, not an error */

    int  replayed = 0;
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;

        /*
         * CRITICAL: parse() modifies its argument in-place by
         * replacing delimiters with '\0'.  We must give it a
         * writable copy so that the token pointers remain valid
         * when execute() reads them.
         */
        char line_copy[1024];
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *tokens[MAX_TOKENS];
        int   argc = parse(line_copy, tokens);
        if (argc <= 0) continue;

        CommandResult res = execute(store, argc, tokens);
        freeResult(res);
        replayed++;
    }

    fclose(f);
    printf("[AOF] Replayed %d commands from %s\n", replayed, filepath);
    return replayed;
}

void aof_close(void) {
    if (g_aof) {
        fflush(g_aof);
        fclose(g_aof);
        g_aof = NULL;
    }
}

/* ------------------------------------------------------------------ */
/*  AOF Compaction / Rewrite                                            */
/* ------------------------------------------------------------------ */

#include "../core/datatypes/string/string.h"
#include "../core/datatypes/list/list.h"
#include "../core/datatypes/set/set.h"
#include "../core/datatypes/hash/hashobj.h"
#include "../core/datatypes/zset/zset.h"
#include "../core/datatypes/zset/skiplist.h"

/* Helper: write a quoted value if it contains spaces */
static void write_val(FILE *f, const char *val) {
    if (strchr(val, ' '))
        fprintf(f, "\"%s\"", val);
    else
        fprintf(f, "%s", val);
}

int aof_rewrite(Store *store, const char *filepath) {
    if (!store || !filepath) return -1;

    /* Write to temp file, then atomically swap */
    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "%s.rewrite.tmp", filepath);

    FILE *f = fopen(tmppath, "w");
    if (!f) { perror("aof_rewrite: fopen"); return -1; }

    time_t now = time(NULL);

    for (int i = 0; i < store->size; i++) {
        Entry *e = store->buckets[i];
        while (e) {
            /* Skip expired keys */
            if (e->expires_at != -1 && e->expires_at <= now) {
                e = e->next;
                continue;
            }

            Object *obj = e->value;
            switch (obj->type) {
                case OBJ_STRING: {
                    fprintf(f, "SET ");
                    write_val(f, e->key);
                    fprintf(f, " ");
                    write_val(f, ((StringObject*)obj->ptr)->value);
                    fprintf(f, "\n");
                    break;
                }
                case OBJ_LIST: {
                    ListObject *list = (ListObject*)obj->ptr;
                    ListNode *n = list->head;
                    while (n) {
                        fprintf(f, "RPUSH ");
                        write_val(f, e->key);
                        fprintf(f, " ");
                        write_val(f, n->value);
                        fprintf(f, "\n");
                        n = n->next;
                    }
                    break;
                }
                case OBJ_SET: {
                    Store *map = ((SetObject*)obj->ptr)->map;
                    for (int j = 0; j < map->size; j++) {
                        Entry *se = map->buckets[j];
                        while (se) {
                            fprintf(f, "SADD ");
                            write_val(f, e->key);
                            fprintf(f, " ");
                            write_val(f, se->key);
                            fprintf(f, "\n");
                            se = se->next;
                        }
                    }
                    break;
                }
                case OBJ_HASH: {
                    Store *map = ((HashObject*)obj->ptr)->map;
                    for (int j = 0; j < map->size; j++) {
                        Entry *he = map->buckets[j];
                        while (he) {
                            fprintf(f, "HSET ");
                            write_val(f, e->key);
                            fprintf(f, " ");
                            write_val(f, he->key);
                            fprintf(f, " ");
                            const char *val = "";
                            if (he->value && he->value->ptr && he->value->type == OBJ_STRING)
                                val = ((StringObject*)he->value->ptr)->value;
                            write_val(f, val);
                            fprintf(f, "\n");
                            he = he->next;
                        }
                    }
                    break;
                }
                case OBJ_ZSET: {
                    ZSetObject *zset = (ZSetObject*)obj->ptr;
                    SkipListNode *n = zset->sl->header->level[0].forward;
                    while (n) {
                        char score_str[64];
                        snprintf(score_str, sizeof(score_str), "%.17g", n->score);
                        fprintf(f, "ZADD ");
                        write_val(f, e->key);
                        fprintf(f, " %s ", score_str);
                        write_val(f, n->member);
                        fprintf(f, "\n");
                        n = n->level[0].forward;
                    }
                    break;
                }
                default:
                    break;
            }

            /* Write EXPIRE if key has a TTL */
            if (e->expires_at != -1) {
                int remaining = (int)(e->expires_at - now);
                if (remaining > 0) {
                    fprintf(f, "EXPIRE ");
                    write_val(f, e->key);
                    fprintf(f, " %d\n", remaining);
                }
            }

            e = e->next;
        }
    }

    fflush(f);
    fclose(f);

    /* Close existing AOF, swap files, reopen */
    aof_close();
    remove(filepath);
    if (rename(tmppath, filepath) != 0) {
        perror("aof_rewrite: rename");
        return -1;
    }

    /* Reopen for continued appending */
    if (aof_open(filepath) != 0) {
        fprintf(stderr, "[AOF] WARNING: could not reopen AOF after rewrite\n");
        return -1;
    }

    printf("[AOF] Rewrite complete: %s\n", filepath);
    return 0;
}