/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

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

#include "brt-internal.h"
#include "toku_assert.h"
#include "kv-pair.h"

static void verify_local_fingerprint (BRTNODE node) {
    u_int32_t fp=0;
    int i;
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++)
	    FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
			      ({
				  fp += node->rand4fingerprint * toku_calccrc32_cmd(type, xid, key, keylen, data, datalen);
			      }));
	assert(fp==node->local_fingerprint);
    } else {
	toku_verify_counts(node);
    }
}

static int compare_pairs (BRT brt, struct kv_pair *a, struct kv_pair *b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
			       toku_fill_dbt(&y, kv_pair_key(b), kv_pair_keylen(b)));
    if (cmp==0 && (brt->flags & TOKU_DB_DUPSORT)) {
	cmp = brt->dup_compare(brt->db,
			       toku_fill_dbt(&x, kv_pair_val(a), kv_pair_vallen(a)),
			       toku_fill_dbt(&y, kv_pair_val(b), kv_pair_vallen(b)));
    }
    return cmp;
}
static int compare_leafentries (BRT brt, LEAFENTRY a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
			       toku_fill_dbt(&x, le_any_key(a), le_any_keylen(a)),
			       toku_fill_dbt(&y, le_any_key(b), le_any_keylen(b)));
    if (cmp==0 && (brt->flags & TOKU_DB_DUPSORT)) {
	cmp = brt->dup_compare(brt->db,
			       toku_fill_dbt(&x, le_any_val(a), le_any_vallen(a)),
			       toku_fill_dbt(&y, le_any_val(b), le_any_vallen(b)));
    }
    return cmp;
}

int toku_verify_brtnode (BRT brt, DISKOFF off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r;
    if ((r = toku_cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
					 toku_brtnode_flush_callback, toku_brtnode_fetch_callback, (void*)(long)brt->h->nodesize)))
	return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
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
		thislolen  =toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]);
	    }
	    if (node->u.n.n_children==0 || i+1>=node->u.n.n_children) {
		thishirange=hirange;
		thishilen  =hilen;
	    } else {
		thishirange=kv_pair_key(node->u.n.childkeys[i]);
		thishilen  =toku_brt_pivot_key_len(brt, node->u.n.childkeys[i]);
	    }
	    {
		void verify_pair (bytevec key, unsigned int keylen,
				  bytevec data __attribute__((__unused__)), 
                                  unsigned int datalen __attribute__((__unused__)),
                                  int type __attribute__((__unused__)),
				  TXNID xid __attribute__((__unused__)),
				  void *ignore __attribute__((__unused__))) {
		    if (thislorange) assert(toku_keycompare(thislorange,thislolen,key,keylen)<0);
		    if (thishirange && toku_keycompare(key,keylen,thishirange,thishilen)>0) {
			printf("%s:%d in buffer %d key %s is bigger than %s\n", __FILE__, __LINE__, i, (char*)key, (char*)thishirange);
			result=1;
		    }
		}
		toku_fifo_iterate(BNC_BUFFER(node,i), verify_pair, 0);
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
		if (lorange) assert(toku_keycompare(lorange,lolen, kv_pair_key(node->u.n.childkeys[i-1]), toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]))<0);
		if (hirange) assert(toku_keycompare(kv_pair_key(node->u.n.childkeys[i-1]), toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]), hirange, hilen)<=0);
	    }
	    if (recurse) {
		result|=toku_verify_brtnode(brt, BNC_DISKOFF(node, i),
                                            (i==0) ? lorange : kv_pair_key(node->u.n.childkeys[i-1]),
                                            (i==0) ? lolen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]),
                                            (i==node->u.n.n_children-1) ? hirange : kv_pair_key(node->u.n.childkeys[i]),
                                            (i==node->u.n.n_children-1) ? hilen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i]),
                                            recurse);
	    }
	}
    } else {
	// Make sure that they are in increasing order.
	void *prev=0;
	GPMA_ITERATE(node->u.l.buffer, idx, dlen, data,
		     ({
			 if (prev==0)
			     prev=data;
			 else
			     assert(compare_leafentries(brt, prev, data)<0);
		     }));
    }
    if ((r = toku_cachetable_unpin(brt->cf, off, 0, 0))) return r;
    return result;
}

int toku_verify_brt (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    if ((r=toku_verify_brtnode(brt, *rootp, 0, 0, 0, 0, 1))) goto died0;
    if ((r = toku_unpin_brt_header(brt))!=0) return r;
    return 0;
}
