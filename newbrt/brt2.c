/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* 
 * We always write nodes to a new location on disk.
 * The nodes themselves contain the information about the tree structure.
 * Q: During recovery, how do we find the root node without looking at every block on disk?
 * A: The root node is either the designated root near the front of the freelist.
 *    The freelist is updated infrequently.  Before updating the stable copy of the freelist, we make sure that
 *    the root is up-to-date.  We can make the freelist-and-root update be an arbitrarily small fraction of disk bandwidth.
 *     
 */

#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "toku_assert.h"
#include "brt-internal.h"
#include "key.h"
#include "log_header.h"

typedef struct weakstrong { char ignore; } *WS;
#define WEAK ((WS)1)
#define STRONG ((WS)0)

extern long long n_items_malloced;

static int malloc_diskblock (DISKOFF *res, BRT brt, int size, TOKULOGGER);
static void verify_local_fingerprint_nonleaf (BRTNODE node);

#ifdef FOO

/* Frees a node, including all the stuff in the hash table. */
void toku_brtnode_free (BRTNODE *nodep) {
    BRTNODE node=*nodep;
    int i;
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, node, node->mdicts[0]);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children-1; i++) {
	    toku_free((void*)node->u.n.childkeys[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (BNC_BUFFER(node,i)) {
		toku_fifo_free(&BNC_BUFFER(node,i));
	    }
	}
    } else {
	if (node->u.l.buffer) // The buffer may have been freed already, in some cases.
	    toku_pma_free(&node->u.l.buffer);
    }
    toku_free(node);
    *nodep=0;
}

#endif
static long brtnode_size(BRTNODE node) {
    return toku_serialize_brtnode_size(node);
}

static void toku_update_brtnode_loggerlsn(BRTNODE node, TOKULOGGER logger) {
    if (logger) {
	node->log_lsn = toku_logger_last_lsn(logger);
    }
}

static void fixup_child_fingerprint(BRTNODE node, int childnum_of_node, BRTNODE child, BRT brt, TOKULOGGER logger) {
    u_int32_t old_fingerprint = BNC_SUBTREE_FINGERPRINT(node,childnum_of_node);
    u_int32_t sum = child->local_fingerprint;
    if (child->height>0) {
	int i;
	for (i=0; i<child->u.n.n_children; i++) {
	    sum += BNC_SUBTREE_FINGERPRINT(child,i);
	}
    }
    // Don't try to get fancy about not modifying the fingerprint if it didn't change.
    // We only call this function if we have reason to believe that the child's fingerprint did change.
    BNC_SUBTREE_FINGERPRINT(node,childnum_of_node)=sum;
    node->dirty=1;
    toku_log_changechildfingerprint(logger, toku_cachefile_filenum(brt->cf), node->thisnodename, childnum_of_node, old_fingerprint, sum);
    toku_update_brtnode_loggerlsn(node, logger);
}

// If you pass in data==0 then it only compares the key, not the data (even if is a DUPSORT database)
static int brt_compare_pivot(BRT brt, DBT *key, DBT *data, bytevec ck) {
    int cmp;
    DBT mydbt;
    struct kv_pair *kv = (struct kv_pair *) ck;
    if (brt->flags & TOKU_DB_DUPSORT) {
        cmp = brt->compare_fun(brt->db, key, toku_fill_dbt(&mydbt, kv_pair_key(kv), kv_pair_keylen(kv)));
        if (cmp == 0 && data != 0)
            cmp = brt->dup_compare(brt->db, data, toku_fill_dbt(&mydbt, kv_pair_val(kv), kv_pair_vallen(kv)));
    } else {
        cmp = brt->compare_fun(brt->db, key, toku_fill_dbt(&mydbt, kv_pair_key(kv), kv_pair_keylen(kv)));
    }
    return cmp;
}

#ifdef FOO

void toku_brtnode_flush_callback (CACHEFILE cachefile, DISKOFF nodename, void *brtnode_v, long size __attribute((unused)), BOOL write_me, BOOL keep_me, LSN modified_lsn __attribute__((__unused__)) , BOOL rename_p __attribute__((__unused__))) {
    BRTNODE brtnode = brtnode_v;
    if (0) {
	printf("%s:%d toku_brtnode_flush_callback %p keep_me=%d height=%d", __FILE__, __LINE__, brtnode, keep_me, brtnode->height);
	if (brtnode->height==0) printf(" pma=%p", brtnode->u.l.buffer);
	printf("\n");
    }
    assert(brtnode->thisnodename==nodename);
    if (write_me) {
	toku_serialize_brtnode_to(toku_cachefile_fd(cachefile), brtnode->thisnodename, brtnode->nodesize, brtnode);
    }
    if (!keep_me) {
	toku_brtnode_free(&brtnode);
    }
}

int toku_brtnode_fetch_callback (CACHEFILE cachefile, DISKOFF nodename, void **brtnode_pv, long *sizep, void*extraargs, LSN *written_lsn) {
    BRT t =(BRT)extraargs;
    BRTNODE *result=(BRTNODE*)brtnode_pv;
    int r = toku_deserialize_brtnode_from(toku_cachefile_fd(cachefile), nodename, result, t->flags, t->nodesize, 
					  t->compare_fun, t->dup_compare, t->db, toku_cachefile_filenum(t->cf));
    if (r == 0) {
        *sizep = brtnode_size(*result);
	*written_lsn = (*result)->disk_lsn;
    }
    return r;
}
	
void toku_brtheader_flush_callback (CACHEFILE cachefile, DISKOFF nodename, void *header_v, long size __attribute((unused)), BOOL write_me, BOOL keep_me, LSN lsn __attribute__((__unused__)), BOOL rename_p __attribute__((__unused__))) {
    struct brt_header *h = header_v;
    assert(nodename==0);
    assert(!h->dirty); // shouldn't be dirty once it is unpinned.
    if (write_me) {
	toku_serialize_brt_header_to(toku_cachefile_fd(cachefile), h);
    }
    if (!keep_me) {
	if (h->n_named_roots>0) {
	    int i;
	    for (i=0; i<h->n_named_roots; i++) {
		toku_free(h->names[i]);
	    }
	    toku_free(h->names);
	    toku_free(h->roots);
	}
	toku_free(h);
    }
}

int toku_brtheader_fetch_callback (CACHEFILE cachefile, DISKOFF nodename, void **headerp_v, long *sizep __attribute__((unused)), void*extraargs __attribute__((__unused__)), LSN *written_lsn) {
    struct brt_header **h = (struct brt_header **)headerp_v;
    assert(nodename==0);
    int r = toku_deserialize_brtheader_from(toku_cachefile_fd(cachefile), nodename, h);
    written_lsn->lsn = 0; // !!! WRONG.  This should be stored or kept redundantly or something.
    return r;
}

