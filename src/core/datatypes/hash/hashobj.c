#include <stdlib.h>
#include "hashobj.h"
#include "../../object.h"
#include "../../store/store.h"
#include "../string/string.h"

HashObject* createHash(void) {
    HashObject *h = (HashObject*)malloc(sizeof(HashObject));
    if (!h) return NULL;
    h->map = createStore(50);
    return h;
}

void freeHash(HashObject *hash) {
    if (!hash) return;
    freeStore(hash->map);   /* fixed: was just free(hash) — leaked inner store */
    free(hash);
}

void hset(HashObject *hash, const char *field, const char *value) {
    if (!hash) return;
    setKey(hash->map, field, createStringObj(value));
}

char* hget(HashObject *hash, const char *field) {
    if (!hash) return NULL;
    Object *obj = getKey(hash->map, field);
    if (!obj) return NULL;
    if (obj->type != OBJ_STRING) return NULL;
    return getStringValue((StringObject*)obj->ptr);
}
