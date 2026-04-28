#ifndef SKIPLIST_H
#define SKIPLIST_H

#define SKIPLIST_MAXLEVEL 32
#define SKIPLIST_P        0.25

typedef struct SkipListNode {
    char                *member;
    double               score;
    struct SkipListNode *backward;
    struct SkipListLevel {
        struct SkipListNode *forward;
        unsigned int         span;
    } level[];
} SkipListNode;

typedef struct {
    SkipListNode *header;
    SkipListNode *tail;
    unsigned long length;
    int           level;
} SkipList;

SkipList*     skiplistCreate(void);
void          skiplistFree(SkipList *sl);
SkipListNode* skiplistInsert(SkipList *sl, double score, const char *member);
int           skiplistDelete(SkipList *sl, double score, const char *member);
SkipListNode* skiplistFind(SkipList *sl, double score, const char *member);
unsigned long skiplistGetRank(SkipList *sl, double score, const char *member);

#endif
