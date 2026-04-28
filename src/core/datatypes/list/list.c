#include <stdlib.h>
#include <string.h>
#include "list.h"

ListObject* createList() {
    ListObject *list = (ListObject*)malloc(sizeof(ListObject));
    if (!list) return NULL;
    list->head   = NULL;
    list->tail   = NULL;
    list->length = 0;
    return list;
}

void lpush(ListObject *list, const char *value) {
    if (!list) return;
    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return;
    node->value = strdup(value);
    if (!node->value) { free(node); return; }
    node->prev = NULL;
    node->next = list->head;
    if (list->head) list->head->prev = node;
    list->head = node;
    if (!list->tail) list->tail = node;
    list->length++;
}

void rpush(ListObject *list, const char *value) {
    if (!list) return;
    ListNode *node = (ListNode*)malloc(sizeof(ListNode));
    if (!node) return;
    node->value = strdup(value);
    if (!node->value) { free(node); return; }
    node->next = NULL;
    node->prev = list->tail;
    if (list->tail) list->tail->next = node;
    list->tail = node;
    if (!list->head) list->head = node;
    list->length++;
}

char* lpop(ListObject *list) {
    if (!list || !list->head) return NULL;
    ListNode *node  = list->head;
    char     *value = strdup(node->value);
    list->head = node->next;
    if (list->head) list->head->prev = NULL;
    else            list->tail = NULL;
    free(node->value);
    free(node);
    list->length--;
    return value;
}

char* rpop(ListObject *list) {
    if (!list || !list->tail) return NULL;
    ListNode *node  = list->tail;
    char     *value = strdup(node->value);
    list->tail = node->prev;
    if (list->tail) list->tail->next = NULL;
    else            list->head = NULL;
    free(node->value);
    free(node);
    list->length--;
    return value;
}

void freeList(ListObject *list) {
    if (!list) return;
    ListNode *curr = list->head;
    while (curr) {
        ListNode *next = curr->next;
        free(curr->value);
        free(curr);
        curr = next;
    }
    free(list);
}
