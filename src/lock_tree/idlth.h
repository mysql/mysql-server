/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if !defined(TOKU_IDLTH_H)
#define TOKU_IDLTH_H
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

#if !defined(TOKU_LOCKTREE_DEFINE)
#define TOKU_LOCKTREE_DEFINE
typedef struct __toku_lock_tree toku_lock_tree;
#endif

typedef struct __toku_lt_map toku_lt_map;
struct __toku_lt_map {
    DICTIONARY_ID   dict_id;
    toku_lock_tree* tree;
};

typedef struct __toku_idlth_elt toku_idlth_elt;
struct __toku_idlth_elt {
    toku_lt_map  value;
    toku_idlth_elt*   next_in_bucket;
    toku_idlth_elt*   next_in_iteration;
    toku_idlth_elt*   prev_in_iteration;
};

typedef struct __toku_idlth toku_idlth;
struct __toku_idlth {
    toku_idlth_elt* buckets;
    uint32_t       num_buckets;
    uint32_t       num_keys;
    toku_idlth_elt  iter_head;
    toku_idlth_elt* iter_curr;
    BOOL            iter_is_valid;
    /** The user malloc function */
    void*         (*malloc) (size_t);
    /** The user free function */
    void          (*free)   (void*);
    /** The user realloc function */
    void*         (*realloc)(void*, size_t);
};

int  toku_idlth_create(toku_idlth** ptable,
                       void* (*user_malloc) (size_t),
                       void  (*user_free)   (void*),
                       void* (*user_realloc)(void*, size_t));

toku_lt_map*    toku_idlth_find       (toku_idlth* table, DICTIONARY_ID dict_id);

void            toku_idlth_start_scan (toku_idlth* table);

toku_lt_map*    toku_idlth_next       (toku_idlth* table);

void            toku_idlth_delete     (toku_idlth* table, DICTIONARY_ID dict_id);

void            toku_idlth_close      (toku_idlth* table);

int             toku_idlth_insert     (toku_idlth* table, DICTIONARY_ID dict_id);

void            toku_idlth_clear      (toku_idlth* idlth);

BOOL            toku_idlth_is_empty   (toku_idlth* idlth);

#if defined(__cplusplus)
}
#endif


#endif
