#ifndef RESULT_H
#define RESULT_H

typedef struct {
    int   status;   /* 1 = OK, 0 = ERROR */
    char *message;  /* heap-allocated response string */
} CommandResult;

CommandResult ok(const char *msg);
CommandResult error(const char *msg);
void          freeResult(CommandResult res);

#endif
