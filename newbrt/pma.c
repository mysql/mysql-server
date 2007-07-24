/* An in-memory Packed Memory Array dictionary.
   The keys and values are arrays of bytes, but are not necessarily kept in scan order.
   Only the pointers are kept.
 */

#include "pma-internal.h"
#include "key.h"
#include "memory.h"
#include "myassert.h"
#include "../include/ydb-constants.h"
#include <stdio.h>
#include <errno.h>

/* Only needed for testing. */
#include <string.h>


int pma_n_entries (PMA pma) {
    return pma->n_pairs_present;
}

int pma_index_limit (PMA pma) {
    return pma->N;
}
int pmanode_valid (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i].key!=0;
}
bytevec pmanode_key (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i].key;
}
ITEMLEN pmanode_keylen (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i].keylen;
}
bytevec pmanode_val (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i].val;
}
ITEMLEN pmanode_vallen (PMA pma, int i) {
    assert(0<=i); assert(i<pma_index_limit(pma));
    return pma->pairs[i].vallen;
}

/* Could pick the same one every time if we wanted. */
int pma_random_pick(PMA pma, bytevec *key, ITEMLEN *keylen, bytevec *val, ITEMLEN *vallen) {
#if 1
    int i;
    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<pma_index_limit(pma); i++) {
	if (pma->pairs[i].key) {
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
	    if (pma->pairs[mid].key!=0) {
		// Found one.
		DBT k2;
		int cmp = pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[mid].key, pma->pairs[mid].keylen));
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
    if (lo>0 && lo < pma_index_limit(pma) && pma->pairs[lo].key) {
	//printf("lo=%d\n", lo);
	DBT k2;
	assert(0 >= pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[lo].key, pma->pairs[lo].keylen)));
    }
    return lo;
}

//int min (int i, int j) { if (i<j) return i; else return j; }
//int max (int i, int j) { if (i<j) return j; else return i; }
//double lg (int n) { return log((double)n)/log(2.0); }

