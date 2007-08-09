/* An in-memory Packed Memory Array dictionary.
   The keys and values are arrays of bytes, but are not necessarily kept in scan order.
   Only the pointers are kept.
 */

#include "key.h"
#include "memory.h"
#include "myassert.h"
#include "../include/ydb-constants.h"
#include <stdio.h>
#include <errno.h>
/* Only needed for testing. */
#include <string.h>

#include "list.h"
#include "kv-pair.h"
#include "pma-internal.h"

/* TODO get this from a include file */
#define KEY_VALUE_OVERHEAD 8

int pma_n_entries (PMA pma) {
    return pma->n_pairs_present;
}

int pma_index_limit (PMA pma) {
    return pma->N;
}

int pmanode_valid (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i] != 0;
}

bytevec pmanode_key (PMA pma, int i) {
    struct kv_pair *pair;
    assert(0<=i); assert(i<pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(pair);
    return pair->key;
}

ITEMLEN pmanode_keylen (PMA pma, int i) {
    struct kv_pair *pair;
    assert(0<=i); assert(i<pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(pair);
    return pair->keylen;
}

bytevec pmanode_val (PMA pma, int i) {
    struct kv_pair *pair;
    assert(0<=i); assert(i<pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(pair);
    return pair->key + pair->keylen;
}

ITEMLEN pmanode_vallen (PMA pma, int i) {
    struct kv_pair *pair;
    assert(0<=i); assert(i<pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(pair);
    return pair->vallen;
}


/* Could pick the same one every time if we wanted. */
int pma_random_pick(PMA pma, bytevec *key, ITEMLEN *keylen, bytevec *val, ITEMLEN *vallen) {
#if 1
    int i;
    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<pma_index_limit(pma); i++) {
	if (pma->pairs[i]) {
	    *key = pmanode_key(pma,i);
	    *keylen = pmanode_keylen(pma,i);
	    *val = pmanode_val(pma,i);
	    *vallen = pmanode_vallen(pma,i);
	    return 0;
	}
    }
    return DB_NOTFOUND;
#else
    /* Maybe we should pick a random item to remove in order to reduce the unbalancing. */
    int i;
    int l = pma_index_limit(pma);
    int r = random()%l;
    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<l; i++) {
	int ir=(i+r)%l;
	if (pma->pairs[ir].key) {
	    *key = pmanode_key(pma,ir);
	    *keylen = pmanode_keylen(pma,ir);
	    *val = pmanode_val(pma,ir);
	    *vallen = pmanode_vallen(pma,ir);
	    return 0;
	}
    }
    return DB_NOTFOUND;

#endif
}

static int pma_count_finds=0;
static int pma_count_divides=0;
static int pma_count_scans=0;
void pma_show_stats (void) {
    printf("%d finds, %d divides, %d scans\n", pma_count_finds, pma_count_divides, pma_count_scans);
}

// Return the smallest index such that no lower index contains a larger key.
// This will be in the range 0 (inclusive) to  pma_index_limit(pma) (inclusive).
// Thus the returned index may not be a valid index into the array if it is == pma_index_limit(pma)
// For example: if the array is empty, that means we return 0.
// For example: if the array is full of small keys, that means we return pma_index_limit(pma), which is off the end of teh array.
// For example: if the array is full of large keys, then we return 0.
int pmainternal_find (PMA pma, DBT *k, DB *db) {
    int lo=0, hi=pma_index_limit(pma);
    /* lo and hi are the minimum and maximum values (inclusive) that we could possibly return. */
    pma_count_finds++;
    while (lo<hi) {
	int mid;
	// Scan forward looking for a non-null value.
	for (mid=(lo+hi)/2; mid<hi; mid++) {
	    if (pma->pairs[mid]!=0) {
		// Found one.
		DBT k2;
		int cmp = pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[mid]->key, pma->pairs[mid]->keylen));
		if (cmp==0) return mid;
		else if (cmp<0) {
		    /* key is smaller than the midpoint, so look in the low half. */
		    hi = (lo+hi)/2; /* recalculate the midpoint, since mid is no necessarily the midpoint now. */ 
		    pma_count_divides++;
		    goto next_range;
		} else {
		    /* key is larger than the midpoint.  So look in the high half. */
		    lo = mid+1; /* The smallest value we could want to return is lo. */
		    pma_count_divides++;
		    goto next_range;
		}
		/* Not reached */
	    }
	    pma_count_scans++;
	}
	/* If we got here, all from mid to hi were null, so adjust hi to the midpoint. */
	/* If the whole array is null, we'll end up returning index 0, which is good. */
	hi = (lo+hi)/2;
	pma_count_divides++;
    next_range: ; /* We have adjusted lo and hi, so look again. */ 
    }
    assert(0<=lo);
    assert(lo==hi);
    assert(hi <= pma_index_limit(pma));
    /* If lo points at something, the something should not be smaller than key. */
    if (lo>0 && lo < pma_index_limit(pma) && pma->pairs[lo]) {
	//printf("lo=%d\n", lo);
	DBT k2;
	assert(0 >= pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[lo]->key, pma->pairs[lo]->keylen)));
    }
    return lo;
}

