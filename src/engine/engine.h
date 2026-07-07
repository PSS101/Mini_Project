/*
 * Declares the engine interface used by the server and CLI.
 * It exposes initialization and command-execution helpers.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "../core/store/store.h"
#include "../commands/result.h"

Store*        init_engine(int size);
void          destroy_engine(Store *db);
CommandResult run_command(Store *db, char *input);

#endif
