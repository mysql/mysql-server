/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Verify a BRT. */
/* Check:
 *   The fingerprint of every node (local check)
 *   The child's fingerprint matches the parent's copy (probably don't actually do thi syet)
 *   The tree is of uniform depth (and the height is correct at every node)
 *   For each pivot key:  the max of the stuff to the left is <= the pivot key < the min of the stuff to the right.
 *   For each leaf node:  All the keys are in strictly increasing order.
 *   For each nonleaf node:  All the messages have keys that are between the associated pivot keys ( left_pivot_key < message <= right_pivot_key)
 */

#include "includes.h"

static int verify_local_fingerprint (BRTNODE node) __attribute__ ((warn_unused_result));

static int verify_local_fingerprint (BRTNODE node) {
    u_int32_t fp=0;
    int i;
    int r = 0;
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++)
	    FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
			      {
				  fp += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
			      });
	if (fp!=node->local_fingerprint) {
	    fprintf(stderr, "%s:%d local fingerprints don't match\n", __FILE__, __LINE__);
	    r = -200001;
	}
    } else {
	toku_verify_or_set_counts(node, FALSE);
    }
    return r;
}

static int compare_pairs (BRT brt, struct kv_pair *a, struct kv_pair *b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, kv_pair_key(b), kv_pair_keylen(b)));
    return cmp;
}
static int compare_leafentries (BRT brt, LEAFENTRY a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, le_key(a), le_keylen(a)),
			       toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}
static int compare_pair_to_leafentry (BRT brt, struct kv_pair *a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}

int toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, int height,
			 struct kv_pair *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
			 struct kv_pair *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
			 int recurse)
{
    int result=0;
    BRTNODE node;
    void *node_v;
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    {
	int r = toku_cachetable_get_and_pin(brt->cf, blocknum, fullhash, &node_v, NULL,
					    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
	if (r) return r;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    if (node->fullhash!=fullhash) {
	fprintf(stderr, "%s:%d fullhash does not match\n", __FILE__, __LINE__);
	return -200001;
    }
    if (height==-1) {
	height = node->height;
    }
    if (node->height  !=height) {
	fprintf(stderr, "%s:%d node->height=%d height=%d\n", __FILE__, __LINE__, node->height, height);
	return -200001;
    }
    {
	int r = verify_local_fingerprint(node);
	if (r) result=r;
    }
    if (node->height>0) {
	// Verify that all the pivot keys are in order.
	for (int i=0; i<node->u.n.n_children-2; i++) {
	    int compare = compare_pairs(brt, node->u.n.childkeys[i], node->u.n.childkeys[i+1]);
	    if (compare>=0) {
		fprintf(stderr, "%s:%d The %dth value is >= the %dth value in block %" PRId64 "\n", __FILE__, __LINE__,
			i, i+1, blocknum.b);
		result = -200001;
	    }
	}
	// Verify that all the pivot keys are lesser_pivot < pivot <= greatereq_pivot
	for (int i=0; i<node->u.n.n_children-1; i++) {
	    if (lesser_pivot) {
		int compare = compare_pairs(brt, lesser_pivot, node->u.n.childkeys[i]);
		if (compare>=0) {
		    fprintf(stderr, "%s:%d The %dth pivot is >= the previous in block %" PRId64 "\n", __FILE__, __LINE__,
			    i, blocknum.b);
		    result = -200001;
		}
	    }
	    if (greatereq_pivot) {
		int compare = compare_pairs(brt, greatereq_pivot, node->u.n.childkeys[i]);
		if (compare < 0) {
		    fprintf(stderr, "%s:%d The %dth pivot is < the next in block %" PRId64 "\n", __FILE__, __LINE__,
			    i, blocknum.b);
		    result = -200001;
		}
	    }
	}

	// Verify that messages in the buffers are in the right place.
	{/*nothing*/} // To do later.
	
	// Verify that the subtrees have the right properties.
	if (recurse) {
	    for (int i=0; i<node->u.n.n_children; i++) {
		int r = toku_verify_brtnode(brt, BNC_BLOCKNUM(node, i), height-1,
					    (i==0)                      ? lesser_pivot        : node->u.n.childkeys[i-1],
					    (i==node->u.n.n_children-1) ? greatereq_pivot     : node->u.n.childkeys[i],
					    recurse);
		if (r) result=r;
	    }
	}
    } else {
	/* It's a leaf.  Make sure every leaf value is between the pivots, and that the leaf values are sorted. */
	for (u_int32_t i=0; i<toku_omt_size(node->u.l.buffer); i++) {
	    OMTVALUE le_v;
	    {
		int r = toku_omt_fetch(node->u.l.buffer, i, &le_v, NULL);
		if (r) {
		    fprintf(stderr, "%s:%d Could not fetch value from OMT, r=%d\n", __FILE__, __LINE__, r);
		    result = r;
		}
	    }
	    LEAFENTRY le = le_v;

	    if (lesser_pivot) {
		int compare = compare_pair_to_leafentry(brt, lesser_pivot, le);
		if (compare>=0) {
		    fprintf(stderr, "%s:%d The %dth leafentry key is >= the previous pivot in block %" PRId64 "\n", __FILE__, __LINE__,
			    i, blocknum.b);
		    result = -200001;
		}
	    }
	    if (greatereq_pivot) {
		int compare = compare_pair_to_leafentry(brt, greatereq_pivot, le);
		if (compare<0) {
		    fprintf(stderr, "%s:%d The %dth leafentry key is < the next pivot in block %" PRId64 "\n", __FILE__, __LINE__,
			    i, blocknum.b);
		    result = -200001;
		}

	    }
	    if (0<i) {
		OMTVALUE prev_le_v;
		int r = toku_omt_fetch(node->u.l.buffer, i-1, &prev_le_v, NULL);
		assert(r==0);
		LEAFENTRY prev_le = prev_le_v;
		int compare = compare_leafentries(brt, prev_le, le);
		if (compare>=0) {
		    fprintf(stderr, "%s:%d The %dth leafentry key is >= the previous leafentry block %" PRId64 "\n", __FILE__, __LINE__,
			    i, blocknum.b);
		    result = -200001;
		}
	    }
	}
    }
    {
	int r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, 0);
	if (r) {
	    fprintf(stderr, "%s:%d could not unpin\n", __FILE__, __LINE__);
	    result = r;
	}
    }
    return result;
}

int toku_verify_brt (BRT brt) {
    CACHEKEY *rootp;
    assert(brt->h);
    u_int32_t root_hash;
    rootp = toku_calculate_root_offset_pointer(brt, &root_hash);
    int n_pinned_before = toku_cachefile_count_pinned(brt->cf, 0);
    int r = toku_verify_brtnode(brt, *rootp, -1, NULL, NULL, 1);
    int n_pinned_after  = toku_cachefile_count_pinned(brt->cf, 0);
    if (n_pinned_before!=n_pinned_after) {// this may stop working if we release the ydb lock (in some future version of the code).
	fprintf(stderr, "%s:%d n_pinned_before=%d n_pinned_after=%d\n", __FILE__, __LINE__, n_pinned_before, n_pinned_after);
    }
    return r;
}