int toku_read_and_pin_brt_header (CACHEFILE cf, struct brt_header **header) {
    void *header_p;
    //fprintf(stderr, "%s:%d read_and_pin_brt_header(...)\n", __FILE__, __LINE__);
    int r = toku_cachetable_get_and_pin(cf, 0, &header_p, NULL,
				   toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    if (r!=0) return r;
    *header = header_p;
    return 0;
}

int toku_unpin_brt_header (BRT brt) {
    int r = toku_cachetable_unpin(brt->cf, 0, brt->h->dirty, 0);
    brt->h->dirty=0;
    brt->h=0;
    return r;
}
#endif
static int unpin_brtnode (BRT brt, BRTNODE node) {
    return toku_cachetable_unpin(brt->cf, node->thisnodename, node->dirty, brtnode_size(node));
}
#ifdef FOO

typedef struct kvpair {
    bytevec key;
    unsigned int keylen;
    bytevec val;
    unsigned int vallen;
} *KVPAIR;

/* Forgot to handle the case where there is something in the freelist. */
static int malloc_diskblock_header_is_in_memory (DISKOFF *res, BRT brt, int size, TOKULOGGER logger) {
    DISKOFF result = brt->h->unused_memory;
    brt->h->unused_memory+=size;
    brt->h->dirty = 1;
    int r = toku_log_changeunusedmemory(logger, toku_cachefile_filenum(brt->cf), result, brt->h->unused_memory);
    *res = result;
    return r;
}

int malloc_diskblock (DISKOFF *res, BRT brt, int size, TOKULOGGER logger) {
#if 0
    int r = read_and_pin_brt_header(brt->fd, &brt->h);
    assert(r==0);
    {
	DISKOFF result = malloc_diskblock_header_is_in_memory(brt, size);
	r = write_brt_header(brt->fd, &brt->h);
	assert(r==0);
	return result;
    }
#else
    return malloc_diskblock_header_is_in_memory(res, brt,size, logger);
#endif
}

static void initialize_brtnode (BRT t, BRTNODE n, DISKOFF nodename, int height) {
    int i;
    n->tag = TYP_BRTNODE;
    n->nodesize = t->h->nodesize;
    n->flags = t->h->flags;
    n->thisnodename = nodename;
    n->disk_lsn.lsn = 0; // a new one can always be 0.
    n->log_lsn = n->disk_lsn;
    n->layout_version = 2;
    n->height       = height;
    n->rand4fingerprint = random();
    n->local_fingerprint = 0;
    n->dirty = 1;
    assert(height>=0);
    if (height>0) {
	n->u.n.n_children   = 0;
	for (i=0; i<TREE_FANOUT; i++) {
//	    n->u.n.childkeys[i] = 0;
//	    n->u.n.childkeylens[i] = 0;
	}
	n->u.n.totalchildkeylens = 0;
	for (i=0; i<TREE_FANOUT+1; i++) {
	    BNC_SUBTREE_FINGERPRINT(n, i) = 0;
//	    n->u.n.children[i] = 0;
//	    n->u.n.buffers[i] = 0;
	    BNC_NBYTESINBUF(n,i) = 0;
	}
	n->u.n.n_bytes_in_buffers = 0;
    } else {
	int r = toku_pma_create(&n->u.l.buffer, t->compare_fun, t->db, toku_cachefile_filenum(t->cf), n->nodesize);
        assert(r==0);
        toku_pma_set_dup_mode(n->u.l.buffer, t->flags & (TOKU_DB_DUP+TOKU_DB_DUPSORT));
        toku_pma_set_dup_compare(n->u.l.buffer, t->dup_compare);
	static int rcount=0;
	//printf("%s:%d n PMA= %p (rcount=%d)\n", __FILE__, __LINE__, n->u.l.buffer, rcount); 
	rcount++;
	n->u.l.n_bytes_in_buffer = 0;
    }
}

int toku_create_new_brtnode (BRT t, BRTNODE *result, int height, TOKULOGGER logger) {
    TAGMALLOC(BRTNODE, n);
    int r;
    DISKOFF name;
    if ((r = malloc_diskblock(&name, t, t->h->nodesize, logger))) return r;
    assert(n);
    assert(t->h->nodesize>0);
    //printf("%s:%d malloced %lld (and malloc again=%lld)\n", __FILE__, __LINE__, name, malloc_diskblock(t, t->nodesize));
    initialize_brtnode(t, n, name, height);
    *result = n;
    assert(n->nodesize>0);
    //    n->brt            = t;
    //printf("%s:%d putting %p (%lld) parent=%p\n", __FILE__, __LINE__, n, n->thisnodename, parent_brtnode);
    if ((r = toku_cachetable_put(t->cf, n->thisnodename, n, brtnode_size(n), toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t)))
	return r;
    if ((r = toku_log_newbrtnode(logger, toku_cachefile_filenum(t->cf), n->thisnodename, height, n->nodesize, (t->flags&TOKU_DB_DUPSORT)!=0, n->rand4fingerprint)))
	return r;
    toku_update_brtnode_loggerlsn(n, logger);
    return 0;
}

#ifdef FOO
static int insert_to_buffer_in_nonleaf (BRTNODE node, int childnum, BRT_CMD cmd) {
    unsigned int n_bytes_added = BRT_CMD_OVERHEAD + KEY_VALUE_OVERHEAD + cmd->u.id.key->size + cmd->u.id.val->size;
    int r = toku_fifo_enq_cmdstruct(BNC_BUFFER(node,childnum), cmd);
    if (r!=0) return r;
    node->local_fingerprint += node->rand4fingerprint*toku_calccrc32_cmdstruct(cmd);
    BNC_NBYTESINBUF(node,childnum) += n_bytes_added;
    node->u.n.n_bytes_in_buffers += n_bytes_added;
    node->dirty = 1;
    return 0;
}
#endif


// Split a leaf node, reusing it in new_nodes (as the last element)
static int split_leaf_node (BRT t, TOKULOGGER logger, BRTNODE node, int *n_new_nodes, BRTNODE **new_nodes, DBT **splitks) {
    assert(node->height==0);
    int r;
    int n_children=1; // Initially we have the node itself.
    BRTNODE *result_nodes=toku_malloc(sizeof(*result_nodes));
    if (errno!=0) { r=errno; if (0) { died0: toku_free(result_nodes); } return r; }
    DBT     *result_splitks=toku_malloc(sizeof(*result_splitks));
    if (errno!=0) { r=errno; if (0) { died1: toku_free(result_splitks); } goto died0; }
    while (toku_serialize_brtnode_size(node)>node->nodesize) {
	BRTNODE B;
	DBT splitk;
	if ((r = toku_create_new_brtnode(t, &B, 0, logger))) return r;
	// Split so that B is at least 1/2 full
	// The stuff in B goes *before* node
	if ((r = toku_pma_split(logger, toku_cachefile_filenum(t->cf),
				node->thisnodename, node->u.l.buffer, &node->u.l.n_bytes_in_buffer, node->rand4fingerprint, &node->local_fingerprint, &node->log_lsn,
				&splitk,
				B->thisnodename, B->u.l.buffer, &B->u.l.n_bytes_in_buffer, B->rand4fingerprint, &B->local_fingerprint, &B->log_lsn)))
	    goto died1;
	n_children++;
	result_nodes = toku_realloc(result_nodes, n_children*sizeof(*result_nodes));
	result_nodes[n_children-2] = B;
	result_splitks = toku_realloc(result_nodes, (n_children-1)*sizeof(*result_splitks));
	result_splitks[n_children-2] = splitk; 
    }
    result_nodes[n_children-1]=node;
    *n_new_nodes = n_children;
    *new_nodes = result_nodes;
    *splitks = result_splitks;
    return 0;
}

/* Side effect: sets splitk->data pointer to a malloc'd value */
static int brt_nonleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, TOKULOGGER logger) {
    int old_n_children = node->u.n.n_children;
    int n_children_in_a = old_n_children/2;
    int n_children_in_b = old_n_children-n_children_in_a;
    BRTNODE B;
    FILENUM fnum = toku_cachefile_filenum(t->cf);
    assert(node->height>0);
    assert(node->u.n.n_children>=2); // Otherwise, how do we split?  We need at least two children to split. */
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    toku_create_new_brtnode(t, &B, node->height, logger);
    B->u.n.n_children   =n_children_in_b;
    //printf("%s:%d %p (%lld) becomes %p and %p\n", __FILE__, __LINE__, node, node->thisnodename, A, B);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    {
	/* The first n_children_in_a go into node a.
	 * That means that the first n_children_in_a-1 keys go into node a.
	 * The splitter key is key number n_children_in_a */
	int i;

	for (i=0; i<n_children_in_b; i++) {
	    int r = toku_fifo_create(&BNC_BUFFER(B,i));
	    if (r!=0) return r;
	}

	for (i=n_children_in_a; i<old_n_children; i++) {

	    int targchild = i-n_children_in_a;
	    FIFO from_htab     = BNC_BUFFER(node,i);
	    FIFO to_htab       = BNC_BUFFER(B,   targchild);
	    DISKOFF thischilddiskoff = BNC_DISKOFF(node, i);

	    BNC_DISKOFF(B, targchild) = thischilddiskoff;

	    int r = toku_log_addchild(logger, fnum, B->thisnodename, targchild, thischilddiskoff, BNC_SUBTREE_FINGERPRINT(node, i));
	    if (r!=0) return r;

	    while (1) {
		bytevec key, data;
		unsigned int keylen, datalen;
		int type;
		TXNID xid;
		int fr = toku_fifo_peek(from_htab, &key, &keylen, &data, &datalen, &type, &xid);
		if (fr!=0) break;
		int n_bytes_moved = keylen+datalen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
		BYTESTRING keybs  = { .len = keylen,  .data = (char*)key  };
		BYTESTRING databs = { .len = datalen, .data = (char*)data };
		u_int32_t old_from_fingerprint = node->local_fingerprint;
		u_int32_t old_to_fingerprint   = B->local_fingerprint;
		u_int32_t delta = toku_calccrc32_cmd(type, xid, key, keylen, data, datalen);
		u_int32_t new_from_fingerprint = old_from_fingerprint - node->rand4fingerprint*delta;
		u_int32_t new_to_fingerprint   = old_to_fingerprint   + B->rand4fingerprint   *delta;
		if (r!=0) return r;
		r = toku_log_brtdeq(logger, fnum, node->thisnodename, n_children_in_a, xid, type, keybs, databs, old_from_fingerprint, new_from_fingerprint);
		if (r!=0) return r;
		r = toku_log_brtenq(logger, fnum, B->thisnodename,    targchild,       xid, type, keybs, databs, old_to_fingerprint,   new_to_fingerprint);
		r = toku_fifo_enq(to_htab, key, keylen, data, datalen, type, xid);
		if (r!=0) return r;
		toku_fifo_deq(from_htab);
		// key and data will no longer be valid
		node->local_fingerprint = new_from_fingerprint;
		B->local_fingerprint    = new_to_fingerprint;

		B->u.n.n_bytes_in_buffers     += n_bytes_moved;
		BNC_NBYTESINBUF(B, targchild) += n_bytes_moved;
		node->u.n.n_bytes_in_buffers  -= n_bytes_moved;
		BNC_NBYTESINBUF(node, i)      -= n_bytes_moved;
		// verify_local_fingerprint_nonleaf(B);
		// verify_local_fingerprint_nonleaf(node);
	    }

	    // Delete a child, removing it's fingerprint, and also the preceeding pivot key.  The child number must be > 0
	    {
		BYTESTRING bs = { .len = kv_pair_keylen(node->u.n.childkeys[i-1]),
				  .data = kv_pair_key(node->u.n.childkeys[i-1]) };
		assert(i>0);
		r = toku_log_delchild(logger, fnum, node->thisnodename, n_children_in_a, thischilddiskoff, BNC_SUBTREE_FINGERPRINT(node, i), bs);
		if (r!=0) return r;
		if (i>n_children_in_a) {
		    r = toku_log_setpivot(logger, fnum, B->thisnodename, targchild-1, bs);
		    if (r!=0) return r;
		    B->u.n.childkeys[targchild-1] = node->u.n.childkeys[i-1];
		    B->u.n.totalchildkeylens += toku_brt_pivot_key_len(t, node->u.n.childkeys[i-1]);
		    node->u.n.totalchildkeylens -= toku_brt_pivot_key_len(t, node->u.n.childkeys[i-1]);
		    node->u.n.childkeys[i-1] = 0;
		}
	    }
	    BNC_DISKOFF(node, i) = 0;
	    
	    BNC_SUBTREE_FINGERPRINT(B, targchild) = BNC_SUBTREE_FINGERPRINT(node, i);
	    BNC_SUBTREE_FINGERPRINT(node, i) = 0;

	    assert(BNC_NBYTESINBUF(node, i) == 0);
	}

	// Drop the n_children now (not earlier) so that we can do the fingerprint verification at any time.
	node->u.n.n_children=n_children_in_a;
	for (i=n_children_in_a; i<old_n_children; i++) {
	    toku_fifo_free(&BNC_BUFFER(node,i));
	}

	splitk->data = (void*)(node->u.n.childkeys[n_children_in_a-1]);
	splitk->size = toku_brt_pivot_key_len(t, node->u.n.childkeys[n_children_in_a-1]);
	node->u.n.totalchildkeylens -= toku_brt_pivot_key_len(t, node->u.n.childkeys[n_children_in_a-1]);
	node->u.n.childkeys[n_children_in_a-1]=0;

	verify_local_fingerprint_nonleaf(node);
	verify_local_fingerprint_nonleaf(B);
    }

    *nodea = node;
    *nodeb = B;

    assert(toku_serialize_brtnode_size(node)<node->nodesize);
    assert(toku_serialize_brtnode_size(B)<B->nodesize);
    return 0;
}