//int min (int i, int j) { if (i<j) return i; else return j; }
//int max (int i, int j) { if (i<j) return j; else return i; }
//double lg (int n) { return log((double)n)/log(2.0); }

int pmainternal_printpairs (struct kv_pair *pairs[], int N) {
    int count=0;
    int i;
    printf("{");
    for (i=0; i<N; i++) {
        if (i!=0) printf(" ");
        if (pairs[i]) {
            printf("%s", (char*)pairs[i]->key);
            count++;
        }
        else printf("_");
    }
    printf("}");
    return count;
}

void print_pma (PMA pma) {
    int count;
    printf("N=%d n_present=%d ", pma_index_limit(pma), pma->n_pairs_present);
    count=pmainternal_printpairs(pma->pairs, pma_index_limit(pma));
    printf("\n");
    assert(count==pma->n_pairs_present);
}

/* Smooth the data, and return the location of the null. */
int distribute_data (struct kv_pair *destpairs[], int   dcount,
		     struct kv_pair_tag sourcepairs[], int scount, PMA pma) {
    assert(scount<=dcount);
    if (scount==0) {
	return -1;
    }
    if (scount==1) {
	destpairs[0]=sourcepairs[0].pair;
        if (pma)
            sourcepairs[0].newtag = destpairs - pma->pairs;
	if (destpairs[0]==0) return 0;
	else return -1;
    } else {
	int r1 = distribute_data(destpairs, dcount/2,
				 sourcepairs, scount/2, pma);
	int r2 = distribute_data(destpairs  +dcount/2, dcount-dcount/2,
				 sourcepairs+scount/2, scount-scount/2, pma);
	assert(r1==-1 || r2==-1);
	if (r1!=-1)      return r1;
	else if (r2!=-1) return r2+dcount/2;
	else             return -1;
    }
}

/* spread the non-empty pairs around.  There are n of them.  Create an empty slot just before the IDXth
   element, and return that slot's index in the smoothed array. */
int pmainternal_smooth_region (struct kv_pair *pairs[], int n, int idx, int base, PMA pma) {
    int i;
    int n_present=0;
    for (i=0; i<n; i++) {
	if (pairs[i]) n_present++;
    }
    n_present++; // Save one for the blank guy.
    {
//#define USE_MALLOC_IN_SMOOTH
#ifdef USE_MALLOC_IN_SMOOTH
	struct kv_pair_tag *MALLOC_N(n_present, tmppairs);
#else
	struct kv_pair_tag tmppairs[n_present];
#endif
	int n_saved=0;
	int r;

	for (i=0; i<n; i++) {
	    if (i==idx) {
		tmppairs[n_saved++].pair = 0;
	    }
	    if (pairs[i]) {
                tmppairs[n_saved].oldtag = base + i;
		tmppairs[n_saved++].pair = pairs[i];
	    }
        pairs[i] = 0;
	}
	if (idx==n) {
	    tmppairs[n_saved++].pair = 0;
	}
	//printf(" temp="); printpairs(tmppairs, n_saved);
	assert(n_saved==n_present);
	/* Now the tricky part.  Distribute the data. */
	r=distribute_data (pairs, n,
			   tmppairs, n_saved, pma);

        if (pma && !list_empty(&pma->cursors)) {
            struct list cursors;

            list_move(&cursors, &pma->cursors);
            pma_update_region(pma, &cursors, tmppairs, n_present);
            while (!list_empty(&cursors)) {
                struct list *list = list_head(&cursors);
                list_remove(list);
                list_push(&pma->cursors, list);
            }
        }
#ifdef USE_MALLOC_IN_SMOOTH
	toku_free(tmppairs);
#endif
	return r;
    }
}

