/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#if !defined(TOKU_LTH_H)
#define TOKU_LTH_H
/**
   \file  hash_table.h
   \brief Hash table
  
*/

//Defines bool data type.
#include <db.h>
#include <brttypes.h>
#include <locktree.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(TOKU_LOCKTREE_DEFINE)
#define TOKU_LOCKTREE_DEFINE
typedef struct __toku_lock_tree toku_lock_tree;
#endif


typedef struct __toku_lth_value toku_lth_value;
struct __toku_lth_value {
    toku_lock_tree*          hash_key;
};

typedef struct __toku_lth_elt toku_lth_elt;
struct __toku_lth_elt {
    toku_lth_value  value;
    toku_lth_elt*   next_in_bucket;
    toku_lth_elt*   next_in_iteration;
    toku_lth_elt*   prev_in_iteration;
};

#if !defined(TOKU_LTH_DEFINE)
#define TOKU_LTH_DEFINE
typedef struct __toku_lth toku_lth;
#endif

struct __toku_lth {
    toku_lth_elt*   buckets;
    uint32_t          num_buckets;
    uint32_t          num_keys;
    toku_lth_elt    iter_head;
    toku_lth_elt*   iter_curr;
    bool            iter_is_valid;
};


int  toku_lth_create(toku_lth** ptable);

toku_lock_tree* toku_lth_find       (toku_lth* table, toku_lock_tree* key);

void            toku_lth_start_scan (toku_lth* table);

toku_lock_tree* toku_lth_next       (toku_lth* table);

void            toku_lth_delete     (toku_lth* table, toku_lock_tree* key);

void            toku_lth_close      (toku_lth* table);

int             toku_lth_insert     (toku_lth* table, toku_lock_tree* key);

#if defined(__cplusplus)
}
#endif

#endif

