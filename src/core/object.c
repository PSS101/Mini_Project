#include <stdlib.h>
#include "object.h"
#include "datatypes/string/string.h"
#include "datatypes/list/list.h"
#include "datatypes/set/set.h"
#include "datatypes/hash/hashobj.h"
#include "datatypes/zset/zset.h"

Object* createStringObj(const char *value) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->type = OBJ_STRING;
    obj->ptr  = createStringObject(value);
    return obj;
}

Object* createListObj(void) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->type = OBJ_LIST;
    obj->ptr  = createList();
    return obj;
}

Object* createSetObj(void) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->type = OBJ_SET;
    obj->ptr  = createSet();
    return obj;
}

Object* createHashObj(void) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->type = OBJ_HASH;
    obj->ptr  = createHash();
    return obj;
}

Object* createZSetObj(void) {
    Object *obj = (Object*)malloc(sizeof(Object));
    if (!obj) return NULL;
    obj->type = OBJ_ZSET;
    obj->ptr  = createZSet();
    return obj;
}

void freeObject(Object *obj) {
    if (!obj) return;
    switch (obj->type) {
        case OBJ_STRING: freeStringObject((StringObject*)obj->ptr); break;
        case OBJ_LIST:   freeList((ListObject*)obj->ptr);           break;
        case OBJ_SET:    freeSet((SetObject*)obj->ptr);             break;
        case OBJ_HASH:   freeHash((HashObject*)obj->ptr);           break;
        case OBJ_ZSET:   freeZSet((ZSetObject*)obj->ptr);           break;
        default: break;
    }
    free(obj);
}
