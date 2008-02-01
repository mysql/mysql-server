#ifndef HASHTABLE_H
#define HASHTABLE_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "brttypes.h"
/* Hash table with chaining. */
/* The keys and values are byte sequences. */
/* The keys and values are malloc'd by the hashtable. */
/* Duplicate keys are allowed by default and are stored in a FIFO list */
   
typedef struct hashtable *HASHTABLE;

int toku_hashtable_create (HASHTABLE*);

/* Configure the hash table for duplicate keys.
   allow_dups != 0 -> duplications allowed,  allow_dups == 0 -> no duplicates */

int toku_hashtable_set_dups (HASHTABLE, unsigned int allow_dups);

/* Return 0 if the key is found in the hashtable, -1 otherwise. */
/* Warning: The data returned points to the internals of the hashtable.  It is set to "const" to try to prevent you from messing it up. */
int toku_hash_find (HASHTABLE tab, bytevec key, ITEMLEN keylen, bytevec *data, ITEMLEN *datalen, int *type);

/* match on key, index on duplicates */
int toku_hash_find_idx (HASHTABLE tab, bytevec key, ITEMLEN keylen, int idx, bytevec *data, ITEMLEN *datalen, int *type);

/* Insert the key/data pair into the hash table.
   If the key is not in the hash table then insert it.
   If the key already exists and duplicates are allowed then append it to the list of duplicates.
   If the key already exists and duplicates are not allowed then return an error */

int toku_hash_insert (HASHTABLE tab, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type);

/* Delete the first entry with the given key 
   It is OK to delete something that isn't there. */

int toku_hash_delete (HASHTABLE tab, const void *key, ITEMLEN keylen);

/* Delete all entries with the given key */

int toku_hash_delete_all (HASHTABLE tab, const void *key, ITEMLEN keylen);

void toku_hashtable_free(HASHTABLE *tab);
int toku_hashtable_n_entries(HASHTABLE);
  
void toku_hashtable_clear(HASHTABLE);

int toku_hashtable_random_pick(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, int *type, long int *randomnumber);
//int hashtable_find_last(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen);

typedef struct hashelt *HASHELT;
struct hashelt {
    HASHELT next;
    unsigned int hash;
    int type;
    ITEMLEN keylen;  
    ITEMLEN vallen;
    char keyval[]; /* the first KEYLEN bytes are the key.  The next bytes are the value. */
};

struct hashelt_list {
    HASHELT head;
    HASHELT tail;
};

typedef struct hashdup *HASHDUP;
struct hashdup {
    HASHDUP next;
    struct hashelt_list kdlist;
};  

struct hashtable {
    HASHDUP *array;
    unsigned int n_keys;
    unsigned int arraysize;
    unsigned int primeidx;
    unsigned int allow_dups;
};

/* You cannot add or delete elements from the hashtable while iterating. */
void toku_hashtable_iterate (HASHTABLE tab, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen,int type, void*), void*);

// If you don't want to use something, do something like use "key __attribute__((__unused__))" for keyvar.
#define HASHTABLE_ITERATE(table,keyvar,keylenvar,datavar,datalenvar,typevar,body) ({ \
    unsigned int hi_counter;                                                         \
    for (hi_counter=0; hi_counter<table->arraysize; hi_counter++) {                  \
        HASHDUP hi_dup;                                                              \
        for (hi_dup=table->array[hi_counter]; hi_dup; hi_dup=hi_dup->next) {         \
            HASHELT hi_he;                                                           \
            for (hi_he=hi_dup->kdlist.head; hi_he; hi_he=hi_he->next) {              \
                const char *keyvar     = &hi_he->keyval[0];                          \
                ITEMLEN     keylenvar  = hi_he->keylen;                              \
                const char *datavar    = &hi_he->keyval[hi_he->keylen];              \
                ITEMLEN     datalenvar = hi_he->vallen;                              \
                int         typevar    = hi_he->type;                                \
                body;                                                                \
            }}}})

#endif
