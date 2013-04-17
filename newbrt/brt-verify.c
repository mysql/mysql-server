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

static void verify_local_fingerprint (BRTNODE node) {
    u_int32_t fp=0;
    int i;
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++)
	    FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
			      {
				  fp += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
			      });
	assert(fp==node->local_fingerprint);
    } else {
	toku_verify_or_set_counts(node, FALSE);
    }
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
    assert(node->fullhash==fullhash);
    if (height==-1) {
	height = node->height;
    }
    assert(node->height  ==height);
    verify_local_fingerprint(node);
    if (node->height>0) {
	// Verify that all the pivot keys are in order.
	for (int i=0; i<node->u.n.n_children-2; i++) {
	    int compare = compare_pairs(brt, node->u.n.childkeys[i], node->u.n.childkeys[i+1]);
	    assert(compare<0);
	}
	// Verify that all the pivot keys are lesser_pivot < pivot <= greatereq_pivot
	for (int i=0; i<node->u.n.n_children-1; i++) {
	    if (lesser_pivot) {
		int compare = compare_pairs(brt, lesser_pivot, node->u.n.childkeys[i]);
		assert(compare < 0);
	    }
	    if (greatereq_pivot) {
		int compare = compare_pairs(brt, greatereq_pivot, node->u.n.childkeys[i]);
		assert(compare >= 0);
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
		assert(r==0);
	    }
	}
    } else {
	/* It's a leaf.  Make sure every leaf value is between the pivots, and that the leaf values are sorted. */
	for (u_int32_t i=0; i<toku_omt_size(node->u.l.buffer); i++) {
	    OMTVALUE le_v;
	    {
		int r = toku_omt_fetch(node->u.l.buffer, i, &le_v, NULL);
		assert(r==0);
	    }
	    LEAFENTRY le = le_v;

	    if (lesser_pivot) {
		int compare = compare_pair_to_leafentry(brt, lesser_pivot, le);
		assert(compare < 0);
	    }
	    if (greatereq_pivot) {
		int compare = compare_pair_to_leafentry(brt, greatereq_pivot, le);
		assert(compare >= 0);
	    }
	    if (0<i) {
		OMTVALUE prev_le_v;
		int r = toku_omt_fetch(node->u.l.buffer, i-1, &prev_le_v, NULL);
		assert(r==0);
		LEAFENTRY prev_le = prev_le_v;
		int compare = compare_leafentries(brt, prev_le, le);
		assert(compare<0);
	    }
	}
    }
    {
	int r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, 0);
	if (r) return r;
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
    assert(n_pinned_before==n_pinned_after); // this may stop working if we release the ydb lock (in some future version of the code).
    return r;
}

