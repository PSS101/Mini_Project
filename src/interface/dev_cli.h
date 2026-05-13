#ifndef DEV_CLI_H
#define DEV_CLI_H

/* In-process REPL: no networking, runs engine directly.
   Optionally loads from and saves to an AOF file.
   Pass NULL for aof_path to disable persistence. */
void start_dev_cli(const char *aof_path);

#endif
