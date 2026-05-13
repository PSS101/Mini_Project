#include <string.h>
#include <ctype.h>
#include "help.h"

const char* getHelpGeneral(void) {
    return
        "Available commands:\n\n"
        "HELP [command|category]\n\n"
        "Categories:\n"
        "  STRING       LIST        SETS        HASH\n"
        "  ZSET         TTL         UTILITY     PERSISTENCE\n"
        "  PUBSUB       CLUSTER     REPLICATION SCRIPTING\n"
        "  TRANSACTION\n";
}

const char* getHelpCategory(const char *category) {
    if (strcmp(category, "STRING") == 0)
        return
            "STRING commands:\n"
            "  SET key value [EX secs]    : Set a string value\n"
            "  GET key                    : Get a string value\n"
            "  INCR key                   : Increment integer by 1\n"
            "  DECR key                   : Decrement integer by 1\n"
            "  INCRBY key amt             : Increment integer by amt\n"
            "  DECRBY key amt             : Decrement integer by amt\n"
            "  APPEND key value           : Append to string\n"
            "  MSET k1 v1 [k2 v2 ...]    : Set multiple keys\n"
            "  MGET k1 [k2 ...]          : Get multiple keys\n";

    if (strcmp(category, "LIST") == 0)
        return
            "LIST commands:\n"
            "  LPUSH  key val             : Push to head\n"
            "  LPOP   key                 : Pop from head\n"
            "  RPUSH  key val             : Push to tail\n"
            "  RPOP   key                 : Pop from tail\n"
            "  LRANGE key start stop      : Range (neg index ok)\n"
            "  LLEN   key                 : List length\n";

    if (strcmp(category, "SETS") == 0)
        return
            "SETS commands:\n"
            "  SADD     key val           : Add to set\n"
            "  SISMEMBER key val          : Check membership\n"
            "  SMEMBERS key               : All members\n";

    if (strcmp(category, "HASH") == 0)
        return
            "HASH commands:\n"
            "  HSET    key field val      : Set field\n"
            "  HGET    key field          : Get field\n"
            "  HGETALL key               : All fields+values\n";

    if (strcmp(category, "ZSET") == 0)
        return
            "ZSET commands:\n"
            "  ZADD   key score member    : Add with score\n"
            "  ZSCORE key member          : Get score\n"
            "  ZRANK  key member          : Get rank\n"
            "  ZRANGE key start stop      : Range by rank\n"
            "  ZREM   key member          : Remove\n"
            "  ZCARD  key                 : Count\n";

    if (strcmp(category, "TTL") == 0)
        return
            "TTL commands:\n"
            "  EXPIRE  key seconds        : Set expiry\n"
            "  TTL     key                : Remaining TTL\n"
            "  PERSIST key                : Remove TTL\n";

    if (strcmp(category, "UTILITY") == 0)
        return
            "UTILITY commands:\n"
            "  DEL    key                 : Delete key\n"
            "  TYPE   key                 : Key type\n"
            "  EXISTS key                 : Key exists?\n"
            "  KEYS   pattern             : Glob match keys\n"
            "  DBSIZE                     : Count all keys\n"
            "  PING [msg]                 : Health check\n"
            "  INFO                       : Server info\n";

    if (strcmp(category, "PERSISTENCE") == 0)
        return
            "PERSISTENCE commands:\n"
            "  SAVE                       : Sync RDB save\n"
            "  BGSAVE                     : Background RDB\n"
            "  BGREWRITEAOF               : Compact AOF\n";

    if (strcmp(category, "PUBSUB") == 0)
        return
            "PUB/SUB commands:\n"
            "  SUBSCRIBE ch [ch ...]      : Subscribe to channels\n"
            "  UNSUBSCRIBE [ch ...]       : Unsubscribe\n"
            "  PUBLISH ch message         : Publish to channel\n";

    if (strcmp(category, "TRANSACTION") == 0)
        return
            "TRANSACTION commands:\n"
            "  MULTI                      : Start transaction\n"
            "  EXEC                       : Execute queued cmds\n"
            "  DISCARD                    : Abort transaction\n";

    if (strcmp(category, "CLUSTER") == 0)
        return
            "CLUSTER commands:\n"
            "  CLUSTER INFO               : Cluster state\n"
            "  CLUSTER MYID               : Node ID\n"
            "  CLUSTER KEYSLOT key        : Hash slot for key\n"
            "  CLUSTER SLOTS              : Slot ownership\n";

    if (strcmp(category, "REPLICATION") == 0)
        return
            "REPLICATION commands:\n"
            "  REPLICAOF host port        : Replicate from master\n"
            "  REPLICAOF NO ONE           : Stop replication\n";

    if (strcmp(category, "SCRIPTING") == 0)
        return
            "SCRIPTING commands:\n"
            "  EVAL script numkeys [keys] [args]\n"
            "    Run Lua script atomically\n";

    return NULL;
}

