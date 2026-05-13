#include <string.h>
#include "parser.h"

int parse(char *input, char *tokens[]) {
    int argc = 0;
    int i    = 0;

    while (input[i] != '\0') {
        /* skip spaces */
        while (input[i] == ' ') i++;
        if (input[i] == '\0') break;

        if (argc >= MAX_TOKENS) break;

        if (input[i] == '"') {
            /* quoted token */
            i++;
            tokens[argc++] = &input[i];
            while (input[i] != '"' && input[i] != '\0') i++;
            if (input[i] == '\0') return -1;   /* unmatched quote */
            input[i] = '\0';
            i++;
        } else {
            /* normal token */
            tokens[argc++] = &input[i];
            while (input[i] != ' ' && input[i] != '\0') i++;
            if (input[i] != '\0') { input[i] = '\0'; i++; }
        }
    }
    return argc;
}
