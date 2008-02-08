/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* An in-memory Packed Memory Array dictionary.
   The keys and values are arrays of bytes, but are not necessarily kept in scan order.
   Only the pointers are kept.
 */

#include "key.h"
#include "memory.h"
#include "toku_assert.h"
#include "../include/db.h"
#include <stdio.h>
#include <errno.h>
/* Only needed for testing. */
#include <string.h>
#include <inttypes.h>
#include "list.h"
#include "kv-pair.h"
#include "pma-internal.h"
#include "log.h"
#include "log_header.h"

/* get KEY_VALUE_OVERHEAD */
#include "brt-internal.h"

/**************************** static functions forward declarations. *********************/

/* resize the pma array to asksize.  zero all array entries starting from startx.*/
static int pma_resize_array(TOKULOGGER, FILENUM, DISKOFF, PMA pma, int asksize, int startx, LSN *node_lsn);
static int old_pma_resize_array(PMA pma, int asksize, int startx) {
    return pma_resize_array((TOKULOGGER)0, (FILENUM){0}, (DISKOFF)0, pma, asksize, startx, (LSN*)0);
}

/* extract pairs from the pma in the window delimited by lo and hi.*/
static struct kv_pair_tag *pma_extract_pairs(PMA pma, int count, unsigned int lo, unsigned int hi);

/*
 * a deletion occured at index "here" in the pma.  rebalance the windows around "here".  if
 * necessary, shrink the pma.
 */
static void pma_delete_at(PMA pma, int here);


/**************************** end of static functions forward declarations. *********************/

static inline int kv_pair_inuse(struct kv_pair *pair) {
    return pair != 0;
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
static struct kv_pair *kv_pair_malloc_mempool(const void *key, int keylen, const void *val, int vallen, struct mempool *mp) {
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
            struct kv_pair *newkv = toku_mempool_malloc(&new_kvspace, kv_pair_size(kv), 4);
            assert(newkv);
            memcpy(newkv, kv, kv_pair_size(kv));
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

static struct kv_pair *pma_malloc_kv_pair(PMA pma __attribute__((unused)), const void *k, int ksize, const void *v, int vsize) {
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
    return kv_pair_inuse(pma->pairs[i]);
}

bytevec toku_pmanode_key (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_inuse(pair));
    return kv_pair_key(pair);
}

ITEMLEN toku_pmanode_keylen (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_inuse(pair));
    return kv_pair_keylen(pair);
}

bytevec toku_pmanode_val (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_inuse(pair));
    return kv_pair_val(pair);
}

ITEMLEN toku_pmanode_vallen (PMA pma, unsigned int i) {
    struct kv_pair *pair;
    assert(i<toku_pma_index_limit(pma));
    pair = pma->pairs[i];
    assert(kv_pair_inuse(pair));
    return kv_pair_vallen(pair);
}

