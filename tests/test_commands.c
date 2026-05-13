/*
 * test_commands.c — Integration tests for the command dispatcher
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

#define TEST_SUITE(name) printf("\n" CYAN "━━━ %s " RESET "\n",(name))
#define ASSERT_TRUE(e) do { g_tests_run++; if(e){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s\n",#e);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s\n",#e);} } while(0)
#define ASSERT_EQ_INT(a,b) do { int _a=(a),_b=(b); g_tests_run++; if(_a==_b){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==%d\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s got %d exp %d\n",#a,_a,_b);} } while(0)
#define ASSERT_EQ_STR(a,b) do { const char*_a=(a),*_b=(b); g_tests_run++; if(_a&&_b&&strcmp(_a,_b)==0){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==\"%s\"\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s exp \"%s\" got \"%s\"\n",#a,_b,_a?_a:"(null)");} } while(0)

#include "../src/engine/engine.h"
#include "../src/commands/result.h"

static void run_ok(Store *db, const char *input, const char *expected) {
    char buf[256];
    strncpy(buf, input, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    CommandResult r = run_command(db, buf);
    ASSERT_EQ_INT(r.status, 1);
    if (expected) ASSERT_EQ_STR(r.message, expected);
    freeResult(r);
}

static void run_err(Store *db, const char *input) {
    char buf[256];
    strncpy(buf, input, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    CommandResult r = run_command(db, buf);
    ASSERT_EQ_INT(r.status, 0);
    freeResult(r);
}

static void test_string_cmds(void) {
    TEST_SUITE("Commands / String");
    Store *db = init_engine(64);
    run_ok(db, "SET mykey hello", "OK");
    run_ok(db, "GET mykey", "hello");
    run_ok(db, "GET missing", "(nil)");
    run_ok(db, "SET mykey world", "OK");
    run_ok(db, "GET mykey", "world");
    destroy_engine(db);
}

static void test_list_cmds(void) {
    TEST_SUITE("Commands / List");
    Store *db = init_engine(64);
    run_ok(db, "LPUSH mylist a", "OK");
    run_ok(db, "LPUSH mylist b", "OK");
    run_ok(db, "RPUSH mylist c", "OK");
    run_ok(db, "LRANGE mylist", "b,a,c");
    run_ok(db, "LPOP mylist", "b");
    run_ok(db, "RPOP mylist", "c");
    destroy_engine(db);
}

static void test_set_cmds(void) {
    TEST_SUITE("Commands / Set");
    Store *db = init_engine(64);
    run_ok(db, "SADD myset x", "1");
    run_ok(db, "SADD myset x", "0");
    run_ok(db, "SISMEMBER myset x", "1");
    run_ok(db, "SISMEMBER myset z", "0");
    destroy_engine(db);
}

static void test_hash_cmds(void) {
    TEST_SUITE("Commands / Hash");
    Store *db = init_engine(64);
    run_ok(db, "HSET myhash f1 v1", "OK");
    run_ok(db, "HGET myhash f1", "v1");
    run_ok(db, "HGET myhash missing", "(nil)");
    destroy_engine(db);
}

static void test_zset_cmds(void) {
    TEST_SUITE("Commands / Sorted Set");
    Store *db = init_engine(64);
    run_ok(db, "ZADD zs 1.0 alice", "1");
    run_ok(db, "ZADD zs 2.0 bob", "1");
    run_ok(db, "ZADD zs 3.0 charlie", "1");
    run_ok(db, "ZCARD zs", "3");
    run_ok(db, "ZRANK zs alice", "0");
    run_ok(db, "ZRANK zs charlie", "2");
    run_ok(db, "ZRANGE zs 0 -1", "alice,bob,charlie");
    run_ok(db, "ZRANGE zs 0 0", "alice");
    run_ok(db, "ZREM zs bob", "1");
    run_ok(db, "ZCARD zs", "2");
    /* ZSCORE */
    {
        char buf[] = "ZSCORE zs alice";
        CommandResult r = run_command(db, buf);
        ASSERT_EQ_INT(r.status, 1);
        ASSERT_TRUE(atof(r.message) == 1.0);
        freeResult(r);
    }
    destroy_engine(db);
}

