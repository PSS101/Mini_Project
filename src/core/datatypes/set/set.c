#include <stdlib.h>
#include <string.h>
#include "set.h"
#include "../../object.h"
#include "../../store/store.h"
#include "../string/string.h"

SetObject* createSet(void) {
    SetObject *set = (SetObject*)malloc(sizeof(SetObject));
    if (!set) return NULL;
    set->map = createStore(50);
    return set;
}

void freeSet(SetObject *set) {
    if (!set) return;
    freeStore(set->map);   /* fixed: was just free(set) — leaked inner store */
    free(set);
}

int sadd(SetObject *set, const char *value) {
    if (!set) return 0;
    Object *existing = getKey(set->map, value);
    if (existing) return 0;
    setKey(set->map, value, createStringObj("1"));
    return 1;
}

int sismember(SetObject *set, const char *value) {
    if (!set) return 0;
    return getKey(set->map, value) != NULL;
}
