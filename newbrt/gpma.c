/* General PMA. */

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <errno.h>

#include "gpma.h"
#include "yerror.h"
#include "toku_assert.h"
#include "memory.h"
// Need this for DB_KEYEXIST
#include "../include/db.h"
#include "gpma-internal.h"

// Find the ceiling of lg n. */
int toku_lg (unsigned int n) {
    int result=0;
    unsigned int two_to_result = 1;
    while (two_to_result<n) {
	result++;
	two_to_result*=2;
    }
    return result;
}

/* find the smallest power of 2 >= n */
inline u_int32_t toku_hyperceil (u_int32_t v) {
    u_int32_t n = 1;
    while (n < v)
        n *= 2;
    return n;
}


/* Calculate densitysteps and uplgN, given N. */
static void calculate_parameters (GPMA pma) {
    unsigned int N = toku_gpma_index_limit(pma);
    int lgN = toku_lg(N);
    int n_divisions = lgN;
    //printf("uplgN = %d n_divisions=%d\n", pma->uplgN, n_divisions);
    assert(n_divisions>0);
    pma->udt_step = (GPMA_UDT_HIGH - GPMA_UDT_LOW)/n_divisions;
    pma->ldt_step = (GPMA_LDT_HIGH - GPMA_LDT_LOW)/n_divisions;
}


int toku_gpma_create(GPMA*gpma, int initial_index_limit) {
    if (initial_index_limit && (initial_index_limit&(initial_index_limit-1))) return EINVAL; // must be a power of two.
    TAGMALLOC(GPMA, result);
    if (result==0) return errno;
    result->N = initial_index_limit ? initial_index_limit : GPMA_MIN_ARRAY_SIZE;
    result->n_items_present=0;
    calculate_parameters(result);
    MALLOC_N(result->N, result->items);
    if (result->items==0) { int r=errno; toku_free(result); return r; }
    {
	u_int32_t i;
	for (i=0; i<result->N; i++) result->items[i].data=0;
    }
    *gpma=result;
    return 0;
}

void toku_gpma_free(GPMA*gpmap, gpma_free_callback_t freeme,void*extra) {
    u_int32_t i;
    GPMA pma=*gpmap;
    for (i=0; i<pma->N; i++) {
	if (pma->items[i].data) {
	    if (freeme)
		freeme(pma->items[i].len, pma->items[i].data, extra);
	    pma->items[i].data=0;
	}
    }
    toku_free(pma->items);
    toku_free(pma);
    *gpmap=0;
}
		   
		   
u_int32_t toku_gpma_n_entries(GPMA pma) {
    return pma->n_items_present;
}

u_int32_t toku_gpma_index_limit(GPMA pma) {
    return pma->N;
}

