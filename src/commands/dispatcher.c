#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "dispatcher.h"
#include "help.h"
#include "../core/object.h"
#include "../core/store/store.h"
#include "../core/datatypes/zset/zset.h"
#include "../persistence/rdb.h"

CommandResult execute(Store *store, int argc, char *argv[]) {
    if (argc == 0) return error("ERR empty command");

    const char *cmd = argv[0];

    /* ------ STRING ------ */
    if (strcmp(cmd, "SET") == 0) {
        if (argc < 3) return error("ERR wrong number of arguments for SET");
        setKey(store, argv[1], createStringObj(argv[2]));

        /* Optional: SET key value EX seconds */
        if (argc >= 5 && (strcmp(argv[3], "EX") == 0 || strcmp(argv[3], "ex") == 0)) {
            int secs = atoi(argv[4]);
            if (secs > 0) setExpire(store, argv[1], secs);
        }

        return ok("OK");
    }

    if (strcmp(cmd, "GET") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for GET");
        Object *obj = getKey(store, argv[1]);
        if (!obj) return ok("(nil)");
        if (obj->type != OBJ_STRING) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = getStringKey(store, argv[1]);
        return val ? ok(val) : ok("(nil)");
    }

    /* ------ LIST ------ */
    if (strcmp(cmd, "LPUSH") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for LPUSH");
        int res = lpushKey(store, argv[1], argv[2]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok("OK");
    }

    if (strcmp(cmd, "LPOP") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for LPOP");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_LIST)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = lpopKey(store, argv[1]);
        if (!val) return ok("(nil)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    if (strcmp(cmd, "RPUSH") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for RPUSH");
        int res = rpushKey(store, argv[1], argv[2]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok("OK");
    }

    if (strcmp(cmd, "RPOP") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for RPOP");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_LIST)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = rpopKey(store, argv[1]);
        if (!val) return ok("(nil)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    if (strcmp(cmd, "LRANGE") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for LRANGE");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_LIST)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = lrangeKey(store, argv[1]);
        if (!val) return ok("(empty list)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    /* ------ SET ------ */
    if (strcmp(cmd, "SADD") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for SADD");
        int res = saddKey(store, argv[1], argv[2]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok(res == 1 ? "1" : "0");
    }

    if (strcmp(cmd, "SISMEMBER") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for SISMEMBER");
        int res = sismemberKey(store, argv[1], argv[2]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok(res ? "1" : "0");
    }

    if (strcmp(cmd, "SMEMBERS") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for SMEMBERS");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_SET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = smembersKey(store, argv[1]);
        if (!val) return ok("(empty set)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    /* ------ HASH ------ */
    if (strcmp(cmd, "HSET") == 0) {
        if (argc != 4) return error("ERR wrong number of arguments for HSET");
        int res = hsetKey(store, argv[1], argv[2], argv[3]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok("OK");
    }

    if (strcmp(cmd, "HGET") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for HGET");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_HASH)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = hgetKey(store, argv[1], argv[2]);
        return val ? ok(val) : ok("(nil)");
    }

    if (strcmp(cmd, "HGETALL") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for HGETALL");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_HASH)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        char *val = hgetallKey(store, argv[1]);
        if (!val) return ok("(empty hash)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    /* ------ SORTED SET (ZSET) ------ */
    if (strcmp(cmd, "ZADD") == 0) {
        if (argc != 4) return error("ERR wrong number of arguments for ZADD");
        double score = atof(argv[2]);
        int res = zaddKey(store, argv[1], score, argv[3]);
        if (res == -2) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok(res == 1 ? "1" : "0");
    }

    if (strcmp(cmd, "ZSCORE") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for ZSCORE");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_ZSET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        double score;
        int found = zscoreKey(store, argv[1], argv[2], &score);
        if (!found) return ok("(nil)");
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", score);
        return ok(buf);
    }

    if (strcmp(cmd, "ZRANK") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for ZRANK");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_ZSET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        unsigned long rank = zrankKey(store, argv[1], argv[2]);
        if (rank == 0) return ok("(nil)");
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu", rank - 1);  /* 0-based rank */
        return ok(buf);
    }

    if (strcmp(cmd, "ZRANGE") == 0) {
        if (argc != 4) return error("ERR wrong number of arguments for ZRANGE");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_ZSET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        int start = atoi(argv[2]);
        int stop  = atoi(argv[3]);
        char *val = zrangeKey(store, argv[1], start, stop);
        if (!val) return ok("(empty sorted set)");
        CommandResult res = ok(val);
        free(val);
        return res;
    }

    if (strcmp(cmd, "ZREM") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for ZREM");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_ZSET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        int res = zremKey(store, argv[1], argv[2]);
        if (res == -1) return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        return ok(res ? "1" : "0");
    }

    if (strcmp(cmd, "ZCARD") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for ZCARD");
        Object *obj = getKey(store, argv[1]);
        if (obj && obj->type != OBJ_ZSET)
            return error("WRONGTYPE Operation against a key holding the wrong kind of value");
        unsigned long card = zcardKey(store, argv[1]);
        char buf[32];
        snprintf(buf, sizeof(buf), "%lu", card);
        return ok(buf);
    }

    /* ------ TTL / EXPIRY ------ */
    if (strcmp(cmd, "EXPIRE") == 0) {
        if (argc != 3) return error("ERR wrong number of arguments for EXPIRE");
        int seconds = atoi(argv[2]);
        if (seconds <= 0) return error("ERR invalid expire time");
        int res = setExpire(store, argv[1], seconds);
        return ok(res ? "1" : "0");
    }

    if (strcmp(cmd, "TTL") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for TTL");
        int ttl = ttlKey(store, argv[1]);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", ttl);
        return ok(buf);
    }

    if (strcmp(cmd, "PERSIST") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for PERSIST");
        int res = persistKey(store, argv[1]);
        return ok(res ? "1" : "0");
    }

    /* ------ UTILITY ------ */
    if (strcmp(cmd, "DEL") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for DEL");
        Object *obj = getKey(store, argv[1]);
        if (!obj) return ok("0");
        deleteKey(store, argv[1]);
        return ok("1");
    }

    if (strcmp(cmd, "TYPE") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for TYPE");
        Object *obj = getKey(store, argv[1]);
        if (!obj) return ok("none");
        switch (obj->type) {
            case OBJ_STRING: return ok("string");
            case OBJ_LIST:   return ok("list");
            case OBJ_SET:    return ok("set");
            case OBJ_HASH:   return ok("hash");
            case OBJ_ZSET:   return ok("zset");
            default:         return ok("unknown");
        }
    }

    if (strcmp(cmd, "EXISTS") == 0) {
        if (argc != 2) return error("ERR wrong number of arguments for EXISTS");
        return ok(getKey(store, argv[1]) ? "1" : "0");
    }

    /* ------ PERSISTENCE ------ */
    if (strcmp(cmd, "SAVE") == 0) {
        int res = rdb_save(store, "miniredis.rdb");
        return res == 0 ? ok("OK") : error("ERR RDB save failed");
    }

    if (strcmp(cmd, "BGSAVE") == 0) {
        int res = rdb_bgsave(store, "miniredis.rdb");
        return res == 0 ? ok("Background saving started") : error("ERR background save already in progress");
    }

    /* ------ HELP ------ */
    if (strcmp(cmd, "HELP") == 0) {
        if (argc == 1) return ok(getHelpGeneral());

        char arg[64];
        strncpy(arg, argv[1], sizeof(arg) - 1);
        arg[sizeof(arg) - 1] = '\0';
        for (int i = 0; arg[i]; i++) arg[i] = (char)toupper((unsigned char)arg[i]);

        const char *cat = getHelpCategory(arg);
        if (cat) return ok(cat);

        const char *cmdHelp = getHelpCommand(arg);
        if (cmdHelp) return ok(cmdHelp);

        return error("ERR no help available for that command");
    }

    return error("ERR unknown command");
}
