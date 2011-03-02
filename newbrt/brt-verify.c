/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Verify a BRT. */
/* Check:
 *   The tree is of uniform depth (and the height is correct at every node)
 *   For each pivot key:  the max of the stuff to the left is <= the pivot key < the min of the stuff to the right.
 *   For each leaf node:  All the keys are in strictly increasing order.
 *   For each nonleaf node:  All the messages have keys that are between the associated pivot keys ( left_pivot_key < message <= right_pivot_key)
 */

#include "includes.h"

static int 
compare_pairs (BRT brt, struct kv_pair *a, struct kv_pair *b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, kv_pair_key(b), kv_pair_keylen(b)));
    return cmp;
}

static int 
compare_leafentries (BRT brt, LEAFENTRY a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, le_key(a), le_keylen(a)),
			       toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}

static int 
compare_pair_to_leafentry (BRT brt, struct kv_pair *a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}

static int 
compare_pair_to_key (BRT brt, struct kv_pair *a, bytevec key, ITEMLEN keylen) {
    DBT x, y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, key, keylen));
    return cmp;
}

static int
verify_msg_in_child_buffer(BRT brt, int type, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot)
    __attribute__((warn_unused_result));

static int
verify_msg_in_child_buffer(BRT brt, int type, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot) {
    int result = 0;
    switch (type) {
    case BRT_INSERT:
    case BRT_INSERT_NO_OVERWRITE:
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
        // verify key in bounds
        if (lesser_pivot) {
            int compare = compare_pair_to_key(brt, lesser_pivot, key, keylen);
            if (compare >= 0)
                result = EINVAL;
        }
        if (result == 0 && greatereq_pivot) {
            int compare = compare_pair_to_key(brt, greatereq_pivot, key, keylen);
            if (compare < 0)
                result = EINVAL;
        }
        break;
    }
    return result;
}

static LEAFENTRY 
get_ith_leafentry (BRTNODE node, int i) {
    OMTVALUE le_v;
    int r = toku_omt_fetch(node->u.l.buffer, i, &le_v, NULL);
    invariant(r == 0); // this is a bad failure if it happens.
    return (LEAFENTRY)le_v;
}

#define VERIFY_ASSERTION(predicate, i, string) ({                                                                              \
    if(!(predicate)) {                                                                                                         \
	if (verbose) {                                                                                                         \
	    fprintf(stderr, "%s:%d: Looking at child %d of block %" PRId64 ": %s\n", __FILE__, __LINE__, i, blocknum.b, string); \
	}                                                                                                                      \
	result = TOKUDB_NEEDS_REPAIR;                                                                                          \
	if (!keep_going_on_failure) goto done;                                                                                 \
    }})

