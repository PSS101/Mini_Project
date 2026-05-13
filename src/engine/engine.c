#include "engine.h"
#include "../commands/parser.h"
#include "../commands/dispatcher.h"

Store* init_engine(int size) {
    return createStore(size);
}

void destroy_engine(Store *db) {
    freeStore(db);
}

CommandResult run_command(Store *db, char *input) {
    char *tokens[MAX_TOKENS];
    int   argc = parse(input, tokens);
    if (argc == -1) return error("ERR syntax error: unmatched quotes");
    if (argc ==  0) return error("ERR empty command");
    return execute(db, argc, tokens);
}
