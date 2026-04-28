/*
 * rdb.c  —  RDB snapshot persistence
 *
 * Binary point-in-time snapshot of the entire keyspace.
 * Supports all 5 data types: string, list, set, hash, zset.
 *
 * BGSAVE spawns a thread (portable alternative to fork on Windows).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "rdb.h"
#include "../core/object.h"
#include "../core/store/store.h"
#include "../core/datatypes/string/string.h"
#include "../core/datatypes/list/list.h"
#include "../core/datatypes/set/set.h"
#include "../core/datatypes/hash/hashobj.h"
#include "../core/datatypes/zset/zset.h"
#include "../core/datatypes/zset/skiplist.h"

static const char RDB_MAGIC[8] = "MINIRDB";
static const uint32_t RDB_VERSION = 1;

static volatile int g_bgsave_running = 0;

/* ------------------------------------------------------------------ */
/*  Write helpers                                                       */
/* ------------------------------------------------------------------ */

static int write_uint8(FILE *f, uint8_t v)  { return fwrite(&v, 1, 1, f) == 1 ? 0 : -1; }
static int write_uint32(FILE *f, uint32_t v){ return fwrite(&v, 4, 1, f) == 1 ? 0 : -1; }
static int write_int64(FILE *f, int64_t v)  { return fwrite(&v, 8, 1, f) == 1 ? 0 : -1; }
static int write_double(FILE *f, double v)  { return fwrite(&v, 8, 1, f) == 1 ? 0 : -1; }

