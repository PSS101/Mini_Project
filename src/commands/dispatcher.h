/*
 * Declares the dispatcher interface used by the command layer.
 * It exposes the helpers needed to route parsed requests.
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "../core/store/store.h"
#include "result.h"

CommandResult execute(Store *store, int argc, char *argv[]);

#endif
