/* Hash table with chaining. */
#include "hashtable.h"
#include "memory.h"
#include "../include/ydb-constants.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "key.h"
#include "yerror.h"
#include "hashfun.h"

int toku_hashtable_create (HASHTABLE *h) {
    HASHTABLE MALLOC(tab);
    int i;
    if (tab==0) return -1;
    tab->n_keys=0;
    tab->arraysize=8;
    assert(sizeof(*tab->array)==sizeof(void*));
    tab->array = toku_calloc(tab->arraysize, sizeof(*tab->array));
    for (i=0; i<tab->arraysize; i++) tab->array[i]=0;
    *h=tab;
    return 0;
}

static void hash_find_internal (HASHTABLE tab, const unsigned char *key, ITEMLEN keylen, HASHELT *hashelt, HASHELT **prev_ptr) {
    unsigned int h = hash_key (key, keylen) % tab->arraysize;
    HASHELT he;
    HASHELT *prev = &tab->array[h];
    for (he=*prev; he; prev=&he->next, he=*prev) {
	if (keylen==he->keylen && memcmp(key, he->key, keylen)==0) {
	    *prev_ptr = prev;
	    *hashelt = he;
	    return;
	}
    }
    *prev_ptr = prev;
    *hashelt = 0;
}
    
int toku_hash_find (HASHTABLE tab, bytevec key, ITEMLEN keylen, bytevec *data, ITEMLEN *datalen) {
    HASHELT he, *prev_ptr;
    hash_find_internal(tab, key, keylen, &he, &prev_ptr);
    if (he==0) {
	return -1;
    } else {
	*data = he->val;
	*datalen = he->vallen;
	return 0;
    }
}

int toku_hash_rehash_everything (HASHTABLE tab, int newarraysize) {
    HASHELT *newarray = toku_calloc(newarraysize, sizeof(*tab->array));
    int i;
    assert(newarray!=0);
    for (i=0; i<newarraysize; i++) newarray[i]=0;
    for (i=0; i<tab->arraysize; i++) {
	HASHELT he;
	while ((he=tab->array[i])!=0) {
	    unsigned int h = hash_key((unsigned char *)he->key, he->keylen)%newarraysize;
	    tab->array[i] = he->next;
	    he->next = newarray[h];
	    newarray[h] = he;
	}
    }
    toku_free(tab->array);
    // printf("Freed\n");
    tab->array=newarray;
    tab->arraysize=newarraysize;
    //printf("Done growing or shrinking\n");
    return 0;
}

int toku_hash_insert (HASHTABLE tab, const void *key, ITEMLEN keylen, const void *val, ITEMLEN vallen)
{
    unsigned int h = hash_key (key,keylen)%tab->arraysize;
    {
	HASHELT he,*prev_ptr;
	hash_find_internal(tab, key, keylen, &he, &prev_ptr);
	if (he!=0) {
	    return BRT_ALREADY_THERE;
	}
    }
    {
	/* Otherwise the key is not already present, so we need to add it. */
	HASHELT MALLOC(he);
	he->key = memdup(key, keylen);
	he->keylen = keylen;
	he->val = memdup(val, vallen);
	he->vallen = vallen;
	he->next = tab->array[h];
	tab->array[h]=he;
	tab->n_keys++;
	if (tab->n_keys > tab->arraysize) {
	    return toku_hash_rehash_everything(tab, tab->arraysize*2);
	}
	return BRT_OK;
    }
}

int toku_hash_delete (HASHTABLE tab, const void *key, ITEMLEN keylen) {
    HASHELT he, *prev_ptr;
    //printf("%s:%d deleting %s (bucket %d)\n", __FILE__, __LINE__, key, hash_key(key,keylen)%tab->arraysize);
    hash_find_internal(tab, key, keylen, &he, &prev_ptr);
    if (he==0) return DB_NOTFOUND;
    else {
	//printf("%s:%d deleting %s %s\n", __FILE__, __LINE__, he->key, he->val);
	assert(*prev_ptr==he);
	*prev_ptr = he->next;
	//printf("Freeing %s %s\n", he->key, he->val);
	toku_free_n(he->key, he->keylen);
	toku_free_n(he->val, he->vallen);
	toku_free_n(he, sizeof(*he));
	tab->n_keys--;

	if ((tab->n_keys * 4 < tab->arraysize) && tab->arraysize>4) {
	    return toku_hash_rehash_everything(tab, tab->arraysize/2);
	}
	return BRT_OK;
    }
}


int toku_hashtable_random_pick(HASHTABLE h, bytevec *key, ITEMLEN *keylen, bytevec *data, ITEMLEN *datalen) {
    int i;
    int usei = random()%h->arraysize;
    for (i=0; i<h->arraysize; i++, usei++) {
	if (usei>=h->arraysize) usei=0;
	HASHELT he=h->array[usei];
	if (he) {
	    *key = he->key;
	    *keylen = he->keylen;
	    *data = he->val;
	    *datalen = he->vallen;
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
			  if (best_k==0 || keycompare(best_k, best_kl, this_k, this_kl)<0) {
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

void toku_hashtable_iterate (HASHTABLE tab, void(*f)(bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen, void*args), void* args) {
  /*
    int i;
    for (i=0; i<tab->arraysize; i++) {
	HASHELT he;
	for (he=tab->array[i]; he; he=he->next) {
	    f(he->key, he->keylen, he->val, he->vallen, args);
	}
    }
  */
    HASHTABLE_ITERATE(tab, key, keylen, val, vallen, f(key,keylen,val,vallen,args));
}

int toku_hashtable_n_entries(HASHTABLE tab) {
    return tab->n_keys;
}

/* Frees the list, but doesn't free the keys. */
static void hasheltlist_free (HASHELT elt) {
    if (elt==0) return;
    else {
	hasheltlist_free(elt->next);
	toku_free_n(elt->key, elt->keylen);
	toku_free_n(elt->val, elt->vallen);
	toku_free_n(elt, sizeof(*elt));
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
    int i;
    for (i=0; i<tab->arraysize; i++) {
	hasheltlist_free(tab->array[i]);
	tab->array[i]=0;
    }
    tab->n_keys = 0;
}