int lg (int n) {
    int result=0;
    int two_to_result = 1;
    while (two_to_result<n) {
	result++;
	two_to_result*=2;
    }
    return result;
}

void pmainternal_calculate_parameters (PMA pma)
/* Calculate densitystep and uplgN, given N. */
{
    int N = pma_index_limit(pma);
    int lgN = lg(N);
    int n_divisions=0;
    //printf("N=%d lgN=%d\n", N, lgN);
    while (N/2>=lgN) {
	n_divisions++;
	N/=2;
    }
    pma->uplgN=N;
    //printf("uplgN = %d n_divisions=%d\n", pma->uplgN, n_divisions);
    assert(n_divisions>0);
    pma->densitystep = 0.5/n_divisions;
}

int pmainternal_count_region (struct kv_pair *pairs[], int lo, int hi) {
    int n=0;
    while (lo<hi) {
	if (pairs[lo]) n++;
	lo++;
    }
    return n;
}

int pma_create (PMA *pma, int (*compare_fun)(DB*,const DBT*,const DBT*)) {
    int error;
    TAGMALLOC(PMA, result);
    if (result==0) return -1;
    result->n_pairs_present = 0;
    result->pairs = 0;
    list_init(&result->cursors);
    result->compare_fun = compare_fun;
    result->skey = 0;
    result->sval = 0;
    result->N = 4;
#if 0 /* memory.c is broken */
    result->pairs = 0;
#else
    result->pairs = toku_malloc((1 + 4) * sizeof (struct kv_pair *));
#endif

    error = pma_resize_array(result, 4);
    if (error) {
        toku_free(result);
        return -1;
    }

    *pma = result;
    assert((unsigned long)result->pairs[result->N]==0xdeadbeefL);
    return 0;
}

int pma_resize_array(PMA pma, int asksize) {
    int i;
    int n;

    /* find the smallest power of 2 >= n */
    n = 4;
    while (n < asksize)
        n *= 2;

    pma->N = n;
    pma->pairs = toku_realloc(pma->pairs, (1 + pma->N) * sizeof (struct kv_pair *));
    if (pma->pairs == 0)
        return -1;
    pma->pairs[pma->N] = (void *) 0xdeadbeef;

    for (i=0; i<pma->N; i++) {
        pma->pairs[i] = 0;
    }
    pmainternal_calculate_parameters(pma);
    return 0;
}

int pma_cursor (PMA pma, PMA_CURSOR *cursp) {
    PMA_CURSOR MALLOC(curs);
    assert(curs!=0);
    if (errno!=0) return errno;
    curs->position=-1; /* undefined */
    curs->pma = pma;
    curs->skey = 0;
    curs->sval=0;
    list_push(&pma->cursors, &curs->next);
    *cursp=curs;
    return 0;
}

int pma_cursor_set_position_last (PMA_CURSOR c) 
{
    PMA pma = c->pma;
    c->position=pma->N-1;
    while (c->pma->pairs[c->position]==0) {
	if (c->position>0) c->position--;
	else return DB_NOTFOUND;
    }
    return 0;
}

int pma_cursor_set_position_prev (PMA_CURSOR c) {
    PMA pma = c->pma;
    int old_position = c->position;
    c->position--;
    while (c->position >= 0) {
        if (pma->pairs[c->position] != 0)
            return 0;
        c->position--;
    }
    c->position = old_position;
    return DB_NOTFOUND;
}

int pma_cursor_set_position_first (PMA_CURSOR c) 
{
    PMA pma = c->pma;
    c->position=0;
    while (c->pma->pairs[c->position]==0) {
	if (c->position+1<pma->N) c->position++;
	else return DB_NOTFOUND;
    }
    return 0;
}
    
int pma_cursor_set_position_next (PMA_CURSOR c)
{
    PMA pma = c->pma;
    int old_position=c->position;
    c->position++;
    while (c->position<pma->N) {
	if (c->pma->pairs[c->position]!=0) return 0;
	c->position++;
    }
    c->position=old_position;
    return DB_NOTFOUND;
}

int pma_cget_current (PMA_CURSOR c, DBT *key, DBT *val) {
    PMA pma = c->pma;
    struct kv_pair *pair = pma->pairs[c->position];
    if (pair==0) return BRT_KEYEMPTY;
    ybt_set_value(key, pair->key, pair->keylen, &c->skey);
    ybt_set_value(val, pair->key + pair->keylen, pair->vallen, &c->sval);
    return 0;
}