static void test_ttl_cmds(void) {
    TEST_SUITE("Commands / TTL");
    Store *db = init_engine(64);
    run_ok(db, "SET k v", "OK");
    run_ok(db, "TTL k", "-1");
    run_ok(db, "EXPIRE k 100", "1");
    {
        char buf[] = "TTL k";
        CommandResult r = run_command(db, buf);
        int ttl = atoi(r.message);
        ASSERT_TRUE(ttl > 90 && ttl <= 100);
        freeResult(r);
    }
    run_ok(db, "PERSIST k", "1");
    run_ok(db, "TTL k", "-1");
    run_ok(db, "TTL missing", "-2");
    run_ok(db, "EXPIRE missing 10", "0");
    destroy_engine(db);
}

static void test_utility_cmds(void) {
    TEST_SUITE("Commands / Utility");
    Store *db = init_engine(64);
    run_ok(db, "SET k v", "OK");
    run_ok(db, "EXISTS k", "1");
    run_ok(db, "TYPE k", "string");
    run_ok(db, "DEL k", "1");
    run_ok(db, "EXISTS k", "0");
    run_ok(db, "DEL k", "0");
    run_ok(db, "TYPE k", "none");
    destroy_engine(db);
}

static void test_wrongtype(void) {
    TEST_SUITE("Commands / WRONGTYPE");
    Store *db = init_engine(64);
    run_ok(db, "SET k v", "OK");
    run_err(db, "LPUSH k x");
    run_err(db, "SADD k x");
    run_err(db, "HSET k f v");
    run_err(db, "ZADD k 1.0 x");
    /* INCR on non-integer string */
    run_ok(db, "SET strkey hello", "OK");
    run_err(db, "INCR strkey");
    /* APPEND on wrong type */
    run_ok(db, "LPUSH listk val", "OK");
    run_err(db, "APPEND listk suffix");
    destroy_engine(db);
}

static void test_incr_decr_cmds(void) {
    TEST_SUITE("Commands / INCR/DECR/INCRBY/DECRBY");
    Store *db = init_engine(64);

    /* INCR on non-existent key creates it with value 1 */
    run_ok(db, "INCR counter", "1");
    run_ok(db, "INCR counter", "2");
    run_ok(db, "INCR counter", "3");

    /* DECR */
    run_ok(db, "DECR counter", "2");
    run_ok(db, "DECR counter", "1");
    run_ok(db, "DECR counter", "0");
    run_ok(db, "DECR counter", "-1");

    /* INCRBY */
    run_ok(db, "SET x 10", "OK");
    run_ok(db, "INCRBY x 5", "15");
    run_ok(db, "INCRBY x 100", "115");

    /* DECRBY */
    run_ok(db, "DECRBY x 15", "100");
    run_ok(db, "DECRBY x 200", "-100");

    /* INCR on non-existent creates from 0 */
    run_ok(db, "DECR fresh", "-1");

    /* INCR on wrong type fails */
    run_ok(db, "LPUSH mylist a", "OK");
    run_err(db, "INCR mylist");

    /* INCR on non-integer string fails */
    run_ok(db, "SET words hello", "OK");
    run_err(db, "INCR words");

    destroy_engine(db);
}

static void test_append_cmds(void) {
    TEST_SUITE("Commands / APPEND");
    Store *db = init_engine(64);

    /* APPEND on non-existent key creates it */
    run_ok(db, "APPEND msg hello", "5");
    run_ok(db, "GET msg", "hello");

    /* APPEND to existing key concatenates */
    run_ok(db, "APPEND msg world", "10");
    run_ok(db, "GET msg", "helloworld");

    /* APPEND on wrong type fails */
    run_ok(db, "LPUSH list a", "OK");
    run_err(db, "APPEND list suffix");

    destroy_engine(db);
}

static void test_mset_mget_cmds(void) {
    TEST_SUITE("Commands / MSET/MGET");
    Store *db = init_engine(64);

    /* MSET multiple keys */
    run_ok(db, "MSET k1 v1 k2 v2 k3 v3", "OK");
    run_ok(db, "GET k1", "v1");
    run_ok(db, "GET k2", "v2");
    run_ok(db, "GET k3", "v3");

    /* MGET multiple keys */
    run_ok(db, "MGET k1 k2 k3", "v1,v2,v3");

    /* MGET with missing keys shows (nil) */
    run_ok(db, "MGET k1 missing k3", "v1,(nil),v3");

    /* MSET overwrites existing */
    run_ok(db, "MSET k1 new1 k2 new2", "OK");
    run_ok(db, "GET k1", "new1");
    run_ok(db, "GET k2", "new2");

    destroy_engine(db);
}

