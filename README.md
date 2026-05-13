# Mini Redis

A lightweight Redis-inspired in-memory database server written in **C**, built as a systems programming and database internals project.

This project implements several core Redis concepts including:

* In-memory key-value storage
* Multiple Redis-like data structures
* TCP server/client architecture
* Persistence using AOF and RDB
* Pub/Sub messaging
* Replication support
* Clustering utilities
* Lua scripting integration (optional)
* Background persistence operations
* Transaction support
* Command parser and dispatcher

The project is designed for learning database internals, networking, concurrency, memory management, and persistence mechanisms.

---

# Project Structure

```text
mini redis (experiment)/
│
├── src/
│   ├── cluster/            # Cluster utilities and slot management
│   ├── commands/           # Command parser, dispatcher, help system
│   ├── config/             # Configuration loading and parsing
│   ├── core/               # Core database objects and data structures
│   │   ├── datatypes/
│   │   │   ├── string/
│   │   │   ├── list/
│   │   │   ├── set/
│   │   │   ├── hash/
│   │   │   └── zset/
│   │   ├── store/
│   │   └── utils/
│   ├── engine/             # Database engine logic
│   ├── interface/          # CLI and TCP client implementation
│   ├── persistence/        # AOF and RDB persistence
│   ├── pubsub/             # Publish/Subscribe implementation
│   ├── replication/        # Master-replica synchronization
│   ├── scripting/          # Lua scripting support
│   ├── server/             # TCP server implementation
│   ├── main.c              # CLI entry point
│   └── server_main.c       # TCP server entry point
│
├── tests/                  # Unit and integration tests
│
├── Makefile                # Build system
├── miniredis.conf          # Default configuration
├── replica.conf            # Replica server configuration
├── miniredis.aof           # Append-only persistence file
├── miniredis.rdb           # Snapshot persistence file
└── README.md
```

---

# Features

## 1. Core Key-Value Store

Supports Redis-style key-value storage with fast in-memory operations.

### Supported Data Types

* Strings
* Lists
* Sets
* Hashes
* Sorted Sets (ZSets)

---

## 2. String Commands

| Command            | Description               |
| ------------------ | ------------------------- |
| `SET key value`    | Store a value             |
| `GET key`          | Retrieve a value          |
| `INCR key`         | Increment integer value   |
| `DECR key`         | Decrement integer value   |
| `INCRBY key n`     | Increment by custom value |
| `DECRBY key n`     | Decrement by custom value |
| `APPEND key value` | Append to existing string |
| `MSET k1 v1 k2 v2` | Set multiple values       |
| `MGET k1 k2`       | Retrieve multiple values  |

---

## 3. List Commands

| Command                 | Description           |
| ----------------------- | --------------------- |
| `LPUSH key value`       | Push element to head  |
| `RPUSH key value`       | Push element to tail  |
| `LPOP key`              | Pop from head         |
| `RPOP key`              | Pop from tail         |
| `LRANGE key start stop` | Get range of elements |
| `LLEN key`              | Get list length       |

---

## 4. Set Commands

| Command               | Description          |
| --------------------- | -------------------- |
| `SADD key value`      | Add member to set    |
| `SISMEMBER key value` | Check membership     |
| `SMEMBERS key`        | Retrieve all members |

---

## 5. Hash Commands

| Command                | Description          |
| ---------------------- | -------------------- |
| `HSET key field value` | Set hash field       |
| `HGET key field`       | Get field value      |
| `HGETALL key`          | Retrieve entire hash |

---

## 6. Sorted Set Commands

Implemented using skip lists.

| Command                 | Description           |
| ----------------------- | --------------------- |
| `ZADD key score member` | Add member with score |
| `ZSCORE key member`     | Get member score      |
| `ZRANK key member`      | Get member rank       |
| `ZRANGE key start stop` | Range query           |
| `ZREM key member`       | Remove member         |
| `ZCARD key`             | Count members         |

---

# Persistence

The project supports both major Redis persistence mechanisms.

## AOF (Append Only File)

Every write command is logged to a file.

### Advantages

* Better durability
* Replayable operation history
* Crash recovery support

### Files

```text
miniredis.aof
```

### Rewrite Support

```bash
BGREWRITEAOF
```

---

## RDB Snapshot Persistence

Creates binary snapshots of the in-memory database.

### Files

```text
miniredis.rdb
```

### Commands

```bash
SAVE
BGSAVE
```

---

# Expiry & TTL Support

Supports Redis-like expiration mechanisms.

| Command              | Description            |
| -------------------- | ---------------------- |
| `EXPIRE key seconds` | Set expiry time        |
| `TTL key`            | Retrieve remaining TTL |
| `PERSIST key`        | Remove expiration      |

---

# Key Management Commands

| Command        | Description      |
| -------------- | ---------------- |
| `DEL key`      | Delete key       |
| `TYPE key`     | Get key type     |
| `EXISTS key`   | Check existence  |
| `KEYS pattern` | Pattern matching |
| `DBSIZE`       | Count total keys |

---

# Networking Architecture

The project supports:

* Standalone development CLI
* TCP server mode
* Dedicated TCP client
* Multiple client connections
* Pub/Sub communication
* Replication