#endif

static void find_heaviest_child (BRTNODE node, int *childnum) {
    int max_child = 0;
    int max_weight = BNC_NBYTESINBUF(node, 0);
    int i;

    if (0) printf("%s:%d weights: %d", __FILE__, __LINE__, max_weight);
    assert(node->u.n.n_children>0);
    for (i=1; i<node->u.n.n_children; i++) {
	int this_weight = BNC_NBYTESINBUF(node,i);
	if (0) printf(" %d", this_weight);
	if (max_weight < this_weight) {
	    max_child = i;
	    max_weight = this_weight;
	}
    }
    *childnum = max_child;
    if (0) printf("\n");
}

/* find the leftmost child that may contain the key */
static unsigned int brtnode_which_child (BRTNODE node , DBT *k, DBT *d, BRT t) {
    int i;
    assert(node->height>0);
    for (i=0; i<node->u.n.n_children-1; i++) {
	int cmp = brt_compare_pivot(t, k, d, node->u.n.childkeys[i]);
        if (cmp > 0) continue;
        if (cmp < 0) return i;
        return i;
    }
    return node->u.n.n_children-1;
}

static int brtnode_put (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p);

// If CHILD is too wide, split it, and create a new node with the new children.  Unpin CHILD or the new children (even if something goes wrong).
// If it does split, unpin the new root node also.
static int maybe_split_root(BRT brt, BRTNODE child, CACHEKEY *rootp, TOKULOGGER logger);
// if CHILD is too wide, split it, and fix up NODE.  Either way, unpin the child or resulting children (even if it fails do the unpin)
static int maybe_split_nonroot (BRT brt, BRTNODE node, int childnum, BRTNODE child, int *n_children_replacing_child, TOKULOGGER logger);

// Push stuff into a child weakly.  (That is don't cause any I/O or cause the child to get too big.)
static int weak_push_to_child (BRT brt, BRTNODE node, int childnum, TOKULOGGER logger) {
    void *child_v;
    int r = toku_cachetable_maybe_get_and_pin(brt->cf, BNC_DISKOFF(node, childnum), &child_v);
    if (r!=0) return 0;
    BRTNODE child = child_v;
    DBT key,val;
    BRT_CMD_S cmd;
    while (0 == toku_fifo_peek_cmdstruct(BNC_BUFFER(node, childnum), &cmd, &key, &val)) {
	r = brtnode_put(brt, child, &cmd, logger, WEAK);
	if (r==EAGAIN) break;
	if (r!=0) goto died;
	r=toku_fifo_deq(BNC_BUFFER(node, childnum));
	if (r!=0) goto died;
    }
    return unpin_brtnode(brt, child);
 died:
    unpin_brtnode(brt, child);
    return r;
		  
}