// If direction==0 then find any match for which the bessel gives 0.   *found is set to 1 iff something with 0.  The return value is the place where the zero is (if found), or the place where it would go (if there's a value there, then that value goes after the zero.)
// If direction>0 then find the first match for which bessel gives >0.  *found is set to 1 iff something with >0.  The return value is the index of the leftmost such value (if found).  In the not-found case, all items are <=0 and the return value is pma->N.
// If direction<0 then find the last match for which bessel gives <0.   *found is set to 1 iff something with <0.  The return value is the index of the rightmost such value (if found).  In the not-found case, all items are >=0 and the return value is 0.
u_int32_t toku_gpma_find_index_bes (GPMA pma, gpma_besselfun_t besf, int direction, void *extra, int *found) {
    if (direction==0) {
	int lo=0, hi=pma->N;
	while (lo<hi) {
	    int mi = (lo+hi)/2;
	    int look = mi;
	    while (look<hi && pma->items[look].data==0)  look++;
	    if (look>=hi) {
		// went too far, so mi is new hi
		hi=mi;
	    } else {
		int cmp = besf(pma->items[look].len, pma->items[look].data, extra);
		if (cmp==0) {
		    /* We found a match. */
		    *found=1;
		    return look;
		} else if (cmp>0) {
		    hi=mi;
		} else {
		    lo=look+1;
		}
	    }
	}
	*found = 0;
	return lo;
    } else if (direction<0) {
	// Find the rightmost negative value.

#if 0
	// Linear-time code, for ease of reading
	u_int32_t i;
	for (i=pma->N; i>0; i--) {
	    if (pma->items[i-1].data) {
		int cmp = besf(pma->items[i-1].len, pma->items[i-1].data, extra);
		if (cmp<0) {
		    *found=1;
		    return i-1;
		}
	    }
	}
	*found=0;
	return 0;
#else
	// direction<0. Log-time code.  For performance.
	int lo=0, hi=pma->N;
	int foundone=0;
	int answer=lo;
	while (lo<hi) {
	    int mi = (lo+hi)/2;
	    int look = mi;
	    while (look<hi && pma->items[look].data==0)  look++;
	    if (look>=hi) {
		// there was nothing in the right half
		hi=mi;
	    } else {
		int cmp = besf(pma->items[look].len, pma->items[look].data, extra);
		if (cmp>=0) {
		    // look is too big.
		    hi=mi;
		} else {
		    // look is is a good answer, so set lo to that.  From now on we can only change lo if we find another good answer.
		    answer=look;
		    foundone=1;
		    lo=look+1;
		}
	    }
	}
	*found = foundone;
	return answer;
#endif
    } else {
	// Find the leftmost postive value.
#if 0
	// Linear-time code, for ease of reading
	u_int32_t i;
	for (i=0; i<pma->N; i++) {
	    if (pma->items[i].data) {
		int cmp = besf(pma->items[i].len, pma->items[i].data, extra);
		if (cmp>0) {
		    *found=1;
		    return i;
		}
	    }
	}
	*found=0;
	return pma->N;
#else
	// direction>0.  Log-time code.  For performance.
	// The loop invariant is that if we found one, then hi is a good answer.
	int lo=0, hi=pma->N;
	int foundone=0;
	while (lo<hi) {
	    int mi = (lo+hi)/2;
	    int look = mi;
	    while (look>lo && pma->items[look].data==0)  look--;
	    if (look==lo && pma->items[look].data==0) {
		// There was nothing in the left half.
		lo = mi+1;
	    } else {
		int cmp = besf(pma->items[look].len, pma->items[look].data, extra);
		if (cmp<=0) {
		    // look is too small.   That means mi is too small.
		    lo = mi+1;
		} else {
		    // look is a good answer, so set hi to that.  From now on we only change hi if we find another good answer.
		    hi = look;
		    foundone=1;
		}
	    }
	}
	*found = foundone;
	return hi;
#endif
    }
}

// Convert a comparison function against a particular item to a besselfun.
struct convert_extra {
    gpma_compare_fun_t comparef;
    u_int32_t dlen;
    void *dval;
    void *extra;
};
static int bessel_from_compare (u_int32_t dlen, void *dval, void *extra) {
    struct convert_extra *ce=extra;
    return -ce->comparef(ce->dlen, ce->dval, dlen, dval, ce->extra);
}

// Find the place where (len,data) is stored.  Return *found==0 iff the item is not actually there.
// Could return anything from 0 to N inclusive.
u_int32_t toku_gpma_find_index (GPMA pma, u_int32_t dlen, void *dval, gpma_compare_fun_t comparef, void *extra, int *found) {
    struct convert_extra ce = {comparef, dlen, dval, extra};
    return toku_gpma_find_index_bes(pma, bessel_from_compare, 0, &ce, found);
}

  
// the region from lo (inclusive) to hi (exclusive) is all empty.
// Distribute the data across it.
void toku_gpma_distribute (GPMA pma,
			   u_int32_t lo, u_int32_t hi,
			   u_int32_t count,
			   struct gitem *items, // some of these may be NULL data, be we leave space for them anyway.
			   /*out*/ u_int32_t *tos)   // the indices where the values end up (we fill this in)
{
    int width = hi-lo;
    u_int32_t nplaced=0;
    u_int32_t nused  =0;
    u_int32_t i;
    assert(hi<=pma->N);
    for (i=lo; i<hi; i++) {
	// if nused/i <= (nitems)/width then place something here
	// But don't do floating point divisions
	if (nused*(u_int64_t)width <= count*(u_int64_t)i) {
	    tos[nplaced] = i;
	    pma->items[i] = items[nplaced++];
	    nused++;
	}
    }
    assert(nplaced==count);
}

