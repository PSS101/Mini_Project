/*
 * test_persistence.c — AOF and RDB round-trip tests
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

#define TEST_SUITE(name) printf("\n" CYAN "━━━ %s " RESET "\n",(name))
#define ASSERT_TRUE(e) do { g_tests_run++; if(e){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s\n",#e);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s\n",#e);} } while(0)
#define ASSERT_EQ_INT(a,b) do { int _a=(a),_b=(b); g_tests_run++; if(_a==_b){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==%d\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s got %d exp %d\n",#a,_a,_b);} } while(0)
#define ASSERT_EQ_STR(a,b) do { const char*_a=(a),*_b=(b); g_tests_run++; if(_a&&_b&&strcmp(_a,_b)==0){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==\"%s\"\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s exp \"%s\" got \"%s\"\n",#a,_b,_a?_a:"(null)");} } while(0)
#define ASSERT_NOT_NULL(a) do { const void*_a=(a); g_tests_run++; if(_a){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s not NULL\n",#a);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s NULL\n",#a);} } while(0)

#include "../src/engine/engine.h"
#include "../src/commands/result.h"
#include "../src/persistence/aof.h"
#include "../src/persistence/rdb.h"
#include "../src/core/store/store.h"
#include "../src/core/object.h"
#include "../src/core/datatypes/string/string.h"
#include "../src/config/config.h"

#define TEST_AOF_FILE "test_tmp.aof"
#define TEST_RDB_FILE "test_tmp.rdb"

static void test_aof_roundtrip(void) {
    TEST_SUITE("Persistence / AOF Round-Trip");

    /* Clean up */
    remove(TEST_AOF_FILE);

    /* Write phase */
    Store *db1 = init_engine(64);
    char b1[] = "SET name alice";
    run_command(db1, b1);
    char b2[] = "SET age 30";
    run_command(db1, b2);
    char b3[] = "LPUSH fruits apple";
    run_command(db1, b3);

    /* Manually write AOF */
    ASSERT_EQ_INT(aof_open(TEST_AOF_FILE), 0);
    char *t1[] = {"SET","name","alice"};  aof_append(3, t1);
    char *t2[] = {"SET","age","30"};      aof_append(3, t2);
    char *t3[] = {"LPUSH","fruits","apple"}; aof_append(3, t3);
    aof_close();
    destroy_engine(db1);

    /* Reload phase */
    Store *db2 = init_engine(64);
    int replayed = aof_load(db2, TEST_AOF_FILE);
    ASSERT_TRUE(replayed >= 3);

    /* Verify data */
    char g1[] = "GET name";
    CommandResult r = run_command(db2, g1);
    ASSERT_EQ_STR(r.message, "alice");
    freeResult(r);

    char g2[] = "GET age";
    r = run_command(db2, g2);
    ASSERT_EQ_STR(r.message, "30");
    freeResult(r);

    destroy_engine(db2);
    remove(TEST_AOF_FILE);
}

static void test_rdb_roundtrip(void) {
    TEST_SUITE("Persistence / RDB Round-Trip");

    remove(TEST_RDB_FILE);

    /* Create and populate store */
    Store *db1 = init_engine(64);
    char c1[] = "SET greeting hello";
    run_command(db1, c1);
    char c2[] = "LPUSH mylist x";
    run_command(db1, c2);
    char c3[] = "LPUSH mylist y";
    run_command(db1, c3);
    char c4[] = "SADD myset a";
    run_command(db1, c4);
    char c5[] = "HSET myhash f1 v1";
    run_command(db1, c5);
    char c6[] = "ZADD myzset 1.5 member1";
    run_command(db1, c6);

    /* Save RDB */
    ASSERT_EQ_INT(rdb_save(db1, TEST_RDB_FILE), 0);
    destroy_engine(db1);

    /* Load into fresh store */
    Store *db2 = init_engine(64);
    int loaded = rdb_load(db2, TEST_RDB_FILE);
    ASSERT_TRUE(loaded >= 5);

    /* Verify string */
    char g1[] = "GET greeting";
    CommandResult r = run_command(db2, g1);
    ASSERT_EQ_STR(r.message, "hello");
    freeResult(r);

    /* Verify list */
    char g2[] = "LRANGE mylist";
    r = run_command(db2, g2);
    ASSERT_EQ_STR(r.message, "y,x");
    freeResult(r);

    /* Verify set */
    char g3[] = "SISMEMBER myset a";
    r = run_command(db2, g3);
    ASSERT_EQ_STR(r.message, "1");
    freeResult(r);

    /* Verify hash */
    char g4[] = "HGET myhash f1";
    r = run_command(db2, g4);
    ASSERT_EQ_STR(r.message, "v1");
    freeResult(r);

    /* Verify zset */
    char g5[] = "ZCARD myzset";
    r = run_command(db2, g5);
    ASSERT_EQ_STR(r.message, "1");
    freeResult(r);

    destroy_engine(db2);
    remove(TEST_RDB_FILE);
}

static void test_config(void) {
    TEST_SUITE("Persistence / Config Loader");

    /* Write a test config */
    FILE *f = fopen("test_tmp.conf", "w");
    fprintf(f, "# test config\n");
    fprintf(f, "port 9999\n");
    fprintf(f, "store-size 512\n");
    fprintf(f, "aof-enabled no\n");
    fprintf(f, "rdb-file custom.rdb\n");
    fclose(f);

    MiniRedisConfig cfg;
    config_load(&cfg, "test_tmp.conf");

    ASSERT_EQ_INT(cfg.port, 9999);
    ASSERT_EQ_INT(cfg.store_size, 512);
    ASSERT_EQ_INT(cfg.aof_enabled, 0);
    ASSERT_EQ_STR(cfg.rdb_file, "custom.rdb");

    remove("test_tmp.conf");
}

void test_persistence(void) {
    test_aof_roundtrip();
    test_rdb_roundtrip();
    test_config();
}