#if 0
int pma_cget_first (PMA_CURSOR c, YBT *key, YBT *val) {
    PMA pma=c->pma;
    c->position=0;
    if (pma->n_pairs_present==0) return DB_NOTFOUND;
    while (pma->pairs[c->position].key==0 && c->position<pma->N) {
	c->position++;
    }
    assert(c->position<pma->N && pma->pairs[c->position].key!=0);
    ybt_set_value(key, pma->pairs[c->position].key, pma->pairs[c->position].keylen, &c->skey);
    ybt_set_value(val, pma->pairs[c->position].val, pma->pairs[c->position].vallen, &c->sval);
    return 0;
}
#endif

int pma_cursor_free (PMA_CURSOR *cursp) {
    PMA_CURSOR curs=*cursp;
    list_remove(&curs->next);
    if (curs->skey) toku_free(curs->skey);
    if (curs->sval) toku_free(curs->sval);
    toku_free(curs);
    *cursp=0;
    return 0;
}

/* Make some space for a key to go at idx (the thing currently at idx should end up at to the right.) */
/* Return the new index.  (Making space may involve moving things around, including the hole at index.) */
int pmainternal_make_space_at (PMA pma, int idx) {
    /* Within a range LO to HI we have a limit of how much packing we will tolerate.
     * We allow the entire array to be 50% full.
     * We allow a region of size lgN to be full.
     * At sizes in between, we interpolate.
     */
    int size=pma->uplgN;
    int lo=idx;
    int hi=idx;
    double density=1.0;
    while (1) {
	/* set hi-lo equal size, make sure it is a supserset of (hi,lo).  */
	lo=idx-size/2;
	hi=idx+size/2;
	//printf("lo=%d hi=%d\n", lo, hi);
	if (lo<0) { hi-=lo; lo=0; }
	else if (hi>pma_index_limit(pma)) { lo-=(hi-pma_index_limit(pma)); hi=pma_index_limit(pma); }
	else { ; /* nothing */ }
	
	//printf("lo=%d hi=%d\n", lo, hi);
	assert(0<=lo); assert(lo<hi); assert(hi<=pma_index_limit(pma)); assert(hi-lo==size); // separate into separate assertions so that gcov doesn't see branches not taken.
	assert(density>0.499); assert(density<=1);
	if (density<0.5001) { assert(lo==0); assert(hi==pma_index_limit(pma)); }
	{
	    int count = (1+ /* Don't forget space for the new guy. */
			 pmainternal_count_region(pma->pairs, lo, hi));
	    if (count/(double)(hi-lo) <= density) break;
	    if (lo==0 && hi==pma_index_limit(pma)) {
		/* The array needs to be doubled in size. */
		int i;
		assert(size==pma_index_limit(pma));
		size*=2;
		//printf("realloc %p to %d\n", pma->pairs, size*sizeof(*pma->pairs));
		pma->pairs = toku_realloc(pma->pairs, (1+size)*sizeof(struct kv_pair *));
		for (i=hi; i<size; i++) pma->pairs[i]=0;
		pma->pairs[size] = (void*)0xdeadbeefL;
		pma->N=size;
		pmainternal_calculate_parameters(pma);
		hi=size;
		//printf("doubled N\n");
		break;
	    }
	}
	density-=pma->densitystep;
	size*=2;
    }
    //printf("%s:%d Smoothing from %d to %d to density %f\n", __FILE__, __LINE__, lo, hi, density);
    {
	int new_index = pmainternal_smooth_region(pma->pairs+lo, hi-lo, idx-lo, lo, pma);

	return new_index+lo;
    }
}

enum pma_errors pma_lookup (PMA pma, DBT *k, DBT *v, DB *db) {
    DBT k2;
    struct kv_pair *pair;
    int l = pmainternal_find(pma, k, db);
    assert(0<=l ); assert(l<=pma_index_limit(pma));
    if (l==pma_index_limit(pma)) return DB_NOTFOUND;
    pair = pma->pairs[l];
    if (pair!=0 && pma->compare_fun(db, k, fill_dbt(&k2, pair->key, pair->keylen))==0) {
        return ybt_set_value(v, pair->key + pair->keylen, pair->vallen, &pma->sval);
    } else {
        return DB_NOTFOUND;
    }
}

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int pma_free (PMA *pmap) {
    int i;
    PMA pma=*pmap;
    if (!list_empty(&pma->cursors)) return -1;
    for (i=0; i<pma_index_limit(pma); i++) {
        if (pma->pairs[i]) {
            kv_pair_free(pma->pairs[i]);
            pma->pairs[i] = 0;
        }
    }
    toku_free(pma->pairs);
    if (pma->skey) toku_free(pma->skey);
    if (pma->sval) toku_free(pma->sval);
    toku_free(pma);
    *pmap=0;
    return 0;
}

