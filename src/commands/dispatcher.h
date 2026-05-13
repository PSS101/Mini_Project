#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "../core/store/store.h"
#include "result.h"

CommandResult execute(Store *store, int argc, char *argv[]);

#endif
