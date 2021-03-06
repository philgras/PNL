#ifndef PNL_LIST_H
#define PNL_LIST_H

#include <stddef.h>

struct pnl_list_s {
    struct pnl_list_s *prev;
    struct pnl_list_s *next;
};

typedef struct pnl_list_s pnl_list_t;

#define PNL_LIST_ENTRY(l, type, fieldname)                                 \
             ((type*)((char*) (l) - (size_t)(&((type*)(0))->fieldname)))


/**
 * allows iterating over the list
 * only reading operations are guaranteed to work correctly
 * the only exception is pnl_list_remove which can be called on the current elememt to remove it
 */
#define PNL_LIST_FOR_EACH(l, i)                                            \
            for(pnl_list_t* (i) = (l)->next, *safe_iter = (i)->next;       \
                (i) != (l); (i) = safe_iter, safe_iter = (i)->next)


static inline void pnl_list_init(pnl_list_t *list) {
    list->next = list;
    list->prev = list;
}

static inline
int pnl_list_is_empty(pnl_list_t *list) {
    return list->next == list;
}

static inline
pnl_list_t *pnl_list_first(pnl_list_t *list) {
    return pnl_list_is_empty(list) ? NULL : list->next;
}

static inline
void pnl_list_insert(pnl_list_t *list, pnl_list_t *node) {
    node->prev = list;
    node->next = list->next;
    list->next->prev = node;
    list->next = node;
}

static inline
void pnl_list_remove(pnl_list_t *list) {
    list->next->prev = list->prev;
    list->prev->next = list->next;
    list->next = list;
    list->prev = list;
}


#endif /*PNL_LIST_H*/
