# File system and workflow

## Overview
This repository implements a small Redis-like server in C. The code is organized by responsibility so each subsystem handles one part of the request lifecycle.

## Project layout
- src/main.c and src/server_main.c: application entry points.
- src/commands/: parses requests, builds replies, and provides help text.
- src/config/: loads server configuration values.
- src/core/: contains the shared object model, in-memory store, and utility helpers.
- src/core/datatypes/: implements string, list, set, hash, and sorted-set types.
- src/engine/: executes commands against the data store.
- src/interface/: provides the CLI and client entry points.
- src/persistence/: saves and restores state through AOF and RDB.
- src/server/: handles network connections and request dispatch.
- tests/: verifies parser, core, and persistence behavior.

## Runtime workflow
1. The program starts from one of the entry points.
2. Startup code initializes the engine, store, and configuration.
3. The server or CLI reads input and sends it to the parser.
4. The parser produces a structured command that the dispatcher routes to the engine.
5. The engine updates the store and any persistence layer.
6. A response object is built and returned to the caller.

## Why the folders exist
The folder split keeps concerns separate: parsing, storage, execution, networking, and durability each live in their own layer. This makes it easier to follow how a request moves through the system.

I'll list the supported commands and quick usage examples from the code (help text and parser). I'll fetch the help data next.

Read [](file:///home/mangoman/Projects/Mini_Project/src/commands/help.c)

Here’s a concise list of supported commands (from help.c) with short examples you can run in the dev CLI (miniredis) or against the server (e.g. `nc 127.0.0.1 6379`).

General help
- `HELP` — show categories
- `HELP STRING` — show string commands
- `HELP SET` — show set commands
- `HELP <command>` — show usage for one command

String
- `SET key value [EX secs]` — set a key (optionally with TTL)  
  Example: `SET mykey hello`  
- `GET key` — read a string  
  Example: `GET mykey`  
- `INCR key` / `DECR key` / `INCRBY key n` / `DECRBY key n`  
- `APPEND key value`  
- `MSET k1 v1 k2 v2` / `MGET k1 k2`

Lists
- `LPUSH key value` / `RPUSH key value` — push  
- `LPOP key` / `RPOP key` — pop  
- `LRANGE key start stop` — range (negative indices allowed)  
  Example: `LPUSH mylist a` then `LRANGE mylist 0 -1`

Sets
- `SADD key value` — add member  
- `SISMEMBER key value` — check membership  
- `SMEMBERS key` — list members

Hashes
- `HSET key field value`  
- `HGET key field`  
- `HGETALL key`

Sorted sets (zset)
- `ZADD key score member`  
- `ZSCORE key member`  
- `ZRANK key member` (0-based)  
- `ZRANGE key start stop`  
- `ZREM key member` / `ZCARD key`

TTL / Expiry
- `EXPIRE key seconds` — set TTL  
- `TTL key` — remaining TTL  
- `PERSIST key` — remove TTL

Utility
- `DEL key` — delete key  
- `TYPE key` — type of key  
- `EXISTS key` — 1 if exists, 0 otherwise  
- `KEYS pattern` — glob matches (use carefully)  
- `DBSIZE` — count keys  
- `PING [msg]` — health check  
- `INFO` — server info

Persistence
- `SAVE` — sync RDB save  
- `BGSAVE` — background RDB save  
- `BGREWRITEAOF` — rewrite/compact AOF

Pub/Sub
- `SUBSCRIBE ch [ch ...]` — subscribe (client will receive messages)  
- `UNSUBSCRIBE [ch ...]`  
- `PUBLISH channel message` — publish a message

Transactions
- `MULTI` — start transaction (queue commands)  
- `EXEC` — run queued commands atomically  
- `DISCARD` — abort queued transaction

Replication
- `REPLICAOF host port` — configure this node as a replica of master  
- `REPLICAOF NO ONE` — stop replication

Scripting
- `EVAL script numkeys [keys...] [args...]` — run Lua script atomically

How to try (examples)
- Dev CLI (single-threaded):
  - Start: miniredis
  - Then type:
    ```
    SET foo 1
    INCR foo
    GET foo
    EXIT
    ```
- Server (multi-threaded) example:
  - Start server: `./miniredis-server` (defaults from `miniredis.conf` or built-in defaults)
  - Connect with provided client: `./miniredis-client 127.0.0.1 6379`
  - Or use netcat for quick checks:
    ```
    printf "SET a b\nGET a\n" | nc 127.0.0.1 6379
    ```

Tip: Use `HELP` in the CLI to see categories, and `HELP <category>` or `HELP <command>` for detailed usage.Tip: Use `HELP` in the CLI to see categories, and `HELP <category>` or `HELP <command>` for detailed usage.


- Client usage: `./miniredis-client <host> <port>` — opens a line-based TCP session to the server and sends commands you type.

- Commands supported (same as server; type `HELP` or `HELP <category|command>` in the client):
  - Strings: `SET key value [EX secs]`, `GET key`, `INCR`, `DECR`, `INCRBY`, `DECRBY`, `APPEND`, `MSET`, `MGET`
  - Lists: `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN`
  - Sets: `SADD`, `SISMEMBER`, `SMEMBERS`
  - Hashes: `HSET`, `HGET`, `HGETALL`
  - Sorted sets: `ZADD`, `ZSCORE`, `ZRANK`, `ZRANGE`, `ZREM`, `ZCARD`
  - TTL: `EXPIRE`, `TTL`, `PERSIST`
  - Utility: `DEL`, `TYPE`, `EXISTS`, `KEYS`, `DBSIZE`, `PING`, `INFO`
  - Persistence: `SAVE`, `BGSAVE`, `BGREWRITEAOF`
  - Pub/Sub: `SUBSCRIBE ch [ch ...]`, `UNSUBSCRIBE [ch ...]`, `PUBLISH ch msg`  (note: `SUBSCRIBE` puts the client into subscriber mode)
  - Transactions: `MULTI`, `EXEC`, `DISCARD`
  - Replication: `REPLICAOF host port` / `REPLICAOF NO ONE`
  - Scripting: `EVAL script numkeys [keys...] [args...]`
  - Help: `HELP`, `HELP <category>`, `HELP <command>`

- Quick examples to run in the client:
  - `SET foo 1` then `GET foo`
  - `LPUSH mylist a` then `LRANGE mylist 0 -1`
  - `SADD myset a` then `SMEMBERS myset`
  - `HSET hm field val` then `HGET hm field`
  - `ZADD zs 1 member` then `ZRANGE zs 0 -1`

- Notes:
  - Use `HELP` in the client for category-specific command lists and usage.
  - `SUBSCRIBE` keeps the connection in pub/sub mode to receive messages.