---

# Pub/Sub Messaging

Redis-style publish-subscribe implementation.

| Command                   | Description          |
| ------------------------- | -------------------- |
| `SUBSCRIBE channel`       | Subscribe to channel |
| `UNSUBSCRIBE channel`     | Unsubscribe          |
| `PUBLISH channel message` | Publish message      |

---

# Transactions

Basic transaction handling similar to Redis.

| Command   | Description             |
| --------- | ----------------------- |
| `MULTI`   | Start transaction       |
| `EXEC`    | Execute queued commands |
| `DISCARD` | Abort transaction       |

---

# Replication

Supports master-replica architecture.

## Replica Configuration

Example configuration:

```conf
port 6380
replicaof 127.0.0.1 6379
```

Run replica server:

```bash
./miniredis-server --config replica.conf
```

---

# Cluster Utilities

Cluster-related functionality includes:

* Slot calculations
* Node identification
* Cluster info commands

### Commands

```bash
CLUSTER INFO
CLUSTER MYID
CLUSTER KEYSLOT
CLUSTER SLOTS
```

---

# Lua Scripting Support

Optional Lua scripting integration.

## Enable Lua Support

Install Lua development libraries:

### Ubuntu/Debian

```bash
sudo apt install liblua5.4-dev
```

### Build with Lua

```bash
make ENABLE_LUA=1
```

### Script Execution

```bash
EVAL script numkeys [keys] [args]
```

---

# Configuration

The server loads configuration from:

```text
miniredis.conf
```

or a custom file:

```bash
./miniredis-server --config custom.conf
```

---

# Build Instructions

## Requirements

### Linux Dependencies

* GCC
* Make
* POSIX Threads
* Optional: Lua 5.4 Development Package

### Install Build Tools

#### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential make gcc
```

---

# Compilation

## Build All Components

```bash
make
```

This builds:

* `miniredis` → Local development CLI
* `miniredis-server` → TCP server
* `miniredis-client` → TCP client

---

## Build Individual Components

### CLI

```bash
make cli
```

### Server

```bash
make server
```

### Client

```bash
make client
```

### Test Suite

```bash
make test
```

### Clean Build Artifacts

```bash
make clean
```

---

# Running the Project

## 1. Start Local CLI

```bash
./miniredis
```

With AOF persistence:

```bash
./miniredis miniredis.aof
```

---

## 2. Start TCP Server

Using default configuration:

```bash
./miniredis-server
```

Using custom port:

```bash
./miniredis-server 6380
```

Using custom configuration:

```bash
./miniredis-server --config miniredis.conf
```

---

## 3. Start TCP Client

```bash
./miniredis-client
```

---

# Example Usage

## String Operations

```bash
SET name redis
GET name
INCR counter
```

---

## List Operations

```bash
LPUSH mylist one
RPUSH mylist two
LRANGE mylist 0 -1
```

---

## Set Operations

```bash
SADD users alice
SADD users bob
SMEMBERS users
```

---

## Hash Operations

```bash
HSET user:1 name Alice
HSET user:1 age 22
HGETALL user:1
```

---

## Sorted Set Operations

```bash
ZADD scores 100 alice
ZADD scores 200 bob
ZRANGE scores 0 -1
```

---

## Expiry Operations

```bash
SET session token
EXPIRE session 60
TTL session
```

---

# Testing

The project contains dedicated test files:

```text
tests/
├── test_core.c
├── test_commands.c
├── test_persistence.c
└── test_runner.c
```

Run all tests:

```bash
make test
```

---

# Internal Architecture

## Core Components

### 1. Command Layer

Handles:

* Command parsing
* Argument validation
* Dispatching to handlers
* Result formatting

Files:

```text
src/commands/
```

---

### 2. Engine Layer

Responsible for:

* Executing database operations
* Managing object lifecycles
* Handling concurrency and storage logic

Files:

```text
src/engine/
```

---

### 3. Persistence Layer

Implements:

* AOF logging
* RDB snapshots
* Recovery mechanisms
* Background persistence tasks

Files:

```text
src/persistence/
```

---

### 4. Networking Layer

Implements:

* TCP server
* Client communication
* Multi-client handling
* Request processing

Files:

```text
src/server/
src/interface/
```

---

# Design Goals

This project was developed to explore:

* Systems programming in C
* Memory-efficient data structures
* Database internals
* Persistence algorithms
* Network programming
* Concurrency
* Redis architecture concepts
* Distributed systems fundamentals

---

# Future Improvements

Potential enhancements include:

* Full RESP protocol support
* Better cluster coordination
* Replication failover
* Authentication and ACLs
* Advanced Lua sandboxing
* Disk compaction improvements
* Optimized memory allocator
* Benchmarking suite
* Web dashboard
* Docker deployment

---

# Technologies Used

* C Programming Language
* POSIX Threads
* GCC
* Makefile Build System
* Lua 5.4 (optional)
* TCP Sockets

---

# Learning Outcomes

This project demonstrates concepts related to:

* In-memory databases
* Distributed systems
* Networking
* Data structures
* Operating systems
* Persistence models
* Concurrent programming
* Database design

---
