/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* An in-memory Packed Memory Array dictionary.
   The keys and values are arrays of bytes, but are not necessarily kept in scan order.
   Only the pointers are kept.
 */

#include "key.h"
#include "memory.h"
#include "myassert.h"
#include "../include/db.h"
#include <stdio.h>
#include <errno.h>
/* Only needed for testing. */
#include <string.h>
#include "list.h"
#include "kv-pair.h"
#include "pma-internal.h"
#include "log.h"
#include "log_header.h"

/* get KEY_VALUE_OVERHEAD */
#include "brt-internal.h"

/**************************** static functions forward declarations. *********************/
/*
 * finish a deletion from the pma. called when there are no cursor references
 * to the kv pair.
 */
static void __pma_delete_finish(PMA pma, int here);

/*
 * resize the pma array to asksize.  zero all array entries starting from startx.
 */
static int pma_resize_array(TOKUTXN, FILENUM, DISKOFF, PMA pma, int asksize, int startx);
static int old_pma_resize_array(PMA pma, int asksize, int startx) {
    return pma_resize_array((TOKUTXN)0, (FILENUM){0}, (DISKOFF)0, pma, asksize, startx);
}

/*
 * extract pairs from the pma in the window delimited by lo and hi.
 */
static struct kv_pair_tag *pma_extract_pairs(PMA pma, int count, unsigned int lo, unsigned int hi);

/*
 * update the cursors in a cursor set given a set of tagged pairs.
 */
static void __pma_update_cursors(PMA pma, struct list *cursorset, struct kv_pair_tag *tpairs, int n);

/*
 * update this pma's cursors given a set of tagged pairs.
 */
static void __pma_update_my_cursors(PMA pma, struct kv_pair_tag *tpairs, int n);

/*
 * a deletion occured at index "here" in the pma.  rebalance the windows around "here".  if
 * necessary, shrink the pma.
 */
static void __pma_delete_at(PMA pma, int here);

/*
 * if the pma entry at here is deleted and there are no more references to it
 * then finish the deletion
 */
static void __pma_delete_resume(PMA pma, int here);

/*
 * count the number of cursors that reference a pma pair
 */
static int __pma_count_cursor_refs(PMA pma, int here);


/**************************** end of static functions forward declarations. *********************/

/* use the low bit in the kv_pair pointer to indicate an inuse pair that is deleted */

static inline int kv_pair_inuse(struct kv_pair *pair) {
    return pair != 0;
}

static inline int kv_pair_deleted(struct kv_pair *pair) {
    return ((long) pair & 1) != 0;
}

static inline int kv_pair_valid(struct kv_pair *pair) {
    return kv_pair_inuse(pair) && !kv_pair_deleted(pair);
}

static inline struct kv_pair *kv_pair_set_deleted(struct kv_pair *pair) {
    return (struct kv_pair *) ((long) pair | 1);
}

static inline struct kv_pair *kv_pair_ptr(struct kv_pair *pair) {
    return (struct kv_pair *) ((long) pair & ~1);
}

struct kv_pair_tag {
    struct kv_pair *pair;
    int oldtag, newtag;
};

#ifndef PMA_USE_MEMPOOL
#define PMA_USE_MEMPOOL 1
#endif

#if PMA_USE_MEMPOOL

/* allocate a kv pair from the pma kv memory pool */
static struct kv_pair *kv_pair_malloc_mempool(void *key, int keylen, void *val, int vallen, struct mempool *mp) {
    struct kv_pair *kv = toku_mempool_malloc(mp, sizeof (struct kv_pair) + keylen + vallen, 4);
    if (kv)
        kv_pair_init(kv, key, keylen, val, vallen);
    return kv;
}

/* compress all of the kv pairs to the left edge of the memory pool and
   update the pma index with the new kv pair locations */
static int pma_compress_kvspace(PMA pma) {
    if (toku_mempool_get_frag_size(&pma->kvspace) == 0)
        return -1;
    void *mp = toku_malloc(pma->kvspace.size);
    if (mp == 0)
        return -2;
    struct mempool new_kvspace;
    toku_mempool_init(&new_kvspace, mp, pma->kvspace.size);
    unsigned int i;
    for (i=0; i<pma->N; i++) {
        struct kv_pair *kv = pma->pairs[i];
        if (kv_pair_inuse(kv)) {
            kv = kv_pair_ptr(kv);
            struct kv_pair *newkv = toku_mempool_malloc(&new_kvspace, kv_pair_size(kv), 4);
            assert(newkv);
            memcpy(newkv, kv, kv_pair_size(kv));
            if (kv_pair_deleted(pma->pairs[i]))
                kv_pair_set_deleted(newkv);
            pma->pairs[i] = newkv;
        }
    }
    toku_free(pma->kvspace.base);
    pma->kvspace = new_kvspace;
    return 0;
}

#endif

/* malloc space for a kv pair from the pma memory pool and initialize it.
   if the allocation fails, try to compress the memory pool and try again. */
static struct kv_pair *pma_malloc_kv_pair(PMA pma __attribute__((unused)), void *k, int ksize, void *v, int vsize) {
#if PMA_USE_MEMPOOL
    struct kv_pair *kv = kv_pair_malloc_mempool(k, ksize, v, vsize, &pma->kvspace);
    if (kv == 0) {
        if (0 == pma_compress_kvspace(pma))
            kv = kv_pair_malloc_mempool(k, ksize, v, vsize, &pma->kvspace);
    }
#else
    struct kv_pair *kv = kv_pair_malloc(k, ksize, v, vsize);
#endif
    return kv;
}

static void pma_mfree_kv_pair(PMA pma __attribute__((unused)), struct kv_pair *kv) {
    kv = kv_pair_ptr(kv);
#if PMA_USE_MEMPOOL
    toku_mempool_mfree(&pma->kvspace, kv, kv_pair_size(kv));
#else
    kv_pair_free(kv);
#endif
}

int toku_pma_n_entries (PMA pma) {
    return pma->n_pairs_present;
}

unsigned int toku_pma_index_limit (PMA pma) {
    return pma->N;
}

int toku_pmanode_valid (PMA pma, unsigned int i) {
    assert(i<toku_pma_index_limit(pma));
    return kv_pair_valid(pma->pairs[i]);
}

bytevec toku_pmanode_key (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_valid(pair));
    return kv_pair_key(pair);
}

ITEMLEN toku_pmanode_keylen (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_valid(pair));
    return kv_pair_keylen(pair);
}

bytevec toku_pmanode_val (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_valid(pair));
    return kv_pair_val(pair);
}

ITEMLEN toku_pmanode_vallen (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_valid(pair));
    return kv_pair_vallen(pair);
}

/* Could pick the same one every time if we wanted. */
int toku_pma_random_pick(PMA pma, bytevec *key, ITEMLEN *keylen, bytevec *val, ITEMLEN *vallen) {
#if 1
    unsigned int i;

    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<toku_pma_index_limit(pma); i++) {
	struct kv_pair *pair = pma->pairs[i];
        if (kv_pair_valid(pair)) {
	    *key = kv_pair_key(pair);
	    *keylen = kv_pair_keylen(pair);
	    *val = kv_pair_val(pair);
	    *vallen = kv_pair_vallen(pair);
	    return 0;
	}
    }
    return DB_NOTFOUND;