// If the buffers are too big, push stuff down.  The subchild may need to be split, in which case our fanout may get too large.
// When are done, this node is has little enough stuff in its buffers (but the fanout may be too large), and all the descendant
// nodes are properly sized (the buffer sizes and fanouts are all small enough).
static int push_down_if_buffers_too_full(BRT brt, BRTNODE node, TOKULOGGER logger) {
    if (node->height==0) return 0; // can't push down for leaf nodes

    while (node->u.n.n_bytes_in_buffers > 0 && toku_serialize_brtnode_size(node)>node->nodesize) {
	int childnum;
	find_heaviest_child(node, &childnum);
	void *child_v;
	int r = toku_cachetable_get_and_pin(brt->cf, BNC_DISKOFF(node, childnum), &child_v, NULL,
					    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
	if (r!=0) return r;
	BRTNODE child=child_v;
	if (0) { died: unpin_brtnode(brt, child); return r; }
	BRT_CMD_S cmd;
	DBT key,val;
	while (0==toku_fifo_peek_cmdstruct(BNC_BUFFER(node, childnum), &cmd, &key, &val)) {
	    r=toku_fifo_deq(BNC_BUFFER(node, childnum));
	    assert(r==0); // we just did a peek, so the buffer must be nonempty
	    r=brtnode_put(brt, child, &cmd, logger, WEAK);
	    if (r!=EAGAIN && r!=0) goto died;
	    if (r==EAGAIN) {
		// Weak pushes ran out of steam.  Now do a strong push if there is still something in the buffer.
		if (0==toku_fifo_peek_cmdstruct(BNC_BUFFER(node, childnum), &cmd, &key, &val)) {
		    r=brtnode_put(brt, child, &cmd, logger, STRONG);
		    if (r!=0) goto died;
		    r=toku_fifo_deq(BNC_BUFFER(node, childnum));
		    if (r!=0) goto died;
		    // Now it's possible that the child must be split.  (Or maybe the child managed to flush stuff to our grandchildren)
		    int n_children_replacing_child;
		    r=maybe_split_nonroot(brt, node, childnum, child, &n_children_replacing_child, logger);
		    if (r!=0) return r; // don't go to died since that unpins
		    int i;
		    for (i=0; i<n_children_replacing_child; i++) {
			r=weak_push_to_child(brt, node, childnum+i, logger);
			if (r!=0) return r;
		    }
		    // we basically pushed as much as we could to that child
		}
	    } 
	}
    }
    return 0;
}

static int split_nonleaf_node(BRT brt, BRTNODE node_to_split, int *n_new_nodes, BRTNODE **new_nodes, DBT **splitks);
static int nonleaf_node_is_too_wide (BRT, BRTNODE);

static int maybe_fixup_fat_child(BRT brt, BRTNODE node, int childnum, BRTNODE child, TOKULOGGER logger) // If the node is too big then deal with it.  Unpin the child (or children if it splits)  NODE may be too big at the end
{
    int r = push_down_if_buffers_too_full(brt, child, logger);
    if (r!=0) return r;
    // now the child may have too much fanout.
    if (child->height>0) {
	if (nonleaf_node_is_too_wide(brt, child)) {
	    int n_new_nodes; BRTNODE *new_nodes; DBT *splitks;
	    if ((r=split_nonleaf_node(brt, child,  &n_new_nodes, &new_nodes, &splitks))) return r;
	    int i;
	    int old_n_children = node->u.n.n_children;
	    FIFO old_fifo = BNC_BUFFER(node, childnum);
	    REALLOC_N(old_n_children+n_new_nodes-1, node->u.n.childinfos);
	    // slide the children over
	    for (i=old_n_children-1; i>childnum; i--)
		node->u.n.childinfos[i+n_new_nodes-1] = node->u.n.childinfos[i];
	    // fill in the new children
	    for (; i<childnum+n_new_nodes-1; i++) {
		node->u.n.childinfos[i] = (struct brtnode_nonleaf_childinfo) { .subtree_fingerprint = 0,
									       .diskoff = new_nodes[i-childnum]->thisnodename,
									       .n_bytes_in_buffer = 0 };
		r=toku_fifo_create(&BNC_BUFFER(node, i));
	    }
	    // slide the keys over
	    node->u.n.childkeys = toku_realloc(node->u.n.childkeys, (old_n_children+n_new_nodes-2 ) * sizeof(node->u.n.childkeys[0]));
	    for (i=node->u.n.n_children; i>=childnum; i--) {
		node->u.n.childkeys[i+n_new_nodes-1] = node->u.n.childkeys[i];
	    }
	    // fix up fingerprints
	    for (i=0; i<n_new_nodes; i++) {
		fixup_child_fingerprint(node, childnum+i, new_nodes[i], brt, logger);
	    }
	    toku_free(new_nodes);
	    // now everything in the fifos must be put again
	    BRT_CMD_S cmd;
	    DBT key,val;
	    while (0==toku_fifo_peek_deq_cmdstruct(old_fifo, &cmd, &key, &val)) {
		for (i=childnum; i<childnum+n_new_nodes-1; i++) {
		    int cmp = brt_compare_pivot(brt, cmd.u.id.key, 0, node->u.n.childkeys[i]);
		    if (cmp<=0) {
			r=toku_fifo_enq_cmdstruct(BNC_BUFFER(node, i), &cmd);
			if (r!=0) return r;
			if (cmd.type!=BRT_DELETE || 0==(brt->flags&TOKU_DB_DUPSORT)) goto filled; // we only need to put one in
		    }
		}
		r=toku_fifo_enq_cmdstruct(BNC_BUFFER(node, i), &cmd);
		if (r!=0) return r;
	    filled: /*nothing*/;
	    }
	    toku_fifo_free(&old_fifo);
	    if (r!=0) return r;
	}
    } else {
	abort(); // if a leaf is too fat need to split it.
    }
    return 0;
}

// There are two kinds of puts:  
//  A "weak" put that is guaranteed to trigger no I/O, and will not leaf the node overfull.
//    A weak put may not actually perform the put, however (in which case it returns EAGAIN instead of 0)
//  A "strong" put that is guaranteed to do the put.  However, it may trigger I/O and the resulting node may be too big.

static int brt_leaf_put (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    FILENUM filenum = toku_cachefile_filenum(t->cf);
    switch (cmd->type) {
    case BRT_INSERT: {
        int r = toku_pma_insert_or_replace_ws(node->u.l.buffer,
					      cmd->u.id.key, cmd->u.id.val,
					      logger, cmd->xid,
					      filenum, node->thisnodename, node->rand4fingerprint, &node->local_fingerprint,
					      &node->log_lsn, &node->u.l.n_bytes_in_buffer,
					      weak_p==WEAK);
	if (r==EAGAIN) return EAGAIN;
	assert(r==0);
	node->dirty=1;
	return r;
    }
    case BRT_DELETE: {
        int r = toku_pma_delete_fixupsize(node->u.l.buffer, cmd->u.id.key, (DBT*)0,
					  logger, cmd->xid, node->thisnodename,
					  node->rand4fingerprint, &node->local_fingerprint, &node->log_lsn, &node->u.l.n_bytes_in_buffer);
	if (r==0) node->dirty=1;
	return r;
    }
    case BRT_DELETE_BOTH: {
        int r = toku_pma_delete_fixupsize(node->u.l.buffer, cmd->u.id.key, cmd->u.id.val,
					  logger, cmd->xid, node->thisnodename,
					  node->rand4fingerprint, &node->local_fingerprint, &node->log_lsn, &node->u.l.n_bytes_in_buffer);
        if (r == 0) node->dirty = 1;
        return r;
    }
    case BRT_NONE: return 0;
    }
    return EINVAL; //  if none of the cases match, then the command is messed up.
}

// Put an command in a particular child's fifo.
// If weak_p then do it without doing I/O or overfilling the child.
//   If the child is in main memory and we can do a weak put on the child, then push into the child.
//   Otherwise we return EAGAIN.
// If not weak_p then we are willing to overfill the child.
static int brt_nonleaf_put_cmd_to_child (BRT t, BRTNODE node, int childnum, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    int r;

    if (toku_fifo_n_entries(BNC_BUFFER(node,childnum))==0) {
	void *child_v;
	r = toku_cachetable_maybe_get_and_pin(t->cf, BNC_DISKOFF(node, childnum), &child_v);
	if (r==0) {
	    BRTNODE child=child_v;
	    r = brtnode_put(t, child, cmd, logger, weak_p);
	    if (r==EAGAIN) {
		r = unpin_brtnode(t, child);
		if (r!=0) return r; // node is still OK
	    } else if (r==0) {
		return maybe_fixup_fat_child(t, node, childnum, child, logger); // If the node is too big then deal with it.  Unpin the child.  NODE may be too big.  I think the only way a node can get fat is if weak_p==STRONG.
	    } else {
		unpin_brtnode(t, child);
		return r; // node is still OK
	    }
	}
    }
    // For some reason we didn't put it into the child, so we must put it in the fifo.
    int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
    if (diff+toku_serialize_brtnode_size(node)>node->nodesize) return EAGAIN; // And it doesn't fit here.
    r=toku_fifo_enq_cmdstruct(BNC_BUFFER(node,childnum), cmd);
    if (r!=0) return r;

    node->local_fingerprint += node->rand4fingerprint * toku_calccrc32_cmdstruct(cmd);
    node->u.n.n_bytes_in_buffers += diff;
    BNC_NBYTESINBUF(node, childnum) += diff;
    node->dirty = 1;
    return 0; // node may be too big
}

static void determine_which_children_to_push_delete (BRT t, BRTNODE node, BRT_CMD cmd, int *n_children_to_push, int *children_to_push) {
    int i;
    *n_children_to_push=0;
    for (i=0; i<node->u.n.n_children-1; i++) {
	int cmp = brt_compare_pivot(t, cmd->u.id.key, 0, node->u.n.childkeys[i]);
	if (cmp>0) continue; // the cmd is bigger than the pivot, so it doesn't go here.
	else if (cmp<0) {
	    // the cmd is smaller than the pivot, so it goes here, and goes nowhere else to the right
	    children_to_push[(*n_children_to_push)++] = i;
	    return;
	} else if (t->flags & TOKU_DB_DUPSORT) {
	    // the cmd is equal and we are in a dupsort, so push and and go around to push additional ones.
	    children_to_push[(*n_children_to_push)++] = i;
	    continue;
	} else {
	    // the cmd is equal but we are not in a dupsort, so we save i, but there is no saving the next one.
	    children_to_push[(*n_children_to_push)++] = i;
	    return;
	}
    }
    // if we fell off the bottom, which means we must include the last one.
    children_to_push[(*n_children_to_push)++] = i;
}

// Put the cmd into all the subtrees that it belong in.  (Deletes can end up in several subtrees.)
// If weak_p then
//   Don't do any I/O and the node will not be overfull.
//   To guarantee that no I/O will occur, we must make sure we can insert everything before inserting anything.
// else put it regardless, possibly overflowing the node.
static int brt_nonleaf_put_delete (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    int singlediff = cmd->u.id.key->size + cmd->u.id.val->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
    int n_children_to_push = 0;
    int children_to_push[node->u.n.n_children];
    determine_which_children_to_push_delete(t, node, cmd, &n_children_to_push, children_to_push);
    int totaldiff = singlediff * n_children_to_push;
    if (weak_p && (totaldiff + toku_serialize_brtnode_size(node) > node->nodesize)) return EAGAIN;
    // Now we know it will fit, so do all the weak pushes.  We are being a little bit conservative,
    // since a soft push might succeed, in getting data to a child without using up the local storage.
    int i;
    for (i=0; i<n_children_to_push; i++) {
	int r=brt_nonleaf_put_cmd_to_child(t, node, children_to_push[i], cmd, logger, WEAK);
	if (r==EAGAIN) {
	    r = toku_fifo_enq_cmdstruct(BNC_BUFFER(node, children_to_push[i]), cmd);
	    if (r!=0) return r;
	} else if (r!=0) return r;
    }
    // We did we weak pushes to the children, but if that didn't work we put it in the buffer.  The node could be overfull now.
    return 0;
}

// a DELETE could be replicating in a dupsort database.  Everything else is non replicating.
static int brt_nonleaf_put_nonreplicating_cmd (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    return brt_nonleaf_put_cmd_to_child(t, node,
					brtnode_which_child(node, cmd->u.id.key, cmd->u.id.val, t),
					cmd, logger,
					weak_p);
}

// Put the cmd into the node.  Possibly results in the node being overfull.  (But not if weak_p is set, in which case EAGAIN is returned instead)
// The command could get pushed into the appropriate child if the child is in main memory and has space to hold the command.
static int brt_nonleaf_put (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    if (cmd->type == BRT_INSERT || cmd->type == BRT_DELETE_BOTH) {
        return brt_nonleaf_put_nonreplicating_cmd(t, node, cmd, logger, weak_p);
    } else if (cmd->type == BRT_DELETE) {
        return brt_nonleaf_put_delete(t, node, cmd, logger, weak_p);
    } else
        return EINVAL;
}

// Put the command into the node.
// If weak_p is set then neither the node nor any descendants will get too big, and no I/O will occur.
// if !weak_p then I/O could occur and the node could end up with too much fanout.  (But the children will all be properly sized)
static int brtnode_put (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, WS weak_p) {
    if (node->height==0) {
	return brt_leaf_put(t, node, cmd, logger, weak_p);
    } else {
	return brt_nonleaf_put(t, node, cmd, logger, weak_p);
    }
}
#ifdef FOO

static void verify_local_fingerprint_nonleaf (BRTNODE node) {
    u_int32_t fp=0;
    int i;
    if (node->height==0) return;
    for (i=0; i<node->u.n.n_children; i++)
	FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
			  ({
			      fp += node->rand4fingerprint * toku_calccrc32_cmd(type, xid, key, keylen, data, datalen);
			  }));
    assert(fp==node->local_fingerprint);
}