int toku_gpma_smooth_region (GPMA pma,
			     u_int32_t lo, u_int32_t hi,
			     u_int32_t count, // The number of nonnull values
			     u_int32_t idx, u_int32_t *newidxp,  // set newidxp to 0 if you don't want to track a particular index
			     gpma_renumber_callback_t rcall, void *extra,
			     u_int32_t old_N) {
    if (count==0) return 0;
    int width = hi-lo;
    u_int32_t *MALLOC_N(count, froms);    if (!froms) return ENOMEM;   
    u_int32_t *MALLOC_N(count, tos);      if (!tos)   { toku_free(froms); return ENOMEM; }
    u_int32_t  nitems=0;
    struct  gitem *MALLOC_N(width, temp); if (!temp)  { toku_free(tos); toku_free(froms); return ENOMEM; }
    u_int32_t  i;
    u_int32_t  idx_goes_to_tmp=pma->N+1; // too big, so we will notice a problem
    u_int32_t  newidx=pma->N+1;
    for (i=lo; i<hi; i++) {
	if (newidxp && idx==i) idx_goes_to_tmp=nitems;
	if (pma->items[i].data) {
	    //printf("froms[%d]=%d (count=%d)\n", nitems, i, count);
	    froms[nitems]=i;
	    temp [nitems]=pma->items[i];
	    pma->items[i].data=0;
	    nitems++;
	}
    }
    if (newidxp && idx==i) idx_goes_to_tmp = nitems;
    // Now they are all compacted into temp.  Spread them out again
    u_int32_t nplaced=0;
    u_int32_t nused  =0;
    u_int64_t nitems_to_place = newidxp ? (nitems+1) : nitems;
    for (i=lo; i<hi; i++) {
	// if nused/i < (nitems+1)/width then place something here
	// But don't do floating point divisions
	if (nused*(u_int64_t)width < nitems_to_place*(u_int64_t)(i-lo)) {
	    if (newidxp && nused==idx_goes_to_tmp) {
		newidx=i;
	    } else {
		tos[nplaced] = i;
		pma->items[i] = temp[nplaced++];
	    }
	    nused++;
	}
    }
    assert((newidxp ? nplaced+1 : nplaced) ==nused);
    assert(nplaced==nitems);
    int r = 0;
    if (rcall) {
	r = rcall(nitems, froms, tos, temp, old_N, pma->N, extra);
    }
    toku_free(temp);
    toku_free(froms);
    toku_free(tos);
    if (newidxp) {
	assert(newidx<pma->N);
	*newidxp = newidx;
    }
    return r;
}

static int double_array (GPMA pma, u_int32_t idx, u_int32_t *newidx, gpma_renumber_callback_t rcall, void *extra) {
    {
	void *olditems = pma->items;
	REALLOC_N(pma->N*2, pma->items);
	if (pma->items==0) { pma->items=olditems; return errno; }
    }
    u_int32_t i;
    for (i=pma->N; i<pma->N*2; i++) pma->items[i].data=0;
    u_int32_t old_N = pma->N;
    pma->N *= 2;
    calculate_parameters(pma);
    int r = toku_gpma_smooth_region(pma, 0, pma->N, pma->n_items_present, idx, newidx, rcall, extra, old_N);
    if (r==ENOMEM) {
	pma->N /= 2;
	// Don't reallocate the memory downward.   We'll just hope that the current memory array is OK.
    }
    return r;
}