const char* getHelpCommand(const char *cmd) {
    if (strcmp(cmd, "SET")       == 0) return "SET key value [EX s]     : Set string (optional TTL)";
    if (strcmp(cmd, "GET")       == 0) return "GET key                  : Get string value";
    if (strcmp(cmd, "INCR")      == 0) return "INCR key                 : Increment by 1";
    if (strcmp(cmd, "DECR")      == 0) return "DECR key                 : Decrement by 1";
    if (strcmp(cmd, "INCRBY")    == 0) return "INCRBY key amount        : Increment by amount";
    if (strcmp(cmd, "DECRBY")    == 0) return "DECRBY key amount        : Decrement by amount";
    if (strcmp(cmd, "APPEND")    == 0) return "APPEND key value         : Append to string";
    if (strcmp(cmd, "MSET")      == 0) return "MSET k1 v1 [k2 v2 ...]  : Set multiple keys";
    if (strcmp(cmd, "MGET")      == 0) return "MGET k1 [k2 ...]        : Get multiple keys";
    if (strcmp(cmd, "LPUSH")     == 0) return "LPUSH key value          : Push to list head";
    if (strcmp(cmd, "LPOP")      == 0) return "LPOP key                 : Pop from list head";
    if (strcmp(cmd, "RPUSH")     == 0) return "RPUSH key value          : Push to list tail";
    if (strcmp(cmd, "RPOP")      == 0) return "RPOP key                 : Pop from list tail";
    if (strcmp(cmd, "LRANGE")    == 0) return "LRANGE key [start stop]  : List range (neg idx ok)";
    if (strcmp(cmd, "LLEN")      == 0) return "LLEN key                 : List length";
    if (strcmp(cmd, "SADD")      == 0) return "SADD key value           : Add to set";
    if (strcmp(cmd, "SISMEMBER") == 0) return "SISMEMBER key value      : Check set membership";
    if (strcmp(cmd, "SMEMBERS")  == 0) return "SMEMBERS key             : All set members";
    if (strcmp(cmd, "HSET")      == 0) return "HSET key field value     : Set hash field";
    if (strcmp(cmd, "HGET")      == 0) return "HGET key field           : Get hash field";
    if (strcmp(cmd, "HGETALL")   == 0) return "HGETALL key              : All hash fields";
    if (strcmp(cmd, "ZADD")      == 0) return "ZADD key score member    : Add to sorted set";
    if (strcmp(cmd, "ZSCORE")    == 0) return "ZSCORE key member        : Get score";
    if (strcmp(cmd, "ZRANK")     == 0) return "ZRANK key member         : Get rank (0-based)";
    if (strcmp(cmd, "ZRANGE")    == 0) return "ZRANGE key start stop    : Range by rank";
    if (strcmp(cmd, "ZREM")      == 0) return "ZREM key member          : Remove from zset";
    if (strcmp(cmd, "ZCARD")     == 0) return "ZCARD key                : Zset cardinality";
    if (strcmp(cmd, "EXPIRE")    == 0) return "EXPIRE key seconds       : Set key expiry";
    if (strcmp(cmd, "TTL")       == 0) return "TTL key                  : Remaining TTL";
    if (strcmp(cmd, "PERSIST")   == 0) return "PERSIST key              : Remove expiry";
    if (strcmp(cmd, "DEL")       == 0) return "DEL key                  : Delete key";
    if (strcmp(cmd, "TYPE")      == 0) return "TYPE key                 : Key type";
    if (strcmp(cmd, "EXISTS")    == 0) return "EXISTS key               : Check existence";
    if (strcmp(cmd, "KEYS")      == 0) return "KEYS pattern             : Glob match (* ? [])";
    if (strcmp(cmd, "DBSIZE")    == 0) return "DBSIZE                   : Count all keys";
    if (strcmp(cmd, "PING")      == 0) return "PING [message]           : Health check";
    if (strcmp(cmd, "INFO")      == 0) return "INFO                     : Server info";
    if (strcmp(cmd, "SAVE")      == 0) return "SAVE                     : Sync RDB save";
    if (strcmp(cmd, "BGSAVE")    == 0) return "BGSAVE                   : Background RDB save";
    if (strcmp(cmd, "BGREWRITEAOF")==0) return "BGREWRITEAOF             : Compact AOF file";
    if (strcmp(cmd, "SUBSCRIBE") == 0) return "SUBSCRIBE ch [ch ...]    : Subscribe to channels";
    if (strcmp(cmd, "UNSUBSCRIBE")==0) return "UNSUBSCRIBE [ch ...]     : Unsubscribe";
    if (strcmp(cmd, "PUBLISH")   == 0) return "PUBLISH channel message  : Publish message";
    if (strcmp(cmd, "MULTI")     == 0) return "MULTI                    : Start transaction";
    if (strcmp(cmd, "EXEC")      == 0) return "EXEC                     : Execute transaction";
    if (strcmp(cmd, "DISCARD")   == 0) return "DISCARD                  : Abort transaction";
    if (strcmp(cmd, "CLUSTER")   == 0) return "CLUSTER INFO|MYID|KEYSLOT|SLOTS";
    if (strcmp(cmd, "REPLICAOF") == 0) return "REPLICAOF host port      : Set replication master";
    if (strcmp(cmd, "EVAL")      == 0) return "EVAL script numkeys [keys] [args]";
    return NULL;
}
