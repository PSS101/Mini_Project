#ifndef LIST_H
#define LIST_H

typedef struct ListNode {
    char            *value;
    struct ListNode *prev;
    struct ListNode *next;
} ListNode;

typedef struct {
    ListNode *head;
    ListNode *tail;
    int       length;
} ListObject;

ListObject* createList();
void        lpush(ListObject *list, const char *value);
void        rpush(ListObject *list, const char *value);
char*       lpop(ListObject *list);
char*       rpop(ListObject *list);
void        freeList(ListObject *list);

#endif
