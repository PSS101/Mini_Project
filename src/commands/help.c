#include <string.h>
#include <ctype.h>
#include "help.h"

const char* getHelpGeneral(void) {
    return
        "Available commands:\n\n"
        "HELP [command|category]\n\n"
        "Categories:\n"
        "  STRING\n"
        "  LIST\n"
        "  SETS\n"
        "  HASH\n"
        "  ZSET\n"
        "  TTL\n"
        "  UTILITY\n"
        "  PERSISTENCE\n";
}

const char* getHelpCategory(const char *category) {
    if (strcmp(category, "STRING") == 0)
        return
            "STRING commands:\n"
            "  SET key value [EX seconds] : Set a string value (optional TTL)\n"
            "  GET key                    : Get a string value\n";

    if (strcmp(category, "LIST") == 0)
        return
            "LIST commands:\n"
            "  LPUSH key val        : Push to head of list\n"
            "  LPOP  key            : Pop from head of list\n"
            "  RPUSH key val        : Push to tail of list\n"
            "  RPOP  key            : Pop from tail of list\n"
            "  LRANGE key           : Get all list elements\n";

    if (strcmp(category, "SETS") == 0)
        return
            "SETS commands:\n"
            "  SADD     key val     : Add element to set\n"
            "  SISMEMBER key val    : Check membership\n"
            "  SMEMBERS key         : Get all set members\n";

    if (strcmp(category, "HASH") == 0)
        return
            "HASH commands:\n"
            "  HSET    key field val : Set hash field\n"
            "  HGET    key field     : Get hash field\n"
            "  HGETALL key           : Get all fields and values\n";

    if (strcmp(category, "ZSET") == 0)
        return
            "ZSET (Sorted Set) commands:\n"
            "  ZADD   key score member : Add member with score\n"
            "  ZSCORE key member       : Get score of member\n"
            "  ZRANK  key member       : Get 0-based rank of member\n"
            "  ZRANGE key start stop   : Get members in rank range\n"
            "  ZREM   key member       : Remove member\n"
            "  ZCARD  key              : Get number of members\n";

    if (strcmp(category, "TTL") == 0)
        return
            "TTL commands:\n"
            "  EXPIRE  key seconds : Set key expiry (seconds)\n"
            "  TTL     key         : Get remaining TTL (-1=none, -2=missing)\n"
            "  PERSIST key         : Remove TTL from key\n";

    if (strcmp(category, "UTILITY") == 0)
        return
            "UTILITY commands:\n"
            "  DEL    key : Delete a key\n"
            "  TYPE   key : Get the type of a key\n"
            "  EXISTS key : Check if a key exists\n";

    if (strcmp(category, "PERSISTENCE") == 0)
        return
            "PERSISTENCE commands:\n"
            "  SAVE   : Synchronous RDB snapshot\n"
            "  BGSAVE : Background RDB snapshot\n";

    return NULL;
}

const char* getHelpCommand(const char *cmd) {
    if (strcmp(cmd, "SET")       == 0) return "SET key value [EX seconds] : Set a string value (optional TTL)";
    if (strcmp(cmd, "GET")       == 0) return "GET key                    : Get a string value";
    if (strcmp(cmd, "LPUSH")     == 0) return "LPUSH key value            : Push value to head of list";
    if (strcmp(cmd, "LPOP")      == 0) return "LPOP key                   : Pop value from head of list";
    if (strcmp(cmd, "RPUSH")     == 0) return "RPUSH key value            : Push value to tail of list";
    if (strcmp(cmd, "RPOP")      == 0) return "RPOP key                   : Pop value from tail of list";
    if (strcmp(cmd, "LRANGE")    == 0) return "LRANGE key                 : Get all list elements";
    if (strcmp(cmd, "SADD")      == 0) return "SADD key value             : Add value to set";
    if (strcmp(cmd, "SISMEMBER") == 0) return "SISMEMBER key value        : Check if value exists in set";
    if (strcmp(cmd, "SMEMBERS")  == 0) return "SMEMBERS key               : Get all members of a set";
    if (strcmp(cmd, "HSET")      == 0) return "HSET key field value       : Set a hash field";
    if (strcmp(cmd, "HGET")      == 0) return "HGET key field             : Get a hash field";
    if (strcmp(cmd, "HGETALL")   == 0) return "HGETALL key                : Get all hash fields and values";
    if (strcmp(cmd, "ZADD")      == 0) return "ZADD key score member      : Add member with score to sorted set";
    if (strcmp(cmd, "ZSCORE")    == 0) return "ZSCORE key member           : Get score of member in sorted set";
    if (strcmp(cmd, "ZRANK")     == 0) return "ZRANK key member            : Get 0-based rank of member";
    if (strcmp(cmd, "ZRANGE")    == 0) return "ZRANGE key start stop       : Get members in rank range (0-based)";
    if (strcmp(cmd, "ZREM")      == 0) return "ZREM key member             : Remove member from sorted set";
    if (strcmp(cmd, "ZCARD")     == 0) return "ZCARD key                   : Get number of members in sorted set";
    if (strcmp(cmd, "EXPIRE")    == 0) return "EXPIRE key seconds          : Set key to expire after N seconds";
    if (strcmp(cmd, "TTL")       == 0) return "TTL key                     : Get remaining TTL in seconds";
    if (strcmp(cmd, "PERSIST")   == 0) return "PERSIST key                 : Remove expiry from key";
    if (strcmp(cmd, "DEL")       == 0) return "DEL key                     : Delete a key";
    if (strcmp(cmd, "TYPE")      == 0) return "TYPE key                    : Get the type of a key";
    if (strcmp(cmd, "EXISTS")    == 0) return "EXISTS key                  : Check if key exists";
    if (strcmp(cmd, "SAVE")      == 0) return "SAVE                        : Synchronous RDB snapshot to disk";
    if (strcmp(cmd, "BGSAVE")    == 0) return "BGSAVE                      : Background RDB snapshot to disk";
    return NULL;
}