static int setup_initial_brt_root_node (BRT t, DISKOFF offset, TOKULOGGER logger) {
    int r;
    TAGMALLOC(BRTNODE, node);
    assert(node);
    initialize_brtnode(t, node,
		       offset, /* the location is one nodesize offset from 0. */
		       0);
    //    node->brt = t;
    if (0) {
	printf("%s:%d for tree %p node %p mdict_create--> %p\n", __FILE__, __LINE__, t, node, node->u.l.buffer);
	printf("%s:%d put root at %lld\n", __FILE__, __LINE__, offset);
    }
    r=toku_cachetable_put(t->cf, offset, node, brtnode_size(node),
			  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    toku_verify_counts(node);
    toku_log_newbrtnode(logger, toku_cachefile_filenum(t->cf), offset, 0, t->h->nodesize, (t->flags&TOKU_DB_DUPSORT)!=0, node->rand4fingerprint);
    toku_update_brtnode_loggerlsn(node, logger);
    r=unpin_brtnode(t, node);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    return 0;
}

int toku_brt_create(BRT *brt_ptr) {
    BRT brt = toku_malloc(sizeof *brt);
    if (brt == 0)
        return ENOMEM;
    memset(brt, 0, sizeof *brt);
    list_init(&brt->cursors);
    brt->flags = 0;
    brt->nodesize = BRT_DEFAULT_NODE_SIZE;
    brt->compare_fun = toku_default_compare_fun;
    brt->dup_compare = toku_default_compare_fun;
    *brt_ptr = brt;
    return 0;
}

int toku_brt_set_flags(BRT brt, unsigned int flags) {
    brt->flags = flags;
    return 0;
}

int toku_brt_get_flags(BRT brt, unsigned int *flags) {
    *flags = brt->flags;
    return 0;
}

int toku_brt_set_nodesize(BRT brt, unsigned int nodesize) {
    brt->nodesize = nodesize;
    return 0;
}

int toku_brt_get_nodesize(BRT brt, unsigned int *nodesize) {
    *nodesize = brt->nodesize;
    return 0;
}

int toku_brt_set_bt_compare(BRT brt, int (*bt_compare)(DB *, const DBT*, const DBT*)) {
    brt->compare_fun = bt_compare;
    return 0;
}

int toku_brt_set_dup_compare(BRT brt, int (*dup_compare)(DB *, const DBT*, const DBT*)) {
    brt->dup_compare = dup_compare;
    return 0;
}

int toku_brt_get_fd(BRT brt, int *fdp) {
    *fdp = toku_cachefile_fd(brt->cf);
    return 0;
}

enum { UNDO_COUNTER_LIMIT=10 };
typedef void(*undo_fun)(void*);
struct undo_rec { undo_fun f; void *v; };
struct undo {
    int undo_counter;
    struct undo_rec undos[UNDO_COUNTER_LIMIT];
};
#define INITUNDO(u) struct undo u = (struct undo){.undo_counter=0}
void push_undo(struct undo *undos, undo_fun f, void *v) {
    assert(undos->undo_counter<UNDO_COUNTER_LIMIT);
    undos->undos[undos->undo_counter++]=(struct undo_rec){f,v};
}
void do_undos(struct undo *undos) {
    while (undos->undo_counter>0) {
	struct undo_rec *r = &undos->undos[--undos->undo_counter];
	r->f(r->v);
    }
}

void undo_free (void *v) {
    void **ptr=v;
    toku_free(*ptr);
    *ptr=0;
}

// tbou means "toku_brt_open undo"
void tbou_close_cachefile (void *v) {
    BRT t = v;
    toku_cachefile_close(&t->cf);
}

struct maybe_unpin_info {
    int is_pinned;
    CACHEFILE cf;
    CACHEKEY ckey;
};
    
void tbou_maybe_unpin (void *v) {
    struct maybe_unpin_info *mui = v;
    if (mui->is_pinned)
	toku_cachetable_unpin(mui->cf, mui->ckey, 0, 0);
    mui->is_pinned=0;
}
    

int toku_brt_open(BRT t, const char *fname, const char *fname_in_env, const char *dbname, int is_create, int only_create, int load_flags, CACHETABLE cachetable, TOKUTXN txn, DB *db) {

    /* If dbname is NULL then we setup to hold a single tree.  Otherwise we setup an array. */
    int r;
    struct maybe_unpin_info mui = {.is_pinned=0};
    INITUNDO(undos);
    push_undo(&undos, tbou_maybe_unpin, &mui); // if we pin a cf, then we put it into the maybe_undo_info so it will get undone on error.
    assert(is_create || !only_create);
    assert(!load_flags || !only_create);
    if (0) {
    died:
	do_undos(&undos);
	return r;
    }
    {
	if (dbname) {
	    char *malloced_name = toku_strdup(dbname);
	    if (malloced_name==0) { r = errno; goto died; }
	    push_undo(&undos, undo_free, &t->database_name);
	    t->database_name = malloced_name;
	} else {
	    t->database_name = 0;
	}
    }
    t->db = db;
    {
	int fd = open(fname, O_RDWR, 0777);
	r = errno;
	if (fd==-1) {
            if (r==ENOENT) {
                if (!is_create) { goto died; }
                fd = open(fname, O_RDWR | O_CREAT, 0777);
                if (fd==-1) { r=errno; goto died; }
                r = toku_logger_log_fcreate(txn, fname_in_env, 0777);
                if (r!=0) goto died;
            } else
                goto died;
	}
        if ((r = toku_cachetable_openfd(&t->cf, cachetable, fd, t))) goto died;
	push_undo(&undos, tbou_close_cachefile, t);
    }
    if ((r = toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(t->cf)))) 	goto died;
    // no undo action for log_fopen
    assert(t->nodesize>0);

    if (is_create) {
	r = toku_read_and_pin_brt_header(t->cf, &t->h);
	if (r!=0 && r!=-1) goto died;
	if (r==0) {
	    mui=(struct maybe_unpin_info){.is_pinned=1, .cf=t->cf, .ckey=0}; // remember to unpin it
	    int i;
	    assert(r==0);
	    assert(dbname);
	    if (t->h->unnamed_root!=-1) { r=EINVAL; goto died; } // Cannot create a subdb in a file that is not enabled for subdbs
	    assert(t->h->n_named_roots>=0);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    if (only_create) {
			r = EEXIST;
			goto died;
		    }
		    else goto found_it;
		}
	    }
	    if ((t->h->names = toku_realloc(t->h->names, (1+t->h->n_named_roots)*sizeof(*t->h->names))) == 0)   { r=errno; goto died; }
	    if ((t->h->roots = toku_realloc(t->h->roots, (1+t->h->n_named_roots)*sizeof(*t->h->roots))) == 0)   { r=errno; goto died; }
	    t->h->n_named_roots++;
	    if ((t->h->names[t->h->n_named_roots-1] = toku_strdup(dbname)) == 0)                                { r=errno; goto died; }
	    push_undo(&undos, undo_free, &t->h->names[t->h->n_named_roots-1]);
	    r = malloc_diskblock_header_is_in_memory(&t->h->roots[t->h->n_named_roots-1], t, t->h->nodesize, toku_txn_logger(txn));
	    if (r!=0) goto died;
	    t->h->dirty = 1;
	    if ((r=setup_initial_brt_root_node(t, t->h->roots[t->h->n_named_roots-1], toku_txn_logger(txn)))!=0) goto died;
	} else {
	    assert(r==-1); // the pin failed because no data was present
	    /* construct a new header. */
	    if ((MALLOC(t->h))==0) { r = errno; goto died; }
	    t->h->dirty=1;
            t->h->flags = t->flags;
	    t->h->nodesize=t->nodesize;
	    t->h->freelist=-1;
	    t->h->unused_memory=2*t->nodesize;
	    if (dbname) {
		t->h->unnamed_root = -1;
		t->h->n_named_roots = 1;
		if ((MALLOC_N(1, t->h->names))==0)             { r=errno; goto died; } push_undo(&undos, undo_free, &t->h->names);
		if ((MALLOC_N(1, t->h->roots))==0)             { r=errno; goto died; } push_undo(&undos, undo_free, &t->h->roots);
		if ((t->h->names[0] = toku_strdup(dbname))==0) { r=errno; goto died; } push_undo(&undos, undo_free, &t->h->names[0]);
		t->h->roots[0] = t->nodesize;
	    } else {
		t->h->unnamed_root = t->nodesize;
		t->h->n_named_roots = -1;
		t->h->names=0;
		t->h->roots=0;
	    }
	    if ((r=toku_logger_log_header(txn, toku_cachefile_filenum(t->cf), t->h)))     goto died;
	    if ((r=setup_initial_brt_root_node(t, t->nodesize, toku_txn_logger(txn)))!=0) goto died;
	    if ((r=toku_cachetable_put(t->cf, 0, t->h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0))) goto died;
	    mui=(struct maybe_unpin_info){.is_pinned=1, .cf=t->cf, .ckey=0}; // remember to unpin it
	} 
    } else {
	if ((r = toku_read_and_pin_brt_header(t->cf, &t->h))!=0) goto died;
	mui=(struct maybe_unpin_info){.is_pinned=1, .cf=t->cf, .ckey=0}; // remember to unpin it
	if (!dbname) {
	    if (t->h->n_named_roots!=-1) { r = EINVAL; goto died; } // requires a subdb
	} else {
	    int i;
	    if (t->h->n_named_roots==-1) { r=EINVAL; goto died; } // no suddbs in the db
	    // printf("%s:%d n_roots=%d\n", __FILE__, __LINE__, t->h->n_named_roots);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    goto found_it;
		}

	    }
	    r=ENOENT; /* the database doesn't exist */
	    goto died;
	}
    found_it:
        t->nodesize = t->h->nodesize;                 /* inherit the pagesize from the file */
        if (t->flags != t->h->flags) {                /* flags must match */
            if (load_flags) t->flags = t->h->flags;
            else { r = EINVAL; goto died; }
        }
    }
    assert(t->h);
    if ((r = toku_unpin_brt_header(t)) !=0) goto died; // it's unpinned
    mui.is_pinned=0;
    assert(t->h==0);
    return 0;
}

