/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_table.h
   \brief Hash table
  
*/

#include "rth.h"
#include <assert.h>
#include <errno.h>
#include <string.h>

/* TODO: reallocate the hash table if it grows too big. Perhaps, use toku_get_prime in newbrt/primes.c */
const uint32 __toku_rth_init_size = 521;

static inline uint32 toku__rth_hash(toku_rth* table, DB_TXN* key) {
    size_t tmp = (size_t)key;
    return tmp % table->array_size;
}

static inline void toku__invalidate_scan(toku_rth* table) {
    table->finger_end   = TRUE;
}

int toku_rth_create(toku_rth** ptable,
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t)) {
    assert(ptable && user_malloc && user_free && user_realloc);
    int r;
    toku_rth* tmp = (toku_rth*)user_malloc(sizeof(*tmp));
    if (0) { died1: user_free(tmp); return r; }
    if (!tmp) return errno;

    memset(tmp, 0, sizeof(*tmp));
    tmp->malloc         = user_malloc;
    tmp->free           = user_free;
    tmp->realloc        = user_realloc;
    tmp->array_size     = __toku_rth_init_size;
    tmp->table          = (toku_rth_elt**)
                          tmp->malloc(tmp->array_size * sizeof(*tmp->table));
    if (!tmp->table) { r = errno; goto died1; }
    memset(tmp->table, 0, tmp->array_size * sizeof(*tmp->table));
    toku__invalidate_scan(tmp);
    *ptable = tmp;
    return 0;
}

toku_rt_forest* toku_rth_find(toku_rth* table, DB_TXN* key) {
    assert(table && key);

    uint32 index            = toku__rth_hash(table, key);
    toku_rth_elt* element   = table->table[index];
    while (element && element->value.hash_key != key) element = element->next;
    return element ? &element->value : NULL;
}

void toku_rth_start_scan(toku_rth* table) {
    assert(table);
    table->finger_index = 0;
    table->finger_ptr   = table->table[table->finger_index];
    table->finger_start = TRUE;
    table->finger_end   = FALSE;
}

static inline toku_rth_elt* toku__rth_next(toku_rth* table) {
    assert(table);
    assert(!table->finger_end);
    
    if (table->finger_ptr && !table->finger_start) {
        table->finger_ptr = table->finger_ptr->next;
    }
    while (!table->finger_ptr && ++table->finger_index < table->array_size) {
       table->finger_ptr = table->table[table->finger_index]; 
    }
    table->finger_start = FALSE;
    table->finger_end   = !table->finger_ptr;
    return table->finger_ptr;
}

toku_rt_forest* toku_rth_next(toku_rth* table) {
    assert(table);
    toku_rth_elt* next = toku__rth_next(table);
    return next ? &next->value : NULL;
}

/* Element MUST exist. */
void toku_rth_delete(toku_rth* table, DB_TXN* key) {
    assert(table && key);
    toku__invalidate_scan(table);

    /* Must have elements. */
    assert(table->num_keys);

    uint32 index = toku__rth_hash(table, key);
    toku_rth_elt* element = table->table[index];

    /* Elements of the right hash must exist. */
    assert(element);
    /* Case where it is the first element. */
    if (element->value.hash_key == key) {
        table->table[index] = element->next;
        table->free(element);
        table->num_keys--;
        return;
    }
    toku_rth_elt* prev;
    /* Case where it is not the first element. */
    do {
        assert(element);
        prev = element;
        element = element->next;
    } while (element->value.hash_key != key);
    /* Must be found. */
    assert(element);
    prev->next              = element->next;
    table->free(element);
    table->num_keys--;
    return;
}
    
/* Will allow you to insert it over and over.  You need to keep track. */
int toku_rth_insert(toku_rth* table, DB_TXN* key) {
    assert(table && key);
    toku__invalidate_scan(table);

    uint32 index = toku__rth_hash(table, key);

    /* Allocate a new one. */
    toku_rth_elt* element = (toku_rth_elt*)table->malloc(sizeof(*element));
    if (!element) return errno;
    memset(element, 0, sizeof(*element));
    element->value.hash_key = key;
    element->next           = table->table[index];
    table->table[index]     = element;
    table->num_keys++;
    return 0;    
}

void toku_rth_close(toku_rth* table) {
    assert(table);
    toku_rth_elt* element;
    toku_rth_elt* next = NULL;

    toku_rth_start_scan(table);
    next = toku__rth_next(table);
    while (next) {
        element = next;
        next    = toku__rth_next(table);
        table->free(element);
    }

    table->free(table->table);
    table->free(table);
}
