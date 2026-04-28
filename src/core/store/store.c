#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "store.h"
#include "../datatypes/string/string.h"
#include "../datatypes/list/list.h"
#include "../datatypes/set/set.h"
#include "../datatypes/hash/hashobj.h"
#include "../datatypes/zset/zset.h"
#include "../utils/hash.h"

/* ------------------------------------------------------------------ */
/*  Core store operations                                               */
/* ------------------------------------------------------------------ */

Store* createStore(int size) {
    Store *store = (Store*)malloc(sizeof(Store));
    if (!store) return NULL;
    store->size    = size;
    store->buckets = (Entry**)calloc(size, sizeof(Entry*));
    if (!store->buckets) { free(store); return NULL; }
    return store;
}

/* NEW: properly walk and free every entry, then the bucket array */
void freeStore(Store *store) {
    if (!store) return;
    for (int i = 0; i < store->size; i++) {
        Entry *curr = store->buckets[i];
        while (curr) {
            Entry *next = curr->next;
            free(curr->key);
            freeObject(curr->value);
            free(curr);
            curr = next;
        }
    }
    free(store->buckets);
    free(store);
}

void setKey(Store *store, const char *key, Object *value) {
    unsigned int idx = hash(key, store->size);
    Entry *curr = store->buckets[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            freeObject(curr->value);
            curr->value      = value;
            curr->expires_at = -1;   /* reset TTL on overwrite */
            return;
        }
        curr = curr->next;
    }
    Entry *e = (Entry*)malloc(sizeof(Entry));
    if (!e) return;
    e->key        = strdup(key);
    e->value      = value;
    e->expires_at = -1;
    e->next       = store->buckets[idx];
    store->buckets[idx] = e;
}

/* Lazy expiry: if the key is expired, delete it and return NULL */
Object* getKey(Store *store, const char *key) {
    unsigned int idx = hash(key, store->size);
    Entry *curr = store->buckets[idx];
    Entry *prev = NULL;
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            /* Check TTL — lazy expiry */
            if (curr->expires_at != -1 && curr->expires_at <= time(NULL)) {
                /* expired: unlink and free */
                if (prev) prev->next = curr->next;
                else      store->buckets[idx] = curr->next;
                free(curr->key);
                freeObject(curr->value);
                free(curr);
                return NULL;
            }
            return curr->value;
        }
        prev = curr;
        curr = curr->next;
    }
    return NULL;
}

void deleteKey(Store *store, const char *key) {
    unsigned int idx  = hash(key, store->size);
    Entry *curr = store->buckets[idx];
    Entry *prev = NULL;
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) prev->next = curr->next;
            else      store->buckets[idx] = curr->next;
            free(curr->key);
            freeObject(curr->value);
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/* ------------------------------------------------------------------ */
/*  TTL / Expiry operations                                             */
/* ------------------------------------------------------------------ */

