/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_table.h
   \brief Hash table
  
*/

#include "hash_table.h"

static uint32 __toku_rth_hash(toku_rt_hashtable* table, DB_TXN* key) {
    assert(table);
    size_t tmp = (size_t)key;
    return tmp % table->array_size;
}

int toku_rth_create(toku_rt_hashtable** ptable,
                    void* (*user_malloc) (size_t),
                    void  (*user_free)   (void*),
                    void* (*user_realloc)(void*, size_t)) {
    int r;
    toku_rt_hashtable* tmp = (toku_rt_hashtable*)user_malloc(sizeof(*tmp));
    if (0) { died1: user_free(tmp); return r; }
    if (!tmp) return errno;

    memset(tmp, 0, sizeof(*tmp));
    tmp->malloc     = user_malloc;
    tmp->free       = user_free;
    tmp->realloc    = user_realloc;
    tmp->array_size = __toku_rth_init_size;
    tmp->table      = (toku_rth_elt**)
                      tmp->malloc(tmp->array_size * sizeof(*tmp->table));
    if (!tmp->table) { r = errno; goto died1; }
    *ptable = tmp;
    return 0;
}

toku_rt_forest* toku_rth_find(toku_rt_hashtable* table, DB_TXN* key) {
    assert(table && key);

    uint32 index = __toku_rth_hash(table, key);
    toku_rt_hash_elt* element = table->table[index];
    while (element && element->key != key) element = element->next;
    return element ? &element->value : NULL;
}

void toku_rth_start_scan(toku_rt_hashtable* table) {
    assert(table);
    table->finger_index = 0;
    table->finger_ptr   = NULL;
}

toku_rt_forest* toku_rth_next(toku_rt_hashtable* table) {
    assert(table && value && found);
    if (table->finger_ptr) table->finger_ptr = table->finger_ptr->next;
    while (!table->finger_ptr && table->finger_index < table->array_size) {
       table->finger_ptr = table->table[++table->finger_index];
    };
    return table->finger_ptr;
}

int toku_rth_delete(toku_rt_hashtable* table, DB_TXN* key) {
    assert(table && key);
    /* No elements. */
    if (!table->num_keys) return EDOM;

    uint32 index = __toku_rth_hash(table, key);
    toku_rt_hash_elt* element = table->table[index];

    /* No elements of the right hash. */
    if (!element) return EDOM;
    /* Case where it is the first element. */
    if (element->key == key) {
        table->table[index] = element->next;
        goto recycle;
    }
    toku_rt_hash_elt* prev;
    /* Case where it is not the first element. */
    do {
        prev = element;
        element = element->next;
    } while (element && element->key != key);
    /* Not found. */
    if (!element) return EDOM;
    prev->next              = element->next;
    goto recycle;

recycle:
    element->next           = table->free_list;
    table->free_list        = element;
    table->num_keys--;
    return 0;    
}
    
void toku_rth_close(toku_rt_hashtable* table) {
    toku_rt_hash_elt* element;
    toku_rt_hash_elt* next = NULL;

    toku_rth_start_scan(table);
    next = toku_rth_next(table);
    while (next) {
        element = next;
        next    = toku_rth_next(table);
        table->free(element);
    }

    next = table->free_list;
    while (next) {
        element = next;
        next    = next->next;
        table->free(element);
    }

    table->free(table->table);
    table->free(table);
}

/* Will allow you to insert it over and over.  You need to keep track. */
int toku_rth_insert(toku_rt_hashtable* table, DB_TXN* key,
                    toku_rt_forsest* value) {
    assert(table && key && value);

    uint32 index = __toku_rth_hash(table, key);
    toku_rt_hash_elt* next      = table->table[index];

    /* Recycle */
    toku_rt_hash_elt* element;
    if (table->free_list) {
        element                 = table->free_list;
        table->free_list        = table->free_list->next;
    }
    else {
        /* Allocate a new one. */
        element = (toku_rt_hash_elt*)table->malloc(sizeof(*element));
        if (!element) return errno;
    }
    element->next               = table->table[index];
    table->table[index]->next   = element;
    table->num_keys++;
    return 0;    
}
       