#ifndef MDICT_H
#define MDICT_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "brttypes.h"

//#define USEPMA
#define USEHASH

int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

#ifdef USEPMA

#include "pma.h"
#define MDICT PMA
#define MDICT_OK PMA_OK
#define MDICT_NOTFOUND PMA_NOTFOUND
#define mdict_free toku_pma_free
#define mdict_n_entries toku_pma_n_entries
#define MDICT_ITERATE PMA_ITERATE
#define mdict_insert toku_pma_insert
#define mdict_create pma_create
#define mdict_delete pma_delete
#define mdict_lookup toku_pma_lookup
#define mdict_random_pick toku_pma_random_pick
#define mdict_iterate pma_iterate

#elif defined(USEHASH)
#include "hashtable.h"
#define MDICT HASHTABLE
#define MDICT_OK 0
#define MDICT_NOTFOUND -1
#define MDICT_ALREADY_THERE -2
#define mdict_free hashtable_free
#define mdict_n_entries hashtable_n_entries
#define MDICT_ITERATE HASHTABLE_ITERATE
#define mdict_insert hash_insert
#define mdict_create hashtable_create
#define mdict_delete hash_delete
#define mdict_lookup hash_find
#define mdict_random_pick hashtable_random_pick
#define mdict_iterate hashtable_iterate
#define mdict_find_last hashtable_find_last

#else
/* In-memory dictionary.  */


enum mdict_errors { MDICT_OK=0, MDICT_NOTFOUND = -1, MDICT_ALREADY_THERE = -2 };

typedef struct mdict *MDICT;

int mdict_create (MDICT*);
void mdict_free (MDICT);

int  mdict_n_entries (MDICT);

/* Returns an error if the key is already present. */
/* The values returned should not be modified. */
/* May damage the cursor. */
int mdict_insert (MDICT, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
/* This returns an error if the key is NOT present. */
int mdict_replace (MDICT, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
/* This returns an error if the key is NOT present. */
int mdict_delete (MDICT, bytevec key, ITEMLEN keylen);

/* Exposes internals of the MDICT by returning a pointer to the guts.
 * Don't modify the returned data.  Don't free it. */
int mdict_lookup (MDICT, bytevec key, ITEMLEN keylen, bytevec*data, ITEMLEN *datalen);


int mdict_random_pick(MDICT, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);

void mdict_iterate (MDICT, void(*)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*);

#define MDICT_ITERATE(table,keyvar,keylenvar,datavar,datalenvar,body) ({ \
  void __do_iterate(bytevec keyvar, ITEMLEN keylenvar, bytevec datavar, ITEMLEN datalenvar, void *__ignore __attribute__((__unused__))) { \
      body; \
  } \
  mdict_iterate(table,__do_iterate, 0);		\
})

#endif
#endif
