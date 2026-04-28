#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "skiplist.h"

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static int sl_random_level(void) {
    static int seeded = 0;
    if (!seeded) { srand((unsigned)time(NULL)); seeded = 1; }
    int level = 1;
    while ((rand() & 0xFFFF) < (int)(SKIPLIST_P * 0xFFFF))
        level++;
    return (level < SKIPLIST_MAXLEVEL) ? level : SKIPLIST_MAXLEVEL;
}

static SkipListNode* sl_create_node(int level, double score, const char *member) {
    SkipListNode *node = (SkipListNode*)malloc(
        sizeof(SkipListNode) + level * sizeof(struct SkipListLevel));
    if (!node) return NULL;
    node->score    = score;
    node->member   = member ? strdup(member) : NULL;
    node->backward = NULL;
    for (int i = 0; i < level; i++) {
        node->level[i].forward = NULL;
        node->level[i].span    = 0;
    }
    return node;
}

static void sl_free_node(SkipListNode *node) {
    if (!node) return;
    free(node->member);
    free(node);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

SkipList* skiplistCreate(void) {
    SkipList *sl = (SkipList*)malloc(sizeof(SkipList));
    if (!sl) return NULL;
    sl->level  = 1;
    sl->length = 0;
    sl->header = sl_create_node(SKIPLIST_MAXLEVEL, 0, NULL);
    sl->tail   = NULL;
    return sl;
}

void skiplistFree(SkipList *sl) {
    if (!sl) return;
    SkipListNode *node = sl->header->level[0].forward;
    while (node) {
        SkipListNode *next = node->level[0].forward;
        sl_free_node(node);
        node = next;
    }
    sl_free_node(sl->header);
    free(sl);
}

SkipListNode* skiplistInsert(SkipList *sl, double score, const char *member) {
    SkipListNode *update[SKIPLIST_MAXLEVEL];
    unsigned int  rank[SKIPLIST_MAXLEVEL];

    SkipListNode *x = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        rank[i] = (i == sl->level - 1) ? 0 : rank[i + 1];
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 strcmp(x->level[i].forward->member, member) < 0))) {
            rank[i] += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    int level = sl_random_level();
    if (level > sl->level) {
        for (int i = sl->level; i < level; i++) {
            rank[i]   = 0;
            update[i] = sl->header;
            update[i]->level[i].span = (unsigned int)sl->length;
        }
        sl->level = level;
    }

    x = sl_create_node(level, score, member);
    for (int i = 0; i < level; i++) {
        x->level[i].forward        = update[i]->level[i].forward;
        update[i]->level[i].forward = x;

        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    for (int i = level; i < sl->level; i++) {
        update[i]->level[i].span++;
    }

    x->backward = (update[0] == sl->header) ? NULL : update[0];
    if (x->level[0].forward)
        x->level[0].forward->backward = x;
    else
        sl->tail = x;

    sl->length++;
    return x;
}

int skiplistDelete(SkipList *sl, double score, const char *member) {
    SkipListNode *update[SKIPLIST_MAXLEVEL];
    SkipListNode *x = sl->header;

    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 strcmp(x->level[i].forward->member, member) < 0))) {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    x = x->level[0].forward;
    if (!x || x->score != score || strcmp(x->member, member) != 0)
        return 0;

    /* unlink */
    for (int i = 0; i < sl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span--;
        }
    }

    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        sl->tail = x->backward;
    }

    while (sl->level > 1 && sl->header->level[sl->level - 1].forward == NULL)
        sl->level--;

    sl->length--;
    sl_free_node(x);
    return 1;
}

SkipListNode* skiplistFind(SkipList *sl, double score, const char *member) {
    SkipListNode *x = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 strcmp(x->level[i].forward->member, member) < 0))) {
            x = x->level[i].forward;
        }
    }
    x = x->level[0].forward;
    if (x && x->score == score && strcmp(x->member, member) == 0)
        return x;
    return NULL;
}

unsigned long skiplistGetRank(SkipList *sl, double score, const char *member) {
    unsigned long rank = 0;
    SkipListNode *x = sl->header;
    for (int i = sl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                (x->level[i].forward->score == score &&
                 strcmp(x->level[i].forward->member, member) <= 0))) {
            rank += x->level[i].span;
            x = x->level[i].forward;
        }
        if (x->member && strcmp(x->member, member) == 0)
            return rank;
    }
    return 0;  /* not found */
}
