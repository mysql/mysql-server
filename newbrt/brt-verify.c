/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


/* Verify a BRT. */
/* Check:
 *   the fingerprint of every node (local check)
 *   the child's fingerprint matches the parent's copy
 *   the tree is of uniform depth (and the height is correct at every node)
 *   For non-dup trees: the values to the left are < the values to the right
 *      and < the pivot
 *   For dup trees: the values to the left are <= the values to the right
 *     the pivots are < or <= left values (according to the PresentL bit)
 *     the pivots are > or >= right values (according to the PresentR bit)
 *
 * Note: We don't yet have DUP trees, so thee checks on duplicate trees are unimplemented. (Nov 1 2007)
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

// all this because we dont have nested functions
struct verify_pair_arg {
    BRT brt;
    int i;
    bytevec thislorange;
    ITEMLEN thislolen;
    bytevec thishirange;
    ITEMLEN thishilen;
    int *resultp;
};

struct check_increasing_arg {
    BRT brt;
    LEAFENTRY prev;
};

// Make sure that they are in increasing order.
static int check_increasing (OMTVALUE lev, u_int32_t idx, void *arg) {
    struct check_increasing_arg *ciarg = (struct check_increasing_arg *)arg;
    LEAFENTRY v=lev;
    LEAFENTRY prev = ciarg->prev;
    if (idx>0)
        assert(compare_leafentries(ciarg->brt, prev, v)<0);
    ciarg->prev=v;
    return 0;
}

int toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r;
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    if ((r = toku_cachetable_get_and_pin(brt->cf, blocknum, fullhash, &node_v, NULL,
					 toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h)))
	return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    assert(node->fullhash==fullhash);
    verify_local_fingerprint(node);
    if (node->height>0) {
	int i;
	for (i=0; i< node->u.n.n_children; i++) {
	    bytevec thislorange,thishirange;
	    ITEMLEN thislolen,  thishilen;
	    if (node->u.n.n_children==0 || i==0) {
		thislorange=lorange;
		thislolen  =lolen;
	    } else {
		thislorange=kv_pair_key(node->u.n.childkeys[i-1]);
		thislolen  =toku_brt_pivot_key_len(node->u.n.childkeys[i-1]);
	    }
	    if (node->u.n.n_children==0 || i+1>=node->u.n.n_children) {
		thishirange=hirange;
		thishilen  =hilen;
	    } else {
		thishirange=kv_pair_key(node->u.n.childkeys[i]);
		thishilen  =toku_brt_pivot_key_len(node->u.n.childkeys[i]);
	    }
	}
	//if (lorange) printf("%s:%d lorange=%s\n", __FILE__, __LINE__, (char*)lorange);
	//if (hirange) printf("%s:%d lorange=%s\n", __FILE__, __LINE__, (char*)hirange);
	for (i=0; i<node->u.n.n_children-2; i++) {
	    assert(compare_pairs(brt, node->u.n.childkeys[i], node->u.n.childkeys[i+1])<0);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (i>0) {
		//printf(" %s:%d i=%d %p v=%s\n", __FILE__, __LINE__, i, node->u.n.childkeys[i-1], (char*)kv_pair_key(node->u.n.childkeys[i-1]));
		DBT k1,k2,k3;
		toku_fill_dbt(&k2, kv_pair_key(node->u.n.childkeys[i-1]), toku_brt_pivot_key_len(node->u.n.childkeys[i-1]));
		if (lorange) assert(brt->compare_fun(brt->db, toku_fill_dbt(&k1, lorange, lolen), &k2) <0);
		if (hirange) assert(brt->compare_fun(brt->db, &k2, toku_fill_dbt(&k3, hirange, hilen)) <=0);
	    }
	    if (recurse) {
		result|=toku_verify_brtnode(brt, BNC_BLOCKNUM(node, i),
                                            (i==0) ? lorange : kv_pair_key(node->u.n.childkeys[i-1]),
                                            (i==0) ? lolen   : toku_brt_pivot_key_len(node->u.n.childkeys[i-1]),
                                            (i==node->u.n.n_children-1) ? hirange : kv_pair_key(node->u.n.childkeys[i]),
                                            (i==node->u.n.n_children-1) ? hilen   : toku_brt_pivot_key_len(node->u.n.childkeys[i]),
                                            recurse);
	    }
	}
    } else {
        struct check_increasing_arg ciarg = { brt , 0 };
	toku_omt_iterate(node->u.l.buffer, check_increasing, &ciarg);
    }
    if ((r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, 0))) return r;
    return result;
}

int toku_verify_brt (BRT brt) {
    CACHEKEY *rootp;
    assert(brt->h);
    u_int32_t root_hash;
    rootp = toku_calculate_root_offset_pointer(brt, &root_hash);
    int n_pinned = toku_cachefile_count_pinned(brt->cf, 0);
    int r = toku_verify_brtnode(brt, *rootp, 0, 0, 0, 0, 1);
    assert(n_pinned ==  toku_cachefile_count_pinned(brt->cf, 0));
    return r;
}
