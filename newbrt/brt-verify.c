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

#include <assert.h>

static void verify_local_fingerprint (BRTNODE node) {
    u_int32_t fp=0;
    int i;
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++)
	    HASHTABLE_ITERATE(node->u.n.htables[i], key, keylen, data, datalen, type,
			      ({
				  fp += node->rand4fingerprint * toku_calccrc32_cmd(type, key, keylen, data, datalen);
			      }));
	assert(fp==node->local_fingerprint);
    } else {
	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->local_fingerprint);
    }
}

static void verify_parent_fingerprint (BRTNODE node) {
    BRTNODE parent=node->parent_brtnode;
    u_int32_t subtree_fingerprint=node->local_fingerprint;
    if (node->height>0) {
	int i;
	for (i=0; i<node->u.n.n_children; i++) {
	    subtree_fingerprint+=node->u.n.child_subtree_fingerprints[i];
	}
    }
    if (parent) {
	int i;
	assert(parent->height>0);
	for (i=0; i<parent->u.n.n_children; i++) {
	    if (parent->u.n.children[i]==node->thisnodename) {
		assert(parent->u.n.child_subtree_fingerprints[i]==subtree_fingerprint);
		return;
	    }
	}
	assert(0); // no parent matches
    }
}

int toku_verify_brtnode (BRT brt, DISKOFF off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse, BRTNODE parent_brtnode) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r;
    if ((r = toku_cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
					 brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize)))
	return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    node->parent_brtnode = parent_brtnode;
    verify_local_fingerprint(node);
    verify_parent_fingerprint(node);
    if (node->height>0) {
	int i;
	for (i=0; i< node->u.n.n_children-1; i++) {
	    bytevec thislorange,thishirange;
	    ITEMLEN thislolen,  thishilen;
	    if (node->u.n.n_children==0 || i==0) {
		thislorange=lorange;
		thislolen  =lolen;
	    } else {
		thislorange=kv_pair_key(node->u.n.childkeys[i-1]);
		thislolen  =node->u.n.childkeylens[i-1];
	    }
	    if (node->u.n.n_children==0 || i+1>=node->u.n.n_children) {
		thishirange=hirange;
		thishilen  =hilen;
	    } else {
		thishirange=kv_pair_key(node->u.n.childkeys[i]);
		thishilen  =node->u.n.childkeylens[i];
	    }
	    {
		void verify_pair (bytevec key, unsigned int keylen,
				  bytevec data __attribute__((__unused__)), 
                                  unsigned int datalen __attribute__((__unused__)),
                                  int type __attribute__((__unused__)),
				  void *ignore __attribute__((__unused__))) {
		    if (thislorange) assert(toku_keycompare(thislorange,thislolen,key,keylen)<0);
		    if (thishirange && toku_keycompare(key,keylen,thishirange,thishilen)>0) {
			printf("%s:%d in buffer %d key %s is bigger than %s\n", __FILE__, __LINE__, i, (char*)key, (char*)thishirange);
			result=1;
		    }
		}
		toku_hashtable_iterate(node->u.n.htables[i], verify_pair, 0);
	    }
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (i>0) {
		if (lorange) assert(toku_keycompare(lorange,lolen, kv_pair_key(node->u.n.childkeys[i-1]), node->u.n.childkeylens[i-1])<0);
		if (hirange) assert(toku_keycompare(kv_pair_key(node->u.n.childkeys[i-1]), node->u.n.childkeylens[i-1], hirange, hilen)<=0);
	    }
	    if (recurse) {
		result|=toku_verify_brtnode(brt, node->u.n.children[i],
                                            (i==0) ? lorange : kv_pair_key(node->u.n.childkeys[i-1]),
                                            (i==0) ? lolen   : node->u.n.childkeylens[i-1],
                                            (i==node->u.n.n_children-1) ? hirange : kv_pair_key(node->u.n.childkeys[i]),
                                            (i==node->u.n.n_children-1) ? hilen   : node->u.n.childkeylens[i],
                                            recurse,
                                            node);
	    }
	}
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
    if ((r=toku_verify_brtnode(brt, *rootp, 0, 0, 0, 0, 1, null_brtnode))) goto died0;
    if ((r = toku_unpin_brt_header(brt))!=0) return r;
    return 0;
}