int toku_make_space_at (GPMA pma, u_int32_t idx, u_int32_t *newidx, gpma_renumber_callback_t rcall, void *extra) {
    if (idx!=pma->N) assert(pma->items[idx].data);
    u_int32_t lo=idx;
    u_int32_t hi=idx+1;
    if (idx==pma->N) { lo--; hi--; }
    double udt=GPMA_UDT_HIGH;
    u_int32_t count = 2; // one for the item that is there, plus one for the new item.
    u_int32_t width=1;
    double one_over_width = 1.0;
    while (1) {
	assert(lo<hi); assert(hi<=pma->N); // Make those separate asserts so that we don't get false complaints from gcov.
	double density = count*one_over_width;
	//printf("%s:%d %d..%d density=%f udt=%f\n", __FILE__, __LINE__, lo, hi, density, udt);
	if (density<=udt) break; // found a region that is good enough
	// Otherwise the density isn't good.
	u_int32_t N = pma->N;
	assert(width<=N);
	if (width<N) {
	    if (idx==N || width&idx) { // Grow the array downward.
		u_int32_t i;
		assert(lo>=width);
		lo -= width;
		for (i=0; i<width; i++) {
		    if (pma->items[lo+i].data) count++;
		}
	    } else { // Grow the array upward.
		u_int32_t i;
		for (i=0; i<width; i++) {
		    if (pma->items[hi+i].data) count++;
		}
		hi += width;
	    }
	    width*=2;
	    one_over_width*=0.5;
	    udt -= pma->udt_step;
	} else {
	    // The array  must be resized. */
	    assert(0==lo); assert(hi==pma->N);
	    return double_array(pma, idx, newidx, rcall, extra);
	}
    }
    return toku_gpma_smooth_region (pma, lo, hi, count, idx, newidx, rcall, extra, pma->N);
}

int toku_gpma_insert(GPMA pma,
		     u_int32_t len, void*data,
		     gpma_compare_fun_t compare,  void *extra_for_compare,
		     gpma_renumber_callback_t rcall, void*extra_for_rcall, // if anything gets renumbered, let the caller know
		     u_int32_t *idxp
		     ) {
    int found;
    u_int32_t idx = toku_gpma_find_index(pma, len, data, compare, extra_for_compare, &found);
    if (found) return DB_KEYEXIST;
    assert(idx<=toku_gpma_index_limit(pma));
    if (idx==toku_gpma_index_limit(pma) || pma->items[idx].data) {
	u_int32_t newidx;
	int r = toku_make_space_at(pma, idx, &newidx, rcall, extra_for_rcall);
	if (r!=0) return r;
	idx=newidx;
	assert(pma->items[idx].data==0);
    }
    pma->items[idx].data=data;
    pma->items[idx].len =len;
    pma->n_items_present++;
    if (idxp) *idxp=idx;
    return 0;
}

inline int toku_max_int (int a, int b) {
    return a<b ? b : a;
}
inline unsigned int toku_max_uint (unsigned int a, unsigned int b) {
    return a<b ? b : a;
}

static int shrink_pma (GPMA pma, gpma_renumber_callback_t renumberf, void *extra_for_renumberf) {
    u_int32_t old_N = pma->N;
    if (pma->n_items_present==0) {
	pma->N=8;
	void *olditems = pma->items;
	REALLOC_N(pma->N, pma->items);
	if (pma->items==0) { pma->items = olditems; return errno; }
	return 0;
    }
    int r;
    u_int32_t *MALLOC_N(pma->n_items_present, froms);    if (froms==0) { r=errno; if (0) { L0: toku_free(froms); } return r; }
    u_int32_t *MALLOC_N(pma->n_items_present, tos);      if (tos==0)   { r=errno; if (0) { L1: toku_free(tos);   } goto L0; }
    struct gitem *MALLOC_N(pma->n_items_present, items); if (items==0) { r=errno; if (0) { L2: toku_free(items); } goto L1; }
    u_int32_t nplaced=0;
    u_int32_t i;
    for (i=0; i<pma->N; i++) {
	if (pma->items[i].data) {
	    froms[nplaced] = i;
	    items[nplaced++] = pma->items[i];
	    pma->items[i].data = 0;
	}
    }
    {
	void *olditems = pma->items;
	REALLOC_N(pma->N/2, pma->items);
	if (pma->items==0) { r=errno; pma->items=olditems; goto L2; }
    }
    u_int32_t new_N = pma->N/2;
    pma->N = new_N;
    //printf("Shrunk to %d\n", pma->N);
    toku_gpma_distribute(pma, 0, pma->N, pma->n_items_present, items, tos);
    if (renumberf) {
	r = renumberf(pma->n_items_present, froms, tos, items, old_N, new_N, extra_for_renumberf);
    } else {
	r = 0;
    }
    goto L2;
}

