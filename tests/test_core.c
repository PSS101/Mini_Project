/*
 * test_core.c — Unit tests for all 5 core data structures
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;
extern const char *g_current_suite;

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"

#define TEST_SUITE(name) do { g_current_suite=(name); printf("\n" CYAN "━━━ %s " RESET "\n",(name)); } while(0)
#define ASSERT_TRUE(e) do { g_tests_run++; if(e){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s\n",#e);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s\n",#e);} } while(0)
#define ASSERT_EQ_INT(a,b) do { int _a=(a),_b=(b); g_tests_run++; if(_a==_b){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==%d\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s got %d exp %d\n",#a,_a,_b);} } while(0)
#define ASSERT_EQ_STR(a,b) do { const char*_a=(a),*_b=(b); g_tests_run++; if(_a&&_b&&strcmp(_a,_b)==0){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s==\"%s\"\n",#a,_b);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s exp \"%s\" got \"%s\"\n",#a,_b,_a?_a:"(null)");} } while(0)
#define ASSERT_NULL(a) do { const void*_a=(a); g_tests_run++; if(!_a){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s NULL\n",#a);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s not NULL\n",#a);} } while(0)
#define ASSERT_NOT_NULL(a) do { const void*_a=(a); g_tests_run++; if(_a){g_tests_passed++;printf(GREEN "  ✓ " RESET "%s not NULL\n",#a);}else{g_tests_failed++;printf(RED "  ✗ " RESET "%s NULL\n",#a);} } while(0)

#include "../src/core/datatypes/string/string.h"
#include "../src/core/datatypes/list/list.h"
#include "../src/core/datatypes/set/set.h"
#include "../src/core/datatypes/hash/hashobj.h"
#include "../src/core/datatypes/zset/zset.h"
#include "../src/core/datatypes/zset/skiplist.h"
#include "../src/core/object.h"
#include "../src/core/store/store.h"

static void test_string(void) {
    TEST_SUITE("Core / String");
    StringObject *s = createStringObject("hello");
    ASSERT_NOT_NULL(s);
    ASSERT_EQ_STR(getStringValue(s), "hello");
    setStringValue(s, "world");
    ASSERT_EQ_STR(getStringValue(s), "world");
    freeStringObject(s);
}

static void test_list(void) {
    TEST_SUITE("Core / List (doubly-linked)");
    ListObject *l = createList();
    ASSERT_NOT_NULL(l);
    ASSERT_EQ_INT(l->length, 0);
    lpush(l, "a"); lpush(l, "b"); lpush(l, "c");
    ASSERT_EQ_INT(l->length, 3);
    ASSERT_EQ_STR(l->head->value, "c");
    ASSERT_EQ_STR(l->tail->value, "a");
    char *v = lpop(l); ASSERT_EQ_STR(v, "c"); free(v);
    rpush(l, "d");
    ASSERT_EQ_STR(l->tail->value, "d");
    v = rpop(l); ASSERT_EQ_STR(v, "d"); free(v);
    /* doubly-linked check */
    ASSERT_NULL(l->head->prev);
    ASSERT_NOT_NULL(l->head->next);
    v = lpop(l); free(v); v = lpop(l); free(v);
    ASSERT_EQ_INT(l->length, 0);
    ASSERT_NULL(lpop(l));
    freeList(l);
}

static void test_set(void) {
    TEST_SUITE("Core / Set");
    SetObject *s = createSet();
    ASSERT_EQ_INT(sadd(s, "apple"), 1);
    ASSERT_EQ_INT(sadd(s, "banana"), 1);
    ASSERT_EQ_INT(sadd(s, "apple"), 0);
    ASSERT_EQ_INT(sismember(s, "apple"), 1);
    ASSERT_EQ_INT(sismember(s, "cherry"), 0);
    freeSet(s);
}

static void test_hash(void) {
    TEST_SUITE("Core / Hash Map");
    HashObject *h = createHash();
    hset(h, "name", "redis");
    hset(h, "version", "1.0");
    ASSERT_EQ_STR(hget(h, "name"), "redis");
    ASSERT_EQ_STR(hget(h, "version"), "1.0");
    ASSERT_NULL(hget(h, "missing"));
    hset(h, "version", "2.0");
    ASSERT_EQ_STR(hget(h, "version"), "2.0");
    freeHash(h);
}

static void test_skiplist(void) {
    TEST_SUITE("Core / Skip List");
    SkipList *sl = skiplistCreate();
    ASSERT_EQ_INT((int)sl->length, 0);
    skiplistInsert(sl, 1.0, "one");
    skiplistInsert(sl, 3.0, "three");
    skiplistInsert(sl, 2.0, "two");
    ASSERT_EQ_INT((int)sl->length, 3);
    SkipListNode *n = skiplistFind(sl, 2.0, "two");
    ASSERT_NOT_NULL(n);
    ASSERT_NULL(skiplistFind(sl, 5.0, "five"));
    ASSERT_EQ_INT((int)skiplistGetRank(sl, 1.0, "one"), 1);
    ASSERT_EQ_INT((int)skiplistGetRank(sl, 3.0, "three"), 3);
    ASSERT_EQ_INT(skiplistDelete(sl, 2.0, "two"), 1);
    ASSERT_EQ_INT((int)sl->length, 2);
    ASSERT_EQ_INT(skiplistDelete(sl, 99.0, "x"), 0);
    skiplistFree(sl);
}

static void test_zset(void) {
    TEST_SUITE("Core / Sorted Set (ZSet)");
    ZSetObject *z = createZSet();
    ASSERT_EQ_INT(zadd(z, 10.0, "alice"), 1);
    ASSERT_EQ_INT(zadd(z, 20.0, "bob"), 1);
    ASSERT_EQ_INT(zadd(z, 5.0, "charlie"), 1);
    ASSERT_EQ_INT(zadd(z, 15.0, "alice"), 0); /* update */
    ASSERT_EQ_INT((int)zcard(z), 3);
    double sc; ASSERT_EQ_INT(zscore(z, "alice", &sc), 1);
    ASSERT_TRUE(sc == 15.0);
    ASSERT_EQ_INT(zscore(z, "missing", &sc), 0);
    /* order: charlie(5), alice(15), bob(20) */
    ASSERT_EQ_INT((int)zrank(z, "charlie"), 1);
    ASSERT_EQ_INT((int)zrank(z, "bob"), 3);
    char *r = zrange(z, 0, -1);
    ASSERT_EQ_STR(r, "charlie,alice,bob");
    free(r);
    ASSERT_EQ_INT(zrem(z, "alice"), 1);
    ASSERT_EQ_INT((int)zcard(z), 2);
    freeZSet(z);
}

static void test_store_ttl(void) {
    TEST_SUITE("Core / Store TTL");
    Store *s = createStore(16);
    setKey(s, "k1", createStringObj("v1"));
    ASSERT_NOT_NULL(getKey(s, "k1"));
    ASSERT_EQ_INT(ttlKey(s, "missing"), -2);
    ASSERT_EQ_INT(ttlKey(s, "k1"), -1);
    ASSERT_EQ_INT(setExpire(s, "k1", 3600), 1);
    int t = ttlKey(s, "k1");
    ASSERT_TRUE(t > 3590 && t <= 3600);
    ASSERT_EQ_INT(persistKey(s, "k1"), 1);
    ASSERT_EQ_INT(ttlKey(s, "k1"), -1);
    ASSERT_EQ_INT(setExpire(s, "nope", 100), 0);
    freeStore(s);
}

void test_core(void) {
    test_string();
    test_list();
    test_set();
    test_hash();
    test_skiplist();
    test_zset();
    test_store_ttl();
}
