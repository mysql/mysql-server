/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  hash_table.h
   \brief Hash table
  
*/

//Defines BOOL data type.
#include <brttypes.h>

typedef u_int32_t uint32;

/* TODO: reallocate the hash table if it grows too big. Perhaps, use toku_get_prime in newbrt/primes.c */
const uint32 __toku_rth_init_size = 521;

typedef struct __toku_rt_forest toku_rt_forest;
struct __toku_rt_forest {
    toku_range_tree* selfread;
    toku_range_tree* selfwrite;
};

typedef struct __toku_rth_elt toku_rth_elt;
struct __toku_rth_elt {
    DB_TXN* key;
    toku_range_forest value;
    toku_rth_elt* next;
};

typedef struct {
    uint32 index;
    toku_rth_elt* next;
} toku_rth_finger;

typedef struct __toku_rt_hash_elt toku_rt_hash_elt;
struct toku_rt_hashtable {
    toku_rth_elt** table;
    uint32 num_keys;
    uint32 array_size;
};

int toku_rth_create(toku_rt_hashtable** ptable);

int toku_rth_find(toku_rt_hashtable* table, DB_TXN* key, toku_rt_forest* value, BOOL* found);
int toku_rth_scan(toku_rt_hashtable* table, toku_rt_forest* value, toku_rth_finger* finger);
int toku_rth_delete(toku_rt_hashtable* table, DB_TXN* key);
int toku_rth_close(toku_rt_hashtable* table);
