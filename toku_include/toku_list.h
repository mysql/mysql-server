#ifndef _TOKUDB_LIST_H
#define _TOKUDB_LIST_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

//TODO: #1378  This is not threadsafe.  Make sure when splitting locks
//that we protect these calls.

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// This toku_list is intended to be embedded in other data structures.
struct toku_list {
    struct toku_list *next, *prev;
};

static inline void toku_list_init(struct toku_list *head) {
    head->next = head->prev = head;
}

static inline int toku_list_empty(struct toku_list *head) {
    return head->next == head;
}

static inline struct toku_list *toku_list_head(struct toku_list *head) {
    return head->next;
}

static inline struct toku_list *toku_list_tail(struct toku_list *head) {
    return head->prev;
}

static inline void toku_list_insert_between(struct toku_list *a, struct toku_list *toku_list, struct toku_list *b) {

    toku_list->next = a->next;
    toku_list->prev = b->prev;
    a->next = b->prev = toku_list;
}

static inline void toku_list_push(struct toku_list *head, struct toku_list *toku_list) {
    toku_list_insert_between(head->prev, toku_list, head);
}

static inline void toku_list_push_head(struct toku_list *head, struct toku_list *toku_list) {
    toku_list_insert_between(head, toku_list, head->next);
}

static inline void toku_list_remove(struct toku_list *toku_list) {
    struct toku_list *prev = toku_list->prev;
    struct toku_list *next = toku_list->next;
    next->prev = prev;
    prev->next = next;
    toku_list_init(toku_list); // Set the toku_list element to be empty
}

static inline struct toku_list *toku_list_pop(struct toku_list *head) {
    struct toku_list *toku_list = head->prev;
    toku_list_remove(toku_list);
    return toku_list;
}

static inline struct toku_list *toku_list_pop_head(struct toku_list *head) {
    struct toku_list *toku_list = head->next;
    toku_list_remove(toku_list);
    return toku_list;
}

static inline void toku_list_move(struct toku_list *newhead, struct toku_list *oldhead) {
    struct toku_list *first = oldhead->next;
    struct toku_list *last = oldhead->prev;
    // assert(!toku_list_empty(oldhead));
    newhead->next = first;
    newhead->prev = last;
    last->next = first->prev = newhead;
    toku_list_init(oldhead);
}

// Note: Need the extra level of parens in these macros so that
//   toku_list_struct(h, foo, b)->zot
// will work right.  Otherwise the type cast will try to include ->zot, and it will be all messed up.
#if (defined(__GNUC__) && __GNUC__ >= 4) || defined(__builtin_offsetof)
#define toku_list_struct(p, t, f) ((t*)((char*)(p) - __builtin_offsetof(t, f)))
#else
#define toku_list_struct(p, t, f) ((t*)((char*)(p) - ((char*)&((t*)0)->f)))
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif


#endif
