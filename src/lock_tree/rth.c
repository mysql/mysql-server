/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_rth.h
   \brief Hash rth
  
*/

#include "rth.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

/* TODO: reallocate the hash rth if it grows too big. Perhaps, use toku_get_prime in newbrt/primes.c */
const uint32 __toku_rth_init_size = 521;

static inline uint32 toku__rth_hash(toku_rth* rth, DB_TXN* key) {
    size_t tmp = (size_t)key;
    return tmp % rth->num_buckets;
}

static inline void toku__invalidate_scan(toku_rth* rth) {
    rth->iter_is_valid = FALSE;
}

int toku_rth_create(toku_rth** prth,
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t)) {
    int r = ENOSYS;
    assert(prth && user_malloc && user_free && user_realloc);
    toku_rth* tmp = NULL;
    tmp = (toku_rth*)user_malloc(sizeof(*tmp));
    if (!tmp) { r = ENOMEM; goto cleanup; }

    memset(tmp, 0, sizeof(*tmp));
    tmp->malloc      = user_malloc;
    tmp->free        = user_free;
    tmp->realloc     = user_realloc;
    tmp->num_buckets = __toku_rth_init_size;
    tmp->buckets     = (toku_rth_elt*)
                          tmp->malloc(tmp->num_buckets * sizeof(*tmp->buckets));
    if (!tmp->buckets) { r = ENOMEM; goto cleanup; }
    memset(tmp->buckets, 0, tmp->num_buckets * sizeof(*tmp->buckets));
    toku__invalidate_scan(tmp);
    tmp->iter_head.next_in_iteration = &tmp->iter_head;
    tmp->iter_head.prev_in_iteration = &tmp->iter_head;    

    *prth = tmp;
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

toku_rt_forest* toku_rth_find(toku_rth* rth, DB_TXN* key) {
    assert(rth && key);

    uint32 index          = toku__rth_hash(rth, key);
    toku_rth_elt* head    = &rth->buckets[index];
    toku_rth_elt* current = head->next_in_bucket;
    while (current) {
        if (current->value.hash_key == key) break;
        current = current->next_in_bucket;
    }
    return current ? &current->value : NULL;
}

void toku_rth_start_scan(toku_rth* rth) {
    assert(rth);
    rth->iter_curr = &rth->iter_head;
    rth->iter_is_valid = TRUE;
}

static inline toku_rth_elt* toku__rth_next(toku_rth* rth) {
    assert(rth);
    assert(rth->iter_is_valid);

    rth->iter_curr     = rth->iter_curr->next_in_iteration;
    rth->iter_is_valid = rth->iter_curr != &rth->iter_head;
    return rth->iter_curr;
}

toku_rt_forest* toku_rth_next(toku_rth* rth) {
    assert(rth);
    toku_rth_elt* next = toku__rth_next(rth);
    return rth->iter_curr != &rth->iter_head ? &next->value : NULL;
}

/* Element MUST exist. */
void toku_rth_delete(toku_rth* rth, DB_TXN* key) {
    assert(rth && key);
    toku__invalidate_scan(rth);

    /* Must have elements. */
    assert(rth->num_keys);

    uint32 index = toku__rth_hash(rth, key);
    toku_rth_elt* head    = &rth->buckets[index]; 
    toku_rth_elt* prev    = head; 
    toku_rth_elt* current = prev->next_in_bucket;

    while (current != head) {
        if (current->value.hash_key == key) break;
        prev = current;
        current = current->next_in_bucket;
    }
    /* Must be found. */
    assert(current != head);
    current->prev_in_iteration->next_in_iteration = current->next_in_iteration;
    current->next_in_iteration->prev_in_iteration = current->prev_in_iteration;
    prev->next_in_bucket = current->next_in_bucket;
    rth->free(current);
    rth->num_keys--;
    return;
}
    
/* Will allow you to insert it over and over.  You need to keep track. */
int toku_rth_insert(toku_rth* rth, DB_TXN* key) {
    int r = ENOSYS;
    assert(rth && key);
    toku__invalidate_scan(rth);

    uint32 index = toku__rth_hash(rth, key);

    /* Allocate a new one. */
    toku_rth_elt* element = (toku_rth_elt*)rth->malloc(sizeof(*element));
    if (!element) { r = ENOMEM; goto cleanup; }
    memset(element, 0, sizeof(*element));
    element->value.hash_key    = key;
    element->next_in_iteration = rth->iter_head.next_in_iteration;
    element->prev_in_iteration = &rth->iter_head;
    element->next_in_iteration->prev_in_iteration = element;
    element->prev_in_iteration->next_in_iteration = element;
    
    element->next_in_bucket            = rth->buckets[index].next_in_bucket;
    rth->buckets[index].next_in_bucket = element;
    rth->num_keys++;

    r = 0;
cleanup:
    return r;    
}

void toku_rth_close(toku_rth* rth) {
    assert(rth);
    toku_rth_elt* element;
    toku_rth_elt* head = &rth->iter_head;
    toku_rth_elt* next = NULL;

    toku_rth_start_scan(rth);
    next = toku__rth_next(rth);
    while (next != head) {
        element = next;
        next    = toku__rth_next(rth);
        rth->free(element);
    }

    rth->free(rth->buckets);
    rth->free(rth);
}
