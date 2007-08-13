#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "brttypes.h"
/* Hash table with chaining. */
/* The keys and values are byte sequences. */
/* The keys and values are malloc'd by the hashtable. */
   
typedef struct hashtable *HASHTABLE;

int toku_hashtable_create (HASHTABLE*);

/* Return 0 if the key is found in the hashtable, -1 otherwise. */
/* Warning: The data returned points to the internals of the hashtable.  It is set to "const" to try to prevent you from messing it up. */
int toku_hash_find (HASHTABLE tab, bytevec key, ITEMLEN keylen, bytevec*data, ITEMLEN *datalen);

/* Replace the key if it was already there. */
int toku_hash_insert (HASHTABLE tab, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen);

/* It is OK to delete something that isn't there. */
int toku_hash_delete (HASHTABLE tab, const void *key, ITEMLEN keylen);
void toku_hashtable_free(HASHTABLE *tab);
int toku_hashtable_n_entries(HASHTABLE);
  
void toku_hashtable_clear(HASHTABLE);

int toku_hashtable_random_pick(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);
//int hashtable_find_last(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);

typedef struct hashelt *HASHELT;
struct hashelt {
    char *key; ITEMLEN keylen;  /* key is NULL for empty elements */
    char *val; ITEMLEN vallen;
    unsigned int hash;
    HASHELT next;
};

struct hashtable {
    int n_keys;
    int arraysize;
    HASHELT *array;
};

/* You cannot add or delete elements from the hashtable while iterating. */
void toku_hashtable_iterate (HASHTABLE tab, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,void*), void*);
// If you don't want to use something, do something like use "key __attribute__((__unused__))" for keyvar.
#define HASHTABLE_ITERATE(table,keyvar,keylenvar,datavar,datalenvar,body) ({ \
  int hi_counter;                                                            \
  for (hi_counter=0; hi_counter<table->arraysize; hi_counter++) {            \
    HASHELT hi_he;                                                           \
    for (hi_he=table->array[hi_counter]; hi_he; hi_he=hi_he->next) {           \
      const char *keyvar     = hi_he->key;                                   \
      ITEMLEN     keylenvar  = hi_he->keylen;                                \
      const char *datavar    = hi_he->val;                                   \
      ITEMLEN     datalenvar = hi_he->vallen;                                \
      body;                                                                  \
  }}})

#endif