/* Could pick the same one every time if we wanted. */
int toku_pma_random_pick(PMA pma, bytevec *key, ITEMLEN *keylen, bytevec *val, ITEMLEN *vallen) {
#if 1
    unsigned int i;

    /* For now a simple implementation where we simply start at the beginning and look. */
    for (i=0; i<toku_pma_index_limit(pma); i++) {
	struct kv_pair *pair = pma->pairs[i];
        if (kv_pair_inuse(pair)) {
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
	if (kv_pair_inuse(pair)) {
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

static int pma_compare_dbt_kv(PMA pma, DBT *k, DBT *v, struct kv_pair *kv) {
    DBT k2, v2;
    int cmp = pma->compare_fun(pma->db, k, toku_fill_dbt(&k2, kv_pair_key(kv), kv_pair_keylen(kv)));
    if (cmp == 0 && v)
        cmp = pma->dup_compare_fun(pma->db, v, toku_fill_dbt(&v2, kv_pair_val(kv), kv_pair_vallen(kv)));
    return cmp;
}

/* search the index for a matching key and maybe value */
static unsigned int pma_search(PMA pma, DBT *k, DBT *v, int lo, int hi, int *found) {
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
            return pma_search(pma, k, v, lo, omi, found);
        int cmp = pma_compare_dbt_kv(pma, k, v, pma->pairs[mi]);
        if (cmp > 0)
            return pma_search(pma, k, v, mi+1, hi, found);
        if (cmp < 0)
            return pma_search(pma, k, v, lo, mi, found);

        /* we have a match, try to find a better match on the left tree */
        int here = pma_search(pma, k, v, lo, mi, found);
        if (*found == 0)
            here = mi;
        *found = 1;
        return here;
    }
}

static unsigned int pma_search_func(PMA pma, brt_search_t *search, int lo, int hi, int *found) {
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
            return pma_search_func(pma, search, lo, omi, found);
        struct kv_pair *kv = pma->pairs[mi];
        DBT x, y;
        int cmp = search->compare(search, search->k ? toku_fill_dbt(&x, kv_pair_key(kv), kv_pair_keylen(kv)) : 0, 
                                  search->v ? toku_fill_dbt(&y, kv_pair_val(kv), kv_pair_vallen(kv)) : 0);
        if (cmp == 0) {
            if (search->direction == BRT_SEARCH_LEFT)
                return pma_search_func(pma, search, mi+1, hi, found);
            else
                return pma_search_func(pma, search, lo, mi, found);
        }

        /* we have a match, try to find a better match on the left or right subtrees */
        int here;
        if (search->direction == BRT_SEARCH_LEFT)
            here = pma_search_func(pma, search, lo, mi, found);
        else
            here = pma_search_func(pma, search, mi+1, hi, found);
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
    int found;
    int lo = pma_search(pma, k, 0, 0, pma->N, &found);
    return lo;
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
        if (kv_pair_inuse(pairs[i])) {
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

static int pma_log_distribute (TOKULOGGER logger, FILENUM filenum, DISKOFF old_diskoff, DISKOFF new_diskoff, int n_pairs, struct kv_pair_tag *pairs, LSN *oldnode_lsn, LSN*newnode_lsn) {
    INTPAIRARRAY ipa;
    MALLOC_N(n_pairs, ipa.array);
    if (ipa.array==0) return errno;
    int j=0;
    int i;
    for (i=0; i<n_pairs; i++) {
	if (pairs[i].pair!=0) {
	    ipa.array[j].a = pairs[i].oldtag;
	    ipa.array[j].b = pairs[i].newtag;
	    j++;
	}
    }
    ipa.size=j;
    int r=toku_log_pmadistribute(logger, filenum, old_diskoff, new_diskoff, ipa);
    if (logger && oldnode_lsn) *oldnode_lsn = toku_logger_last_lsn(logger);
    if (logger && newnode_lsn) *newnode_lsn = toku_logger_last_lsn(logger);
//    if (0 && pma) {
//	printf("Pma state:\n");
//	PMA_ITERATE_IDX (pma, pidx, key, keylen, data, datalen,
//			 printf(" %d:(%d:%s) (%d:%s)\n", pidx, keylen, (char*)key, datalen, (char*)data));
//    }
    toku_free(ipa.array);
    return r;
}

/* spread the non-empty pairs around.  There are n of them.  Create an empty slot just before the IDXth
   element, and return that slot's index in the smoothed array. */
int toku_pmainternal_smooth_region (TOKULOGGER logger, FILENUM filenum, DISKOFF diskoff, struct kv_pair *pairs[], int n, int idx, int base, PMA pma, int *new_idx, LSN *node_lsn) {
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
	int r = pma_log_distribute(logger, filenum, diskoff, diskoff,
				   n_saved,
				   tmppairs,
				   node_lsn, node_lsn);
	if (r!=0) goto cleanup;

	*new_idx = newidx;
    cleanup:
#ifdef USE_MALLOC_IN_SMOOTH
	toku_free(tmppairs);
#endif
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

/* find the smallest power of 2 >= n */
static unsigned int pma_array_size(PMA pma __attribute__((unused)), int asksize) {
    int n = PMA_MIN_ARRAY_SIZE;
    while (n < asksize)
        n *= 2;
    return n;
}

int toku_pma_create(PMA *pma, pma_compare_fun_t compare_fun, DB *db, FILENUM filenum, int maxsize) {
    int error;
    TAGMALLOC(PMA, result);
    if (result==0) return -1;
    result->dup_mode = 0;
    result->n_pairs_present = 0;
    result->pairs = 0;
    result->compare_fun = compare_fun;
    result->dup_compare_fun = 0;
    result->db = db;
    result->filenum = filenum;
    result->skey = 0;
    result->sval = 0;
    result->N = PMA_MIN_ARRAY_SIZE;
    result->pairs = 0;
    {
	unsigned int n = pma_array_size(result, result->N);
	error = toku_resize_pma_exactly(result, 0, n);
	if (error) {
	    toku_free(result);
	    return -1;
	}
	toku_pmainternal_calculate_parameters(result);
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

static int pma_resize_array_nolog(PMA pma, int asksize, int startz, unsigned int *oldn, unsigned int *newn) {
    unsigned int oldN = pma->N;
    unsigned int n = pma_array_size(pma, asksize);
    int r = toku_resize_pma_exactly(pma, startz, n);
    if (r!=0) return r;
    toku_pmainternal_calculate_parameters(pma);
    *oldn = oldN;
    *newn = n;
    return 0;
}

static int pma_resize_array(TOKULOGGER logger, FILENUM filenum, DISKOFF offset, PMA pma, int asksize, int startz, LSN *node_lsn) {
    unsigned int oldN, n;
    int r = pma_resize_array_nolog(pma, asksize, startz, &oldN, &n);
    if (r!=0) return r;
    toku_log_resizepma (logger, filenum, offset, oldN, n);
    if (logger && node_lsn) *node_lsn = toku_logger_last_lsn(logger);
    return 0;
}

int toku_pma_set_compare(PMA pma, pma_compare_fun_t compare_fun) {
    pma->compare_fun = compare_fun;
    return 0;
}

int toku_pma_set_dup_mode(PMA pma, int dup_mode) {
    if (!(dup_mode == 0 || dup_mode == (TOKU_DB_DUP+TOKU_DB_DUPSORT)))
        return EINVAL;
    pma->dup_mode = dup_mode;
    return 0;
}

int toku_pma_set_dup_compare(PMA pma, pma_compare_fun_t dup_compare_fun) {
    pma->dup_compare_fun = dup_compare_fun;
    return 0;
}

/* find the next matching key in the pma starting from index here */
static int pma_next_key(PMA pma, DBT *k, DBT *v, int here, int n, int *found) {
    assert(0 <= here);
    *found = 0;
    while (here < n && !kv_pair_inuse(pma->pairs[here]))
        here += 1;
    if (here < n) {
        int cmp = pma_compare_dbt_kv(pma, k, v, pma->pairs[here]);
        if (cmp == 0)
            *found = 1;
    }
    return here;
}

/* Make some space for a key to go at idx (the thing currently at idx should end up at to the right.) */
/* (Making space may involve moving things around, including the hole at index.) */
int toku_pmainternal_make_space_at (TOKULOGGER logger, FILENUM filenum, DISKOFF offset, PMA pma, int idx, unsigned int *new_index, LSN *node_lsn) {
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
                pma_resize_array(logger, filenum, offset, pma, size, hi, node_lsn);

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
	int r = toku_pmainternal_smooth_region(logger, filenum, offset, pma->pairs+lo, hi-lo, idx-lo, lo, pma, &sub_new_index, node_lsn);
	if (r!=0) return r;
	*new_index=sub_new_index+lo; 
	return 0;
    }
}

enum pma_errors toku_pma_lookup (PMA pma, DBT *k, DBT *v) {
    int found;
    unsigned int here = pma_search(pma, k, 0, 0, pma->N, &found);
    struct kv_pair *kv = pma->pairs[here];
    if (found && kv_pair_inuse(kv))
        return toku_dbt_set_value(v, kv->key + kv->keylen, kv->vallen, &pma->sval);
    else
        return DB_NOTFOUND;
}

int toku_pma_search(PMA pma, brt_search_t *search, DBT *foundk, DBT *foundv) {
    int found;
    unsigned int here = pma_search_func(pma, search, 0, pma->N, &found);
    struct kv_pair *kv = pma->pairs[here];
    if (found && kv_pair_inuse(kv)) {
        int r = 0;
        if (foundk)
            r = toku_dbt_set_value(foundk, kv_pair_key(kv), kv_pair_keylen(kv), &pma->skey);
        if (r == 0 && foundv)
            r = toku_dbt_set_value(foundv, kv_pair_val(kv), kv_pair_vallen(kv), &pma->sval);
        return r;
    } else
        return DB_NOTFOUND;
}

/* returns 0 if OK.
 * You must have freed all the cursors, otherwise returns nonzero and does nothing. */
int toku_pma_free (PMA *pmap) {
    PMA pma=*pmap;
    
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
int toku_pma_insert (PMA pma, DBT *k, DBT *v, TOKULOGGER logger, TXNID xid, FILENUM filenum, DISKOFF diskoff, u_int32_t rand4fingerprint, u_int32_t *fingerprint, LSN *node_lsn) {
    int found;
    unsigned int idx = pma_search(pma, k, pma->dup_mode & TOKU_DB_DUPSORT ? v : 0, 0, pma->N, &found);
    if (found) 
        return BRT_ALREADY_THERE; /* It is already here.  Return an error. */
    
    if (kv_pair_inuse(pma->pairs[idx])) {
	unsigned int newidx;
        int r = toku_pmainternal_make_space_at (logger, filenum, diskoff, pma, idx, &newidx, (LSN*)0); /* returns the new idx. */
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
	int r = toku_log_insertinleaf (logger, xid, pma->filenum, diskoff, idx, key, data);
	if (logger && node_lsn) *node_lsn = toku_logger_last_lsn(logger);
	return r;
    }
}    

static int pma_delete_dup (PMA pma, DBT *k, DBT *v, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    /* find the left most matching key in the pma */
    int found;
    unsigned int lefthere = pma_search(pma, k, v, 0, pma->N, &found);
    int rightfound = found, righthere = lefthere;
    while (rightfound) {
        struct kv_pair *kv = pma->pairs[righthere];
        if (kv_pair_inuse(kv)) {
            *deleted_size += PMA_ITEM_OVERHEAD+ KEY_VALUE_OVERHEAD + kv_pair_keylen(kv) + kv_pair_vallen(kv);
	    *fingerprint -= rand4sem*toku_calccrc32_kvpair (kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
            pma_mfree_kv_pair(pma, kv);
            pma->pairs[righthere] = 0;
            pma->n_pairs_present--;
        }
        /* find the next matching key in the pma */
        righthere = pma_next_key(pma, k, v, righthere+1, pma->N, &rightfound);
    }
    if (found) {
        /* check the density of the region centered around the deleted pairs */
        pma_delete_at(pma, (lefthere + righthere) / 2);
    }
    return found ? BRT_OK : DB_NOTFOUND;
}

static int pma_delete_nodup (PMA pma, DBT *k, DBT *v, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    /* find the left most matching key in the pma */
    int found;
    unsigned int here;
    here = pma_search(pma, k, v, 0, pma->N, &found);
    struct kv_pair *kv = pma->pairs[here];
    if (!found || !kv_pair_inuse(kv))
        return DB_NOTFOUND;
    *deleted_size = PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + kv_pair_keylen(kv) + kv_pair_vallen(kv); 
    *fingerprint -= rand4sem*toku_calccrc32_kvpair (kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
    pma_mfree_kv_pair(pma, kv);
    pma->pairs[here] = 0;
    pma->n_pairs_present--;
    pma_delete_at(pma, here);
    return BRT_OK;
}

int toku_pma_delete (PMA pma, DBT *k, DBT *v, u_int32_t rand4sem, u_int32_t *fingerprint, u_int32_t *deleted_size) {
    v = v;
    u_int32_t my_deleted_size;
    if (!deleted_size)
        deleted_size = &my_deleted_size;
    *deleted_size = 0;
    if (pma->dup_mode & TOKU_DB_DUPSORT) 
        return pma_delete_dup(pma, k, v, rand4sem, fingerprint, deleted_size);
    else
        return pma_delete_nodup(pma, k, v, rand4sem, fingerprint, deleted_size);
}

static void pma_delete_at(PMA pma, int here) {
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
    toku_free(newpairs);
}

int toku_pma_insert_or_replace (PMA pma, DBT *k, DBT *v,
				int *replaced_v_size, /* If it is a replacement, set to the size of the old value, otherwise set to -1. */
				TOKULOGGER logger, TXNID xid, FILENUM filenum, DISKOFF diskoff,
				u_int32_t rand4fingerprint, u_int32_t *fingerprint,
				LSN *node_lsn) {
    //printf("%s:%d v->size=%d\n", __FILE__, __LINE__, v->size);
    int r;
    int found;
    unsigned int idx = pma_search(pma, k, pma->dup_mode & TOKU_DB_DUPSORT ? v : 0, 0, pma->N, &found);
    if (found) {
        struct kv_pair *kv = pma->pairs[idx];
        *replaced_v_size = kv->vallen;
        *fingerprint -= rand4fingerprint*toku_calccrc32_kvpair(kv_pair_key_const(kv), kv_pair_keylen(kv), kv_pair_val_const(kv), kv_pair_vallen(kv));
	{
	    const BYTESTRING deletedkey  = { kv->keylen, kv_pair_key(kv) };
	    const BYTESTRING deleteddata = { kv->vallen, kv_pair_val(kv) };
	    r=toku_log_deleteinleaf(logger, xid, pma->filenum, diskoff, idx, deletedkey, deleteddata);
	    if (r!=0) return r;
	}
        if (logger && node_lsn) *node_lsn = toku_logger_last_lsn(logger);
        if (v->size == (unsigned int) kv_pair_vallen(kv)) {
            memcpy(kv_pair_val(kv), v->data, v->size);
        } else {
            pma_mfree_kv_pair(pma, kv);
            pma->pairs[idx] = pma_malloc_kv_pair(pma, k->data, k->size, v->data, v->size);
            assert(pma->pairs[idx]);
        }
	/* idx is live here */
	goto logit_and_update_fingerprint;
    }
    if (kv_pair_inuse(pma->pairs[idx])) {
	unsigned int newidx;
        r = toku_pmainternal_make_space_at (logger, filenum, diskoff, pma, idx, &newidx, node_lsn); /* returns the new idx. */
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
 logit_and_update_fingerprint:
    {
	const struct kv_pair *pair = pma->pairs[idx];
	const BYTESTRING key  = { pair->keylen, (char*)kv_pair_key_const(pair) };
	const BYTESTRING data = { pair->vallen, (char*)kv_pair_val_const(pair) };
	r = toku_log_insertinleaf (logger, xid, pma->filenum, diskoff, idx, key, data);
	if (logger && node_lsn) *node_lsn = toku_logger_last_lsn(logger);
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


int toku_pma_split(TOKULOGGER logger, FILENUM filenum,
		   DISKOFF diskoff,    PMA pma,     unsigned int *pma_size_p,     u_int32_t rand4fp,    u_int32_t *fingerprint_p,    LSN *lsn,
		   DBT *splitk,
		   DISKOFF newdiskoff, PMA newpma,  unsigned int *newpma_size_p,  u_int32_t newrand4fp, u_int32_t *newfingerprint_p, LSN *newlsn) {
    int error;
    int npairs;
    struct kv_pair_tag *pairs;
    int i;
    int n;
    int spliti;

    /* extract the pairs */
    npairs = toku_pma_n_entries(pma);
    if (npairs == 0) {
        if (splitk)
            memset(splitk, 0, sizeof *splitk);
        return 0;
    }
    /* TODO move pairs to the stack */
    pairs = pma_extract_pairs(pma, npairs, 0, pma->N);
    assert(pairs);

    assert(toku_pma_n_entries(newpma) == 0);

    /* debug check the kv length sum */
    unsigned int sumlen = 0;
    for (i=0; i<npairs; i++)
        sumlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD;

    if (pma_size_p)
        assert(*pma_size_p == sumlen);

    unsigned int runlen = 0;
    for (i=0; i<npairs;) {
        runlen += kv_pair_keylen(pairs[i].pair) + kv_pair_vallen(pairs[i].pair) + PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD;
        i++;
        if (2*runlen >= sumlen)
            break;
    }
    spliti = i;

    unsigned int revised_leftpmasize  = runlen;
    unsigned int revised_rightpmasize = sumlen-runlen;

    u_int32_t revised_left_fingerprint;
    u_int32_t revised_right_fingerprint;
    {
	u_int32_t sum = 0;
	for (i=spliti; i<npairs; i++) {
	    sum+=toku_calccrc32_kvpair(kv_pair_key_const(pairs[i].pair), kv_pair_keylen(pairs[i].pair),
				       kv_pair_val_const(pairs[i].pair), kv_pair_vallen(pairs[i].pair));
	}
	revised_left_fingerprint  = -rand4fp * sum;
	revised_right_fingerprint =  newrand4fp * sum;
    }

    if (splitk) {
        struct kv_pair *a = pairs[spliti-1].pair;
        if (pma->dup_mode & TOKU_DB_DUPSORT) {
            splitk->data = kv_pair_malloc(kv_pair_key(a), kv_pair_keylen(a), kv_pair_val(a), kv_pair_vallen(a));
            splitk->size = kv_pair_keylen(a) + kv_pair_vallen(a);
        } else {
            splitk->data = kv_pair_malloc(kv_pair_key(a), kv_pair_keylen(a), 0, 0);
            splitk->size = kv_pair_keylen(a);
        }
        splitk->flags = 0;
    }

    /* put the second half of pairs into the right pma */
    /* Do this first, so that the logging will move the stuff out of the left pma first, and then later when we redistribute in the left PMA, we won't overwrite something. */ 
    n = npairs - spliti;
    error = pma_resize_array(logger, filenum, newdiskoff, newpma, n + n/4, 0, newlsn);
    assert(error == 0);
    distribute_data(newpma->pairs, toku_pma_index_limit(newpma), &pairs[spliti], n, newpma);
    {
	int r = pma_log_distribute(logger, filenum, diskoff, newdiskoff, n, &pairs[spliti], lsn, newlsn);
	if (r!=0) { toku_free(pairs); return r; }
    }
#if PMA_USE_MEMPOOL
    __pma_relocate_kvpairs(newpma);
    // If it's in an mpool, we must free those pairs.
    for (i=spliti; i<npairs; i++) {
	pma_mfree_kv_pair(pma, pairs[i].pair);
    }
#endif
    newpma->n_pairs_present = n;

    /* put the first half of pairs into the left pma */
    n = spliti;
    // Since the new array is smaller than the old one, during recovery we need to do the resize after moving the elements.
    // But we must actually do the resize first here so we can determine the size.
    unsigned int oldn_for_logging, newn_for_logging;
    error = pma_resize_array_nolog(pma, n + n/4, 0, // zeros the elements
				   &oldn_for_logging, &newn_for_logging);
    assert(error == 0);
    distribute_data(pma->pairs, toku_pma_index_limit(pma), &pairs[0], n, pma);
    {
	int r = pma_log_distribute(logger, filenum, diskoff, diskoff, spliti, &pairs[0], lsn, lsn);
	if (r!=0) { toku_free(pairs); return r; }
	r = toku_log_resizepma(logger, filenum, diskoff, oldn_for_logging, newn_for_logging);
	if (r!=0) { toku_free(pairs); return r; }
	if (logger && lsn) *lsn = toku_logger_last_lsn(logger);

    }
    // Don't have to relocate kvpairs, because these ones are still there.
    pma->n_pairs_present = spliti;

    toku_free(pairs);

    /* The remaining cursors are in the left pma */

    if (fingerprint_p)    *fingerprint_p    += revised_left_fingerprint;
    if (newfingerprint_p) *newfingerprint_p += revised_right_fingerprint;
    if (pma_size_p)       *pma_size_p       = revised_leftpmasize;
    if (newpma_size_p)    *newpma_size_p    = revised_rightpmasize;

    return 0;
}

static void __pma_bulk_cleanup(struct pma *pma, struct kv_pair_tag *pairs, int n) {
    int i;

    for (i=0; i<n; i++)
        if (pairs[i].pair)
            pma_mfree_kv_pair(pma, pairs[i].pair);
}

int toku_pma_bulk_insert(TOKULOGGER logger, FILENUM filenum, DISKOFF diskoff, PMA pma, DBT *keys, DBT *vals, int n_newpairs, u_int32_t rand4fp, u_int32_t *sum, LSN *node_lsn) {
    struct kv_pair_tag *newpairs;
    int i;
    int error;
    u_int32_t delta=0;

    if (n_newpairs == 0)
        return 0;
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

    error = pma_resize_array(logger, filenum, diskoff, pma, n_newpairs + n_newpairs/4, 0, node_lsn);
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
            i += 1;
            break;
        }
    }

    /* compare the current key with the next key in the index */
    struct kv_pair *nextkv;
    for (; i<pma->N; i++) {
        nextkv = pma->pairs[i];
        if (kv_pair_inuse(nextkv)) {
            DBT kv_dbt, nextkv_dbt;
            toku_fill_dbt(&kv_dbt, kv_pair_key(kv), kv_pair_keylen(kv));
            toku_fill_dbt(&nextkv_dbt, kv_pair_key(nextkv), kv_pair_keylen(nextkv));
            int r = pma->compare_fun(pma->db, &kv_dbt, &nextkv_dbt);
            if (pma->dup_mode == 0)
                assert(r < 0);
            else if (pma->dup_mode & TOKU_DB_DUPSORT)
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

int toku_pma_clear_at_index (PMA pma, unsigned int idx) {
    if (idx>=pma->N) return -1;
    if (!kv_pair_inuse(pma->pairs[idx])) return -1;
    pma_mfree_kv_pair(pma, pma->pairs[idx]);
    pma->pairs[idx]=0;
    pma->n_pairs_present--;
    return 0;
}

// Move from a to b
static void pma_move (PMA pmaa, int idxa, u_int32_t randa, u_int32_t *fingerprinta, u_int32_t *n_in_bufa,
		      PMA pmab, int idxb, u_int32_t randb, u_int32_t *fingerprintb, u_int32_t *n_in_bufb) {
    if (pmaa==pmab) {
	assert(pmab->pairs[idxb]==0);
	pmab->pairs[idxb] = pmaa->pairs[idxa];
	pmaa->pairs[idxa] = 0;
    } else {
	struct kv_pair *pair = pmaa->pairs[idxa];
	u_int32_t fdiff = toku_calccrc32_kvpair(kv_pair_key_const(pair), kv_pair_keylen(pair), kv_pair_val_const(pair), kv_pair_vallen(pair));
	u_int32_t sizediff = PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + kv_pair_keylen(pair) + kv_pair_vallen(pair);
	*fingerprinta -= randa*fdiff;    *fingerprintb += randb*fdiff;
	*n_in_bufa    -= sizediff;       *n_in_bufb    += sizediff;
	pmab->pairs[idxb] = pma_malloc_kv_pair(pmab, kv_pair_key_const(pair), kv_pair_keylen(pair), kv_pair_val_const(pair), kv_pair_vallen(pair));
	pma_mfree_kv_pair(pmaa, pair);
	pmaa->pairs[idxa] = 0;
	pmaa->n_pairs_present--;
	pmab->n_pairs_present++;
    }
}

// assume no cursors
// Move stuff from pmaa to pmab
int toku_pma_move_indices (PMA pma_from, PMA pma_to, INTPAIRARRAY fromto,
			   u_int32_t rand_from, u_int32_t *fingerprint_from,
			   u_int32_t rand_to,   u_int32_t *fingerprint_to,
			   u_int32_t *n_in_buf_from, u_int32_t *n_in_buf_to
			   ) {
    u_int32_t i;
    for (i=0; i<fromto.size; i++) {
	// First handle the case for sliding something left.  We can simply move it.
	{
	    int a=fromto.array[i].a;
	    int b=fromto.array[i].b;
	    if (b==a) continue;
	    if (b<a) {
		pma_move(pma_from, a, rand_from, fingerprint_from, n_in_buf_from,
			 pma_to,   b, rand_to,   fingerprint_to,   n_in_buf_to);
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
		    pma_move(pma_from, a, rand_from, fingerprint_from, n_in_buf_from,
			     pma_to,   b, rand_to,   fingerprint_to,   n_in_buf_to);
		}
		if (i==jdown) break; // Do it this way so everything can be unsigned and we won't try to go negative.
	    }
	    i=j-1;
	}
    }
    return 0;
}

static void reverse_fromto (INTPAIRARRAY fromto) {
    u_int32_t i;
    for (i=0; i<fromto.size; i++) {
	int tmp = fromto.array[i].a;
	fromto.array[i].a = fromto.array[i].b;
	fromto.array[i].b = tmp;
    }
}

int toku_pma_move_indices_back (PMA pma_backto, PMA pma_backfrom, INTPAIRARRAY fromto,
				u_int32_t rand_backto,   u_int32_t *fingerprint_backto,
				u_int32_t rand_backfrom, u_int32_t *fingerprint_backfrom,
				u_int32_t *n_in_buf_backto, u_int32_t *n_in_buf_backfrom
				) {
    int r;
    reverse_fromto(fromto);
    r = toku_pma_move_indices(pma_backfrom, pma_backto, fromto,
			      rand_backfrom, fingerprint_backfrom,
			      rand_backto,   fingerprint_backto,
			      n_in_buf_backfrom, n_in_buf_backto
			      );
    reverse_fromto(fromto);
    return r;
}
