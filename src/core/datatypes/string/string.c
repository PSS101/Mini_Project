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

int appendStringValue(StringObject *obj, const char *suffix) {
    if (!obj || !suffix) return 0;
    size_t old_len = obj->value ? strlen(obj->value) : 0;
    size_t suf_len = strlen(suffix);
    char *newval = (char*)realloc(obj->value, old_len + suf_len + 1);
    if (!newval) return (int)old_len;
    memcpy(newval + old_len, suffix, suf_len + 1);
    obj->value = newval;
    return (int)(old_len + suf_len);
}

void freeStringObject(StringObject *obj) {
    if (!obj) return;
    free(obj->value);
    free(obj);
}