int toku_brt_remove_subdb(BRT brt, const char *dbname, u_int32_t flags) {
    int r;
    int i;
    int found = -1;

    assert(flags == 0);
    r = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    if (r!=0) return r;

    assert(brt->h->unnamed_root==-1);
    assert(brt->h->n_named_roots>=0);
    for (i = 0; i < brt->h->n_named_roots; i++) {
        if (strcmp(brt->h->names[i], dbname) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) {
        //Should not be possible.
        r = ENOENT;
        goto error;
    }
    //Free old db name
    toku_free(brt->h->names[found]);
    //TODO: Free Diskblocks including root
    
    for (i = found + 1; i < brt->h->n_named_roots; i++) {
        brt->h->names[i - 1] = brt->h->names[i];
        brt->h->roots[i - 1] = brt->h->roots[i];
    }
    brt->h->n_named_roots--;
    brt->h->dirty = 1;
    //TODO: What if n_named_roots becomes 0?  Should we handle it specially?  Should we delete the file?
    if ((brt->h->names = toku_realloc(brt->h->names, (brt->h->n_named_roots)*sizeof(*brt->h->names))) == 0)   { r=errno; goto error; }
    if ((brt->h->roots = toku_realloc(brt->h->roots, (brt->h->n_named_roots)*sizeof(*brt->h->roots))) == 0)   { r=errno; goto error; }

    r = toku_unpin_brt_header(brt);
    return r;

error:
    toku_unpin_brt_header(brt);
    return r;
}

// This one has no env
int toku_open_brt (const char *fname, const char *dbname, int is_create, BRT *newbrt, int nodesize, CACHETABLE cachetable, TOKUTXN txn,
		   int (*compare_fun)(DB*,const DBT*,const DBT*), DB *db) {
    BRT brt;
    int r;
    const int only_create = 0;
    const int load_flags  = 0;

    r = toku_brt_create(&brt);
    if (r != 0) return r;
    toku_brt_set_nodesize(brt, nodesize);
    toku_brt_set_bt_compare(brt, compare_fun);

    r = toku_brt_open(brt, fname, fname, dbname, is_create, only_create, load_flags, cachetable, txn, db);
    if (r != 0) {
	toku_free(brt);
        return r;
    }

    *newbrt = brt;
    return 0;
}

int toku_close_brt (BRT brt) {
    int r;
    while (!list_empty(&brt->cursors)) {
	BRT_CURSOR c = list_struct(list_pop(&brt->cursors), struct brt_cursor, cursors_link);
	r=toku_brt_cursor_close(c);
	if (r!=0) return r;
    }
    if (brt->cf) {
        assert(0==toku_cachefile_count_pinned(brt->cf, 1)); // For the brt, the pinned count should be zero.
        //printf("%s:%d closing cachetable\n", __FILE__, __LINE__);
        if ((r = toku_cachefile_close(&brt->cf))!=0) return r;
    }
    if (brt->database_name) toku_free(brt->database_name);
    if (brt->skey) { toku_free(brt->skey); }
    if (brt->sval) { toku_free(brt->sval); }
    toku_free(brt);
    return 0;
}

CACHEKEY* toku_calculate_root_offset_pointer (BRT brt) {
    if (brt->database_name==0) {
	return &brt->h->unnamed_root;
    } else {
	int i;
	for (i=0; i<brt->h->n_named_roots; i++) {
	    if (strcmp(brt->database_name, brt->h->names[i])==0) {
		return &brt->h->roots[i];
	    }
	}
    }
    abort();
}

static int brt_init_new_root(BRT brt, int n_new_nodes, BRTNODE *new_nodes, DBT *splitks, CACHEKEY *rootp, TOKULOGGER logger, BRTNODE *newrootp) {
    assert(n_new_nodes>0);
    TAGMALLOC(BRTNODE, newroot);
    int r;
    int new_height = new_nodes[0]->height+1;
    int new_nodesize = brt->h->nodesize;
    DISKOFF newroot_diskoff;
    if ((r=malloc_diskblock(&newroot_diskoff, brt, new_nodesize, logger))) return r;
    assert(newroot);
    if (brt->database_name==0) {
	toku_log_changeunnamedroot(logger, toku_cachefile_filenum(brt->cf), *rootp, newroot_diskoff);
    } else {
	BYTESTRING bs;
	bs.len = 1+strlen(brt->database_name);
	bs.data = brt->database_name;
	toku_log_changenamedroot(logger, toku_cachefile_filenum(brt->cf), bs, *rootp, newroot_diskoff);
    }
    *rootp=newroot_diskoff;
    brt->h->dirty=1;
    initialize_brtnode (brt, newroot, newroot_diskoff, new_height);
    newroot->u.n.n_children=n_new_nodes;
    r=toku_log_newbrtnode(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, new_height, new_nodesize, (brt->flags&TOKU_DB_DUPSORT)!=0, newroot->rand4fingerprint);
    if (r!=0) return r;
    int i;
    for (i=0; i<n_new_nodes; i++) {
	BNC_DISKOFF(newroot, i)=new_nodes[i]->thisnodename;
	r=toku_fifo_create(&BNC_BUFFER(newroot,i)); if (r!=0) return r;
	r=toku_log_addchild(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, new_nodes[i]->thisnodename, 0);
	if (r!=0) return r;
	fixup_child_fingerprint(newroot, i, new_nodes[i], brt, logger);
    }
    toku_verify_counts(newroot);
    int sum_splitk_sizes=0;
    for (i=0; i+1<n_new_nodes; i++) {
	sum_splitk_sizes += splitks[i].size;
	newroot->u.n.childkeys[i] = splitks[i].data;
	BYTESTRING bs = { .len = kv_pair_keylen(newroot->u.n.childkeys[0]),
			  .data = kv_pair_key(newroot->u.n.childkeys[0]) };
	r=toku_log_setpivot(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, bs);
	if (r!=0) return r;
	toku_update_brtnode_loggerlsn(newroot, logger);
    }
    newroot->u.n.totalchildkeylens=sum_splitk_sizes;
    for (i=0; i<n_new_nodes; i++) {
	r=unpin_brtnode(brt, new_nodes[i]);
	if (r!=0) return r;
    }
    toku_cachetable_put(brt->cf, newroot_diskoff, newroot, brtnode_size(newroot),
                        toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    *newrootp = newroot;
    return 0;
}

static int split_nonleaf_node(BRT, int *n_new_nodes, BRTNODE **new_nodes, DBT **splitks);
static int leaf_node_is_too_full (BRT, BRTNODE);

// push things down into node's children (and into their children and so forth) but don't make any descendant too big.
static int push_down_without_overfilling (BRT brt, BRTNODE node, TOKULOGGER logger);

// Push data toward a child.  If the child gets too big then the child will push down or split.
// If a split happens, then return immediately so that we can check to see if NODE needs to be split
static int flush_toward_child (BRT brt, BRTNODE node, int childnum, TOKULOGGER logger);

static int maybe_fixup_root (BRT brt, BRTNODE node, CACHEKEY *rootp, TOKULOGGER logger) {
    int r;
    if (node->height>0) {
	// internal nodes can be too wide, but if too full, they did a push down
    maybe_reshape_internal_node:
	while (nonleaf_node_is_too_wide(brt, node)) {
	    int n_new_nodes; BRTNODE *new_nodes; DBT *splitks;
	    if ((r=split_nonleaf_node(brt, node, &n_new_nodes, &new_nodes, &splitks))) return r;
	    if ((r=brt_init_new_root(brt, n_new_nodes, new_nodes, splitks, rootp, logger, &node))) return r; // unpins all the new nodes, which are all small enough
	    // now node is still possibly too wide, hence the loop
	}
    } else {
	// leaf nodes can be too full
	if (leaf_node_is_too_full(brt, node)) {
	    int n_new_nodes; BRTNODE *new_nodes; DBT *splitks;
	    if ((r==split_leaf_node(brt, logger, node, &n_new_nodes, &new_nodes, &splitks))) return r;
	    if ((r==brt_init_new_root(brt, n_new_nodes, new_nodes, splitks, rootp, logger, &node))) return r; // unpins all the new nodes, which are all small enough
	    assert(node->height>0);
	    goto maybe_reshape_internal_node;
	}
    }
    return 0;
}

#endif

static int brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger) {
    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
    int r;
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, 
				  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt))) {
	if (0) { died1: unpin_brtnode(brt, node); goto died0; }
	goto died0;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    if ((r = brtnode_put(brt, node, cmd, logger, STRONG))) goto died1;     // put stuff in, possibly causing the buffers to get too big
    if ((r = push_down_if_buffers_too_full(brt, node, logger))) goto died1;  // if the buffers are too big, push stuff down
    if ((r = maybe_split_root(brt, node, rootp, logger))) goto died1;        // now the node might have to split (leaf nodes can't push down, and internal nodes have too much fanout)   This will change node.
    // Now the node is OK, 
    brt->h->dirty=1;
    return toku_unpin_brt_header(brt);
}

int toku_brt_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    int r;
    BRT_CMD_S brtcmd = { BRT_INSERT, toku_txn_get_txnid(txn), .u.id={key,val}};

    r = brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

int toku_brt_lookup (BRT brt, DBT *k, DBT *v) {
    int r, rr;
    BRT_CURSOR cursor;

    rr = toku_brt_cursor(brt, &cursor);
    if (rr != 0) return rr;

    int op = brt->flags & TOKU_DB_DUPSORT ? DB_GET_BOTH : DB_SET;
    r = toku_brt_cursor_get(cursor, k, v, op, 0);

    rr = toku_brt_cursor_close(cursor); assert(rr == 0);

    return r;
}

