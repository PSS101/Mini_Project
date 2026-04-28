#ifndef SET_H
#define SET_H

/* Forward-declare Store to break circular include */
struct Store;

typedef struct {
    struct Store *map;
} SetObject;

SetObject* createSet(void);
void       freeSet(SetObject *set);
int        sadd(SetObject *set, const char *value);
int        sismember(SetObject *set, const char *value);

#endif
