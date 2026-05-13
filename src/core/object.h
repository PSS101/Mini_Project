#ifndef OBJECT_H
#define OBJECT_H

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_SET,
    OBJ_HASH,
    OBJ_ZSET
} ObjectType;

typedef struct {
    ObjectType  type;
    void       *ptr;
} Object;

/* constructors */
Object* createStringObj(const char *value);
Object* createListObj(void);
Object* createSetObj(void);
Object* createHashObj(void);
Object* createZSetObj(void);

/* destructor */
void freeObject(Object *obj);

#endif
