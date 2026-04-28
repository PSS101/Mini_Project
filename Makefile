# ─────────────────────────────────────────────────────────
#  Mini Redis — Makefile
#  Targets:
#    make           → build all three binaries
#    make cli       → in-process dev REPL
#    make server    → TCP server
#    make client    → TCP client
#    make test      → build and run the test suite
#    make clean     → remove all build artefacts
# ─────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -g -Isrc
LDFLAGS = -lpthread

SRC = src

# ── Shared object files ────────────────────────────────────
CORE_OBJS = \
    $(SRC)/core/utils/hash.o \
    $(SRC)/core/datatypes/string/string.o \
    $(SRC)/core/datatypes/list/list.o \
    $(SRC)/core/datatypes/set/set.o \
    $(SRC)/core/datatypes/hash/hashobj.o \
    $(SRC)/core/datatypes/zset/skiplist.o \
    $(SRC)/core/datatypes/zset/zset.o \
    $(SRC)/core/object.o \
    $(SRC)/core/store/store.o

CMD_OBJS = \
    $(SRC)/commands/result.o \
    $(SRC)/commands/parser.o \
    $(SRC)/commands/help.o \
    $(SRC)/commands/dispatcher.o

ENGINE_OBJS = \
    $(SRC)/engine/engine.o

PERSIST_OBJS = \
    $(SRC)/persistence/aof.o \
    $(SRC)/persistence/rdb.o

CONFIG_OBJS = \
    $(SRC)/config/config.o

COMMON_OBJS = $(CORE_OBJS) $(CMD_OBJS) $(ENGINE_OBJS) $(PERSIST_OBJS) $(CONFIG_OBJS)

# ── Test object files ──────────────────────────────────────
TEST_OBJS = \
    tests/test_runner.o \
    tests/test_core.o \
    tests/test_commands.o \
    tests/test_persistence.o

# ── Targets ────────────────────────────────────────────────
.PHONY: all cli server client test clean

all: cli server client

cli: $(COMMON_OBJS) $(SRC)/interface/dev_cli.o $(SRC)/main.o
	$(CC) $(CFLAGS) -o miniredis $^ $(LDFLAGS)

server: $(COMMON_OBJS) $(SRC)/server/server.o $(SRC)/server_main.o
	$(CC) $(CFLAGS) -o miniredis-server $^ $(LDFLAGS)

client: $(SRC)/interface/client.o
	$(CC) $(CFLAGS) -o miniredis-client $< $(LDFLAGS)

test: $(COMMON_OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o miniredis-test $^ $(LDFLAGS)
	./miniredis-test

# ── Generic compile rule ───────────────────────────────────
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# compile tests with -I for project root
tests/%.o: tests/%.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

# ── Clean ─────────────────────────────────────────────────
clean:
	find $(SRC) -name "*.o" -delete
	find tests -name "*.o" -delete 2>/dev/null || true
	rm -f miniredis miniredis-server miniredis-client miniredis-test
	rm -f test_tmp.aof test_tmp.rdb test_tmp.conf
	rm -f miniredis.rdb.tmp
