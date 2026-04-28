#ifndef STORE_H
#define STORE_H

#include <time.h>
#include "../object.h"

typedef struct Entry {
    char        *key;
    Object      *value;
    time_t       expires_at;   /* -1 = no expiry, else epoch seconds */
    struct Entry *next;
} Entry;

typedef struct Store {
    Entry **buckets;
    int     size;
} Store;

/* core */
Store*  createStore(int size);
void    freeStore(Store *store);
void    setKey(Store *store, const char *key, Object *value);
Object* getKey(Store *store, const char *key);
void    deleteKey(Store *store, const char *key);

/* TTL / expiry */
int   setExpire(Store *store, const char *key, int seconds);
int   ttlKey(Store *store, const char *key);      /* -2 = missing, -1 = no TTL */
int   persistKey(Store *store, const char *key);   /* remove TTL */
void  activeExpiryCycle(Store *store);              /* scan & delete expired keys */

/* iteration helper (used by RDB, SMEMBERS, HGETALL) */
Entry* storeGetEntry(Store *store, const char *key);

/* string ops */
char* getStringKey(Store *store, const char *key);

/* list ops */
int   lpushKey(Store *store, const char *key, const char *value);
char* lpopKey(Store *store, const char *key);
char* lrangeKey(Store *store, const char *key);
int   rpushKey(Store *store, const char *key, const char *value);
char* rpopKey(Store *store, const char *key);

/* set ops */
int   saddKey(Store *store, const char *key, const char *value);
int   sismemberKey(Store *store, const char *key, const char *value);
char* smembersKey(Store *store, const char *key);

/* hash ops */
int   hsetKey(Store *store, const char *key, const char *field, const char *value);
char* hgetKey(Store *store, const char *key, const char *field);
char* hgetallKey(Store *store, const char *key);

/* sorted set ops */
int           zaddKey(Store *store, const char *key, double score, const char *member);
int           zscoreKey(Store *store, const char *key, const char *member, double *out);
unsigned long zrankKey(Store *store, const char *key, const char *member);
char*         zrangeKey(Store *store, const char *key, int start, int stop);
int           zremKey(Store *store, const char *key, const char *member);
unsigned long zcardKey(Store *store, const char *key);

#endif