// if minidx (inclusive) to maxidx (inclusive) gives a range of empty slots, find a big enough region and renumber everything.
int toku_smooth_deleted_region (GPMA pma, u_int32_t minidx, u_int32_t maxidx, gpma_renumber_callback_t renumberf, void *extra_for_renumberf) {
    if (pma->N<=8) return 0;
    u_int32_t lgN    = toku_lg(pma->N);
    u_int32_t lglgN  = toku_lg(lgN);
    u_int32_t n_steps = toku_max_uint(1, lgN-lglgN);
    double increment = (GPMA_LDT_HIGH-GPMA_LDT_LOW)/n_steps;
    u_int32_t initial_width = maxidx+1-minidx;
    u_int32_t lg_initw = toku_lg(initial_width);
    u_int32_t next_width = 1<<lg_initw;
    double target = GPMA_LDT_LOW+increment*lg_initw;

    u_int32_t count = 0;

    u_int32_t lo=minidx;
    u_int32_t hi=maxidx+1;

    while (1) {
	assert(next_width<=pma->N);
	while (hi-lo < next_width) { 
	    if (hi<pma->N) {
		if (pma->items[hi].data) count++;
		hi++;
	    } else {
		assert(lo>0);
		lo--;
		if (pma->items[lo].data) count++;
	    }
	}
	// if count/(hi-lo) >= target then we are happy
	if (count >= target*(hi-lo)) {
	    // we are happy with this width, spread things out.
	    return toku_gpma_smooth_region(pma, lo, hi, count, lo, 0, renumberf, extra_for_renumberf, pma->N);
	}
	if (next_width==pma->N) {
	    return shrink_pma(pma, renumberf, extra_for_renumberf);
	}

	next_width*=2;
    }
}

int toku_gpma_delete_bessel (GPMA pma,
			     gpma_besselfun_t besself,           void*extra_for_besself,
			     gpma_delete_callback_t deletef,     void*extra_for_deletef,  // for each deleted item, let the caller know
			     gpma_renumber_callback_t renumberf, void*extra_for_renumberf // if anything gets renumbered, let the caller know

			     ) {
    int r;
    u_int32_t len;
    void *data;
    u_int32_t idx;
    
    r = toku_gpma_lookup_bessel(pma, besself, 0, extra_for_besself, &len, &data, &idx);
    // Find how many items there are to delete.  Scan back and forward.
    if (r!=0) return DB_NOTFOUND;
    u_int32_t i;
    int nitems=1;
    u_int32_t maxidx=idx, minidx=idx;
    for (i=idx+1; i<pma->N; i++) {
	if (pma->items[i].data) {
	    if (besself(pma->items[i].len, pma->items[i].data, extra_for_besself)!=0)
		break;
	    nitems++;
	    maxidx=i;
	}
    }
    for (i=idx; i>0 ; i--) {
	if (pma->items[i-1].data) {
	    if (besself(pma->items[i-1].len, pma->items[i-1].data, extra_for_besself)!=0)
		break;
	    nitems++;
	    minidx=i-1;
	}
    }
    pma->n_items_present -= nitems;
    // Now we know the range and how many items will be deleted.
    for (i=minidx; i<=maxidx; i++) {
	if (pma->items[i].data) {
	    r = deletef(i, pma->items[i].len, pma->items[i].data, extra_for_deletef);
	    pma->items[i].data = 0;
	    if (r!=0) return r;
	}
    }
    // Now we must find a region that is sufficiently densely packed and spread things out.
    return toku_smooth_deleted_region(pma, minidx, maxidx, renumberf, extra_for_renumberf);
}

int toku_gpma_delete_item (GPMA pma,
			   u_int32_t len, void *data,
			   gpma_compare_fun_t comparef,        void *extra_for_comparef,
			   gpma_delete_callback_t deletef,     void *extra_for_deletef,
			   gpma_renumber_callback_t renumberf, void *extra_for_renumberf) {
    struct convert_extra ce = { comparef, len, data, extra_for_comparef };
    return toku_gpma_delete_bessel (pma, bessel_from_compare, &ce,
				    deletef, extra_for_deletef,
				    renumberf, extra_for_renumberf);
}