static int write_string(FILE *f, const char *s) {
    uint32_t len = s ? (uint32_t)strlen(s) : 0;
    if (write_uint32(f, len) != 0) return -1;
    if (len > 0 && fwrite(s, 1, len, f) != len) return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Read helpers                                                        */
/* ------------------------------------------------------------------ */

static int read_uint8(FILE *f, uint8_t *v)  { return fread(v, 1, 1, f) == 1 ? 0 : -1; }
static int read_uint32(FILE *f, uint32_t *v){ return fread(v, 4, 1, f) == 1 ? 0 : -1; }
static int read_int64(FILE *f, int64_t *v)  { return fread(v, 8, 1, f) == 1 ? 0 : -1; }
static int read_double(FILE *f, double *v)  { return fread(v, 8, 1, f) == 1 ? 0 : -1; }

static char* read_string_alloc(FILE *f) {
    uint32_t len;
    if (read_uint32(f, &len) != 0) return NULL;
    char *s = (char*)malloc(len + 1);
    if (!s) return NULL;
    if (len > 0 && fread(s, 1, len, f) != len) { free(s); return NULL; }
    s[len] = '\0';
    return s;
}

/* ------------------------------------------------------------------ */
/*  Save                                                                */
/* ------------------------------------------------------------------ */

static int save_string_obj(FILE *f, StringObject *str) {
    return write_string(f, str->value);
}

static int save_list_obj(FILE *f, ListObject *list) {
    if (write_uint32(f, (uint32_t)list->length) != 0) return -1;
    ListNode *n = list->head;
    while (n) {
        if (write_string(f, n->value) != 0) return -1;
        n = n->next;
    }
    return 0;
}

static int save_set_obj(FILE *f, SetObject *set) {
    /* Count elements */
    Store *map = set->map;
    uint32_t count = 0;
    for (int i = 0; i < map->size; i++) {
        Entry *e = map->buckets[i];
        while (e) { count++; e = e->next; }
    }
    if (write_uint32(f, count) != 0) return -1;
    for (int i = 0; i < map->size; i++) {
        Entry *e = map->buckets[i];
        while (e) {
            if (write_string(f, e->key) != 0) return -1;
            e = e->next;
        }
    }
    return 0;
}

static int save_hash_obj(FILE *f, HashObject *h) {
    Store *map = h->map;
    uint32_t count = 0;
    for (int i = 0; i < map->size; i++) {
        Entry *e = map->buckets[i];
        while (e) { count++; e = e->next; }
    }
    if (write_uint32(f, count) != 0) return -1;
    for (int i = 0; i < map->size; i++) {
        Entry *e = map->buckets[i];
        while (e) {
            if (write_string(f, e->key) != 0) return -1;
            /* value is always a StringObject */
            Object *valObj = e->value;
            const char *val = "";
            if (valObj && valObj->ptr && valObj->type == OBJ_STRING)
                val = ((StringObject*)valObj->ptr)->value;
            if (write_string(f, val) != 0) return -1;
            e = e->next;
        }
    }
    return 0;
}

static int save_zset_obj(FILE *f, ZSetObject *zset) {
    if (write_uint32(f, (uint32_t)zset->sl->length) != 0) return -1;
    SkipListNode *n = zset->sl->header->level[0].forward;
    while (n) {
        if (write_double(f, n->score) != 0) return -1;
        if (write_string(f, n->member) != 0) return -1;
        n = n->level[0].forward;
    }
    return 0;
}

int rdb_save(Store *store, const char *filepath) {
    if (!store || !filepath) return -1;

    /* Write to temp file, then rename for atomicity */
    char tmppath[512];
    snprintf(tmppath, sizeof(tmppath), "%s.tmp", filepath);

    FILE *f = fopen(tmppath, "wb");
    if (!f) { perror("rdb_save: fopen"); return -1; }

    /* Header */
    fwrite(RDB_MAGIC, 1, 8, f);
    write_uint32(f, RDB_VERSION);

    /* Iterate all keys */
    for (int i = 0; i < store->size; i++) {
        Entry *e = store->buckets[i];
        while (e) {
            /* Skip expired keys */
            if (e->expires_at != -1 && e->expires_at <= time(NULL)) {
                e = e->next;
                continue;
            }

            Object *obj = e->value;
            uint8_t type;
            switch (obj->type) {
                case OBJ_STRING: type = RDB_TYPE_STRING; break;
                case OBJ_LIST:   type = RDB_TYPE_LIST;   break;
                case OBJ_SET:    type = RDB_TYPE_SET;    break;
                case OBJ_HASH:   type = RDB_TYPE_HASH;   break;
                case OBJ_ZSET:   type = RDB_TYPE_ZSET;   break;
                default:         e = e->next; continue;
            }

            write_uint8(f, type);
            write_int64(f, (int64_t)e->expires_at);
            write_string(f, e->key);

            int err = 0;
            switch (type) {
                case RDB_TYPE_STRING: err = save_string_obj(f, (StringObject*)obj->ptr); break;
                case RDB_TYPE_LIST:   err = save_list_obj(f, (ListObject*)obj->ptr);     break;
                case RDB_TYPE_SET:    err = save_set_obj(f, (SetObject*)obj->ptr);       break;
                case RDB_TYPE_HASH:   err = save_hash_obj(f, (HashObject*)obj->ptr);     break;
                case RDB_TYPE_ZSET:   err = save_zset_obj(f, (ZSetObject*)obj->ptr);     break;
            }
            if (err != 0) { fclose(f); return -1; }

            e = e->next;
        }
    }

    /* EOF marker */
    write_uint8(f, RDB_EOF_MARKER);
    fclose(f);

    /* Atomic rename */
    remove(filepath);
    if (rename(tmppath, filepath) != 0) {
        perror("rdb_save: rename");
        return -1;
    }

    printf("[RDB] Saved snapshot to %s\n", filepath);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Load                                                                */
/* ------------------------------------------------------------------ */

int rdb_load(Store *store, const char *filepath) {
    if (!store || !filepath) return -1;

    FILE *f = fopen(filepath, "rb");
    if (!f) return 0;   /* no file = fresh start */

    /* Verify header */
    char magic[8];
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, RDB_MAGIC, 8) != 0) {
        fprintf(stderr, "[RDB] Invalid file header\n");
        fclose(f);
        return -1;
    }

    uint32_t version;
    read_uint32(f, &version);
    if (version != RDB_VERSION) {
        fprintf(stderr, "[RDB] Unsupported version %u\n", version);
        fclose(f);
        return -1;
    }

    int loaded = 0;
    time_t now = time(NULL);

    while (1) {
        uint8_t type;
        if (read_uint8(f, &type) != 0) break;
        if (type == RDB_EOF_MARKER) break;

        int64_t expires;
        read_int64(f, &expires);

        char *key = read_string_alloc(f);
        if (!key) break;

        /* Skip expired keys during load */
        if (expires != -1 && expires <= (int64_t)now) {
            /* Still need to consume the value data */
            switch (type) {
                case RDB_TYPE_STRING: {
                    char *v = read_string_alloc(f); free(v);
                    break;
                }
                case RDB_TYPE_LIST: {
                    uint32_t cnt; read_uint32(f, &cnt);
                    for (uint32_t j = 0; j < cnt; j++) {
                        char *v = read_string_alloc(f); free(v);
                    }
                    break;
                }
                case RDB_TYPE_SET: {
                    uint32_t cnt; read_uint32(f, &cnt);
                    for (uint32_t j = 0; j < cnt; j++) {
                        char *v = read_string_alloc(f); free(v);
                    }
                    break;
                }
                case RDB_TYPE_HASH: {
                    uint32_t cnt; read_uint32(f, &cnt);
                    for (uint32_t j = 0; j < cnt; j++) {
                        char *k = read_string_alloc(f); free(k);
                        char *v = read_string_alloc(f); free(v);
                    }
                    break;
                }
                case RDB_TYPE_ZSET: {
                    uint32_t cnt; read_uint32(f, &cnt);
                    for (uint32_t j = 0; j < cnt; j++) {
                        double sc; read_double(f, &sc);
                        char *m = read_string_alloc(f); free(m);
                    }
                    break;
                }
            }
            free(key);
            continue;
        }

        Object *obj = NULL;
        switch (type) {
            case RDB_TYPE_STRING: {
                char *val = read_string_alloc(f);
                if (val) { obj = createStringObj(val); free(val); }
                break;
            }
            case RDB_TYPE_LIST: {
                obj = createListObj();
                uint32_t cnt; read_uint32(f, &cnt);
                for (uint32_t j = 0; j < cnt; j++) {
                    char *val = read_string_alloc(f);
                    if (val) { rpush((ListObject*)obj->ptr, val); free(val); }
                }
                break;
            }
            case RDB_TYPE_SET: {
                obj = createSetObj();
                uint32_t cnt; read_uint32(f, &cnt);
                for (uint32_t j = 0; j < cnt; j++) {
                    char *member = read_string_alloc(f);
                    if (member) { sadd((SetObject*)obj->ptr, member); free(member); }
                }
                break;
            }
            case RDB_TYPE_HASH: {
                obj = createHashObj();
                uint32_t cnt; read_uint32(f, &cnt);
                for (uint32_t j = 0; j < cnt; j++) {
                    char *field = read_string_alloc(f);
                    char *val   = read_string_alloc(f);
                    if (field && val) hset((HashObject*)obj->ptr, field, val);
                    free(field); free(val);
                }
                break;
            }
            case RDB_TYPE_ZSET: {
                obj = createZSetObj();
                uint32_t cnt; read_uint32(f, &cnt);
                for (uint32_t j = 0; j < cnt; j++) {
                    double score;
                    read_double(f, &score);
                    char *member = read_string_alloc(f);
                    if (member) { zadd((ZSetObject*)obj->ptr, score, member); free(member); }
                }
                break;
            }
            default:
                free(key);
                continue;
        }

        if (obj) {
            setKey(store, key, obj);
            if (expires != -1) {
                Entry *entry = storeGetEntry(store, key);
                if (entry) entry->expires_at = (time_t)expires;
            }
            loaded++;
        }
        free(key);
    }

    fclose(f);
    printf("[RDB] Loaded %d keys from %s\n", loaded, filepath);
    return loaded;
}

/* ------------------------------------------------------------------ */
/*  Background save                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    Store      *store;
    char       *filepath;
} BgsaveArg;

static void* bgsave_thread(void *arg) {
    BgsaveArg *ba = (BgsaveArg*)arg;
    rdb_save(ba->store, ba->filepath);
    free(ba->filepath);
    free(ba);
    g_bgsave_running = 0;
    return NULL;
}

int rdb_bgsave(Store *store, const char *filepath) {
    if (g_bgsave_running) {
        fprintf(stderr, "[RDB] Background save already in progress\n");
        return -1;
    }

    BgsaveArg *ba = (BgsaveArg*)malloc(sizeof(BgsaveArg));
    if (!ba) return -1;
    ba->store    = store;
    ba->filepath = strdup(filepath);

    g_bgsave_running = 1;

    pthread_t tid;
    if (pthread_create(&tid, NULL, bgsave_thread, ba) != 0) {
        perror("rdb_bgsave: pthread_create");
        free(ba->filepath);
        free(ba);
        g_bgsave_running = 0;
        return -1;
    }
    pthread_detach(tid);

    printf("[RDB] Background save started to %s\n", filepath);
    return 0;
}

int rdb_bgsave_in_progress(void) {
    return g_bgsave_running;
}
