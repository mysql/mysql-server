/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Hash table with chaining. */
#include "hashtable.h"
#include "memory.h"
#include "primes.h"
// #include "../include/ydb-constants.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "key.h"
#include "yerror.h"
#include "hashfun.h"

static int hashelt_list_verify(struct hashelt_list *helist) {
    HASHELT e = helist->head;
    if (e == 0)
        return helist->tail == 0;
    while (e->next)
        e = e->next;
    return helist->tail == e;
}

static inline void hashelt_list_init(struct hashelt_list *helist) {
    helist->head = helist->tail = 0;
}

static inline void hashelt_list_append(struct hashelt_list *helist, HASHELT e) {
    assert(hashelt_list_verify(helist));
    e->next = 0;
    if (helist->tail)
        helist->tail->next = e;
    else
        helist->head = e;
    helist->tail = e;
    assert(hashelt_list_verify(helist));
}

static inline HASHELT hashelt_list_pop(struct hashelt_list *helist) {
    assert(hashelt_list_verify(helist));
    HASHELT e = helist->head;
    if (e) {
        helist->head = e->next;
        if (helist->head == 0)
            helist->tail = 0;
        assert(hashelt_list_verify(helist));
    }
    return e;
}

static inline HASHELT hashelt_list_peek(struct hashelt_list *helist) {
    return helist->head;
}

int toku_hashtable_create (HASHTABLE *h) {
    HASHTABLE MALLOC(tab);
    unsigned int i;
    if (tab==0) return -1;
    tab->n_keys=0;
    tab->primeidx=0;
    tab->arraysize=toku_get_prime(tab->primeidx);
    assert(sizeof(*tab->array)==sizeof(void*));
    tab->array = toku_calloc(tab->arraysize, sizeof(*tab->array));
    for (i=0; i<tab->arraysize; i++) tab->array[i]=0;
    tab->allow_dups = 1;
    *h=tab;
    return 0;
}

int toku_hashtable_set_dups (HASHTABLE tab, unsigned int allow_dups) {
    tab->allow_dups = allow_dups;
    return 0;
}

static void hash_find_internal (HASHTABLE tab, unsigned int hash, const unsigned char *key, ITEMLEN keylen, HASHDUP *dup_ptr, HASHDUP **prev_ptr) {
    unsigned int h = hash % tab->arraysize;
    HASHDUP dup;
    HASHDUP *prev = &tab->array[h];
    for (dup=*prev; dup; prev=&dup->next, dup=*prev) {
        HASHELT he = hashelt_list_peek(&dup->kdlist); assert(he);
	if (keylen==he->keylen && memcmp(key, he->keyval, keylen)==0) {
	    *prev_ptr = prev;
	    *dup_ptr = dup;
	    return;
	}
    }
    *prev_ptr = prev;
    *dup_ptr = 0;
}

int toku_hash_find_idx (HASHTABLE tab, bytevec key, ITEMLEN keylen, int idx, bytevec *data, ITEMLEN *datalen, int *type) {
    HASHDUP dup, *prev;
    hash_find_internal(tab, hash_key (key, keylen), key, keylen, &dup, &prev);
    if (dup==0) {
	return -1;
    } else {
        HASHELT he = hashelt_list_peek(&dup->kdlist);
        int i;
        for (i=0; i<idx; i++) {
            he = he->next;
            if (he == 0)
                return -2;
        }
	*data = &he->keyval[he->keylen];
	*datalen = he->vallen;
        *type = he->type;
	return 0;
    }
}
    
int toku_hash_find (HASHTABLE tab, bytevec key, ITEMLEN keylen, bytevec *data, ITEMLEN *datalen, int *type) {
    HASHDUP dup, *prev;
    hash_find_internal(tab, hash_key (key, keylen), key, keylen, &dup, &prev);
    if (dup==0) {
	return -1;
    } else {
        HASHELT he = hashelt_list_peek(&dup->kdlist);
	*data = &he->keyval[he->keylen];
	*datalen = he->vallen;
        *type = he->type;
	return 0;
    }
}

