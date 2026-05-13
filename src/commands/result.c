#include <stdlib.h>
#include <string.h>
#include "result.h"

CommandResult ok(const char *msg) {
    CommandResult res;
    res.status  = 1;
    res.message = msg ? strdup(msg) : strdup("");
    return res;
}

CommandResult error(const char *msg) {
    CommandResult res;
    res.status  = 0;
    res.message = msg ? strdup(msg) : strdup("ERR unknown error");
    return res;
}

void freeResult(CommandResult res) {
    if (res.message) free(res.message);
}
