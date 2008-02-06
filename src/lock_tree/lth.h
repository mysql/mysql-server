/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_table.h
   \brief Hash table
  
*/

//Defines BOOL data type.
#include <db.h>
#include <brttypes.h>
#include <locktree.h>

typedef struct __toku_lth_elt toku_lth_elt;
struct __toku_lth_elt {
    toku_lock_tree* key;
    toku_lth_elt*   next;
};

typedef struct __toku_lth toku_lth;
struct __toku_lth {
    toku_lth_elt**  table;
    uint32          num_keys;
    uint32          array_size;
    uint32          finger_index;
    toku_lth_elt*   finger_ptr;
    BOOL            finger_start;
    BOOL            finger_end;
    /** The user malloc function */
    void*         (*malloc) (size_t);
    /** The user free function */
    void          (*free)   (void*);
    /** The user realloc function */
    void*         (*realloc)(void*, size_t);
};

int  toku_lth_create(toku_lth** ptable,
                     void* (*user_malloc) (size_t),
                     void  (*user_free)   (void*),
                     void* (*user_realloc)(void*, size_t));

toku_lock_tree* toku_lth_find      (toku_lth* table, toku_lock_tree* key);

void            toku_lth_start_scan(toku_lth* table);

toku_lock_tree* toku_lth_next      (toku_lth* table);

void            toku_lth_delete    (toku_lth* table, toku_lock_tree* key);

void            toku_lth_close     (toku_lth* table);

int             toku_lth_insert    (toku_lth* table, toku_lock_tree* key);