Entry* storeGetEntry(Store *store, const char *key) {
    unsigned int idx = hash(key, store->size);
    Entry *curr = store->buckets[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

int setExpire(Store *store, const char *key, int seconds) {
    Entry *e = storeGetEntry(store, key);
    if (!e) return 0;
    e->expires_at = time(NULL) + seconds;
    return 1;
}

int ttlKey(Store *store, const char *key) {
    Entry *e = storeGetEntry(store, key);
    if (!e) return -2;   /* key does not exist */
    if (e->expires_at == -1) return -1;   /* no TTL set */

    int remaining = (int)(e->expires_at - time(NULL));
    if (remaining <= 0) {
        /* expired — lazy delete */
        deleteKey(store, key);
        return -2;
    }
    return remaining;
}

int persistKey(Store *store, const char *key) {
    Entry *e = storeGetEntry(store, key);
    if (!e) return 0;
    if (e->expires_at == -1) return 0;   /* no TTL to remove */
    e->expires_at = -1;
    return 1;
}

/* Active expiry: scan random buckets and delete expired keys.
   Called periodically from a background thread.  */
void activeExpiryCycle(Store *store) {
    if (!store) return;
    time_t now = time(NULL);
    /* Sample up to 20 buckets per cycle to limit CPU usage */
    int samples = (store->size < 20) ? store->size : 20;
    for (int s = 0; s < samples; s++) {
        int idx = rand() % store->size;
        Entry *curr = store->buckets[idx];
        Entry *prev = NULL;
        while (curr) {
            Entry *next = curr->next;
            if (curr->expires_at != -1 && curr->expires_at <= now) {
                if (prev) prev->next = next;
                else      store->buckets[idx] = next;
                free(curr->key);
                freeObject(curr->value);
                free(curr);
            } else {
                prev = curr;
            }
            curr = next;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Helper: dynamic string builder — avoids fixed-buffer overflow      */
/* ------------------------------------------------------------------ */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->cap = 256;
    sb->buf = (char*)malloc(sb->cap);
    sb->len = 0;
    if (sb->buf) sb->buf[0] = '\0';
}

static void sb_append(StrBuf *sb, const char *s) {
    if (!sb->buf || !s) return;
    size_t slen = strlen(s);
    if (sb->len + slen + 1 > sb->cap) {
        sb->cap = (sb->len + slen + 1) * 2;
        sb->buf = (char*)realloc(sb->buf, sb->cap);
        if (!sb->buf) return;
    }
    memcpy(sb->buf + sb->len, s, slen + 1);
    sb->len += slen;
}

static char* sb_finish(StrBuf *sb) {
    /* remove trailing comma */
    if (sb->len > 0 && sb->buf[sb->len - 1] == ',') {
        sb->buf[sb->len - 1] = '\0';
        sb->len--;
    }
    return sb->buf;   /* caller owns and must free */
}

/* ------------------------------------------------------------------ */
/*  String operations                                                   */
/* ------------------------------------------------------------------ */

/* Returns the string value, NULL if key missing, or NULL + sets
   *wrong_type = 1 if the key holds a different type.
   Callers check wrong_type flag instead of comparing pointer to "-1". */
char* getStringKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_STRING) return NULL;   /* dispatcher handles via getKey type check */
    return getStringValue((StringObject*)obj->ptr);
}

/* ------------------------------------------------------------------ */
/*  List operations                                                     */
/* ------------------------------------------------------------------ */

int lpushKey(Store *store, const char *key, const char *value) {
    Object *obj = getKey(store, key);
    if (!obj) {
        obj = createListObj();
        setKey(store, key, obj);
    }
    if (obj->type != OBJ_LIST) return -1;
    lpush((ListObject*)obj->ptr, value);
    return 1;
}

char* lpopKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_LIST) return NULL;   /* type error handled in dispatcher */
    return lpop((ListObject*)obj->ptr);
}

/* Fixed: dynamic buffer, no fixed 256-byte limit */
char* lrangeKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_LIST) return NULL;

    ListObject *list = (ListObject*)obj->ptr;
    StrBuf sb;
    sb_init(&sb);
    ListNode *curr = list->head;
    while (curr) {
        sb_append(&sb, curr->value);
        sb_append(&sb, ",");
        curr = curr->next;
    }
    return sb_finish(&sb);
}

int rpushKey(Store *store, const char *key, const char *value) {
    Object *obj = getKey(store, key);
    if (!obj) {
        obj = createListObj();
        setKey(store, key, obj);
    }
    if (obj->type != OBJ_LIST) return -1;
    rpush((ListObject*)obj->ptr, value);
    return 1;
}

char* rpopKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_LIST) return NULL;
    return rpop((ListObject*)obj->ptr);
}

/* ------------------------------------------------------------------ */
/*  Set operations                                                      */
/* ------------------------------------------------------------------ */

int saddKey(Store *store, const char *key, const char *value) {
    Object *obj = getKey(store, key);
    if (!obj) {
        obj = createSetObj();
        setKey(store, key, obj);
    }
    if (obj->type != OBJ_SET) return -1;
    return sadd((SetObject*)obj->ptr, value);
}

