/*
 * dev_cli.c  —  Person C
 *
 * In-process REPL: calls the engine directly (no TCP).
 * Supports AOF persistence:
 *   - On start : aof_load() restores previous state.
 *   - On write : aof_append() records the command.
 *   - On EXIT  : aof_close() flushes and closes the file.
 *
 * Bug fixed: run_command() calls parse() internally, which modifies
 * the input buffer in-place.  We therefore parse a copy first to
 * capture tokens for aof_append(), then let run_command() operate
 * on the original — both see a pristine buffer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dev_cli.h"
#include "../engine/engine.h"
#include "../commands/result.h"
#include "../commands/parser.h"
#include "../persistence/aof.h"

#define MAX_INPUT 1024

void start_dev_cli(const char *aof_path) {
    Store *db = init_engine(256);
    if (!db) { fprintf(stderr, "Failed to initialise store\n"); return; }

    if (aof_path) {
        aof_load(db, aof_path);
        if (aof_open(aof_path) != 0)
            fprintf(stderr, "Warning: could not open AOF for writing\n");
    }

    char input[MAX_INPUT];
    printf("Mini Redis Dev CLI  (type EXIT to quit)\n");
    printf("Persistence: %s\n\n", aof_path ? aof_path : "disabled");

    while (1) {
        printf("mini-redis> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\r\n")] = '\0';

        if (strcmp(input, "EXIT") == 0) break;
        if (strlen(input) == 0) continue;

        /*
         * Parse a copy to get tokens for aof_append().
         * run_command() will call parse() again on the original —
         * that is fine because the original is still intact here.
         */
        char copy[MAX_INPUT];
        strncpy(copy, input, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *tokens[MAX_TOKENS];
        int   argc = parse(copy, tokens);

        /* Execute first so we only log commands that actually succeed */
        CommandResult res = run_command(db, input);

        if (res.status == 1 && aof_path && argc > 0)
            aof_append(argc, tokens);

        if (res.message && res.message[0])
            printf("%s\n", res.message);

        freeResult(res);
    }

    aof_close();
    destroy_engine(db);
    printf("Bye!\n");
}