#else
    /* Maybe we should pick a random item to remove in order to reduce the unbalancing. */
    int i;
    int l = toku_pma_index_limit(pma);
    int r = random()%l;
    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<l; i++) {
	int ir=(i+r)%l;
        struct kv_pair *pair = pma->pairs[ir];
	if (kv_pair_valid(pair)) {
	    *key = kv_pair_key(pair);
	    *keylen = kv_pair_keylen(pair);
	    *val = kv_pair_val(pair);
	    *vallen = kv_pair_vallen(pair);
	    return 0;
	}
    }
    return DB_NOTFOUND;

#endif
}

static int pma_count_finds=0;
static int pma_count_divides=0;
static int pma_count_scans=0;

void toku_pma_show_stats (void) {
    printf("%d finds, %d divides, %d scans\n", pma_count_finds, pma_count_divides, pma_count_scans);
}

static int pma_compare_kv_kv(PMA pma, struct kv_pair *a, struct kv_pair *b) {
    DBT dbta, dbtb;;
    int cmp = pma->compare_fun(pma->db, toku_fill_dbt(&dbta, kv_pair_key(a), kv_pair_keylen(a)), toku_fill_dbt(&dbtb, kv_pair_key(b), kv_pair_keylen(b)));
    if (cmp == 0 && (pma->dup_mode & TOKU_DB_DUPSORT)) {
        cmp = pma->dup_compare_fun(pma->db, toku_fill_dbt(&dbta, kv_pair_val(a), kv_pair_vallen(b)), toku_fill_dbt(&dbtb, kv_pair_val(b), kv_pair_vallen(b)));
    }
    return cmp;
}

static int pma_compare_dbt_kv(PMA pma, DBT *k, DBT *v, struct kv_pair *kv) {
    DBT k2, v2;
    int cmp = pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, kv_pair_key(kv), kv_pair_keylen(kv)));
    if (cmp == 0 && v)
        cmp = pma->dup_compare_fun(pma->db, v, toku_fill_dbt(&v2, kv_pair_val(kv), kv_pair_vallen(kv)));
    return cmp;
}

/* search the index for a matching key */
static unsigned int __pma_search(PMA pma, DBT *k, int lo, int hi, int *found) {
    assert(0 <= lo && lo <= hi);
    if (lo >= hi) {
        *found = 0;
        return lo;
    } else {
        int mi = (lo + hi)/2;
        assert(lo <= mi && mi < hi);
        int omi = mi;
        while (mi < hi && !kv_pair_inuse(pma->pairs[mi]))
            mi++;
        if (mi >= hi)
            return __pma_search(pma, k, lo, omi, found);
        int cmp = pma_compare_dbt_kv(pma, k, 0, kv_pair_ptr(pma->pairs[mi]));
        if (cmp > 0)
            return __pma_search(pma, k, mi+1, hi, found);
        if (cmp < 0)
            return __pma_search(pma, k, lo, mi, found);
        *found = 1;
        return mi;
    }
}

/* search the index for the rightmost matching key */
static unsigned int pma_right_search(PMA pma, DBT *k, DBT *v, int lo, int hi, int *found) {
    assert(0 <= lo && lo <= hi);
    if (lo >= hi) {
        *found = 0;
        return lo;
    } else {
        int mi = (lo + hi)/2;
        assert(lo <= mi && mi < hi);
        int omi = mi;
        while (mi < hi && !kv_pair_inuse(pma->pairs[mi]))
            mi++;
        if (mi >= hi)
            return pma_right_search(pma, k, v, lo, omi, found);
        int cmp = pma_compare_dbt_kv(pma, k, v, kv_pair_ptr(pma->pairs[mi]));
        if (cmp > 0)
            return pma_right_search(pma, k, v, mi+1, hi, found);
        if (cmp < 0)
            return pma_right_search(pma, k, v, lo, mi, found);

        /* we have a match, try to find a match on the right tree */
        int here;
        here = pma_right_search(pma, k, v, mi+1, hi, found);
        if (*found == 0)
            here = mi;
        *found = 1;
        return here;
    }
}

/* search the index for the left most matching key */
static unsigned int pma_left_search(PMA pma, DBT *k, DBT *v, int lo, int hi, int *found) {
    assert(0 <= lo && lo <= hi);
    if (lo >= hi) {
        *found = 0;
        return lo;
    } else {
        int mi = (lo + hi)/2;
        assert(lo <= mi && mi < hi);
        int omi = mi;
        while (mi < hi && !kv_pair_inuse(pma->pairs[mi]))
            mi++;
        if (mi >= hi)
            return pma_left_search(pma, k, v, lo, omi, found);
        int cmp = pma_compare_dbt_kv(pma, k, v, kv_pair_ptr(pma->pairs[mi]));
        if (cmp > 0)
            return pma_left_search(pma, k, v, mi+1, hi, found);
        if (cmp < 0)
            return pma_left_search(pma, k, v, lo, mi, found);

        /* we have a match, try to find a match on the left tree */
        int here;
        here = pma_left_search(pma, k, v, lo, mi, found);
        if (*found == 0)
            here = mi;
        *found = 1;
        return here;
    }
}

