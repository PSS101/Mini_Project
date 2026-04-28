#ifndef ENGINE_H
#define ENGINE_H

#include "../core/store/store.h"
#include "../commands/result.h"

Store*        init_engine(int size);
void          destroy_engine(Store *db);
CommandResult run_command(Store *db, char *input);

#endif
