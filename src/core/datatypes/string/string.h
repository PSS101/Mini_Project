#ifndef STRING_H
#define STRING_H

typedef struct {
    char *value;
} StringObject;

StringObject* createStringObject(const char *value);
void          setStringValue(StringObject *obj, const char *value);
char*         getStringValue(StringObject *obj);
int           appendStringValue(StringObject *obj, const char *suffix);
void          freeStringObject(StringObject *obj);

#endif
