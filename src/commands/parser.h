/*
 * Declares the parser interface and command structures.
 * It defines the data types used to represent parsed requests.
 */

#ifndef PARSER_H
#define PARSER_H

#define MAX_TOKENS 64

int parse(char *input, char *tokens[]);

#endif
