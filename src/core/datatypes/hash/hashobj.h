#ifndef HASHOBJ_H
#define HASHOBJ_H

/* Forward-declare Store to break circular include */
struct Store;

typedef struct {
    struct Store *map;
} HashObject;

HashObject* createHash(void);
void        freeHash(HashObject *hash);
void        hset(HashObject *hash, const char *field, const char *value);
char*       hget(HashObject *hash, const char *field);

#endif
