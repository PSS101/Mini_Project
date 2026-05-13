#ifndef ZSET_H
#define ZSET_H

#include "skiplist.h"

/* Forward-declare Store to break circular include */
struct Store;

typedef struct {
    SkipList     *sl;     /* ordered by (score, member) */
    struct Store *dict;   /* member -> score for O(1) lookup */
} ZSetObject;

ZSetObject* createZSet(void);
void        freeZSet(ZSetObject *zset);

/* Returns 1 if new element, 0 if score updated */
int    zadd(ZSetObject *zset, double score, const char *member);

/* Returns score via *out_score, returns 1 if found, 0 if not */
int    zscore(ZSetObject *zset, const char *member, double *out_score);

/* Returns 1-based rank (0 = not found) */
unsigned long zrank(ZSetObject *zset, const char *member);

/* Returns a comma-separated string of members in [start, stop] (0-based).
   Caller must free the returned string. */
char*  zrange(ZSetObject *zset, int start, int stop);

/* Returns 1 if removed, 0 if not found */
int    zrem(ZSetObject *zset, const char *member);

/* Returns the number of elements */
unsigned long zcard(ZSetObject *zset);

#endif