int toku_hash_rehash_everything (HASHTABLE tab, unsigned int primeindexdelta) {
    int newprimeindex = primeindexdelta+tab->primeidx;
    assert(newprimeindex>=0);
    unsigned int newarraysize = toku_get_prime(newprimeindex);
    HASHDUP *newarray = toku_calloc(newarraysize, sizeof(*tab->array));
    unsigned int i;
    //printf("%s:%d newarraysize=%d\n", __FILE__, __LINE__, newarraysize);
    assert(newarray!=0);
    tab->primeidx=newprimeindex;
    for (i=0; i<newarraysize; i++) newarray[i]=0;
    for (i=0; i<tab->arraysize; i++) {
	HASHDUP dup;
	while ((dup=tab->array[i])!=0) {
            HASHELT he = hashelt_list_peek(&dup->kdlist); assert(he);
	    //unsigned int hk = hash_key((unsigned char *)he->key, he->keylen);
	    unsigned int h = he->hash%newarraysize;
	    //assert(he->hash==hk);
	    tab->array[i] = dup->next;
	    dup->next = newarray[h];
	    newarray[h] = dup;
	}
    }
    toku_free(tab->array);
    // printf("Freed\n");
    tab->array=newarray;
    tab->arraysize=newarraysize;
    //printf("Done growing or shrinking\n");
    return 0;
}

int toku_hash_insert (HASHTABLE tab, const void *key, ITEMLEN keylen, const void *val, ITEMLEN vallen, int type)
{
    unsigned int hk = hash_key (key,keylen);
    unsigned int h = hk%tab->arraysize;
    HASHDUP dup,*prev_ptr;
    hash_find_internal(tab, hk, key, keylen, &dup, &prev_ptr);
    if (dup == 0) {
        dup = toku_malloc(sizeof *dup); 
        assert(dup);
        hashelt_list_init(&dup->kdlist);

        dup->next = tab->array[h];
        tab->array[h]=dup;
    } else if (!tab->allow_dups)
        return BRT_ALREADY_THERE;

    HASHELT he=toku_malloc(sizeof(*he)+keylen+vallen);
    assert(he); // ?????
    he->type = type;
    he->keylen = keylen;
    he->vallen = vallen;
    memmove(&he->keyval[0], key, keylen);
    memmove(&he->keyval[keylen], val, vallen);
    he->hash = hk;

    hashelt_list_append(&dup->kdlist, he);   

    tab->n_keys++;
    if (tab->n_keys > tab->arraysize) {
        return toku_hash_rehash_everything(tab, +1);
    }
    return BRT_OK;
}

int toku_hash_delete (HASHTABLE tab, const void *key, ITEMLEN keylen) {
    HASHDUP dup, *prev_ptr;
    //printf("%s:%d deleting %s (bucket %d)\n", __FILE__, __LINE__, key, hash_key(key,keylen)%tab->arraysize);
    hash_find_internal(tab, hash_key (key, keylen), key, keylen, &dup, &prev_ptr);
    if (dup==0) return DB_NOTFOUND;
    else {
	assert(*prev_ptr==dup);
        
        HASHELT he = hashelt_list_pop(&dup->kdlist);
        assert(he);
        //printf("%s:%d deleting %s %s\n", __FILE__, __LINE__, he->key, he->val);
        toku_free_n(he, sizeof(*he)+he->keylen+he->vallen);
        tab->n_keys--;

        if (!hashelt_list_peek(&dup->kdlist)) {
            /* delete the dups from the hash list  */
            *prev_ptr = dup->next;
            toku_free_n(dup, sizeof *dup);
        }

	if ((tab->n_keys * 4 < tab->arraysize) && tab->primeidx>0) {
	    return toku_hash_rehash_everything(tab, -1);
	}
	return BRT_OK;
    }
}