// Return the smallest index such that no lower index contains a larger key.
// This will be in the range 0 (inclusive) to  toku_pma_index_limit(pma) (inclusive).
// Thus the returned index may not be a valid index into the array if it is == toku_pma_index_limit(pma)
// For example: if the array is empty, that means we return 0.
// For example: if the array is full of small keys, that means we return toku_pma_index_limit(pma), which is off the end of teh array.
// For example: if the array is full of large keys, then we return 0.
int toku_pmainternal_find (PMA pma, DBT *k) {
#if 1
    int lo=0, hi=toku_pma_index_limit(pma);
    /* lo and hi are the minimum and maximum values (inclusive) that we could possibly return. */
    pma_count_finds++;
    while (lo<hi) {
	int mid;
	// Scan forward looking for a non-null value.
	for (mid=(lo+hi)/2; mid<hi; mid++) {
            struct kv_pair *kv = pma->pairs[mid];
	    if (kv_pair_inuse(kv)) {
		// Found one.
                kv = kv_pair_ptr(kv);
		DBT k2;
		int cmp = pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, kv->key, kv->keylen));
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
    assert((unsigned)hi <= toku_pma_index_limit(pma));
    return lo;
#else
    int found;
    int lo = __pma_search(pma, k, 0, pma->N, &found);
    return lo;
#endif
}

//int min (int i, int j) { if (i<j) return i; else return j; }
//int max (int i, int j) { if (i<j) return j; else return i; }
//double lg (int n) { return log((double)n)/log(2.0); }

int toku_pmainternal_printpairs (struct kv_pair *pairs[], int N) {
    int count=0;
    int i;
    printf("{");
    for (i=0; i<N; i++) {
        if (i!=0) printf(" ");
        if (kv_pair_valid(pairs[i])) {
            printf("%s", (char*)kv_pair_key(pairs[i]));
            count++;
        }
        else printf("_");
    }
    printf("}");
    return count;
}

void toku_print_pma (PMA pma) {
    int count;
    printf("N=%d n_present=%d ", toku_pma_index_limit(pma), pma->n_pairs_present);
    count=toku_pmainternal_printpairs(pma->pairs, toku_pma_index_limit(pma));
    printf("\n");
    assert(count==pma->n_pairs_present);
}

/* Smooth the data, and return the location of the null. */
static int distribute_data (struct kv_pair *destpairs[],      int dcount,
			    struct kv_pair_tag sourcepairs[], int scount,
			    PMA pma) {
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
int toku_pmainternal_smooth_region (TOKUTXN txn, FILENUM filenum, DISKOFF diskoff, struct kv_pair *pairs[], int n, int idx, int base, PMA pma, int *new_idx) {
    int i;
    int n_present=0;
    for (i=0; i<n; i++) {
	if (kv_pair_inuse(pairs[i])) n_present++;
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
	int newidx;

	for (i=0; i<n; i++) {
	    if (i==idx) {
		tmppairs[n_saved++].pair = 0;
	    }
	    if (kv_pair_inuse(pairs[i])) {
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
	newidx=distribute_data (pairs, n,
				tmppairs, n_saved, pma);
	{
	    INTPAIRARRAY ipa;
	    ipa.size=n_saved-1; /* Don't move the blank spot. */
	    MALLOC_N(n_saved, ipa.array);
	    if (ipa.array==0) return errno;
	    int j=0;
	    for (i=0; i<n_saved; i++) {
		if (tmppairs[i].pair!=0) {
		    ipa.array[j].a = tmppairs[i].oldtag;
		    ipa.array[j].b = tmppairs[i].newtag;
		    j++;
		}
	    }
	    int r=toku_log_pmadistribute(txn, toku_txn_get_txnid(txn), filenum, diskoff, ipa);
	    if (0 && pma) {
		printf("Pma state:\n");
		PMA_ITERATE_IDX (pma, pidx, key, keylen, data, datalen,
				 printf(" %d:(%d:%s) (%d:%s)\n", pidx, keylen, (char*)key, datalen, (char*)data));
	    }
	    toku_free(ipa.array);
	    if (r!=0) return r;
	}
        if (pma && !list_empty(&pma->cursors))
            __pma_update_my_cursors(pma, tmppairs, n_present);
#ifdef USE_MALLOC_IN_SMOOTH
	toku_free(tmppairs);
#endif
	
	*new_idx = newidx;
	return 0;
    }
}

int toku_lg (int n) {
    int result=0;
    int two_to_result = 1;
    while (two_to_result<n) {
	result++;
	two_to_result*=2;
    }
    return result;
}

/* Calculate densitysteps and uplgN, given N. */
void toku_pmainternal_calculate_parameters (PMA pma) {
    int N = toku_pma_index_limit(pma);
    int lgN = toku_lg(N);
    int n_divisions=0;
    //printf("N=%d lgN=%d\n", N, lgN);
    while (N/2>=lgN) {
	n_divisions++;
	N/=2;
    }
    pma->uplgN=N;
    //printf("uplgN = %d n_divisions=%d\n", pma->uplgN, n_divisions);
    assert(n_divisions>0);
    pma->udt_step = (PMA_UDT_HIGH - PMA_UDT_LOW)/n_divisions;
    pma->ldt_step = (PMA_LDT_HIGH - PMA_LDT_LOW)/n_divisions;
}

int toku_pmainternal_count_region (struct kv_pair *pairs[], int lo, int hi) {
    int n=0;
    while (lo<hi) {
	if (kv_pair_inuse(pairs[lo])) n++;
	lo++;
    }
    return n;
}

int toku_pma_create(PMA *pma, pma_compare_fun_t compare_fun, DB *db, FILENUM filenum, int maxsize) {
    int error;
    TAGMALLOC(PMA, result);
    if (result==0) return -1;
    result->dup_mode = 0;
    result->n_pairs_present = 0;
    result->pairs = 0;
    list_init(&result->cursors);
    result->compare_fun = compare_fun;
    result->db = db;
    result->filenum = filenum;
    result->skey = 0;
    result->sval = 0;
    result->N = PMA_MIN_ARRAY_SIZE;
    result->pairs = 0;
    error = old_pma_resize_array(result, result->N, 0);
    if (error) {
        toku_free(result);
        return -1;
    }
    if (maxsize == 0)
        maxsize = 4*1024;
    maxsize = maxsize + maxsize/4;
#if PMA_USE_MEMPOOL
    void *mpbase = toku_malloc(maxsize); assert(mpbase);
    toku_mempool_init(&result->kvspace, mpbase, maxsize);
#endif
    *pma = result;
    assert((unsigned long)result->pairs[result->N]==0xdeadbeefL);
    return 0;
}

/* find the smallest power of 2 >= n */
static unsigned int pma_array_size(PMA pma __attribute__((unused)), int asksize) {
    int n = PMA_MIN_ARRAY_SIZE;
    while (n < asksize)
        n *= 2;
    return n;
}

int toku_resize_pma_exactly (PMA pma, int oldsize, int newsize) {
    pma->N = newsize;

    if (pma->pairs == 0)
        pma->pairs = toku_malloc((1 + pma->N) * sizeof (struct kv_pair *));
    else
        pma->pairs = toku_realloc(pma->pairs, (1 + pma->N) * sizeof (struct kv_pair *));
    if (pma->pairs == 0)
        return -1;
    pma->pairs[pma->N] = (void *) 0xdeadbeef;

    unsigned int i;
    for (i=oldsize; i<pma->N; i++) {
        pma->pairs[i] = 0;
    }
    return 0;
}

static int pma_resize_array(TOKUTXN txn, FILENUM filenum, DISKOFF offset, PMA pma, int asksize, int startz) {
    unsigned int oldN = pma->N;
    unsigned int n = pma_array_size(pma, asksize);
    int r = toku_resize_pma_exactly(pma, startz, n);
    if (r!=0) return r;
    toku_pmainternal_calculate_parameters(pma);
    toku_log_resizepma (txn, toku_txn_get_txnid(txn), filenum, offset, oldN, n);
    return 0;
}

int toku_pma_set_compare(PMA pma, pma_compare_fun_t compare_fun) {
    pma->compare_fun = compare_fun;
    return 0;
}

int toku_pma_set_dup_mode(PMA pma, int dup_mode) {
    assert(dup_mode == 0 || dup_mode == TOKU_DB_DUP || dup_mode == (TOKU_DB_DUP+TOKU_DB_DUPSORT));
    pma->dup_mode = dup_mode;
    return 0;
}

int toku_pma_set_dup_compare(PMA pma, pma_compare_fun_t dup_compare_fun) {
    pma->dup_compare_fun = dup_compare_fun;
    return 0;
}

int toku_pma_cursor (PMA pma, PMA_CURSOR *cursp, void **sskey, void **ssval) {
    PMA_CURSOR MALLOC(curs);
    assert(curs!=0);
    if (errno!=0) return errno;
    curs->position=-1; /* undefined */
    curs->pma = pma;
    curs->sskey = sskey;
    curs->ssval = ssval;
    list_push(&pma->cursors, &curs->next);
    *cursp=curs;
    return 0;
}

int toku_pma_cursor_get_pma(PMA_CURSOR c, PMA *pmap) {
    *pmap = c->pma;
    return 0;
}

int toku_pma_cursor_set_position_last (PMA_CURSOR c) {
    PMA pma = c->pma;
    int result = 0;
    int old_position = c->position;
    c->position=pma->N-1;
    while (!kv_pair_valid(c->pma->pairs[c->position])) {
	if (c->position>0) 
            c->position--;
	else {
            c->position = -1;
            result = DB_NOTFOUND;
            break;
        } 
    }
    if (old_position != c->position)
        __pma_delete_resume(pma, old_position);
    return result;
}

int toku_pma_cursor_set_position_prev (PMA_CURSOR c) {
    PMA pma = c->pma;
    int old_position = c->position;
    c->position--;
    while (c->position >= 0) {
        if (kv_pair_valid(pma->pairs[c->position])) {
            __pma_delete_resume(pma, old_position);
            return 0;
        }
        c->position--;
    }
    c->position = old_position;
    return DB_NOTFOUND;
}

int toku_pma_cursor_set_position_first (PMA_CURSOR c) {
    PMA pma = c->pma;
    int result = 0;
    int old_position = c->position;
    c->position=0;
    while (!kv_pair_valid(c->pma->pairs[c->position])) {
	if (c->position+1<(signed)pma->N) 
            c->position++;
	else {
            c->position = -1;
            result =DB_NOTFOUND;
            break;
        }
    }
    if (old_position != c->position)
        __pma_delete_resume(pma, old_position);
    return result;
}
    
int toku_pma_cursor_set_position_next (PMA_CURSOR c) {
    PMA pma = c->pma;
    int old_position=c->position;
    c->position++;
    while (c->position<(signed)pma->N) {
	if (kv_pair_valid(c->pma->pairs[c->position])) {
            __pma_delete_resume(pma, old_position);
            return 0;
	}
        c->position++;
    }
    c->position=old_position;
    return DB_NOTFOUND;
}

int toku_pma_cursor_get_current(PMA_CURSOR c, DBT *key, DBT *val, int even_deleted) {
    if (c->position == -1)
        return DB_NOTFOUND;
    PMA pma = c->pma;
    struct kv_pair *pair = pma->pairs[c->position];
    if (!kv_pair_inuse(pair) || (kv_pair_deleted(pair) && !even_deleted)) 
        return DB_KEYEMPTY;
    pair = kv_pair_ptr(pair);
    if (key) toku_dbt_set_value(key, kv_pair_key(pair), kv_pair_keylen(pair), c->sskey);
    if (val) toku_dbt_set_value(val, kv_pair_val(pair), kv_pair_vallen(pair), c->ssval);
    return 0;
}

int toku_pma_cursor_set_both(PMA_CURSOR c, DBT *key, DBT *val) {
    PMA pma = c->pma;
    unsigned int here; int found;
    here = pma_left_search(pma, key, val, 0, pma->N, &found);
    assert(here<=toku_pma_index_limit(pma));
    int r = DB_NOTFOUND;
    if (found && kv_pair_valid(pma->pairs[here])) {
        __pma_delete_resume(c->pma, c->position);
        c->position = here;
        r = 0;
    } 
    return r;
}

int toku_pma_cursor_set_range_both(PMA_CURSOR c, DBT *key, DBT *val) {
    PMA pma = c->pma;
    unsigned int here; int found;
    here = pma_left_search(pma, key, val, 0, pma->N, &found);
    assert(here<=toku_pma_index_limit(pma));

    /* find the first valid pair where key[here] >= key */
    int r = DB_NOTFOUND;
    while (here < pma->N) {
        if (kv_pair_valid(pma->pairs[here])) {
            __pma_delete_resume(c->pma, c->position);
            c->position = here;
            r = 0;
            break;
        }
        here += 1;
    }
    return r;
}

/* find the next matching key in the pma starting from index here */
static int pma_next_key(PMA pma, DBT *k, DBT *v, int here, int n, int *found) {
    assert(0 <= here);
    *found = 0;
    while (here < n && !kv_pair_inuse(pma->pairs[here]))
        here += 1;
    if (here < n) {
        int cmp = pma_compare_dbt_kv(pma, k, v, kv_pair_ptr(pma->pairs[here]));
        if (cmp == 0)
            *found = 1;
    }
    return here;
}

/* find the previous matching key in the pma starting from index here */
static int pma_prev_key(PMA pma, DBT *k, DBT *v, int here, int n, int *found) {
    assert(here < n);
    *found = 0;
    while (0 <= here && !kv_pair_inuse(pma->pairs[here]))
        here -= 1;
    if (0 <= here) {
        int cmp = pma_compare_dbt_kv(pma, k, v, kv_pair_ptr(pma->pairs[here]));
        if (cmp == 0)
            *found = 1;
    }
    return here;
}

/* set lastkeymatch if the kv pair under the cursor is the last one in the pma
   compare with the next and previous valid pma entries */

static void pma_cursor_key_last(PMA_CURSOR c, int *lastkeymatch) {
    *lastkeymatch = 1;
    PMA pma = c->pma;
    if (pma->dup_mode & TOKU_DB_DUP) {
        int here, found;

        /* get the current key */
        here = c->position; assert(0 <= here && here < (int) pma->N);
        struct kv_pair *kv = kv_pair_ptr(pma->pairs[here]);
        DBT currentkey; toku_fill_dbt(&currentkey, kv_pair_key(kv), kv_pair_keylen(kv));
        DBT currentval, *v; 

        if (pma->dup_mode & TOKU_DB_DUPSORT) {
            toku_fill_dbt(&currentval, kv_pair_val(kv), kv_pair_vallen(kv));
            v = &currentval;
        } else
            v = 0;

        /* check if the next key == current key */
        here = c->position+1;
        for (;;) {
            here = pma_next_key(pma, &currentkey, v, here, pma->N, &found);
            if (!found) break; 
            if (kv_pair_valid(pma->pairs[here])) {
                *lastkeymatch = 0; /* next key == current key */
                return;
            }
        }

        /* check if the prev key == current key */
        here = c->position-1;
        for (;;) {
            here = pma_prev_key(pma, &currentkey, v, here, pma->N, &found);
            if (!found) break;
            if (kv_pair_valid(pma->pairs[here])) {
                *lastkeymatch = 0; /* prev key == current key */
                return;
            }
        }
    }
}

int toku_pma_cursor_delete_under(PMA_CURSOR c, int *kvsize, u_int32_t rand4sem, u_int32_t *fingerprint, int *lastkeymatch) {
    int r = DB_NOTFOUND;
    if (c->position >= 0) {
        PMA pma = c->pma;
        assert((unsigned)c->position < pma->N);
        struct kv_pair *kv = pma->pairs[c->position];
        if (kv_pair_valid(kv)) {
            if (kvsize) 
                *kvsize = kv_pair_keylen(kv) + kv_pair_vallen(kv);
	    *fingerprint -= rand4sem*toku_calccrc32_kvpair (kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
            pma->pairs[c->position] = kv_pair_set_deleted(kv);
            r = 0;
            if (lastkeymatch) 
                pma_cursor_key_last(c, lastkeymatch);
        }
    }
    return r;
}

int toku_pma_cursor_free (PMA_CURSOR *cursp) {
    PMA_CURSOR curs=*cursp;
    PMA pma = curs->pma;
    list_remove(&curs->next);
    if (curs->position >= 0 && kv_pair_deleted(pma->pairs[curs->position]) &&
        __pma_count_cursor_refs(pma, curs->position) == 0) {
        __pma_delete_finish(pma, curs->position);
    }
    // It's not our job to free the sskey and ssval blocks.
    toku_free(curs);
    *cursp=0;
    return 0;
}

/* Make some space for a key to go at idx (the thing currently at idx should end up at to the right.) */
/* (Making space may involve moving things around, including the hole at index.) */
int toku_pmainternal_make_space_at (TOKUTXN txn, FILENUM filenum, DISKOFF offset, PMA pma, int idx, unsigned int *new_index) {
    /* Within a range LO to HI we have a limit of how much packing we will tolerate.
     * We allow the entire array to be 50% full.
     * We allow a region of size lgN to be full.
     * At sizes in between, we interpolate.
     */
    unsigned int size=pma->uplgN;
    int lo=idx;
    int hi=idx;
    double udt=PMA_UDT_HIGH;
    while (1) {
	/* set hi-lo equal size, make sure it is a supserset of (hi,lo).  */
	lo=idx-size/2;
	hi=idx+size/2;
	//printf("lo=%d hi=%d\n", lo, hi);
	if (lo<0) { hi-=lo; lo=0; }
	else if ((unsigned)hi>toku_pma_index_limit(pma)) { lo-=(hi-toku_pma_index_limit(pma)); hi=toku_pma_index_limit(pma); }
	else { ; /* nothing */ }
	
	//printf("lo=%d hi=%d\n", lo, hi);
	assert(0<=lo); assert(lo<hi); assert((unsigned)hi<=toku_pma_index_limit(pma)); assert((unsigned)(hi-lo)==size); // separate into separate assertions so that gcov doesn't see branches not taken.
	assert(udt>0.499); assert(udt<=1);
	if (udt<0.5001) { assert(lo==0); assert((unsigned)hi==toku_pma_index_limit(pma)); }
	{
	    int count = (1+ /* Don't forget space for the new guy. */
			 toku_pmainternal_count_region(pma->pairs, lo, hi));
            double density = (double) count / (double) (hi - lo);
	    if (density <= udt) 
                break;
	    if (lo==0 && (unsigned)hi==toku_pma_index_limit(pma)) {
		/* The array needs to be doubled in size. */

		assert(size==toku_pma_index_limit(pma));
		size*=2;

                // printf("pma_make_space_realloc %d to %d hi %d\n", pma->N, size, hi);
                pma_resize_array(txn, filenum, offset, pma, size, hi);

		hi=size;
		//printf("doubled N\n");
		break;
	    }
	}
	udt-=pma->udt_step;
	size*=2;
    }
    //printf("%s:%d Smoothing from %d to %d to density %f\n", __FILE__, __LINE__, lo, hi, density);
    {
	int sub_new_index;
	int r = toku_pmainternal_smooth_region(txn, filenum, offset, pma->pairs+lo, hi-lo, idx-lo, lo, pma, &sub_new_index);
	if (r!=0) return r;
	*new_index=sub_new_index+lo; 
	return 0;
    }
}

enum pma_errors toku_pma_lookup (PMA pma, DBT *k, DBT *v) {
    unsigned int here;
    int found;
    if (pma->dup_mode & TOKU_DB_DUP) {
        here = pma_left_search(pma, k, 0, 0, pma->N, &found);
    } else
        here = toku_pmainternal_find(pma, k);
    assert(here<=toku_pma_index_limit(pma));
    if (here==toku_pma_index_limit(pma)) return DB_NOTFOUND;
    DBT k2;
    struct kv_pair *pair;
    pair = pma->pairs[here];
    if (kv_pair_valid(pair) && pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, pair->key, pair->keylen))==0) {
        return toku_dbt_set_value(v, pair->key + pair->keylen, pair->vallen, &pma->sval);
    } else {
        return DB_NOTFOUND;
    }
}

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int toku_pma_free (PMA *pmap) {
    PMA pma=*pmap;
    if (!list_empty(&pma->cursors)) 
        return -1;
    
    if (pma->n_pairs_present > 0) {
	unsigned int i;
        for (i=0; i < pma->N; i++) {
            struct kv_pair *kv = pma->pairs[i];
            if (kv_pair_inuse(kv)) {
                pma_mfree_kv_pair(pma, kv);
                pma->pairs[i] = 0;
                pma->n_pairs_present--;
            }
        }
    }
    assert(pma->n_pairs_present == 0);
#if PMA_USE_MEMPOOL
    void *mpbase = toku_mempool_get_base(&pma->kvspace);
    toku_mempool_fini(&pma->kvspace);
    toku_free(mpbase);
#endif
    toku_free(pma->pairs);
    if (pma->skey) toku_free(pma->skey);
    if (pma->sval) toku_free(pma->sval);
    toku_free(pma);
    *pmap=0;
    return 0;
}

/* Copies keylen and datalen */ 
/* returns an error if the key is already present. */
int toku_pma_insert (PMA pma, DBT *k, DBT *v, TOKUTXN txn, FILENUM filenum, DISKOFF diskoff, u_int32_t rand4fingerprint, u_int32_t *fingerprint) {
    int found;
    unsigned int idx;

    if (pma->dup_mode & TOKU_DB_DUPSORT) {
        idx = pma_right_search(pma, k, v, 0, pma->N, &found);
        if (found) return BRT_ALREADY_THERE;
    } else if (pma->dup_mode & TOKU_DB_DUP) {
        idx = pma_right_search(pma, k, 0, 0, pma->N, &found);
        if (found) idx += 1;
    } else {
        idx = toku_pmainternal_find(pma, k);
        if (idx < toku_pma_index_limit(pma) && pma->pairs[idx]) {
            DBT k2;
            struct kv_pair *kv = kv_pair_ptr(pma->pairs[idx]);
            if (0==pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, kv->key, kv->keylen))) {
                if (kv_pair_deleted(pma->pairs[idx])) {
                    pma_mfree_kv_pair(pma, pma->pairs[idx]);
                    pma->pairs[idx] = pma_malloc_kv_pair(pma, k->data, k->size, v->data, v->size);
                    assert(pma->pairs[idx]);
		    *fingerprint += rand4fingerprint*toku_calccrc32_kvpair(k->data, k->size, v->data, v->size); 
                    int r = toku_logger_log_phys_add_or_delete_in_leaf(pma->db, txn, diskoff, 0, pma->pairs[idx]);
                    return r;
                } else
                    return BRT_ALREADY_THERE; /* It is already here.  Return an error. */
            }
        }
    }
    if (kv_pair_inuse(pma->pairs[idx])) {
	unsigned int newidx;
        int r = toku_pmainternal_make_space_at (txn, filenum, diskoff, pma, idx, &newidx); /* returns the new idx. */
	if (r!=0) return r;
	idx = newidx;
    }
    assert(idx < pma->N);
    assert(!kv_pair_inuse(pma->pairs[idx]));
    pma->pairs[idx] = pma_malloc_kv_pair(pma, k->data, k->size, v->data, v->size);
    assert(pma->pairs[idx]);
    pma->n_pairs_present++;
    *fingerprint += rand4fingerprint*toku_calccrc32_kvpair(k->data, k->size, v->data, v->size); 
    {
	const struct kv_pair *pair = pma->pairs[idx];
	const BYTESTRING key  = { pair->keylen, (char*)kv_pair_key_const(pair) };
	const BYTESTRING data = { pair->vallen, (char*)kv_pair_val_const(pair) };
	return toku_log_insertinleaf (txn, toku_txn_get_txnid(txn), pma->filenum, diskoff, idx, key, data);
    }
}    

static int pma_delete_dup (PMA pma, DBT *k, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    /* find the left most matching key in the pma */
    int found;
    unsigned int lefthere;
    lefthere = pma_left_search(pma, k, 0, 0, pma->N, &found);
    int rightfound = found, righthere = lefthere;
    while (rightfound) {
        struct kv_pair *kv = pma->pairs[righthere];
        if (kv_pair_valid(kv)) {
            /* mark the pair as deleted */
            *deleted_size += PMA_ITEM_OVERHEAD+ KEY_VALUE_OVERHEAD + kv_pair_keylen(kv) + kv_pair_vallen(kv);
	    *fingerprint -= rand4sem*toku_calccrc32_kvpair (kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
            pma->pairs[righthere] = kv_pair_set_deleted(kv);
            if (__pma_count_cursor_refs(pma, righthere) == 0) {
                pma_mfree_kv_pair(pma, kv);
                pma->pairs[righthere] = 0;
                pma->n_pairs_present--;
            }
        }
        /* find the next matching key in the pma */
        righthere = pma_next_key(pma, k, 0, righthere+1, pma->N, &rightfound);
    }
    if (found) {
        /* check the density of the region centered around the deleted pairs */
        __pma_delete_at(pma, (lefthere + righthere) / 2);
    }
    return found ? BRT_OK : DB_NOTFOUND;
}

static int pma_delete_nodup (PMA pma, DBT *k, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    int idx = toku_pmainternal_find(pma, k);
    struct kv_pair *kv = pma->pairs[idx];
    if (!kv_pair_valid(kv)) {
        if (0) printf("%s:%d l=%d r=%d\n", __FILE__, __LINE__, idx, DB_NOTFOUND);
        return DB_NOTFOUND;
    }
    *deleted_size = PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + kv_pair_keylen(kv) + kv_pair_vallen(kv); 
    *fingerprint -= rand4sem*toku_calccrc32_kvpair (kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
    pma->pairs[idx] = kv_pair_set_deleted(kv);
    if (__pma_count_cursor_refs(pma, idx) == 0)
        __pma_delete_finish(pma, idx);
    return BRT_OK;
}

int toku_pma_delete (PMA pma, DBT *k, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    u_int32_t my_deleted_size;
    if (!deleted_size)
        deleted_size = &my_deleted_size;
    *deleted_size = 0;
    if (pma->dup_mode & TOKU_DB_DUP) 
        return pma_delete_dup(pma, k, rand4sem, fingerprint, deleted_size);
    else
        return pma_delete_nodup(pma, k, rand4sem, fingerprint, deleted_size);
}

void __pma_delete_resume(PMA pma, int here) {
    if (here >= 0 && kv_pair_deleted(pma->pairs[here]) &&__pma_count_cursor_refs(pma, here) == 0)
        __pma_delete_finish(pma, here);
}

static void __pma_delete_finish(PMA pma, int here) {
    struct kv_pair *kv = pma->pairs[here];
    if (!kv_pair_inuse(kv))
        return;
    pma_mfree_kv_pair(pma, kv);
    pma->pairs[here] = 0;
    pma->n_pairs_present--;
    __pma_delete_at(pma, here);
}

static void __pma_delete_at(PMA pma, int here) {
    int count;
    struct kv_pair_tag *newpairs;

    unsigned int lgN = pma->uplgN;
    unsigned int size = lgN;
    double       ldt = PMA_LDT_HIGH;

    /* check the density of regions from lg(N) size to the entire array */
    for (;;) { 
        int lo, hi;
        double density;

        /* select a region centered on here */
        lo = here - size/2;
        hi = here + size/2;
        if (lo < 0) {
            hi -= lo; 
            lo = 0; 
            if ((unsigned)hi > pma->N) 
                hi = pma->N;
        } else if ((unsigned)hi > pma->N) {
            lo -= hi - pma->N; 
            hi = pma->N; 
            if (lo < 0) 
                lo = 0;
        }
        assert(lo <= hi);

        /* compute the density of the region */
        count = toku_pmainternal_count_region(pma->pairs, lo, hi);
        density = (double) count / ((double) (hi - lo));

        /* rebalance if the density exceeds the lower threadshold */
        if (0) printf("check size %d h %d density %d/%d %f %d-%d ldt %f\n", size, 
               lgN, count, hi-lo, density, lo, hi, ldt);
        if (density >= ldt) {
            if (size == lgN)
                return;
            if (0) printf("delete_at_rebalance %d over %d %d\n", count, lo, hi);
            newpairs = pma_extract_pairs(pma, count, lo, hi);
            distribute_data(pma->pairs + lo, hi - lo, newpairs, count, pma);
            __pma_update_my_cursors(pma, newpairs, count);
            toku_free(newpairs);
            return;
        }
        ldt -= pma->ldt_step;
        size *= 2;
        if (0 == lo && pma->N == (unsigned)hi)
            break;
    }
    
    /* shrink */
    size = pma_array_size(pma, count + count/4);
    if (size == pma->N)
        return;    
    if (0) printf("shrink %d from %d to %d\n", count, pma->N, size);
    newpairs = pma_extract_pairs(pma, count, 0, pma->N);
    assert(newpairs);
    old_pma_resize_array(pma, size, 0);
    distribute_data(pma->pairs, pma->N, newpairs, count, pma);
    /* update the cursors */
    __pma_update_my_cursors(pma, newpairs, count);
    toku_free(newpairs);
}

int toku_pma_insert_or_replace (PMA pma, DBT *k, DBT *v,
				int *replaced_v_size, /* If it is a replacement, set to the size of the old value, otherwise set to -1. */
				TOKUTXN txn, FILENUM filenum, DISKOFF diskoff,
				u_int32_t rand4fingerprint, u_int32_t *fingerprint) {
    //printf("%s:%d v->size=%d\n", __FILE__, __LINE__, v->size);
    int r;
    struct kv_pair *kv;
    unsigned int idx;
    int found;
    if (pma->dup_mode & TOKU_DB_DUPSORT) {
        idx = pma_right_search(pma, k, v, 0, pma->N, &found);
#if PMA_DUP_DUP
        if (found) idx += 1;
#else
        if (found) {
            kv = kv_pair_ptr(pma->pairs[idx]); goto replaceit;
        }
#endif
    } else if (pma->dup_mode & TOKU_DB_DUP) {
        idx = pma_right_search(pma, k, 0, 0, pma->N, &found);
        if (found) idx += 1;
    } else {
        idx = toku_pmainternal_find(pma, k);
        if (idx < toku_pma_index_limit(pma) && (kv = pma->pairs[idx])) {
            DBT k2;
            // printf("%s:%d\n", __FILE__, __LINE__);
            kv = kv_pair_ptr(kv);
            if (0==pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, kv->key, kv->keylen))) {
            replaceit:
                if (kv_pair_deleted(pma->pairs[idx])) {
                    *replaced_v_size = -1;
                    pma->pairs[idx] = kv;
                } else {
                    *replaced_v_size = kv->vallen;
                    *fingerprint -= rand4fingerprint*toku_calccrc32_kvpair(kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
                    r=toku_logger_log_phys_add_or_delete_in_leaf(pma->db, txn, diskoff, 0, kv);
                    if (r!=0) return r;
                }
                if (v->size == (unsigned int) kv_pair_vallen(kv)) {
                    memcpy(kv_pair_val(kv), v->data, v->size);
                } else {
                    pma_mfree_kv_pair(pma, kv);
                    pma->pairs[idx] = pma_malloc_kv_pair(pma, k->data, k->size, v->data, v->size);
                    assert(pma->pairs[idx]);
                }
                r = toku_logger_log_phys_add_or_delete_in_leaf(pma->db, txn, diskoff, 0, pma->pairs[idx]);
                *fingerprint += rand4fingerprint*toku_calccrc32_kvpair(k->data, k->size, v->data, v->size);
                return r;
            }
        }
    }
    if (kv_pair_inuse(pma->pairs[idx])) {
	unsigned int newidx;
        r = toku_pmainternal_make_space_at (txn, filenum, diskoff, pma, idx, &newidx); /* returns the new idx. */
	if (r!=0) return r;
	idx=newidx;
    }
    assert(!kv_pair_inuse(pma->pairs[idx]));
    //printf("%s:%d v->size=%d\n", __FILE__, __LINE__, v->size);
    pma->pairs[idx] = pma_malloc_kv_pair(pma, k->data, k->size, v->data, v->size);
    assert(pma->pairs[idx]);
    pma->n_pairs_present++;
    *replaced_v_size = -1;
    //printf("%s:%d txn=%p\n", __FILE__, __LINE__, txn);
    {
	const struct kv_pair *pair = pma->pairs[idx];
	const BYTESTRING key  = { pair->keylen, (char*)kv_pair_key_const(pair) };
	const BYTESTRING data = { pair->vallen, (char*)kv_pair_val_const(pair) };
	r = toku_log_insertinleaf (txn, toku_txn_get_txnid(txn), pma->filenum, diskoff, idx, key, data);
    }
    *fingerprint += rand4fingerprint*toku_calccrc32_kvpair(k->data, k->size, v->data, v->size);
    return r;
}

void toku_pma_iterate (PMA pma, void(*f)(bytevec,ITEMLEN,bytevec,ITEMLEN, void*), void*v) {
    unsigned int i;
    for (i=0; i<toku_pma_index_limit(pma); i++) {
        struct kv_pair *pair = pma->pairs[i];
        if (pair) {
            f(pair->key, pair->keylen,
                pair->key + pair->keylen, pair->vallen, v);
        }
    }
}

int __pma_count_cursor_refs(PMA pma, int here) {
    int refs = 0;
    struct list *list;
    struct pma_cursor *cursor;
    
    list = list_head(&pma->cursors);
    while (list != &pma->cursors) {
        cursor = list_struct(list, struct pma_cursor, next);
        if (cursor->position == here)
            refs += 1;
        list = list->next;
    }
    return refs;
}

static void __pma_update_cursors_position(PMA pma, struct list *cursor_set, int oldposition, int newposition) {
    struct list *list, *nextlist;
    struct pma_cursor *cursor;

    list = list_head(cursor_set);
    while (list != cursor_set) {
        nextlist = list->next;  /* may be removed later */
        cursor = list_struct(list, struct pma_cursor, next);
        if (cursor->position == oldposition) {
            if (0) printf("cursor %p %d -> %d\n", cursor, oldposition, newposition);
            cursor->position = newposition;
            cursor->pma = pma;
            list_remove(list);
            list_push(&pma->cursors, list);
        }
        list = nextlist;
    }
}

void __pma_update_cursors(PMA pma, struct list *cursor_set, struct kv_pair_tag *tpairs, int n) {
    /* short cut */
    if (list_empty(cursor_set))
        return;

    /* update all cursors to their new positions */
    int i;
    for (i=0; i<n; i++) {
        if (tpairs[i].pair && tpairs[i].oldtag >= 0)
            __pma_update_cursors_position(pma, cursor_set, tpairs[i].oldtag, tpairs[i].newtag);
    }
}

static void __pma_update_my_cursors(PMA pma, struct kv_pair_tag *tpairs, int n) {
    if  (list_empty(&pma->cursors))
        return;

    struct list cursors;

    list_move(&cursors, &pma->cursors);
    __pma_update_cursors(pma, &cursors, tpairs, n);
    while (!list_empty(&cursors)) {
        struct list *list = list_head(&cursors);
        list_remove(list);
        list_push(&pma->cursors, list);
    }
}

static struct kv_pair_tag *pma_extract_pairs(PMA pma, int npairs, unsigned int lo, unsigned int hi) {
    struct kv_pair_tag *pairs;
    unsigned int i;
    int lastpair;

    pairs = toku_malloc(npairs * sizeof (struct kv_pair_tag));
    if (pairs == 0)
        return 0;
    lastpair = 0;
    for (i=lo; i<hi; i++) {
        assert(i < pma->N);
        if (pma->pairs[i] != 0) {
            assert(pma->pairs[i] != (void*)0xdeadbeef);
            pairs[lastpair].pair = pma->pairs[i];
            pairs[lastpair].oldtag = i;
            pma->pairs[i] = 0;
            lastpair += 1;
        }
    }
    assert(lastpair == npairs);
    return pairs;
}

#if PMA_USE_MEMPOOL

static void __pma_relocate_kvpairs(PMA pma) {
    unsigned int i;
    for (i=0; i<pma->N; i++) {
        struct kv_pair *kv = pma->pairs[i];
        if (kv) {
            pma->pairs[i] = kv_pair_malloc_mempool(kv_pair_key(kv), kv_pair_keylen(kv), kv_pair_val(kv),
                                                   kv_pair_vallen(kv), &pma->kvspace);
            assert(pma->pairs[i]);
        }
    }
}

#endif


int toku_pma_split(TOKUTXN txn, FILENUM filenum,
		   PMA origpma, unsigned int *origpma_size, DBT *splitk,
		   DISKOFF leftdiskoff, PMA leftpma,  unsigned int *leftpma_size,  u_int32_t leftrand4fp,  u_int32_t *leftfingerprint,
		   DISKOFF rightdiskoff, PMA rightpma, unsigned int *rightpma_size, u_int32_t rightrand4fp, u_int32_t *rightfingerprint) {
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
    npairs = toku_pma_n_entries(origpma);
    if (npairs == 0) {
        if (splitk)
            memset(splitk, 0, sizeof *splitk);
        return 0;
    }
    assert(toku_pma_n_entries(leftpma) == 0);
    assert(toku_pma_n_entries(rightpma) == 0);

    /* TODO move pairs to the stack */
    pairs = pma_extract_pairs(origpma, npairs, 0, origpma->N);
    assert(pairs);
    origpma->n_pairs_present = 0;

    /* debug check the kv length sum */
    sumlen = 0;
    for (i=0; i<npairs; i++)
        sumlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD;

    if (origpma_size)
        assert(*(int *)origpma_size == sumlen);

    runlen = 0;
    for (i=0; i<npairs;) {
        runlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD;
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

    {
	u_int32_t sum = 0;
	for (i=0; i<spliti; i++) {
	    sum+=toku_calccrc32_kvpair(kv_pair_key_const(pairs[i].pair), kv_pair_keylen(pairs[i].pair),
				       kv_pair_val_const(pairs[i].pair), kv_pair_vallen(pairs[i].pair));
	}
	*leftfingerprint += leftrand4fp * sum;
    }
    {
	u_int32_t sum = 0;
	for (i=spliti; i<npairs; i++) {
	    sum+=toku_calccrc32_kvpair(kv_pair_key_const(pairs[i].pair), kv_pair_keylen(pairs[i].pair),
				       kv_pair_val_const(pairs[i].pair), kv_pair_vallen(pairs[i].pair));
	}
	*rightfingerprint += rightrand4fp * sum;
    }

    if (splitk) {
        struct kv_pair *a = pairs[spliti-1].pair;
        if (origpma->dup_mode & TOKU_DB_DUPSORT) {
            splitk->data = kv_pair_malloc(kv_pair_key(a), kv_pair_keylen(a), kv_pair_val(a), kv_pair_vallen(a));
            splitk->size = kv_pair_keylen(a) + kv_pair_vallen(a);
        } else {
            splitk->data = kv_pair_malloc(kv_pair_key(a), kv_pair_keylen(a), 0, 0);
            splitk->size = kv_pair_keylen(a);
        }
        splitk->flags = BRT_PIVOT_PRESENT_L;
        if (spliti < npairs && pma_compare_kv_kv(origpma, a, pairs[spliti].pair) == 0) {
            splitk->flags += BRT_PIVOT_PRESENT_R;
        }
    }

    /* put the first half of pairs into the left pma */
    n = spliti;
    error = pma_resize_array(txn, filenum, leftdiskoff, leftpma, n + n/4, 0);
    assert(error == 0);
    distribute_data(leftpma->pairs, toku_pma_index_limit(leftpma), &pairs[0], n, leftpma);
#if PMA_USE_MEMPOOL
    __pma_relocate_kvpairs(leftpma);
#endif
    __pma_update_cursors(leftpma, &cursors, &pairs[0], spliti);
    leftpma->n_pairs_present = spliti;

    /* put the second half of pairs into the right pma */
    n = npairs - spliti;
    error = pma_resize_array(txn, filenum, rightdiskoff, rightpma, n + n/4, 0);
    assert(error == 0);
    distribute_data(rightpma->pairs, toku_pma_index_limit(rightpma), &pairs[spliti], n, rightpma);
#if PMA_USE_MEMPOOL
    __pma_relocate_kvpairs(rightpma);
#endif
    __pma_update_cursors(rightpma, &cursors, &pairs[spliti], n);
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

static void __pma_bulk_cleanup(struct pma *pma, struct kv_pair_tag *pairs, int n) {
    int i;

    for (i=0; i<n; i++)
        if (pairs[i].pair)
            pma_mfree_kv_pair(pma, pairs[i].pair);
}

int toku_pma_bulk_insert(TOKUTXN txn, FILENUM filenum, DISKOFF diskoff, PMA pma, DBT *keys, DBT *vals, int n_newpairs, u_int32_t rand4fp, u_int32_t *sum) {
    struct kv_pair_tag *newpairs;
    int i;
    int error;
    u_int32_t delta=0;

    if (n_newpairs == 0)
        return 0;
    if (!list_empty(&pma->cursors))
        return -1;
    if (toku_pma_n_entries(pma) > 0)
        return -2;

    /* TODO put newpairs on the stack */
    newpairs = toku_malloc(n_newpairs * sizeof (struct kv_pair_tag));
    if (newpairs == 0) {
        error = -3; return error;
    }

    for (i=0; i<n_newpairs; i++) {
	delta += rand4fp*toku_calccrc32_kvpair (keys[i].data, keys[i].size, vals[i].data, vals[i].size);
#if PMA_USE_MEMPOOL
        newpairs[i].pair = kv_pair_malloc_mempool(keys[i].data, keys[i].size, 
                                                  vals[i].data, vals[i].size, &pma->kvspace);
#else
        newpairs[i].pair = kv_pair_malloc(keys[i].data, keys[i].size, vals[i].data, vals[i].size);
#endif
        if (newpairs[i].pair == 0) {
            __pma_bulk_cleanup(pma, newpairs, i);
            toku_free(newpairs);
            error = -4; return error;
        }
    }

    error = pma_resize_array(txn, filenum, diskoff, pma, n_newpairs + n_newpairs/4, 0);
    if (error) {
        __pma_bulk_cleanup(pma, newpairs, n_newpairs);
        toku_free(newpairs);
        error = -5; return error;
    }
    distribute_data(pma->pairs, toku_pma_index_limit(pma), newpairs, n_newpairs, pma);
    pma->n_pairs_present = n_newpairs;

    toku_free(newpairs);
    *sum += delta;

    return 0;
}

/* verify that the keys in the pma index are sorted subject to the pma mode
 * no duplications, duplicates, sorted duplicates.
 */

void toku_pma_verify(PMA pma) {
    unsigned int i;
    struct kv_pair *kv;
    
    /* find the first key in the index */
    for (i=0; i<pma->N; i++) {
        kv = pma->pairs[i];
        if (kv_pair_inuse(kv)) {
            kv = kv_pair_ptr(kv);
            i += 1;
            break;
        }
    }

    /* compare the current key with the next key in the index */
    struct kv_pair *nextkv;
    for (; i<pma->N; i++) {
        nextkv = pma->pairs[i];
        if (kv_pair_inuse(nextkv)) {
            nextkv = kv_pair_ptr(nextkv);
            DBT kv_dbt, nextkv_dbt;
            toku_fill_dbt(&kv_dbt, kv_pair_key(kv), kv_pair_keylen(kv));
            toku_fill_dbt(&nextkv_dbt, kv_pair_key(nextkv), kv_pair_keylen(nextkv));
            int r = pma->compare_fun(pma->db, &kv_dbt, &nextkv_dbt);
            if (pma->dup_mode == 0)
                assert(r < 0);
            else if (pma->dup_mode & TOKU_DB_DUP)
                assert(r <= 0);
            if (r == 0 && (pma->dup_mode & TOKU_DB_DUPSORT)) {
                toku_fill_dbt(&kv_dbt, kv_pair_val(kv), kv_pair_vallen(kv));
                toku_fill_dbt(&nextkv_dbt, kv_pair_val(nextkv), kv_pair_vallen(nextkv));
                r = pma->dup_compare_fun(pma->db, &kv_dbt, &nextkv_dbt);
                assert(r <= 0);
            }
            kv = nextkv;
        }
    }

#if PMA_USE_MEMPOOL
    /* verify all kv pairs are in the memory pool */
    for (i=0; i<pma->N; i++) {
        kv = pma->pairs[i];
        if (kv_pair_inuse(kv)) {
            kv = kv_pair_ptr(kv);
            assert(toku_mempool_inrange(&pma->kvspace, kv, kv_pair_size(kv)));
        }
    }
#endif
}

void toku_pma_verify_fingerprint (PMA pma, u_int32_t rand4fingerprint, u_int32_t fingerprint) {
    u_int32_t actual_fingerprint=0;
    PMA_ITERATE(pma, kv, kl, dv, dl,
		actual_fingerprint+=rand4fingerprint*toku_calccrc32_kvpair(kv,kl,dv,dl)
		);
    assert(actual_fingerprint==fingerprint);
}

// If the index is wrong or there is a value already, return nonzero
// There should be no cursors, but if there were they wouldn't need to be updated.
int toku_pma_set_at_index (PMA pma, unsigned int idx, DBT *key, DBT *value) {
    if (idx>=pma->N) return -1;
    if (kv_pair_inuse(pma->pairs[idx])) return -1;
    pma->pairs[idx] = pma_malloc_kv_pair(pma, key->data, key->size, value->data, value->size);
    pma->n_pairs_present++;
    return 0;
}

// assume no cursors
int toku_pma_move_indices (PMA pma, INTPAIRARRAY fromto) {
    u_int32_t i;
    for (i=0; i<fromto.size; i++) {
	// First handle the case for sliding something left.  We can simply move it.
	{
	    int a=fromto.array[i].a;
	    int b=fromto.array[i].b;
	    if (b==a) continue;
	    if (b<a) {
		assert(pma->pairs[b]==0);
		pma->pairs[b] = pma->pairs[a];
		pma->pairs[a] = 0;
		continue;
	    }
	}
	// Otherwise slide things to the right  We have to find the rightmost thing that slides right and move it first.
	{
	    // We must slide things to the right.
	    // Find the next index that does want to go to the left
	    u_int32_t j;
	    for (j=i+1; j<fromto.size &&  fromto.array[j].a < fromto.array[j].b; j++) {
		/*nothing */
	    }
	    // everything from i (inclusive) to j (exclusive) wants to slide to the right.
	    u_int32_t jdown;
	    for (jdown=j-1; 1; jdown--) {
		int a=fromto.array[jdown].a;
		int b=fromto.array[jdown].b;
		if (a!=b) {
		    assert(pma->pairs[b]==0);
		    pma->pairs[b] = pma->pairs[a];
		    pma->pairs[a] = 0;
		}
		if (i==jdown) break; // Do it this way so everything can be unsigned and we won't try to go negative.
	    }
	    i=j-1;
	}
    }
    return 0;
}