int toku_brt_delete(BRT brt, DBT *key, TOKUTXN txn) {
    int r;
    DBT val;
    BRT_CMD_S brtcmd = { BRT_DELETE, toku_txn_get_txnid(txn), .u.id={key, toku_init_dbt(&val)}};
    r = brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

int toku_brt_delete_both(BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    int r;
    BRT_CMD_S brtcmd = { BRT_DELETE_BOTH, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

int toku_verify_brtnode (BRT brt, DISKOFF off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse, BRTNODE parent_brtnode);

int toku_dump_brtnode (BRT brt, DISKOFF off, int depth, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, BRTNODE parent_brtnode) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r = toku_cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
					toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(r==0);
    printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    result=toku_verify_brtnode(brt, off, lorange, lolen, hirange, hilen, 0, parent_brtnode);
    printf("%*sNode=%p\n", depth, "", node);
    if (node->height>0) {
	printf("%*sNode %lld nodesize=%d height=%d n_children=%d  n_bytes_in_buffers=%d keyrange=%s %s\n",
	       depth, "", off, node->nodesize, node->height, node->u.n.n_children, node->u.n.n_bytes_in_buffers, (char*)lorange, (char*)hirange);
	//printf("%s %s\n", lorange ? lorange : "NULL", hirange ? hirange : "NULL");
	{
	    int i;
	    for (i=0; i< node->u.n.n_children; i++) {
		printf("%*schild %d buffered (%d entries):\n", depth+1, "", i, toku_fifo_n_entries(BNC_BUFFER(node,i)));
		FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
				  ({
				      data=data; datalen=datalen; keylen=keylen;
				      printf("%*s xid=%"PRId64" %d (type=%d)\n", depth+2, "", xid, ntohl(*(int*)key), type);
				      //assert(strlen((char*)key)+1==keylen);
				      //assert(strlen((char*)data)+1==datalen);
				  }));
	    }
	    for (i=0; i<node->u.n.n_children; i++) {
		printf("%*schild %d\n", depth, "", i);
		if (i>0) {
		    printf("%*spivot %d len=%d %d\n", depth+1, "", i-1, node->u.n.childkeys[i-1]->keylen, ntohl(*(int*)&node->u.n.childkeys[i-1]->key));
		}
		toku_dump_brtnode(brt, BNC_DISKOFF(node, i), depth+4,
				  (i==0) ? lorange : node->u.n.childkeys[i-1],
				  (i==0) ? lolen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]),
				  (i==node->u.n.n_children-1) ? hirange : node->u.n.childkeys[i],
				  (i==node->u.n.n_children-1) ? hilen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i]),
				  node
				  );
	    }
	}
    } else {
	printf("%*sNode %lld nodesize=%d height=%d n_bytes_in_buffer=%d keyrange=%d %d\n",
	       depth, "", off, node->nodesize, node->height, node->u.l.n_bytes_in_buffer, lorange ? ntohl(*(int*)lorange) : 0, hirange ? ntohl(*(int*)hirange) : 0);
	PMA_ITERATE(node->u.l.buffer, key, keylen, val __attribute__((__unused__)), vallen,
		    ( keylen=keylen, vallen=vallen, printf(" (%d)%d ", keylen, ntohl(*(int*)key))));
	printf("\n");
    }
    r = toku_cachetable_unpin(brt->cf, off, 0, 0);
    assert(r==0);
    return result;
}

int toku_dump_brt (BRT brt) {
    int r;
    CACHEKEY *rootp;
    struct brt_header *prev_header = brt->h;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    if ((r = toku_dump_brtnode(brt, *rootp, 0, 0, 0, 0, 0, null_brtnode))) goto died0;
    if ((r = toku_unpin_brt_header(brt))!=0) return r;
    brt->h = prev_header;
    return 0;
}

static int show_brtnode_blocknumbers (BRT brt, DISKOFF off) {
    BRTNODE node;
    void *node_v;
    int i,r;
    assert(off%brt->h->nodesize==0);
    if ((r = toku_cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt))) {
	if (0) { died0: toku_cachetable_unpin(brt->cf, off, 0, 0); }
	return r;
    }
    printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    printf(" %lld", off/brt->h->nodesize);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++) {
	    if ((r=show_brtnode_blocknumbers(brt, BNC_DISKOFF(node, i)))) goto died0;
	}
    }
    r = toku_cachetable_unpin(brt->cf, off, 0, 0);
    return r;
}

#if 0
int show_brt_blocknumbers (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    printf("BRT %p has blocks:", brt);
    if ((r=show_brtnode_blocknumbers (brt, *rootp, 0))) goto died0;
    printf("\n");
    if ((r = toku_unpin_brt_header(brt))!=0) return r;
    return 0;
}
#endif


int toku_brt_dbt_set_key(BRT brt, DBT *ybt, bytevec val, ITEMLEN vallen) {
    int r = toku_dbt_set_value(ybt, val, vallen, &brt->skey);
    return r;
}

int toku_brt_dbt_set_value(BRT brt, DBT *ybt, bytevec val, ITEMLEN vallen) {
    int r = toku_dbt_set_value(ybt, val, vallen, &brt->sval);
    return r;
}

#ifdef FOO
/* search in a node's child */
static int brt_search_child(BRT brt, BRTNODE node, int childnum, brt_search_t *search, DBT *newkey, DBT *newval, TOKULOGGER logger) {
    int r, rr;

    /* if the child's buffer is not empty then try to empty it */
    if (BNC_NBYTESINBUF(node, childnum) > 0) {
        rr = maybe_push_some_brt_cmds_down(brt, node, childnum, logger);
        if (rr!=0) return rr;
        /* push down may cause a child split, so childnum may not be appropriate, and the node itself may split, so retry */
        return EAGAIN;
    }

    void *node_v;
    rr = toku_cachetable_get_and_pin(brt->cf, BNC_DISKOFF(node,childnum), &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(rr == 0);

    for (;;) {
        BRTNODE childnode = node_v;
        BRT_SPLIT childsplit; brt_split_init(&childsplit);
        r = brt_search_node(brt, childnode, search, newkey, newval, &childsplit, logger);

        if (childsplit.did_split) {
            rr = handle_split_of_child(brt, node, childnum, childsplit.nodea, childsplit.nodeb, &childsplit.splitk,
                                       &split->did_split, &split->nodea, &split->nodeb, &split->splitk, logger);
            assert(rr == 0);
            break;
        } else {
            if (r == EAGAIN) 
                continue;
            rr = toku_cachetable_unpin(brt->cf, childnode->thisnodename, childnode->dirty, brtnode_size(childnode)); 
            assert(rr == 0);
            break;
        }
    }

    return r;
}

static int brt_search_nonleaf_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, TOKULOGGER logger) {
    int c;

 restart:
    {
    /* binary search is overkill for a small array */
	int child[node->u.n.n_children];

	/* scan left to right or right to left depending on the search direction */
	for (c = 0; c < node->u.n.n_children; c++) 
	    child[c] = search->direction & BRT_SEARCH_LEFT ? c : node->u.n.n_children - 1 - c;
	
	for (c = 0; c < node->u.n.n_children-1; c++) {
	    int p = search->direction & BRT_SEARCH_LEFT ? child[c] : child[c] - 1;
	    struct kv_pair *pivot = node->u.n.childkeys[p];
	    DBT pivotkey, pivotval;
	    if (search->compare(search, 
				toku_fill_dbt(&pivotkey, kv_pair_key(pivot), kv_pair_keylen(pivot)), 
				brt->flags & TOKU_DB_DUPSORT ? toku_fill_dbt(&pivotval, kv_pair_val(pivot), kv_pair_vallen(pivot)): 0)) {
		// We know which child we want to search.  First make sure the buffer is empty.
		r = flush_toward_child(brt, node, child[c], logger, &did_split);
		if (did_split) goto restart;
		// If we didn't split, then the buffer is empty, so search that child
		r=search_that_child();
		// Now that child may be bent out of shape
???
		

            int r = brt_search_child(brt, node, child[c], search, newkey, newval, logger);
	    // searching the child can cause it to get bent out of shape
	    int rr = maybe_fixup_nonroot(brt, node, child[c], logger);
	    if (rr!=0) return rr;
            if (r == 0) return r;
        }
    }
    
    /* check the first (left) or last (right) node if nothing has been found */
    if (r == DB_NOTFOUND && c == node->u.n.n_children-1)
        r = brt_search_child(brt, node, child[c], search, newkey, newval, split, logger);
    

    return r;
}

static int brt_search_leaf_node(BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval) {
    PMA pma = node->u.l.buffer;
    int r = toku_pma_search(pma, search, newkey, newval);
    return r;
}

static int brt_search_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, TOKULOGGER logger) {
    if (node->height > 0)
        return brt_search_nonleaf_node(brt, node, search, newkey, newval, logger);
    else
        return brt_search_leaf_node(node, search, newkey, newval);
}

int toku_brt_search(BRT brt, brt_search_t *search, DBT *newkey, DBT *newval, TOKULOGGER logger) {
    int r, rr;

    rr = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    if (rr!=0) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return rr;
    }

    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt);

    void *node_v;
    BRTNODE node;
    rr = toku_cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    if (rr!=0) {
	if (0) { died1: unpin_brtnode(brt, node); }
	goto died0;
    }
    node = node_v;    

    r = brt_search_node(brt, node, search, newkey, newval, logger);
    
    rr = maybe_fixup_root(brt, node, rootp, logger);
    if (rr!=0) { goto died1; }
    rr = unpin_brtnode(brt, node);
    if (rr!=0) { goto died0; }
    rr = toku_unpin_brt_header(brt); 
    if (rr!=0) return rr;

    return r;
}

static inline void dbt_cleanup(DBT *dbt) {
    if (dbt->data && (dbt->flags & DB_DBT_MALLOC)) {
        toku_free_n(dbt->data, dbt->size); dbt->data = 0; 
    }
}

static inline void brt_cursor_cleanup(BRT_CURSOR cursor) {
    dbt_cleanup(&cursor->key);
    dbt_cleanup(&cursor->val);
}

static inline int brt_cursor_not_set(BRT_CURSOR cursor) {
    return cursor->key.data == 0 || cursor->val.data == 0;
}

BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c) {
    return brt_cursor_not_set(c);
}

static inline void brt_cursor_set_key_val(BRT_CURSOR cursor, DBT *newkey, DBT *newval) {
    brt_cursor_cleanup(cursor);
    cursor->key = *newkey; memset(newkey, 0, sizeof *newkey);
    cursor->val = *newval; memset(newval, 0, sizeof *newval);
}

int toku_brt_cursor(BRT brt, BRT_CURSOR *cursorptr) {
    BRT_CURSOR cursor = toku_malloc(sizeof *cursor);
    if (cursor == 0)
        return ENOMEM;
    cursor->brt = brt;
    toku_init_dbt(&cursor->key);
    toku_init_dbt(&cursor->val);
    list_push(&brt->cursors, &cursor->cursors_link);
    *cursorptr = cursor;
    return 0;
}