int toku_hash_delete_all (HASHTABLE tab, const void *key, ITEMLEN keylen) {
    HASHDUP dup, *prev_ptr;
    //printf("%s:%d deleting %s (bucket %d)\n", __FILE__, __LINE__, key, hash_key(key,keylen)%tab->arraysize);
    hash_find_internal(tab, hash_key (key, keylen), key, keylen, &dup, &prev_ptr);
    if (dup==0) return DB_NOTFOUND;
    else {
	assert(*prev_ptr==dup);
	/* delete the dups from the hash list  */
        *prev_ptr = dup->next;
        
        /* delete all of the kd pairs in the dup list */
        HASHELT he;
        while ((he = hashelt_list_pop(&dup->kdlist)) != 0 ) {
            //printf("%s:%d deleting %s %s\n", __FILE__, __LINE__, he->key, he->val);
            toku_free_n(he, sizeof(*he)+he->keylen+he->vallen);
            tab->n_keys--;
        }

	toku_free_n(dup, sizeof *dup);

	if ((tab->n_keys * 4 < tab->arraysize) && tab->primeidx>0) {
	    return toku_hash_rehash_everything(tab, -1);
	}
	return BRT_OK;
    }
}


int toku_hashtable_random_pick(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen, int *type, long int *randomnumber) {
    unsigned int i;
    unsigned int usei = (*randomnumber)%h->arraysize;
    for (i=0; i<h->arraysize; i++, usei++) {
	if (usei>=h->arraysize) usei=0;
	HASHDUP dup=h->array[usei];
	if (dup) {
            HASHELT he = hashelt_list_peek(&dup->kdlist); assert(he);
	    *key = &he->keyval[0];
	    *keylen = he->keylen;
	    *data = &he->keyval[he->keylen];
	    *datalen = he->vallen;
            *type = he->type;
	    *randomnumber = usei;
	    return 0;
	}
    }
    return -1;
}

#if 0
int hashtable_find_last(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen) {
    bytevec best_k=0, best_d;
    ITEMLEN best_kl, best_dl;
    HASHTABLE_ITERATE(h, this_k, this_kl, this_d, this_dl,
		      ({
			  if (best_k==0 || toku_keycompare(best_k, best_kl, this_k, this_kl)<0) {
			      best_k  = this_k;
			      best_kl = this_kl;
			      best_d  = this_d;
			      best_dl = this_dl;
			  }
		      }));
    if (best_k) {
	*key     = best_k;
	*keylen  = best_kl;
	*data    = best_d;
	*datalen = best_dl;
	return 0;
    } else {
	return -1;
    }
}
#endif

void toku_hashtable_iterate (HASHTABLE tab, void(*f)(bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen, int type, void*args), void* args) {
  /*
    int i;
    for (i=0; i<tab->arraysize; i++) {
	HASHELT he;
	for (he=tab->array[i]; he; he=he->next) {
	    f(he->key, he->keylen, he->val, he->vallen, args);
	}
    }
  */
    HASHTABLE_ITERATE(tab, key, keylen, val, vallen, type, f(key,keylen,val,vallen,type,args));
}

int toku_hashtable_n_entries(HASHTABLE tab) {
    return tab->n_keys;
}

/* Frees the list, but doesn't free the keys. */
static void hasheltlist_free (HASHELT elt) {
    if (elt==0) return;
    else {
	hasheltlist_free(elt->next);
	toku_free_n(elt, sizeof(*elt)+elt->keylen+elt->vallen);
    }
}

/* Frees the table, but doesn't do anything to the contents of the table.   The keys are still alloc'd.  The internal storage of the hashtable is freed. */
void toku_hashtable_free(HASHTABLE *tab) {
    //printf("%s:%d free hashtable %p\n", __FILE__, __LINE__, tab);
    toku_hashtable_clear(*tab);
    //printf("%s:%d free %p\n", __FILE__, __LINE__, tab);n
    toku_free((*tab)->array);
    toku_free_n(*tab, sizeof(**tab));
    *tab=0;
}


void toku_hashtable_clear(HASHTABLE tab) {
    unsigned int i;
    for (i=0; i<tab->arraysize; i++) {
        HASHDUP dup = tab->array[i];
        while (dup) {
            HASHDUP nextdup = dup->next;
            hasheltlist_free(hashelt_list_peek(&dup->kdlist));
            toku_free_n(dup, sizeof *dup);
            dup = nextdup;
        }
	tab->array[i]=0;
    }
    tab->n_keys = 0;
}
