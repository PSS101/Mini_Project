# Persistence folder

## aof.c
- aof_open(), aof_append(), aof_load(), aof_close(): manage AOF logging and replay.

## rdb.c
- rdb_save(), rdb_load(): create and restore snapshots.
- bgsave_thread(), rdb_bgsave(), rdb_bgsave_in_progress(): manage background saves.

## Workflow
The persistence layer makes the server durable by logging writes and creating snapshots.