int 
toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, int height,
                     struct kv_pair *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     struct kv_pair *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     int (*progress_callback)(void *extra, float progress), void *progress_extra,
                     int recurse, int verbose, int keep_going_on_failure)
{
    int result=0;
    BRTNODE node;
    void *node_v;
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    {
	int r = toku_cachetable_get_and_pin(brt->cf, blocknum, fullhash, &node_v, NULL,
					    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
	assert_zero(r); // this is a bad failure if it happens.
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node = node_v;
    assert(node->fullhash == fullhash);   // this is a bad failure if wrong
    if (height >= 0) 
        invariant(height == node->height);   // this is a bad failure if wrong
    if (node->height > 0) {
	// Verify that all the pivot keys are in order.
	for (int i = 0; i < node->u.n.n_children-2; i++) {
	    int compare = compare_pairs(brt, node->u.n.childkeys[i], node->u.n.childkeys[i+1]);
	    VERIFY_ASSERTION(compare < 0, i, "Value is >= the next value");
	}
	// Verify that all the pivot keys are lesser_pivot < pivot <= greatereq_pivot
	for (int i = 0; i < node->u.n.n_children-1; i++) {
	    if (lesser_pivot) {
		int compare = compare_pairs(brt, lesser_pivot, node->u.n.childkeys[i]);
		VERIFY_ASSERTION(compare < 0, i, "Pivot is >= the lower-bound pivot");
	    }
	    if (greatereq_pivot) {
		int compare = compare_pairs(brt, greatereq_pivot, node->u.n.childkeys[i]);
		VERIFY_ASSERTION(compare >= 0, i, "Pivot is < the upper-bound pivot");
	    }
	}

	// Verify that messages in the buffers are in the right place.
	for (int i = 0; i < node->u.n.n_children; i++) {
	    FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
			 { 
			     int r = verify_msg_in_child_buffer(brt, type, key, keylen, data, datalen, xid, 
								(i==0)                      ? lesser_pivot        : node->u.n.childkeys[i-1],
								(i==node->u.n.n_children-1) ? greatereq_pivot     : node->u.n.childkeys[i]);
			     VERIFY_ASSERTION(r==0, i, "A message in the buffer is out of place");
			 });
        } 
	
	// Verify that the subtrees have the right properties.
	if (recurse) {
	    for (int i = 0; i < node->u.n.n_children; i++) {
		int r = toku_verify_brtnode(brt, BNC_BLOCKNUM(node, i), node->height-1,
					    (i==0)                      ? lesser_pivot        : node->u.n.childkeys[i-1],
					    (i==node->u.n.n_children-1) ? greatereq_pivot     : node->u.n.childkeys[i],
                                            progress_callback, progress_extra,
					    recurse, verbose, keep_going_on_failure);
		if (r) {
		    result = r;
		    if (!keep_going_on_failure || result != TOKUDB_NEEDS_REPAIR) goto done;
		}
	    }
	}
    } else {
	/* It's a leaf.  Make sure every leaf value is between the pivots, and that the leaf values are sorted. */
	for (u_int32_t i = 0; i < toku_omt_size(node->u.l.buffer); i++) {
	    LEAFENTRY le = get_ith_leafentry(node, i);
	    if (lesser_pivot) {
		int compare = compare_pair_to_leafentry(brt, lesser_pivot, le);
		VERIFY_ASSERTION(compare < 0, i, "The leafentry is >= the lower-bound pivot");
	    }
	    if (greatereq_pivot) {
		int compare = compare_pair_to_leafentry(brt, greatereq_pivot, le);
		VERIFY_ASSERTION(compare >= 0, i, "The leafentry is < the upper-bound pivot");
	    }
	    if (0 < i) {
		LEAFENTRY prev_le = get_ith_leafentry(node, i-1);
		int compare = compare_leafentries(brt, prev_le, le);
		VERIFY_ASSERTION(compare < 0, i, "Adjacent leafentries are out of order");
	    }
	}
    }
 done:
    {
	int r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, 0);
	assert_zero(r); // this is a bad failure if it happens.
    }
    
    if (result == 0 && progress_callback) 
        result = progress_callback(progress_extra, 0.0);

    return result;
}

int 
toku_verify_brt_with_progress (BRT brt, int (*progress_callback)(void *extra, float progress), void *progress_extra, int verbose, int keep_on_going) {
    assert(brt->h);
    u_int32_t root_hash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &root_hash);
    int n_pinned_before = toku_cachefile_count_pinned(brt->cf, 0);
    int r = toku_verify_brtnode(brt, *rootp, -1, NULL, NULL, progress_callback, progress_extra, 1, verbose, keep_on_going);
    int n_pinned_after  = toku_cachefile_count_pinned(brt->cf, 0);
    if (n_pinned_before!=n_pinned_after) {// this may stop working if we release the ydb lock (in some future version of the code).
	fprintf(stderr, "%s:%d n_pinned_before=%d n_pinned_after=%d\n", __FILE__, __LINE__, n_pinned_before, n_pinned_after);
    }
    return r;
}

int 
toku_verify_brt (BRT brt) {
    return toku_verify_brt_with_progress(brt, NULL, NULL, 0, 0);
}