/* Copies keylen and datalen */ 
int pma_insert (PMA pma, DBT *k, DBT *v, DB* db) {
    int idx = pmainternal_find(pma, k, db);
    if (idx < pma_index_limit(pma) && pma->pairs[idx]) {
        DBT k2;
        if (0==pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[idx]->key, pma->pairs[idx]->keylen))) {
            return BRT_ALREADY_THERE; /* It is already here.  Return an error. */
        }
    }
    if (pma->pairs[idx]) {
        idx = pmainternal_make_space_at (pma, idx); /* returns the new idx. */
    }
    assert(!pma->pairs[idx]);
    pma->pairs[idx] = kv_pair_malloc(k->data, k->size, v->data, v->size);
    assert(pma->pairs[idx]);
    pma->n_pairs_present++;
    return BRT_OK;
}    

int pma_delete (PMA pma, DBT *k, DB *db) {
    int l = pmainternal_find(pma, k, db);
    struct kv_pair *pair = pma->pairs[l];
    if (pair==0) {
	printf("%s:%d l=%d r=%d\n", __FILE__, __LINE__, l, DB_NOTFOUND);
	return DB_NOTFOUND;
    }
    kv_pair_free(pair);
    pma->pairs[l] = 0;
    pma->n_pairs_present--;
    // Need to rebalance
//    smooth_after_delete(pma,l);
    return BRT_OK;
}

void pma_iterate (PMA pma, void(*f)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*v) {
    int i;
    for (i=0; i<pma_index_limit(pma); i++) {
        struct kv_pair *pair = pma->pairs[i];
        if (pair) {
            f(pair->key, pair->keylen,
                pair->key + pair->keylen, pair->vallen, v);
        }
    }
}

void pma_update_cursors(PMA pma, struct list *cursor_set, int oldposition, int newposition) {
    struct list *list, *nextlist;
    struct pma_cursor *cursor;

    list = list_head(cursor_set);
    while (list != cursor_set) {
        nextlist = list->next;  /* may be removed later */
        cursor = list_struct(list, struct pma_cursor, next);
        if (cursor->position == oldposition) {
            cursor->position = newposition;
            cursor->pma = pma;
            list_remove(list);
            list_push(&pma->cursors, list);
        }
        list = nextlist;
    }
}

void pma_update_region(PMA pma, struct list *cursor_set, struct kv_pair_tag *pairs, int n) {
    int i;

    /* short cut */
    if (list_empty(cursor_set))
        return;

    /* update all cursors to their new positions */
    for (i=0; i<n; i++) {
        if (pairs[i].pair && pairs[i].oldtag >= 0)
            pma_update_cursors(pma, cursor_set, pairs[i].oldtag, pairs[i].newtag);
    }
}

struct kv_pair_tag *pma_extract_pairs(PMA pma, int lo, int hi) {
    int npairs;
    struct kv_pair_tag *pairs;
    int i;
    int lastpair;

    npairs = pma_n_entries(pma);
    pairs = toku_malloc(npairs * sizeof (struct kv_pair_tag));
    if (pairs == 0)
        return 0;
    lastpair = 0;
    for (i=lo; i<hi; i++) {
        if (pma->pairs[i] != 0) {
            pairs[lastpair].pair = pma->pairs[i];
            pairs[lastpair].oldtag = i;
            pma->pairs[i] = 0;
            lastpair += 1;
        }
    }
    assert(lastpair == npairs);
    return pairs;
}

