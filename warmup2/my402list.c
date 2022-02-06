#include <stdlib.h>

#include "my402list.h"


int My402ListLength(My402List* list) {
    return list->num_members;
}

int My402ListEmpty(My402List* list) {
    return (list->num_members == 0) ? TRUE : FALSE;
}

int My402ListAppend(My402List* list, void* obj) {
    return My402ListInsertAfter(list, obj, My402ListLast(list));
}

int My402ListPrepend(My402List* list, void* obj) {
     return My402ListInsertBefore(list, obj, My402ListFirst(list));
}

void My402ListUnlink(My402List* list, My402ListElem* elem) {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    free(elem);
    list->num_members--;
}

void My402ListUnlinkAll(My402List* list) {
    My402ListElem* curElem = My402ListFirst(list);
    while (curElem != NULL) {
        My402ListElem* nextElem = My402ListNext(list, curElem);
        free(curElem);
        curElem = nextElem;
    }
    My402ListInit(list);
}

int My402ListInsertAfter(My402List* list, void* obj, My402ListElem* elem) {
    if (elem == NULL) {
        elem = list->anchor.prev;
    }
    My402ListElem* newElem = (My402ListElem*) malloc(sizeof(My402ListElem));
    newElem->obj = obj;
    newElem->prev = elem;
    newElem->next = elem->next;
    elem->next->prev = newElem;
    elem->next = newElem;
    list->num_members++;
    return TRUE;
}

int My402ListInsertBefore(My402List* list, void* obj, My402ListElem* elem) {
    if (elem == NULL) {
        elem = list->anchor.next;
    }
    My402ListElem* newElem = (My402ListElem*) malloc(sizeof(My402ListElem));
    newElem->obj = obj;
    newElem->prev = elem->prev;
    newElem->next = elem;
    elem->prev->next = newElem;
    elem->prev = newElem;
    list->num_members++;
    return TRUE;
}

My402ListElem *My402ListFirst(My402List* list) {
    return (My402ListEmpty(list)) ? NULL : list->anchor.next;
}

My402ListElem *My402ListLast(My402List* list) {
    return (My402ListEmpty(list)) ? NULL : list->anchor.prev;
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem) {
    return (elem->next == &list->anchor) ? NULL : elem->next;
}

My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem) {
    return (elem->prev == &list->anchor) ? NULL : elem->prev;
}

My402ListElem *My402ListFind(My402List* list, void* obj) {
    My402ListElem* curElem = My402ListNext(list, &list->anchor);
    while (curElem != NULL) {
        if (curElem->obj == obj) return curElem;
        curElem = My402ListNext(list, curElem);
    }
    return NULL;
}

int My402ListInit(My402List* list) {
    list->num_members = 0;
    list->anchor.obj = NULL;
    list->anchor.next = &list->anchor;
    list->anchor.prev = &list->anchor;
    return TRUE;
}