static void test_lrange_idx_cmds(void) {
    TEST_SUITE("Commands / LRANGE with indices");
    Store *db = init_engine(64);

    run_ok(db, "RPUSH mylist a", "OK");
    run_ok(db, "RPUSH mylist b", "OK");
    run_ok(db, "RPUSH mylist c", "OK");
    run_ok(db, "RPUSH mylist d", "OK");
    run_ok(db, "RPUSH mylist e", "OK");

    /* Full range with 0 -1 */
    run_ok(db, "LRANGE mylist 0 -1", "a,b,c,d,e");

    /* Subrange */
    run_ok(db, "LRANGE mylist 1 3", "b,c,d");

    /* Single element */
    run_ok(db, "LRANGE mylist 0 0", "a");

    /* Negative indices */
    run_ok(db, "LRANGE mylist -2 -1", "d,e");

    /* Out of bounds clamped */
    run_ok(db, "LRANGE mylist 0 100", "a,b,c,d,e");

    /* Invalid range returns empty */
    run_ok(db, "LRANGE mylist 3 1", "(empty list)");

    /* Legacy LRANGE (no indices) still works */
    run_ok(db, "LRANGE mylist", "a,b,c,d,e");

    destroy_engine(db);
}

static void test_llen_cmds(void) {
    TEST_SUITE("Commands / LLEN");
    Store *db = init_engine(64);

    run_ok(db, "LLEN missing", "0");
    run_ok(db, "RPUSH mylist a", "OK");
    run_ok(db, "RPUSH mylist b", "OK");
    run_ok(db, "RPUSH mylist c", "OK");
    run_ok(db, "LLEN mylist", "3");

    /* LLEN on wrong type */
    run_ok(db, "SET k v", "OK");
    run_err(db, "LLEN k");

    destroy_engine(db);
}

static void test_keys_cmds(void) {
    TEST_SUITE("Commands / KEYS pattern");
    Store *db = init_engine(64);

    run_ok(db, "SET user:1 alice", "OK");
    run_ok(db, "SET user:2 bob", "OK");
    run_ok(db, "SET user:3 charlie", "OK");
    run_ok(db, "SET session:abc token1", "OK");
    run_ok(db, "SET session:def token2", "OK");

    /* KEYS with * at end */
    {
        char buf[] = "KEYS user:*";
        CommandResult r = run_command(db, buf);
        ASSERT_EQ_INT(r.status, 1);
        /* Should contain all three user keys - check that they're present */
        ASSERT_TRUE(strstr(r.message, "user:1") != NULL);
        ASSERT_TRUE(strstr(r.message, "user:2") != NULL);
        ASSERT_TRUE(strstr(r.message, "user:3") != NULL);
        ASSERT_TRUE(strstr(r.message, "session") == NULL);
        freeResult(r);
    }

    /* KEYS with ? wildcard */
    {
        char buf[] = "KEYS user:?";
        CommandResult r = run_command(db, buf);
        ASSERT_EQ_INT(r.status, 1);
        ASSERT_TRUE(strstr(r.message, "user:1") != NULL);
        freeResult(r);
    }

    /* KEYS * matches everything */
    {
        char buf[] = "KEYS *";
        CommandResult r = run_command(db, buf);
        ASSERT_EQ_INT(r.status, 1);
        ASSERT_TRUE(strstr(r.message, "user:") != NULL);
        ASSERT_TRUE(strstr(r.message, "session:") != NULL);
        freeResult(r);
    }

    /* KEYS with no match */
    run_ok(db, "KEYS zzz*", "(empty list)");

    destroy_engine(db);
}

static void test_dbsize_ping_cmds(void) {
    TEST_SUITE("Commands / DBSIZE+PING");
    Store *db = init_engine(64);

    run_ok(db, "DBSIZE", "0");
    run_ok(db, "SET a 1", "OK");
    run_ok(db, "SET b 2", "OK");
    run_ok(db, "SET c 3", "OK");
    run_ok(db, "DBSIZE", "3");
    run_ok(db, "DEL b", "1");
    run_ok(db, "DBSIZE", "2");

    run_ok(db, "PING", "PONG");
    run_ok(db, "PING hello", "hello");

    destroy_engine(db);
}

void test_commands(void) {
    test_string_cmds();
    test_list_cmds();
    test_set_cmds();
    test_hash_cmds();
    test_zset_cmds();
    test_ttl_cmds();
    test_utility_cmds();
    test_wrongtype();
    test_incr_decr_cmds();
    test_append_cmds();
    test_mset_mget_cmds();
    test_lrange_idx_cmds();
    test_llen_cmds();
    test_keys_cmds();
    test_dbsize_ping_cmds();
}

