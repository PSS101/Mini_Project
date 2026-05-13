#ifndef SCRIPTING_H
#define SCRIPTING_H

#include "../core/store/store.h"
#include "../commands/result.h"

/* Initialise the Lua interpreter (call once at startup). */
int  scripting_init(void);

/* Execute a Lua script: EVAL script numkeys key [key ...] arg [arg ...]
   Returns a CommandResult with the script's return value. */
CommandResult scripting_eval(Store *store, const char *script,
                             int numkeys, char **keys, char **args, int numargs);

/* Shut down the Lua interpreter. */
void scripting_close(void);

#endif
