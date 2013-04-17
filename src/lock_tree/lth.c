/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_lth.h
   \brief Hash lth
  
*/

#include <toku_portability.h>
#include "lth.h"
#include <toku_assert.h>
#include <errno.h>
#include <string.h>

/* TODO: reallocate the hash lth if it grows too big. Perhaps, use toku_get_prime in newbrt/primes.c */
const uint32_t __toku_lth_init_size = 521;

static inline uint32_t toku__lth_hash(toku_lth* lth, toku_lock_tree* key) {
    size_t tmp = (size_t)key;
    return tmp % lth->num_buckets;
}

static inline void toku__invalidate_scan(toku_lth* lth) {
    lth->iter_is_valid = FALSE;
}

int toku_lth_create(toku_lth** plth,
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t)) {
    int r = ENOSYS;
    assert(plth && user_malloc && user_free && user_realloc);
    toku_lth* tmp = NULL;
    tmp = (toku_lth*)user_malloc(sizeof(*tmp));
    if (!tmp) { r = ENOMEM; goto cleanup; }

    memset(tmp, 0, sizeof(*tmp));
    tmp->malloc      = user_malloc;
    tmp->free        = user_free;
    tmp->realloc     = user_realloc;
    tmp->num_buckets = __toku_lth_init_size;
    tmp->buckets     = (toku_lth_elt*)
                          tmp->malloc(tmp->num_buckets * sizeof(*tmp->buckets));
    if (!tmp->buckets) { r = ENOMEM; goto cleanup; }
    memset(tmp->buckets, 0, tmp->num_buckets * sizeof(*tmp->buckets));
    toku__invalidate_scan(tmp);
    tmp->iter_head.next_in_iteration = &tmp->iter_head;
    tmp->iter_head.prev_in_iteration = &tmp->iter_head;    

    *plth = tmp;
    r = 0;
cleanup:
    if (r != 0) {
        if (tmp) {
            if (tmp->buckets) { user_free(tmp->buckets); }
            user_free(tmp);
        }
    }
    return r;
}

toku_lock_tree* toku_lth_find(toku_lth* lth, toku_lock_tree* key) {
    assert(lth && key);

    uint32_t index          = toku__lth_hash(lth, key);
    toku_lth_elt* head    = &lth->buckets[index];
    toku_lth_elt* current = head->next_in_bucket;
    while (current) {
        if (current->value.hash_key == key) break;
        current = current->next_in_bucket;
    }
    return current ? current->value.hash_key : NULL;
}

void toku_lth_start_scan(toku_lth* lth) {
    assert(lth);
    lth->iter_curr = &lth->iter_head;
    lth->iter_is_valid = TRUE;
}

static inline toku_lth_elt* toku__lth_next(toku_lth* lth) {
    assert(lth);
    assert(lth->iter_is_valid);

    lth->iter_curr     = lth->iter_curr->next_in_iteration;
    lth->iter_is_valid = (BOOL)(lth->iter_curr != &lth->iter_head);
    return lth->iter_curr;
}

toku_lock_tree* toku_lth_next(toku_lth* lth) {
    assert(lth);
    toku_lth_elt* next = toku__lth_next(lth);
    return lth->iter_curr != &lth->iter_head ? next->value.hash_key : NULL;
}

/* Element MUST exist. */
void toku_lth_delete(toku_lth* lth, toku_lock_tree* key) {
    assert(lth && key);
    toku__invalidate_scan(lth);

    /* Must have elements. */
    assert(lth->num_keys);

    uint32_t index = toku__lth_hash(lth, key);
    toku_lth_elt* head    = &lth->buckets[index]; 
    toku_lth_elt* prev    = head; 
    toku_lth_elt* current = prev->next_in_bucket;

    while (current != NULL) {
        if (current->value.hash_key == key) break;
        prev = current;
        current = current->next_in_bucket;
    }
    /* Must be found. */
    assert(current);
    current->prev_in_iteration->next_in_iteration = current->next_in_iteration;
    current->next_in_iteration->prev_in_iteration = current->prev_in_iteration;
    prev->next_in_bucket = current->next_in_bucket;
    lth->free(current);
    lth->num_keys--;
    return;
}
    
/* Will allow you to insert it over and over.  You need to keep track. */
int toku_lth_insert(toku_lth* lth, toku_lock_tree* key) {
    int r = ENOSYS;
    assert(lth && key);
    toku__invalidate_scan(lth);

    uint32_t index = toku__lth_hash(lth, key);

    /* Allocate a new one. */
    toku_lth_elt* element = (toku_lth_elt*)lth->malloc(sizeof(*element));
    if (!element) { r = ENOMEM; goto cleanup; }
    memset(element, 0, sizeof(*element));
    element->value.hash_key    = key;
    element->next_in_iteration = lth->iter_head.next_in_iteration;
    element->prev_in_iteration = &lth->iter_head;
    element->next_in_iteration->prev_in_iteration = element;
    element->prev_in_iteration->next_in_iteration = element;
    
    element->next_in_bucket            = lth->buckets[index].next_in_bucket;
    lth->buckets[index].next_in_bucket = element;
    lth->num_keys++;

    r = 0;
cleanup:
    return r;    
}

void toku_lth_close(toku_lth* lth) {
    assert(lth);
    toku_lth_elt* element;
    toku_lth_elt* head = &lth->iter_head;
    toku_lth_elt* next = NULL;

    toku_lth_start_scan(lth);
    next = toku__lth_next(lth);
    while (next != head) {
        element = next;
        next    = toku__lth_next(lth);
        lth->free(element);
    }

    lth->free(lth->buckets);
    lth->free(lth);
}
