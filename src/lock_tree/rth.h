/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if !defined(TOKU_RTH_H)
#define TOKU_RTH_H
/**
   \file  hash_table.h
   \brief Hash table
  
*/

//Defines BOOL data type.
#include <db.h>
#include <brttypes.h>
#include <rangetree.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __rt_forest rt_forest;
struct __rt_forest {
    TXNID            hash_key;
    toku_range_tree* self_read;  //Set of range read locks held by txn 'hash_key'
    toku_range_tree* self_write; //Set of range write locks held by txn 'hash_key'
};

typedef struct __toku_rth_elt toku_rth_elt;
struct __toku_rth_elt {
    rt_forest  value;
    toku_rth_elt*   next_in_bucket;
    toku_rth_elt*   next_in_iteration;
    toku_rth_elt*   prev_in_iteration;
};

typedef struct __toku_rth toku_rth;
struct __toku_rth {
    toku_rth_elt*   buckets;
    uint32_t       num_buckets;
    uint32_t       num_keys;
    toku_rth_elt    iter_head;
    toku_rth_elt*   iter_curr;
    BOOL            iter_is_valid;
    /** The user malloc function */
    void*         (*malloc) (size_t);
    /** The user free function */
    void          (*free)   (void*);
    /** The user realloc function */
    void*         (*realloc)(void*, size_t);
};

int  toku_rth_create(toku_rth** ptable,
                     void* (*user_malloc) (size_t),
                     void  (*user_free)   (void*),
                     void* (*user_realloc)(void*, size_t));

rt_forest* toku_rth_find       (toku_rth* table, TXNID key);

void            toku_rth_start_scan (toku_rth* table);

rt_forest* toku_rth_next       (toku_rth* table);

void            toku_rth_delete     (toku_rth* table, TXNID key);

void            toku_rth_close      (toku_rth* table);

int             toku_rth_insert     (toku_rth* table, TXNID key);

void            toku_rth_clear      (toku_rth* rth);

BOOL            toku_rth_is_empty   (toku_rth* rth);

#if defined(__cplusplus)
}
#endif

#endif
