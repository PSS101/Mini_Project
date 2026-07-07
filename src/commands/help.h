/*
 * Declares the help-system API for command documentation.
 * It exposes helpers that return general and command-specific help text.
 */

#ifndef HELP_H
#define HELP_H

const char* getHelpGeneral(void);
const char* getHelpCategory(const char *category);
const char* getHelpCommand(const char *cmd);

#endif