int sismemberKey(Store *store, const char *key, const char *value) {
    Object *obj = getKey(store, key);
    if (!obj) return 0;
    if (obj->type != OBJ_SET) return -1;
    return sismember((SetObject*)obj->ptr, value);
}

/* Fixed: dynamic buffer */
char* smembersKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_SET) return NULL;

    Store *map = ((SetObject*)obj->ptr)->map;
    StrBuf sb;
    sb_init(&sb);
    for (int i = 0; i < map->size; i++) {
        Entry *curr = map->buckets[i];
        while (curr) {
            sb_append(&sb, curr->key);
            sb_append(&sb, ",");
            curr = curr->next;
        }
    }
    return sb_finish(&sb);
}

/* ------------------------------------------------------------------ */
/*  Hash operations                                                     */
/* ------------------------------------------------------------------ */

int hsetKey(Store *store, const char *key, const char *field, const char *value) {
    Object *obj = getKey(store, key);
    if (!obj) {
        obj = createHashObj();
        setKey(store, key, obj);
    }
    if (obj->type != OBJ_HASH) return -1;
    hset((HashObject*)obj->ptr, field, value);
    return 1;
}

char* hgetKey(Store *store, const char *key, const char *field) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_HASH) return NULL;
    return hget((HashObject*)obj->ptr, field);
}

/* Fixed: dynamic buffer, guarded ptr dereference */
char* hgetallKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_HASH) return NULL;

    Store *map = ((HashObject*)obj->ptr)->map;
    StrBuf sb;
    sb_init(&sb);
    for (int i = 0; i < map->size; i++) {
        Entry *curr = map->buckets[i];
        while (curr) {
            sb_append(&sb, curr->key);
            sb_append(&sb, ":");
            Object *valObj = curr->value;
            if (valObj && valObj->ptr && valObj->type == OBJ_STRING) {
                sb_append(&sb, ((StringObject*)valObj->ptr)->value);
            }
            sb_append(&sb, ",");
            curr = curr->next;
        }
    }
    return sb_finish(&sb);
}

/* ------------------------------------------------------------------ */
/*  Sorted Set operations                                               */
/* ------------------------------------------------------------------ */

int zaddKey(Store *store, const char *key, double score, const char *member) {
    Object *obj = getKey(store, key);
    if (!obj) {
        obj = createZSetObj();
        setKey(store, key, obj);
    }
    if (obj->type != OBJ_ZSET) return -2;
    return zadd((ZSetObject*)obj->ptr, score, member);
}

int zscoreKey(Store *store, const char *key, const char *member, double *out) {
    Object *obj = getKey(store, key);
    if (!obj) return 0;
    if (obj->type != OBJ_ZSET) return -1;
    return zscore((ZSetObject*)obj->ptr, member, out);
}

unsigned long zrankKey(Store *store, const char *key, const char *member) {
    Object *obj = getKey(store, key);
    if (!obj) return 0;
    if (obj->type != OBJ_ZSET) return 0;
    return zrank((ZSetObject*)obj->ptr, member);
}

char* zrangeKey(Store *store, const char *key, int start, int stop) {
    Object *obj = getKey(store, key);
    if (!obj) return NULL;
    if (obj->type != OBJ_ZSET) return NULL;
    return zrange((ZSetObject*)obj->ptr, start, stop);
}

int zremKey(Store *store, const char *key, const char *member) {
    Object *obj = getKey(store, key);
    if (!obj) return 0;
    if (obj->type != OBJ_ZSET) return -1;
    return zrem((ZSetObject*)obj->ptr, member);
}

unsigned long zcardKey(Store *store, const char *key) {
    Object *obj = getKey(store, key);
    if (!obj) return 0;
    if (obj->type != OBJ_ZSET) return 0;
    return zcard((ZSetObject*)obj->ptr);
}