int pma_split(PMA origpma, unsigned int *origpma_size, 
    PMA leftpma, unsigned int *leftpma_size,
    PMA rightpma, unsigned int *rightpma_size) {
    int error;
    int npairs;
    struct kv_pair_tag *pairs;
    int sumlen;
    int runlen;
    int i;
    int n;
    int spliti;
    struct list cursors;

    /* extract the pairs */
    npairs = pma_n_entries(origpma);
    if (npairs == 0)
        return 0;
    assert(pma_n_entries(leftpma) == 0);
    assert(pma_n_entries(rightpma) == 0);

    /* TODO move pairs to the stack */
    pairs = pma_extract_pairs(origpma, 0, origpma->N);
    assert(pairs);
    origpma->n_pairs_present = 0;

    /* debug check the kv length sum */
    sumlen = 0;
    for (i=0; i<npairs; i++)
        sumlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + KEY_VALUE_OVERHEAD;

    if (origpma_size)
        assert(*(int *)origpma_size == sumlen);

    runlen = 0;
    for (i=0; i<npairs;) {
        runlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + KEY_VALUE_OVERHEAD;
        i++;
        if (2*runlen >= sumlen)
            break;
    }
    spliti = i;
    if (leftpma_size) 
        *leftpma_size = runlen;
    if (rightpma_size)
        *rightpma_size = sumlen - runlen;

    /* set the cursor set to be all of the cursors from the original pma */
    list_init(&cursors);
    if (!list_empty(&origpma->cursors))
        list_move(&cursors, &origpma->cursors);

    /* put the first half of pairs into the left pma */
    n = spliti;
    error = pma_resize_array(leftpma, n + n/4);
    assert(error == 0);
    distribute_data(leftpma->pairs, pma_index_limit(leftpma), &pairs[0], n, leftpma);
    pma_update_region(leftpma, &cursors, &pairs[0], spliti);
    leftpma->n_pairs_present = spliti;

    /* put the second half of pairs into the right pma */
    n = npairs - spliti;
    error = pma_resize_array(rightpma, n + n/4);
    assert(error == 0);
    distribute_data(rightpma->pairs, pma_index_limit(rightpma), &pairs[spliti], n, rightpma);
    pma_update_region(rightpma, &cursors, &pairs[spliti], n);
    rightpma->n_pairs_present = n;

    toku_free(pairs);

    /* bind the remaining cursors to the left pma*/
    while (!list_empty(&cursors)) {
        struct list *list = list_head(&cursors);
        list_remove(list);
        list_push(&leftpma->cursors, list);
    }

    return 0;
}

int pma_get_last(PMA pma, DBT *key, DBT *val) {
    int position;
    struct kv_pair *pair;
    void *v; int vlen;

    position = pma->N - 1;
    while ((pair = pma->pairs[position]) == 0) {
        if (position > 0)
            position--;
        else
            return DB_NOTFOUND;
    }

    if (key) {
        v = kv_pair_key(pair);
        vlen = kv_pair_keylen(pair);
        fill_dbt(key, memdup(v, vlen), vlen);
    }

    if (val) {
        v = kv_pair_val(pair);
        vlen = kv_pair_vallen(pair);
        fill_dbt(val, memdup(v, vlen), vlen);
    }

    return 0;
}

void __pma_bulk_cleanup(struct kv_pair_tag *pairs, int n) {
    int i;

    for (i=0; i<n; i++)
        if (pairs[i].pair)
            kv_pair_free(pairs[i].pair);
}

int pma_bulk_insert(PMA pma, DBT *keys, DBT *vals, int n_newpairs) {
    struct kv_pair_tag *newpairs;
    int i;
    int error;

    if (n_newpairs == 0)
        return 0;
    if (!list_empty(&pma->cursors))
        return -1;
    if (pma_n_entries(pma) > 0)
        return -2;

    /* TODO put newpairs on the stack */
    newpairs = toku_malloc(n_newpairs * sizeof (struct kv_pair_tag));
    if (newpairs == 0) {
        error = -3; return error;
    }

    for (i=0; i<n_newpairs; i++) {
        newpairs[i].pair = kv_pair_malloc(keys[i].data, keys[i].size, 
            vals[i].data, vals[i].size);
        if (newpairs[i].pair == 0) {
            __pma_bulk_cleanup(newpairs, i);
            toku_free(newpairs);
            error = -4; return error;
        }
    }

    error = pma_resize_array(pma, n_newpairs + n_newpairs/4);
    if (error) {
        __pma_bulk_cleanup(newpairs, n_newpairs);
        toku_free(newpairs);
        error = -5; return error;
    }
    distribute_data(pma->pairs, pma_index_limit(pma), newpairs, n_newpairs, pma);
    pma->n_pairs_present = n_newpairs;

    toku_free(newpairs);

    return 0;
}