int pmainternal_printpairs (struct pair *pairs, int N) {
    int count=0;
    int i;
    printf("{");
    for (i=0; i<N; i++) {
	if (i!=0) printf(" ");
	if (pairs[i].key) {
	    printf("%s", (char*)pairs[i].key);
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
int distribute_data (struct pair *destpairs, int   dcount,
		     struct pair *sourcepairs, int scount) {
    assert(scount<=dcount);
    if (scount==0) {
	return -1;
    }
    if (scount==1) {
	*destpairs=*sourcepairs;
	if (destpairs->key==0) return 0;
	else return -1;
    } else {
	int r1 = distribute_data(destpairs, dcount/2,
				 sourcepairs, scount/2);
	int r2 = distribute_data(destpairs  +dcount/2, dcount-dcount/2,
				 sourcepairs+scount/2, scount-scount/2);
	assert(r1==-1 || r2==-1);
	if (r1!=-1)      return r1;
	else if (r2!=-1) return r2+dcount/2;
	else             return -1;
    }
}

/* spread the non-empty pairs around.  There are n of them.  Create an empty slot just before the IDXth
   element, and return that slot's index in the smoothed array. */
int pmainternal_smooth_region (struct pair *pairs, int n, int idx) {
    int i;
    int n_present=0;
    for (i=0; i<n; i++) {
	if (pairs[i].key) n_present++;
    }
    n_present++; // Save one for the blank guy.
    {
	struct pair *MALLOC_N(n_present,tmppairs);
	int n_saved=0;
	int r;
	for (i=0; i<n; i++) {
	    if (i==idx) {
		tmppairs[n_saved++].key = 0;
	    }
	    if (pairs[i].key) {
		tmppairs[n_saved++] = pairs[i];
	    }
	    pairs[i].key    = 0;
	    pairs[i].keylen = 0;
	    pairs[i].val    = 0;
	    pairs[i].vallen = 0;
	}
	if (idx==n) {
	    tmppairs[n_saved++].key = 0;
	}
	//printf(" temp="); printpairs(tmppairs, n_saved);
	assert(n_saved==n_present);
	/* Now the tricky part.  Distribute the data. */
	r=distribute_data (pairs, n,
			   tmppairs, n_saved);
	toku_free(tmppairs);
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

int pmainternal_count_region (struct pair *pairs, int lo, int hi) {
    int n=0;
    while (lo<hi) {
	if (pairs[lo].key) n++;
	lo++;
    }
    return n;
}

int pma_create (PMA *pma, int (*compare_fun)(DB*,const DBT*,const DBT*)) {
    TAGMALLOC(PMA, result);
    int i;
    if (result==0) return -1;
    result->N = 4;
    result->n_pairs_present = 0;
    MALLOC_N((1+result->N),result->pairs);
    result->pairs[result->N].key = (void*)0xdeadbeef;
    //printf("pairs=%p (size=%d)\n", result->pairs,result->N*sizeof(*result->pairs));
    if (result->pairs==0) {
	toku_free(result);
	return -1;
    }
    for (i=0; i<result->N; i++) {
	result->pairs[i].key = 0;
	result->pairs[i].keylen = 0;
	result->pairs[i].val = 0;
	result->pairs[i].vallen = 0;
    }
    pmainternal_calculate_parameters(result);
    result->cursors_head = result->cursors_tail = 0;
    result->compare_fun = compare_fun;
    result->skey=0;
    result->sval = 0;
    *pma = result;
    assert((unsigned long)result->pairs[result->N].key==0xdeadbeefL);
    return 0;
}


int pma_cursor (PMA pma, PMA_CURSOR *cursp) {
    PMA_CURSOR MALLOC(curs);
    if (errno!=0) return errno;
    assert(curs!=0);
    curs->position=-1; /* undefined */
    if (pma->cursors_head) {
	pma->cursors_head->prev = curs;
    } else {
	pma->cursors_tail = curs;
    }
    curs->next = pma->cursors_head;
    curs->prev = 0;
    curs->pma = pma;
    curs->skey = 0;
    curs->sval=0;
    pma->cursors_head = curs;
    *cursp=curs;
    return 0;
}

int pma_cursor_set_position_last (PMA_CURSOR c) 
{
    PMA pma = c->pma;
    c->position=pma->N-1;
    while (c->pma->pairs[c->position].key==0) {
	if (c->position>0) c->position--;
	else return DB_NOTFOUND;
    }
    return 0;
}

int pma_cursor_set_position_first (PMA_CURSOR c) 
{
    PMA pma = c->pma;
    c->position=0;
    while (c->pma->pairs[c->position].key==0) {
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
	if (c->pma->pairs[c->position].key!=0) return 0;
	c->position++;
    }
    c->position=old_position;
    return DB_NOTFOUND;
}

int pma_cget_current (PMA_CURSOR c, DBT *key, DBT *val) {
    PMA pma = c->pma;
    if (pma->pairs[c->position].key==0) return BRT_KEYEMPTY;
    ybt_set_value(key, pma->pairs[c->position].key, pma->pairs[c->position].keylen, &c->skey);
    ybt_set_value(val, pma->pairs[c->position].val, pma->pairs[c->position].vallen, &c->sval);
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
    PMA pma = curs->pma;
    if (curs->prev==0) {
	assert(pma->cursors_head==curs);
	pma->cursors_head = curs->next;
    } else {
	curs->prev->next  = curs->next;
    }
    if (curs->next==0) {
	assert(pma->cursors_tail==curs);
	pma->cursors_tail = curs->prev;
    } else {
	curs->next->prev  = curs->prev;
    }
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
		pma->pairs = toku_realloc(pma->pairs, (1+size)*sizeof(*pma->pairs));
		for (i=hi; i<size; i++) pma->pairs[i].key=0;
		pma->pairs[size].key = (void*)0xdeadbeefL;
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
	int new_index = pmainternal_smooth_region(pma->pairs+lo, hi-lo, idx-lo);
	return new_index+lo;
    }
}


enum pma_errors pma_lookup (PMA pma, DBT *k, DBT *v, DB *db) {
    DBT k2;
    int l = pmainternal_find(pma, k, db);
    assert(0<=l ); assert(l<=pma_index_limit(pma));
    if (l==pma_index_limit(pma)) return DB_NOTFOUND;
    if (pma->pairs[l].key!=0 && pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[l].key,pma->pairs[l].keylen))==0) {
	return ybt_set_value(v, pma->pairs[l].val, pma->pairs[l].vallen, &pma->sval);
    } else {
	return DB_NOTFOUND;
    }
}

void maybe_free (const void *p) {
    if (p) toku_free((void*)p);
}

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int pma_free (PMA *pmap) {
    int i;
    PMA pma=*pmap;
    if (pma->cursors_head) return -1;
    for (i=0; i<pma_index_limit(pma); i++) {
	if (pma->pairs[i].key) {
	    maybe_free(pma->pairs[i].key);
	    maybe_free(pma->pairs[i].val);
	    pma->pairs[i].key=0;
	    pma->pairs[i].val=0;
	}
    }
    toku_free(pma->pairs);
    toku_free(pma);
    if (pma->skey) toku_free(pma->skey);
    if (pma->sval) toku_free(pma->sval);
    *pmap=0;
    return 0;
}

/* Copies keylen and datalen */ 
int pma_insert (PMA pma, DBT *k, DBT *v, DB* db) {
    int idx = pmainternal_find(pma, k, db);
    if (idx < pma_index_limit(pma) && pma->pairs[idx].key) {
	DBT k2;
	if (0==pma->compare_fun(db, k, fill_dbt(&k2, pma->pairs[idx].key, pma->pairs[idx].keylen))) {
	    return BRT_ALREADY_THERE; /* It is already here.  Return an error. */
	}
    }
    if (pma->pairs[idx].key) {
	idx = pmainternal_make_space_at (pma, idx); /* returns the new idx. */
    }
    assert(!pma->pairs[idx].key);
    pma->pairs[idx].key    = memdup(k->data, k->size);
    pma->pairs[idx].keylen = k->size;
    pma->pairs[idx].val    = memdup(v->data, v->size);
    pma->pairs[idx].vallen = v->size;
    pma->n_pairs_present++;
    return BRT_OK;
}    

int pma_delete (PMA pma, DBT *k, DB *db) {
    int l = pmainternal_find(pma, k, db);
    if (pma->pairs[l].key==0) {
	printf("%s:%d l=%d r=%d\n", __FILE__, __LINE__, l, DB_NOTFOUND);
	return DB_NOTFOUND;
    }
    assert(pma->pairs[l].val!=0);
    toku_free((void*)pma->pairs[l].key);
    toku_free((void*)pma->pairs[l].val);
    pma->pairs[l].key = 0;
    pma->pairs[l].val = 0;
    pma->pairs[l].keylen = 0;
    pma->pairs[l].vallen = 0;
    pma->n_pairs_present--;
    // Need to rebalance
//    smooth_after_delete(pma,l);
    return BRT_OK;
}

void pma_iterate (PMA pma, void(*f)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*v) {
    int i;
    for (i=0; i<pma_index_limit(pma); i++) {
	if (pma->pairs[i].key) {
	    f(pma->pairs[i].key, pma->pairs[i].keylen,
	      pma->pairs[i].val, pma->pairs[i].vallen,
	      v);
	}
    }
}