int toku_brt_cursor_close(BRT_CURSOR cursor) {
    brt_cursor_cleanup(cursor);
    list_remove(&cursor->cursors_link);
    toku_free_n(cursor, sizeof *cursor);
    return 0;
}

static inline int compare_k_x(BRT brt, DBT *k, DBT *x) {
    return brt->compare_fun(brt->db, k, x);
}

static inline int compare_v_y(BRT brt, DBT *v, DBT *y) {
    return brt->dup_compare(brt->db, v, y);
}

static inline int compare_kv_xy(BRT brt, DBT *k, DBT *v, DBT *x, DBT *y) {
    int cmp = brt->compare_fun(brt->db, k, x);
    if (cmp == 0 && v && y)
        cmp = brt->dup_compare(brt->db, v, y);
    return cmp;
}

static inline int brt_cursor_copyout(BRT_CURSOR cursor, DBT *key, DBT *val) {
    int r = 0;
    if (key) 
        r = toku_dbt_set_value(key, cursor->key.data, cursor->key.size, &cursor->brt->skey);
    if (r == 0 && val)
        r = toku_dbt_set_value(val, cursor->val.data, cursor->val.size, &cursor->brt->sval);
    return r;
}

static int brt_cursor_compare_set(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) <= 0; /* return min xy: kv <= xy */
}

static int brt_cursor_current(BRT_CURSOR cursor, int op, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    if (brt_cursor_not_set(cursor))
        return EINVAL;
    if (op == DB_CURRENT) {
        DBT newkey; toku_init_dbt(&newkey);
        DBT newval; toku_init_dbt(&newval);

        brt_search_t search; brt_search_init(&search, brt_cursor_compare_set, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
        int r = toku_brt_search(cursor->brt, &search, &newkey, &newval, logger);
        if (r != 0 || compare_kv_xy(cursor->brt, &cursor->key, &cursor->val, &newkey, &newval) != 0)
            return DB_KEYEMPTY;
    }
    return brt_cursor_copyout(cursor, outkey, outval);
}

/* search for the first kv pair that matches the search object */
static int brt_cursor_search(BRT_CURSOR cursor, brt_search_t *search, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;

    int r = toku_brt_search(cursor->brt, search, &newkey, &newval, logger);
    if (r == 0) {
        brt_cursor_set_key_val(cursor, &newkey, &newval);
        r = brt_cursor_copyout(cursor, outkey, outval);
    }
    dbt_cleanup(&newkey);
    dbt_cleanup(&newval);
    return r;
}

/* search for the kv pair that matches the search object and is equal to kv */
static int brt_cursor_search_eq_kv_xy(BRT_CURSOR cursor, brt_search_t *search, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;

    int r = toku_brt_search(cursor->brt, search, &newkey, &newval, logger);
    if (r == 0) {
        if (compare_kv_xy(cursor->brt, search->k, search->v, &newkey, &newval) == 0) {
            brt_cursor_set_key_val(cursor, &newkey, &newval);
            r = brt_cursor_copyout(cursor, outkey, outval);
        } else 
            r = DB_NOTFOUND;
    }
    dbt_cleanup(&newkey);
    dbt_cleanup(&newval);
    return r;
}

/* search for the kv pair that matches the search object and is equal to k */
static int brt_cursor_search_eq_k_x(BRT_CURSOR cursor, brt_search_t *search, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;

    int r = toku_brt_search(cursor->brt, search, &newkey, &newval, logger);
    if (r == 0) {
        if (compare_k_x(cursor->brt, search->k, &newkey) == 0) {
            brt_cursor_set_key_val(cursor, &newkey, &newval);
            r = brt_cursor_copyout(cursor, outkey, outval);
        } else 
            r = DB_NOTFOUND;
    }
    dbt_cleanup(&newkey);
    dbt_cleanup(&newval);
    return r;
}

static int brt_cursor_compare_one(brt_search_t *search, DBT *x, DBT *y) {
    search = search; x = x; y = y;
    return 1;
}

static int brt_cursor_first(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_LEFT, 0, 0, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_last(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_RIGHT, 0, 0, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_next(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) < 0; /* return min xy: kv < xy */
}

static int brt_cursor_next(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_next_nodup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context; y = y;
    return compare_k_x(brt, search->k, x) < 0; /* return min x: k < x */
}

static int brt_cursor_next_nodup(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next_nodup, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_next_dup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp < 0) 
        return 1; 
    else 
        return keycmp == 0 && y && compare_v_y(brt, search->v, y) < 0; /* return min xy: k <= x && v < y */
}

static int brt_cursor_next_dup(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next_dup, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search_eq_k_x(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_get_both_range(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp < 0) 
        return 1; 
    else
        return keycmp == 0 && (y == 0 || compare_v_y(brt, search->v, y) <= 0); /* return min xy: k <= x && v <= y */
}

static int brt_cursor_get_both_range(BRT_CURSOR cursor, DBT *key, DBT *val, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_get_both_range, BRT_SEARCH_LEFT, key, val, cursor->brt);
    return brt_cursor_search_eq_k_x(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_prev(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) > 0; /* return max xy: kv > xy */
}

static int brt_cursor_prev(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_prev_nodup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context; y = y;
    return compare_k_x(brt, search->k, x) > 0; /* return max x: k > x */
}

static int brt_cursor_prev_nodup(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev_nodup, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

#ifdef DB_PREV_DUP

static int brt_cursor_compare_prev_dup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp > 0) 
        return 1; 
    else 
        return keycmp == 0 && y && compare_v_y(brt, search->v, y) > 0; /* return max xy: k >= x && v > y */
}

static int brt_cursor_prev_dup(BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev_dup, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search_eq_k_x(cursor, &search, outkey, outval, logger);
}

#endif

static int brt_cursor_compare_set_range(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) <= 0; /* return kv <= xy */
}

static int brt_cursor_set(BRT_CURSOR cursor, DBT *key, DBT *val, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, val, cursor->brt);
    return brt_cursor_search_eq_kv_xy(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_set_range(BRT_CURSOR cursor, DBT *key, DBT *outkey, DBT *outval, TOKULOGGER logger) {
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, 0, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

int toku_brt_cursor_get (BRT_CURSOR cursor, DBT *key, DBT *val, int get_flags, TOKUTXN txn) {
    int r;

    int op = get_flags & DB_OPFLAGS_MASK;
    TOKULOGGER logger = toku_txn_logger(txn);
    if (get_flags & ~DB_OPFLAGS_MASK) 
        return EINVAL;

    switch (op) {
    case DB_CURRENT:
    case DB_CURRENT_BINDING:
        r = brt_cursor_current(cursor, op, key, val, logger);
        break;
    case DB_FIRST:
        r = brt_cursor_first(cursor, key, val, logger);
        break;
    case DB_LAST:
        r = brt_cursor_last(cursor, key, val, logger);
        break;
    case DB_NEXT:
        if (brt_cursor_not_set(cursor))
            r = brt_cursor_first(cursor, key, val, logger);
        else
            r = brt_cursor_next(cursor, key, val, logger);
        break;
    case DB_NEXT_DUP:
        if (brt_cursor_not_set(cursor))
            r = EINVAL;
        else
            r = brt_cursor_next_dup(cursor, key, val, logger);
        break;
    case DB_NEXT_NODUP:
        if (brt_cursor_not_set(cursor))
            r = brt_cursor_first(cursor, key, val, logger);
        else
            r = brt_cursor_next_nodup(cursor, key, val, logger);
        break;
    case DB_PREV:
        if (brt_cursor_not_set(cursor))
            r = brt_cursor_last(cursor, key, val, logger);
        else
            r = brt_cursor_prev(cursor, key, val, logger);
        break;
#ifdef DB_PREV_DUP
    case DB_PREV_DUP:
        if (brt_cursor_not_set(cursor))
            r = EINVAL;
        else
            r = brt_cursor_prev_dup(cursor, key, val, logger);
        break;
#endif
    case DB_PREV_NODUP:
        if (brt_cursor_not_set(cursor))
            r = brt_cursor_last(cursor, key, val, logger);
        else
            r = brt_cursor_prev_nodup(cursor, key, val, logger);
        break;
    case DB_SET:
        r = brt_cursor_set(cursor, key, 0, 0, val, logger);
        break;
    case DB_SET_RANGE:
        r = brt_cursor_set_range(cursor, key, key, val, logger);
        break;
    case DB_GET_BOTH:
        r = brt_cursor_set(cursor, key, val, 0, 0, logger);
        break;
    case DB_GET_BOTH_RANGE:
        r = brt_cursor_get_both_range(cursor, key, val, 0, val, logger);
        break;
    default:
        r = EINVAL;
        break;
    }
    return r;
}

int toku_brt_cursor_delete(BRT_CURSOR cursor, int flags, TOKUTXN txn) {
    if ((flags & ~DB_DELETE_ANY) != 0)
        return EINVAL;
    if (brt_cursor_not_set(cursor))
        return EINVAL;
    int r = 0;
    if (!(flags & DB_DELETE_ANY))
        r = brt_cursor_current(cursor, DB_CURRENT, 0, 0, toku_txn_logger(txn));
    if (r == 0)
        r = toku_brt_delete_both(cursor->brt, &cursor->key, &cursor->val, txn);
    return r;
}

int toku_brt_height_of_root(BRT brt, int *height) {
    // for an open brt, return the current height.
    int r;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt);
    void *node_v;
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, 
				       toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt))) {
	goto died0;
    }
    BRTNODE node = node_v;
    *height = node->height;
    r = unpin_brtnode(brt, node);   assert(r==0);
    r = toku_unpin_brt_header(brt); assert(r==0);
    return 0;
}
#endif
