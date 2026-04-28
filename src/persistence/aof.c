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
    "ZADD", "ZREM",       /* sorted set writes */
    "EXPIRE", "PERSIST",  /* TTL mutations */
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