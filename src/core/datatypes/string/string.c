#include <stdlib.h>
#include <string.h>
#include "string.h"

StringObject* createStringObject(const char *value) {
    StringObject *obj = (StringObject*)malloc(sizeof(StringObject));
    if (!obj) return NULL;
    obj->value = strdup(value);
    if (!obj->value) { free(obj); return NULL; }
    return obj;
}

void setStringValue(StringObject *obj, const char *value) {
    if (!obj) return;
    char *newValue = strdup(value);
    if (!newValue) return;
    free(obj->value);
    obj->value = newValue;
}

char* getStringValue(StringObject *obj) {
    if (!obj) return NULL;
    return obj->value;
}

void freeStringObject(StringObject *obj) {
    if (!obj) return;
    free(obj->value);
    free(obj);
}