#if 0
// Delete anything for which the besselfun is zero.
// If things go wrong (e.g., the renumber_callback returns nonzero, or memory runs out
int toku_gpma_delete(GPMA pma,
		     gpma_besselfun_t besf,
		     gpma_delete_callback_t delcall, // call this on each deleted object
 		     gpma_renumber_callback_t rcall, // if anything gets renumbered, let the caller know
		     void*extra) {
    
}
#endif

int toku_gpma_lookup_item (GPMA pma,
			   u_int32_t len, void *data, gpma_compare_fun_t comparef, void *extra, u_int32_t *resultlen, void **resultdata, u_int32_t *idxp) {
    int found;
    u_int32_t idx = toku_gpma_find_index(pma, len, data, comparef, extra, &found);
    if (!found) return DB_NOTFOUND;
    *resultlen  = pma->items[idx].len;
    *resultdata = pma->items[idx].data;
    if (idxp) *idxp=idx;
    return 0;
}

int toku_gpma_lookup_bessel(GPMA pma, gpma_besselfun_t besf, int direction, void*extra, u_int32_t *resultlen, void **resultdata, u_int32_t *idxp) {
    int found;
    u_int32_t idx = toku_gpma_find_index_bes(pma, besf, direction, extra, &found);
    if (found) {
	*resultlen =pma->items[idx].len;
	*resultdata=pma->items[idx].data;
	if (idxp) *idxp=idx;
	return 0;
    } else {
	return DB_NOTFOUND;
    }
}

// Split the pma, putting some right suffix into newpma.  Try to split up so sum(lengths)+ overhead*N is equal.
// Move at least one element (if there is one)
// newpma is an empty pma
// If an error code is returned, then the pmas are likely to be all messed up.  Probably all you can do is close them.
int toku_gpma_split (GPMA pma, GPMA newpma, u_int32_t overhead,
		     int (*realloc_data)(u_int32_t olen, void *odata, void **ndata, void *extra),
		     gpma_renumber_callback_t rcall,
		     gpma_renumber_callback_t rcall_across_pmas, // This one is called for everything that moved
		     void *extra) {
    unsigned long totalweight=0;
    u_int32_t old_N = pma->N;
    {
	u_int32_t i;
	for (i=0; i<pma->N; i++) if (pma->items[i].data) totalweight += overhead +pma->items[i].len;
    }
    //toku_verify_gpma(pma);
    if (totalweight==0) return 0; // Nothing there
    unsigned long weight=0;
    u_int32_t prev=0;
    u_int32_t n_to_move=0;
    u_int32_t i;
    for (i=0; 1; i++) {
	assert(i<pma->N);
	if (pma->items[i].data) {
	    u_int32_t delta = 1 + pma->items[i].len;
	    if (weight+delta > totalweight/2) break; // prev is the last one to split.
	    weight += delta;
	    n_to_move++;
	    prev = i;
	}
    }

    u_int32_t split_here = prev;
    u_int32_t n_left = n_to_move;
    u_int32_t n_right  = pma->n_items_present - n_left;
#define MALLOC_N_ECK(n,v,l,lp) MALLOC_N(n,v); if (!v) { r=errno; if (0) { l: toku_free(v); } goto lp; }
    int r;
    if (0) { L0: return r; }
    struct gitem *MALLOC_N_ECK(n_left, leftitems,   L1,L0);
    struct gitem *MALLOC_N_ECK(n_right, rightitems, L2,L1);
    u_int32_t    *MALLOC_N_ECK(n_left, leftfroms,   L3,L2);
    u_int32_t    *MALLOC_N_ECK(n_right, rightfroms, L4,L3);
    u_int32_t    *MALLOC_N_ECK(n_left, lefttos,     L5,L4);
    u_int32_t    *MALLOC_N_ECK(n_right, righttos,   L6,L5);
    {
	u_int32_t n_moved=0;
	for (i=0; i<=split_here; i++) {
	    if (pma->items[i].data) {
		leftfroms[n_moved]   = i;
		leftitems[n_moved++] = pma->items[i];
		pma->items[i].data   = 0;
	    }
	}
	assert(n_moved==n_left);
    }
    {
	u_int32_t n_moved=0;
	for (i=split_here+1; i<pma->N; i++) {
	    if (pma->items[i].data) {
		rightfroms[n_moved]   = i;
		rightitems[n_moved++] = pma->items[i];
		pma->items[i].data    = 0;
	    }
	}
	assert(n_moved==n_right);
    }
    for (i=0; i<n_right; i++) {
	void *ndata;
	//printf("%s:%d len=%d\n", __FILE__, __LINE__, rightitems[i].len);
	r = realloc_data (rightitems[i].len, rightitems[i].data, &ndata, extra);
	if (r!=0) { goto L6; } // At this point the PMA is all messed up, and there is no easy way to put it all back together again.
	rightitems[i].data=ndata;
    }

    // Now we have split out the left and right stuff.  All we have to do is put it back.
    pma->N = toku_hyperceil(2*n_left);
    newpma->N = toku_hyperceil(2*n_left);

    REALLOC_N(pma->N,    pma->items);      if (!pma->items) return errno;
    REALLOC_N(newpma->N, newpma->items);   if (!pma->items) return errno;

    for (i=0; i<pma->N;    i++)    pma->items[i].data=0;
    for (i=0; i<newpma->N; i++) newpma->items[i].data=0;

    toku_gpma_distribute(pma, 0, pma->N, n_left, leftitems, lefttos);
    toku_gpma_distribute(newpma, 0, newpma->N, n_right, rightitems, righttos);

    pma->n_items_present    = n_left;
    newpma->n_items_present = n_right;
    //toku_verify_gpma(pma);
    //toku_verify_gpma(newpma);

    r = rcall(n_left, leftfroms, lefttos, leftitems, old_N, pma->N, extra);
    if (r!=0) { goto L6; }
    r = rcall_across_pmas(n_right, rightfroms, righttos, rightitems, 0, newpma->N, extra);
    if (r!=0) { goto L6; }
    r=0;
    goto L6; // free all that stuff
}

