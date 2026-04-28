#ifndef SERVER_H
#define SERVER_H

#include "../config/config.h"

/* Start the TCP server using the given configuration.
   Blocks indefinitely, handling clients in detached threads. */
void start_server(const MiniRedisConfig *cfg);

#endif
