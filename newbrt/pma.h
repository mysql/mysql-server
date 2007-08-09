#ifndef PMA_H
#define PMA_H

#include "brttypes.h"
#include "ybt.h"
#include "yerror.h"

/* An in-memory Packed Memory Array dictionary. */
/* There is a built-in-cursor. */

typedef struct pma *PMA;
typedef struct pma_cursor *PMA_CURSOR;

/* All functions return 0 on success. */
int pma_create (PMA *,  int (*compare_fun)(DB*,const DBT*,const DBT*));

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int pma_free (PMA *);

int  pma_n_entries (PMA);

/* Returns an error if the key is already present. */
/* The values returned should not be modified.by the caller. */
/* Any cursors should be updated. */
/* Duplicates the key and keylen. */
//enum pma_errors pma_insert (PMA, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
// The DB pointer is there so that the comparison function can be called.
enum pma_errors pma_insert (PMA, DBT*, DBT*, DB*);
/* This returns an error if the key is NOT present. */
int pma_replace (PMA, bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen);
/* This returns an error if the key is NOT present. */
int pma_delete (PMA, DBT *, DB*);

/* Exposes internals of the PMA by returning a pointer to the guts.
 * Don't modify the returned data.  Don't free it. */
enum pma_errors pma_lookup (PMA, DBT*, DBT*, DB*);

/*
 * The kv pairs in the original pma are split into 2 equal sized sets
 * and moved to the leftpma and rightpma.  The size is determined by
 * the sum of the keys and values. the left and right pma's must be
 * empty.
 *
 * origpma - the pma to be split
 * leftpma - the pma assigned keys <= pivot key
 * rightpma - the pma assigned keys > pivot key
 */
int pma_split(PMA origpma, unsigned int *origpma_size,
    PMA leftpma, unsigned int *leftpma_size,
    PMA rightpma, unsigned int *rightpma_size);

/*
 * Insert several key value pairs into an empty pma.  The keys are
 * assumed to be sorted.
 *
 * pma - the pma that the key value pairs will be inserted into.
 *      must be empty with no cursors.
 * keys - an array of keys
 * vals - an array of values
 * n_newpairs - the number of key value pairs
 */
int pma_bulk_insert(PMA pma, DBT *keys, DBT *vals, int n_newpairs);

/* Move the cursor to the beginning or the end or to a key */
int pma_cursor (PMA, PMA_CURSOR *);
int pma_cursor_free (PMA_CURSOR*);

int pma_cursor_set_position_last (PMA_CURSOR c);
int pma_cursor_set_position_first (PMA_CURSOR c);
int pma_cursor_set_position_next (PMA_CURSOR c); /* Requires the cursor is init'd.  Returns DB_NOTFOUND if we fall off the end. */
int pma_cursor_set_position_prev (PMA_CURSOR c);
int pma_cget_current (PMA_CURSOR c, DBT *key, DBT *val);

/*
 * Get the last key and value in the pma
 */
int pma_get_last(PMA pma, DBT *key, DBT *val);

/* Return PMA_NOTFOUND if the pma is empty. */
#if 0
int pma_cget_first (PMA_CURSOR, YBT */*key*/, YBT */*val*/);
int pma_cursor_first (PMA);
int pma_cursor_last  (PMA);
int pma_cursor_set (PMA, bytevec key, int keylen);
int pma_cursor_next (PMA);
int pma_cursor_prev (PMA);
int pma_cursor_get (PMA, bytevec *key, int *keylen, bytevec *data, int *datalen);
#endif

int pma_random_pick(PMA, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);

int pma_index_limit(PMA);
int pmanode_valid(PMA,int);
bytevec pmanode_key(PMA,int);
ITEMLEN pmanode_keylen(PMA,int);
bytevec pmanode_val(PMA,int);
ITEMLEN pmanode_vallen(PMA,int);

void pma_iterate (PMA, void(*)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*);

#define PMA_ITERATE(table,keyvar,keylenvar,datavar,datalenvar,body) ({ \
  int __i;                                                             \
  for (__i=0; __i<pma_index_limit(table); __i++) {		       \
    if (pmanode_valid(table,__i)) {                                    \
      bytevec keyvar = pmanode_key(table,__i);                         \
      ITEMLEN keylenvar = pmanode_keylen(table,__i);                   \
      bytevec datavar = pmanode_val(table, __i);                       \
      ITEMLEN datalenvar = pmanode_vallen(table, __i);                 \
      body;                                                            \
} } })
#endif

int keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