int toku_gpma_valididx (GPMA pma, u_int32_t idx) {
    return (idx<pma->N) && pma->items[idx].data;
}

int toku_gpma_get_from_index(GPMA pma, u_int32_t idx, u_int32_t *len, void **data) {
    if (idx>=pma->N) return EINVAL;
    void *d=pma->items[idx].data;
    if (d==0) return DB_NOTFOUND;
    *data=d;
    *len =pma->items[idx].len;
    return 0;
}

void toku_gpma_set_at_index (GPMA pma, u_int32_t idx, u_int32_t len, void *data) {
    assert(idx<pma->N);
    if (pma->items[idx].data==0)
	pma->n_items_present++;
    pma->items[idx].data=data;
    pma->items[idx].len =len;
}

void toku_gpma_clear_at_index (GPMA pma, u_int32_t idx) {
    assert(idx<pma->N);
    if (pma->items[idx].data==0) {
	pma->n_items_present--;
    }
    pma->items[idx].data = 0;
}

void toku_verify_gpma (GPMA pma) {
    // The only thing we can really verify is that the n_items_present is OK.
    u_int32_t i;
    u_int32_t count=0;
    for (i=0; i<pma->N; i++) {
	if (pma->items[i].data) count++;
    }
    assert(count==pma->n_items_present);
#if 0
    // We can also check that the lengths match up, but that's really brt-specific.
    for (i=0; i<pma->N; i++) {
	if (pma->items[i].data) {
	    struct foo {unsigned int a,b;} *foop = pma->items[i].data;
	    assert(sizeof(*foop)+foop->a+foop->b==pma->items[i].len);
	}
    }
#endif
}

int toku_resize_gpma_exactly (GPMA pma, u_int32_t newsize) {
    void *old = pma->items;
    REALLOC_N(newsize, pma->items);
    if (pma->items==0) {
	pma->items = old;
	return errno;
    }
    u_int32_t i;
    for (i=pma->N; i<newsize; i++) pma->items[i].data=0;
    pma->N = newsize;
    return 0;
}
