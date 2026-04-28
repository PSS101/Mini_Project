#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zset.h"
#include "../../object.h"
#include "../../store/store.h"
#include "../string/string.h"

ZSetObject* createZSet(void) {
    ZSetObject *zset = (ZSetObject*)malloc(sizeof(ZSetObject));
    if (!zset) return NULL;
    zset->sl   = skiplistCreate();
    zset->dict = createStore(64);
    if (!zset->sl || !zset->dict) {
        if (zset->sl)   skiplistFree(zset->sl);
        if (zset->dict) freeStore(zset->dict);
        free(zset);
        return NULL;
    }
    return zset;
}

void freeZSet(ZSetObject *zset) {
    if (!zset) return;
    skiplistFree(zset->sl);
    freeStore(zset->dict);
    free(zset);
}

int zadd(ZSetObject *zset, double score, const char *member) {
    if (!zset) return 0;

    /* Check if member already exists */
    Object *existing = getKey(zset->dict, member);
    if (existing) {
        /* Update: remove old skip list entry, insert new one */
        double old_score = atof(((StringObject*)existing->ptr)->value);
        if (old_score != score) {
            skiplistDelete(zset->sl, old_score, member);
            skiplistInsert(zset->sl, score, member);
            /* Update the score in dict */
            char score_str[64];
            snprintf(score_str, sizeof(score_str), "%.17g", score);
            setStringValue((StringObject*)existing->ptr, score_str);
        }
        return 0;  /* updated, not new */
    }

    /* New member */
    skiplistInsert(zset->sl, score, member);
    char score_str[64];
    snprintf(score_str, sizeof(score_str), "%.17g", score);
    setKey(zset->dict, member, createStringObj(score_str));
    return 1;
}

int zscore(ZSetObject *zset, const char *member, double *out_score) {
    if (!zset) return 0;
    Object *obj = getKey(zset->dict, member);
    if (!obj) return 0;
    *out_score = atof(((StringObject*)obj->ptr)->value);
    return 1;
}

unsigned long zrank(ZSetObject *zset, const char *member) {
    if (!zset) return 0;
    Object *obj = getKey(zset->dict, member);
    if (!obj) return 0;
    double score = atof(((StringObject*)obj->ptr)->value);
    return skiplistGetRank(zset->sl, score, member);
}

char* zrange(ZSetObject *zset, int start, int stop) {
    if (!zset) return NULL;
    int len = (int)zset->sl->length;
    if (len == 0) return NULL;

    /* Handle negative indices */
    if (start < 0) start = len + start;
    if (stop  < 0) stop  = len + stop;
    if (start < 0) start = 0;
    if (stop >= len) stop = len - 1;
    if (start > stop) return NULL;

    /* Walk to start position */
    SkipListNode *node = zset->sl->header->level[0].forward;
    for (int i = 0; i < start && node; i++)
        node = node->level[0].forward;

    /* Build result string */
    size_t cap = 256;
    char  *buf = (char*)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';
    size_t pos = 0;

    for (int i = start; i <= stop && node; i++) {
        size_t mlen = strlen(node->member);
        /* +32 for score formatting + comma + space */
        while (pos + mlen + 64 > cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
            if (!buf) return NULL;
        }
        if (pos > 0) {
            buf[pos++] = ',';
        }
        memcpy(buf + pos, node->member, mlen);
        pos += mlen;
        buf[pos] = '\0';
        node = node->level[0].forward;
    }

    return buf;
}

int zrem(ZSetObject *zset, const char *member) {
    if (!zset) return 0;
    Object *obj = getKey(zset->dict, member);
    if (!obj) return 0;
    double score = atof(((StringObject*)obj->ptr)->value);
    skiplistDelete(zset->sl, score, member);
    deleteKey(zset->dict, member);
    return 1;
}

unsigned long zcard(ZSetObject *zset) {
    if (!zset) return 0;
    return zset->sl->length;
}
