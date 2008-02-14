/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Buffered repository tree.
 * Observation:  The in-memory representation of a node doesn't have to be the same as the on-disk representation.
 *  Goal for the in-memory representation:  fast
 *  Goal for on-disk: small
 * 
 * So to get this running fast, I'll  make a version that doesn't do range queries:
 *   use a hash table for in-memory
 *   simply write the strings on disk.
 * Later I'll do a PMA or a skiplist for the in-memory version.
 * Also, later I'll convert the format to network order fromn host order.
 * Later, for on disk, I'll compress it (perhaps with gzip, perhaps with the bzip2 algorithm.)
 *
 * The collection of nodes forms a data structure like a B-tree.  The complexities of keeping it balanced apply.
 *
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

extern long long n_items_malloced;

static int malloc_diskblock (DISKOFF *res, BRT brt, int size, TOKULOGGER);
static void verify_local_fingerprint_nonleaf (BRTNODE node);

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

static long brtnode_size(BRTNODE node) {
    long size;
    assert(node->tag == TYP_BRTNODE);
    if (node->height > 0)
        size = node->u.n.n_bytes_in_buffers;
    else
        size = node->u.l.n_bytes_in_buffer;
    return size;
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


void toku_brtnode_flush_callback (CACHEFILE cachefile, DISKOFF nodename, void *brtnode_v, long size __attribute((unused)), BOOL write_me, BOOL keep_me, LSN modified_lsn __attribute__((__unused__)) , BOOL rename_p __attribute__((__unused__))) {
    BRTNODE brtnode = brtnode_v;
//    if ((write_me || keep_me) && (brtnode->height==0)) {
//	toku_pma_verify_fingerprint(brtnode->u.l.buffer, brtnode->rand4fingerprint, brtnode->subtree_fingerprint);
//    }
    if (0) {
	printf("%s:%d toku_brtnode_flush_callback %p keep_me=%d height=%d", __FILE__, __LINE__, brtnode, keep_me, brtnode->height);
	if (brtnode->height==0) printf(" pma=%p", brtnode->u.l.buffer);
	printf("\n");
    }
    //if (modified_lsn.lsn > brtnode->lsn.lsn) brtnode->lsn=modified_lsn;
    assert(brtnode->thisnodename==nodename);
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (write_me) {
	toku_serialize_brtnode_to(toku_cachefile_fd(cachefile), brtnode->thisnodename, brtnode->nodesize, brtnode);
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (!keep_me) {
	toku_brtnode_free(&brtnode);
    }
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
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
    //(*result)->parent_brtnode = 0; /* Don't know it right now. */
    //printf("%s:%d installed %p (offset=%lld)\n", __FILE__, __LINE__, *result, nodename);
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
static int unpin_brtnode (BRT brt, BRTNODE node) {
//    if (node->dirty && txn) {
//	// For now just update the log_lsn.  Later we'll have to deal with the checksums.
//	node->log_lsn = toku_txn_get_last_lsn(txn);
//	//if (node->log_lsn.lsn>33320) printf("%s:%d node%lld lsn=%lld\n", __FILE__, __LINE__, node->thisnodename, node->log_lsn.lsn);
//    }
    return toku_cachetable_unpin(brt->cf, node->thisnodename, node->dirty, brtnode_size(node));
}

typedef struct kvpair {
    bytevec key;
    unsigned int keylen;
    bytevec val;
    unsigned int vallen;
} *KVPAIR;

#if 0
int kvpair_compare (const void *av, const void *bv) {
    const KVPAIR a = (const KVPAIR)av;
    const KVPAIR b = (const KVPAIR)bv;
    int r = toku_keycompare(a->key, a->keylen, b->key, b->keylen);
    //printf("keycompare(%s,\n           %s)-->%d\n", a->key, b->key, r);
    return r;
}
#endif

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

static void create_new_brtnode (BRT t, BRTNODE *result, int height, TOKULOGGER logger) {
    TAGMALLOC(BRTNODE, n);
    int r;
    DISKOFF name;
    r = malloc_diskblock(&name, t, t->h->nodesize, logger);
    assert(r==0);
    assert(n);
    assert(t->h->nodesize>0);
    //printf("%s:%d malloced %lld (and malloc again=%lld)\n", __FILE__, __LINE__, name, malloc_diskblock(t, t->nodesize));
    initialize_brtnode(t, n, name, height);
    *result = n;
    assert(n->nodesize>0);
    //    n->brt            = t;
    //printf("%s:%d putting %p (%lld) parent=%p\n", __FILE__, __LINE__, n, n->thisnodename, parent_brtnode);
    r=toku_cachetable_put(t->cf, n->thisnodename, n, brtnode_size(n),
			  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t);
    assert(r==0);
    r=toku_log_newbrtnode(logger, toku_cachefile_filenum(t->cf), n->thisnodename, height, n->nodesize, (t->flags&TOKU_DB_DUPSORT)!=0, n->rand4fingerprint);
    assert(r==0);
    toku_update_brtnode_loggerlsn(n, logger);
}

static int insert_to_buffer_in_nonleaf (BRTNODE node, int childnum, DBT *k, DBT *v, int type, TXNID xid) {
    unsigned int n_bytes_added = BRT_CMD_OVERHEAD + KEY_VALUE_OVERHEAD + k->size + v->size;
    int r = toku_fifo_enq(BNC_BUFFER(node,childnum), k->data, k->size, v->data, v->size, type, xid);
    if (r!=0) return r;
    node->local_fingerprint += node->rand4fingerprint*toku_calccrc32_cmd(type, xid, k->data, k->size, v->data, v->size);
    BNC_NBYTESINBUF(node,childnum) += n_bytes_added;
    node->u.n.n_bytes_in_buffers += n_bytes_added;
    node->dirty = 1;
    return 0;
}


static int brtleaf_split (TOKULOGGER logger, FILENUM filenum, BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk) {
    BRTNODE B;
    assert(node->height==0);
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    create_new_brtnode(t, &B, 0, logger);
    //printf("leaf_split %lld - %lld %lld\n", node->thisnodename, A->thisnodename, B->thisnodename);
    //printf("%s:%d A PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    //printf("%s:%d B PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    assert(B->nodesize>0);
    assert(node->nodesize>0);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    //printf("%s:%d B is at %lld nodesize=%d\n", __FILE__, __LINE__, B->thisnodename, B->nodesize);
    assert(node->height>0 || node->u.l.buffer!=0);
    int r;
    r = toku_pma_split(logger, filenum,
		       node->thisnodename, node->u.l.buffer, &node->u.l.n_bytes_in_buffer, node->rand4fingerprint, &node->local_fingerprint, &node->log_lsn,
		       splitk,
		       B->thisnodename,    B->u.l.buffer,    &B->u.l.n_bytes_in_buffer,    B->rand4fingerprint,    &B->local_fingerprint,    &B->log_lsn);
    assert(r == 0);
    assert(node->height>0 || node->u.l.buffer!=0);
    /* Remove it from the cache table, and free its storage. */
    //printf("%s:%d old pma = %p\n", __FILE__, __LINE__, node->u.l.buffer);

    *nodea = node;
    *nodeb = B;
    assert(toku_serialize_brtnode_size(node)<node->nodesize);
    assert(toku_serialize_brtnode_size(B)   <B->nodesize);
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
    create_new_brtnode(t, &B, node->height, logger);
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

static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
			    DBT *split,
			    int debug,
			    TOKULOGGER);

/* key is not in the buffer.  Either put the key-value pair in the child, or put it in the node. */
static int push_brt_cmd_down_only_if_it_wont_push_more_else_put_here (BRT t, BRTNODE node, BRTNODE child,
								      BRT_CMD cmd,
								      int childnum_of_node,
								      TOKULOGGER logger) {
    assert(node->height>0); /* Not a leaf. */
    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    unsigned int newsize = toku_serialize_brtnode_size(child) + k->size + v->size + KEY_VALUE_OVERHEAD;
    newsize += (child->height > 0) ? BRT_CMD_OVERHEAD : PMA_ITEM_OVERHEAD;
    int to_child = newsize <= child->nodesize;
    if (toku_brt_debug_mode) {
	printf("%s:%d pushing %s to %s %d", __FILE__, __LINE__, (char*)k->data, to_child? "child" : "hash", childnum_of_node);
	if (childnum_of_node+1<node->u.n.n_children) {
	    DBT k2;
	    printf(" nextsplitkey=%s\n", (char*)node->u.n.childkeys[childnum_of_node]);
	    assert(t->compare_fun(t->db, k, toku_fill_dbt(&k2, node->u.n.childkeys[childnum_of_node], toku_brt_pivot_key_len(t, node->u.n.childkeys[childnum_of_node])))<=0);
	} else {
	    printf("\n");
	}
    }
    int r;
    if (to_child) {
	int again_split=-1; BRTNODE againa,againb;
	DBT againk;
	toku_init_dbt(&againk);
	//printf("%s:%d hello!\n", __FILE__, __LINE__);
	r = brtnode_put_cmd(t, child, cmd,
			    &again_split, &againa, &againb, &againk,
			    0,
			    logger);
	if (r!=0) return r;
	assert(again_split==0); /* I only did the insert if I knew it wouldn't push down, and hence wouldn't split. */
    } else {
	r=insert_to_buffer_in_nonleaf(node, childnum_of_node, k, v, cmd->type, cmd->xid);
    }
    fixup_child_fingerprint(node, childnum_of_node, child, t, logger);
    return r;
}

static int push_a_brt_cmd_down (BRT t, BRTNODE node, BRTNODE child, int childnum,
				BRT_CMD cmd,
				int *child_did_split, BRTNODE *childa, BRTNODE *childb,
				DBT *childsplitk,
				TOKULOGGER logger) {
    //if (debug) printf("%s:%d %*sinserting down\n", __FILE__, __LINE__, debug, "");
    //printf("%s:%d hello!\n", __FILE__, __LINE__);
    assert(node->height>0);
    {
	int r = brtnode_put_cmd(t, child, cmd,
				child_did_split, childa, childb, childsplitk,
				0,
				logger);
	if (r!=0) return r;
    }

    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    //if (debug) printf("%s:%d %*sinserted down child_did_split=%d\n", __FILE__, __LINE__, debug, "", child_did_split);
    node->local_fingerprint -= node->rand4fingerprint*toku_calccrc32_cmdstruct(cmd);
    {
	int r = toku_fifo_deq(BNC_BUFFER(node,childnum));
	//printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
	if (r!=0) return r;
    }
    {
	int n_bytes_removed = (k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD);
	node->u.n.n_bytes_in_buffers -= n_bytes_removed;
	BNC_NBYTESINBUF(node, childnum) -= n_bytes_removed;
        node->dirty = 1;
    }
    if (*child_did_split) {
	fixup_child_fingerprint(node, childnum,   *childa, t, logger);
	fixup_child_fingerprint(node, childnum+1, *childb, t, logger);
    } else {
	fixup_child_fingerprint(node, childnum,   child, t, logger);
    }
    return 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, int debug,  TOKULOGGER logger);

static int split_count=0;

/* NODE is a node with a child.
 * childnum was split into two nodes childa, and childb.  childa is the same as the original child.  childb is a new child.
 * We must slide things around, & move things from the old table to the new tables.
 * We also move things to the new children as much as we can without doing any pushdowns or splitting of the child.
 * We must delete the old buffer (but the old child is already deleted.)
 * We also unpin the new children.
 */
static int handle_split_of_child (BRT t, BRTNODE node, int childnum,
				  BRTNODE childa, BRTNODE childb,
				  DBT *childsplitk, /* the data in the childsplitk is alloc'd and is consumed by this call. */
				  int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				  DBT *splitk,
				  TOKULOGGER logger) {
    assert(node->height>0);
    assert(0 <= childnum && childnum < node->u.n.n_children);
    FIFO      old_h = BNC_BUFFER(node,childnum);
    int       old_count = BNC_NBYTESINBUF(node, childnum);
    int cnum;
    int r;
    assert(node->u.n.n_children<=TREE_FANOUT);

    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d Child %d did split on %s\n", __FILE__, __LINE__, childnum, (char*)childsplitk->data);
	printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    node->dirty = 1;

    //verify_local_fingerprint_nonleaf(node);

    // Slide the children over.
    for (cnum=node->u.n.n_children; cnum>childnum+1; cnum--) {
	node->u.n.childinfos[cnum] = node->u.n.childinfos[cnum-1];
    }
    r = toku_log_addchild(logger, toku_cachefile_filenum(t->cf), node->thisnodename, childnum+1, childb->thisnodename, 0);
    assert(BNC_DISKOFF(node, childnum)==childa->thisnodename);
    BNC_DISKOFF(node, childnum+1) = childb->thisnodename;
    fixup_child_fingerprint(node, childnum,   childa, t, logger);
    fixup_child_fingerprint(node, childnum+1, childb, t, logger);
    r=toku_fifo_create(&BNC_BUFFER(node,childnum));   assert(r==0); // ??? SHould handle this error case
    r=toku_fifo_create(&BNC_BUFFER(node,childnum+1)); assert(r==0);
    BNC_NBYTESINBUF(node, childnum) = 0;
    BNC_NBYTESINBUF(node, childnum+1) = 0;

    // Remove all the cmds from the local fingerprint.  Some may get added in again when we try to push to the child.
    FIFO_ITERATE(old_h, skey, skeylen, sval, svallen, type, xid,
                 node->local_fingerprint -= node->rand4fingerprint*toku_calccrc32_cmd(type, xid, skey, skeylen, sval, svallen));

    // Slide the keys over
    {
	struct kv_pair *pivot = childsplitk->data;
	BYTESTRING bs = { .len  = childsplitk->size,
			  .data = kv_pair_key(pivot) };
	r = toku_log_setpivot(logger, toku_cachefile_filenum(t->cf), node->thisnodename, childnum, bs);
	if (r!=0) return r;

	for (cnum=node->u.n.n_children-1; cnum>childnum; cnum--) {
	    node->u.n.childkeys[cnum] = node->u.n.childkeys[cnum-1];
	}
	if (logger) assert((t->flags&TOKU_DB_DUPSORT)==0); // none of this works for dupsort databases. The size is wrong. The setpivot is wrong.
	node->u.n.childkeys[childnum]= pivot;
	node->u.n.totalchildkeylens += childsplitk->size;
    }

    node->u.n.n_children++;

    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d splitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    node->u.n.n_bytes_in_buffers -= old_count; /* By default, they are all removed.  We might add them back in. */
    /* Keep pushing to the children, but not if the children would require a pushdown */
    FIFO_ITERATE(old_h, skey, skeylen, sval, svallen, type, xid, ({
        DBT skd, svd;
        BRT_CMD_S brtcmd = { type, xid, .u.id= {toku_fill_dbt(&skd, skey, skeylen),
						toku_fill_dbt(&svd, sval, svallen)} };
	//verify_local_fingerprint_nonleaf(childa); 	verify_local_fingerprint_nonleaf(childb);
	int pusha = 0, pushb = 0;
	switch (type) {
	case BRT_INSERT:
	case BRT_DELETE_BOTH:
	case BRT_DELETE:
	    if (type!=BRT_DELETE || 0==(t->flags&TOKU_DB_DUPSORT)) {
		// If it's an INSERT or DELETE_BOTH or there are no duplicates then we just put the command into one subtree
		int cmp = brt_compare_pivot(t, &skd, &svd, childsplitk->data);
		if (cmp <= 0) pusha = 1;
		else          pushb = 1;
	    } else {
		assert(type==BRT_DELETE && t->flags&TOKU_DB_DUPSORT);
		// It is a DELETE and it's a DUPSORT database, in which case if the comparison function comes up 0 we must write the command to both children.  (See #201)
		int cmp = brt_compare_pivot(t, &skd, 0, childsplitk->data);
		if (cmp<=0)   pusha=1;
		if (cmp>=0)   pushb=1;  // Could be that both pusha and pushb are set
	    }
	    if (pusha) {
		// If we already have something in the buffer, we must add the new command to the buffer so that commands don't get out of order.
		if (toku_fifo_n_entries(BNC_BUFFER(node,childnum))==0) {
		    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childa, &brtcmd, childnum, logger);
		} else {
		    r=insert_to_buffer_in_nonleaf(node, childnum, &skd, &svd, type, xid);
		}
	    }
	    if (pushb) {
		// If we already have something in the buffer, we must add the new command to the buffer so that commands don't get out of order.
		if (toku_fifo_n_entries(BNC_BUFFER(node,childnum+1))==0) {
		    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childb, &brtcmd, childnum+1, logger);
		} else {
		    r=insert_to_buffer_in_nonleaf(node, childnum+1, &skd, &svd, type, xid);
		}
	    }
	    //verify_local_fingerprint_nonleaf(childa); 	verify_local_fingerprint_nonleaf(childb); 
	    if (r!=0) printf("r=%d\n", r);
	    assert(r==0);

	    goto ok;


	case BRT_NONE:
	    // Don't have to do anything in this case, can just drop the command
            goto ok;
	}
	printf("Bad type %d\n", type); // Don't use default: because I want a compiler warning if I forget a enum case, and I want a runtime error if the type isn't one of the expected ones.
	assert(0);
     ok: /*nothing*/;
		     }));

    toku_fifo_free(&old_h);

    //verify_local_fingerprint_nonleaf(childa);
    //verify_local_fingerprint_nonleaf(childb);
    //verify_local_fingerprint_nonleaf(node);

    toku_verify_counts(node);
    toku_verify_counts(childa);
    toku_verify_counts(childb);

    r=unpin_brtnode(t, childa);
    assert(r==0);
    r=unpin_brtnode(t, childb);
    assert(r==0);
		
    if (node->u.n.n_children>TREE_FANOUT) {
	//printf("%s:%d about to split having pushed %d out of %d keys\n", __FILE__, __LINE__, i, n_pairs);
	r=brt_nonleaf_split(t, node, nodea, nodeb, splitk, logger);
	if (r!=0) return r;
	//printf("%s:%d did split\n", __FILE__, __LINE__);
	split_count++;
	*did_split=1;
	assert((*nodea)->height>0);
	assert((*nodeb)->height>0);
	assert((*nodea)->u.n.n_children>0);
	assert((*nodeb)->u.n.n_children>0);
	assert(BNC_DISKOFF(*nodea, (*nodea)->u.n.n_children-1)!=0);
	assert(BNC_DISKOFF(*nodeb, (*nodeb)->u.n.n_children-1)!=0);
	assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
	//verify_local_fingerprint_nonleaf(*nodea);
	//verify_local_fingerprint_nonleaf(*nodeb);
    } else {
	*did_split=0;
        if (toku_serialize_brtnode_size(node) > node->nodesize) {
            /* lighten the node by pushing down its buffers.  this may cause
               the current node to split and go away */
            r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, 0, logger);
            assert(r == 0);
        }
	if (*did_split == 0) assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    }
    return 0;
}

static int push_some_brt_cmds_down (BRT t, BRTNODE node, int childnum,
				    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				    DBT *splitk,
				    int debug,
				    TOKULOGGER logger) {
    void *childnode_v;
    BRTNODE child;
    int r;
    assert(node->height>0);
    DISKOFF targetchild = BNC_DISKOFF(node, childnum);
    assert(targetchild>=0 && targetchild<t->h->unused_memory); // This assertion could fail in a concurrent setting since another process might have bumped unused memory.
    r = toku_cachetable_get_and_pin(t->cf, targetchild, &childnode_v, NULL, 
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t);
    if (r!=0) return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, childnode_v);
    child=childnode_v;
    //verify_local_fingerprint_nonleaf(child);
    toku_verify_counts(child);
    //printf("%s:%d height=%d n_bytes_in_buffer = {%d, %d, %d, ...}\n", __FILE__, __LINE__, child->height, child->n_bytes_in_buffer[0], child->n_bytes_in_buffer[1], child->n_bytes_in_buffer[2]);
    if (child->height>0 && child->u.n.n_children>0) assert(BNC_DISKOFF(child, child->u.n.n_children-1)!=0);
    if (debug) printf("%s:%d %*spush_some_brt_cmds_down to %lld\n", __FILE__, __LINE__, debug, "", child->thisnodename);
    /* I am exposing the internals of the hash table here, mostly because I am not thinking of a really
     * good way to do it otherwise.  I want to loop over the elements of the hash table, deleting some as I
     * go.  The FIFO_ITERATE macro will break if I delete something from the hash table. */
  
    if (0) {
	static int count=0;
	count++;
	printf("%s:%d pushing %d count=%d\n", __FILE__, __LINE__, childnum, count);
    }
    {
	bytevec key,val;
	ITEMLEN keylen, vallen;
	//printf("%s:%d Try random_pick, weight=%d \n", __FILE__, __LINE__, BNC_NBYTESINBUF(node, childnum));
	assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))>0);
	int type;
	TXNID xid;
        while(0==toku_fifo_peek(BNC_BUFFER(node,childnum), &key, &keylen, &val, &vallen, &type, &xid)) {
	    int child_did_split=0; BRTNODE childa, childb;
	    DBT hk,hv;
	    DBT childsplitk;

	    BRT_CMD_S brtcmd = { type, xid, .u.id= {toku_fill_dbt(&hk, key, keylen),
						    toku_fill_dbt(&hv, val, vallen)} };

	    //printf("%s:%d random_picked\n", __FILE__, __LINE__);
	    toku_init_dbt(&childsplitk);
	    if (debug) printf("%s:%d %*spush down %s\n", __FILE__, __LINE__, debug, "", (char*)key);
	    r = push_a_brt_cmd_down (t, node, child, childnum,
				     &brtcmd,
				     &child_did_split, &childa, &childb,
				     &childsplitk,
				     logger);

	    if (0){
		unsigned int sum=0;
		FIFO_ITERATE(BNC_BUFFER(node,childnum), subhk __attribute__((__unused__)), hkl, hd __attribute__((__unused__)), hdl, subtype __attribute__((__unused__)), subxid __attribute__((__unused__)),
                             sum+=hkl+hdl+KEY_VALUE_OVERHEAD+BRT_CMD_OVERHEAD);
		printf("%s:%d sum=%d\n", __FILE__, __LINE__, sum);
		assert(sum==BNC_NBYTESINBUF(node, childnum));
	    }
	    if (BNC_NBYTESINBUF(node, childnum)>0) assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))>0);
	    //printf("%s:%d %d=push_a_brt_cmd_down=();  child_did_split=%d (weight=%d)\n", __FILE__, __LINE__, r, child_did_split, BNC_NBYTESINBUF(node, childnum));
	    if (r!=0) return r;
	    if (child_did_split) {
		// If the child splits, we don't push down any further.
		if (debug) printf("%s:%d %*shandle split splitkey=%s\n", __FILE__, __LINE__, debug, "", (char*)childsplitk.data);
		r=handle_split_of_child (t, node, childnum,
					 childa, childb, &childsplitk,
					 did_split, nodea, nodeb, splitk,
					 logger);
		//if (*did_split) {
		//    verify_local_fingerprint_nonleaf(*nodea);
		//    verify_local_fingerprint_nonleaf(*nodeb);
		//}
		return r; /* Don't do any more pushing if the child splits. */ 
	    }
	}
	if (0) printf("%s:%d done random picking\n", __FILE__, __LINE__);
    }
    if (debug) printf("%s:%d %*sdone push_some_brt_cmds_down, unpinning %lld\n", __FILE__, __LINE__, debug, "", targetchild);
    assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    //verify_local_fingerprint_nonleaf(node);
    r=unpin_brtnode(t, child);
    if (r!=0) return r;
    *did_split=0;
    return 0;
}

static int debugp1 (int debug) {
    return debug ? debug+1 : 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, int debug,  TOKULOGGER logger)
/* If the buffer is too full, then push down.  Possibly the child will split.  That may make us split. */
{
    assert(node->height>0);
    if (debug) printf("%s:%d %*sIn maybe_push_down in_buffer=%d childkeylens=%d size=%d\n", __FILE__, __LINE__, debug, "", node->u.n.n_bytes_in_buffers, node->u.n.totalchildkeylens, toku_serialize_brtnode_size(node));
    if (toku_serialize_brtnode_size(node) > node->nodesize ) {
	if (debug) printf("%s:%d %*stoo full, height=%d\n", __FILE__, __LINE__, debug, "", node->height);	
	{
	    /* Push to a child. */
	    /* Find the heaviest child, and push stuff to it.  Keep pushing to the child until we run out.
	     * But if the child pushes something to its child and our buffer has gotten small enough, then we stop pushing. */
	    int childnum;
	    if (0) printf("%s:%d %*sfind_heaviest_data\n", __FILE__, __LINE__, debug, "");
	    find_heaviest_child(node, &childnum);
	    if (0) printf("%s:%d %*spush some down from %lld into %lld (child %d)\n", __FILE__, __LINE__, debug, "", node->thisnodename, BNC_DISKOFF(node, childnum), childnum);
	    assert(BNC_DISKOFF(node, childnum)!=0);
	    int r = push_some_brt_cmds_down(t, node, childnum, did_split, nodea, nodeb, splitk, debugp1(debug), logger);
	    if (r!=0) return r;
	    assert(*did_split==0 || *did_split==1);
	    if (debug) printf("%s:%d %*sdid push_some_brt_cmds_down did_split=%d\n", __FILE__, __LINE__, debug, "", *did_split);
	    if (*did_split) {
		assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);		
		assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);		
		assert((*nodea)->u.n.n_children>0);
		assert((*nodeb)->u.n.n_children>0);
		assert(BNC_DISKOFF(*nodea, (*nodea)->u.n.n_children-1)!=0);
		assert(BNC_DISKOFF(*nodeb, (*nodeb)->u.n.n_children-1)!=0);
		//verify_local_fingerprint_nonleaf(*nodea);
		//verify_local_fingerprint_nonleaf(*nodeb);
	    } else {
		assert(toku_serialize_brtnode_size(node)<=node->nodesize);
	    }
	}
    } else {
	*did_split=0;
	assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    }
    //if (*did_split) {
    //	verify_local_fingerprint_nonleaf(*nodea);
    //	verify_local_fingerprint_nonleaf(*nodeb);
    //} else {
    //  verify_local_fingerprint_nonleaf(node);
    //}
    return 0;
}

static int brt_leaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			     int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			     int debug,
			     TOKULOGGER logger) {
//    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
    assert(node->height==0);
    FILENUM filenum = toku_cachefile_filenum(t->cf);
    if  (cmd->type == BRT_INSERT) {
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;
        int replaced_v_size;
        enum pma_errors pma_status = toku_pma_insert_or_replace(node->u.l.buffer,
								k, v, &replaced_v_size,
								logger, cmd->xid,
								filenum, node->thisnodename, node->rand4fingerprint, &node->local_fingerprint, &node->log_lsn);
        assert(pma_status==BRT_OK);
        //printf("replaced_v_size=%d\n", replaced_v_size);
        if (replaced_v_size>=0) {
            node->u.l.n_bytes_in_buffer += v->size - replaced_v_size;
        } else {
            node->u.l.n_bytes_in_buffer += k->size + v->size + KEY_VALUE_OVERHEAD + PMA_ITEM_OVERHEAD;
        }
        node->dirty = 1;
	
//	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);

        // If it doesn't fit, then split the leaf.
        if (toku_serialize_brtnode_size(node) > node->nodesize) {
            int r = brtleaf_split (logger, filenum, t, node, nodea, nodeb, splitk);
            if (r!=0) return r;
            //printf("%s:%d splitkey=%s\n", __FILE__, __LINE__, (char*)*splitkey);
            split_count++;
            *did_split = 1;
            toku_verify_counts(*nodea); toku_verify_counts(*nodeb);
            if (debug) printf("%s:%d %*snodeb->thisnodename=%lld nodeb->size=%d\n", __FILE__, __LINE__, debug, "", (*nodeb)->thisnodename, (*nodeb)->nodesize);
            assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
            assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
//	    toku_pma_verify_fingerprint((*nodea)->u.l.buffer, (*nodea)->rand4fingerprint, (*nodea)->subtree_fingerprint);
//	    toku_pma_verify_fingerprint((*nodeb)->u.l.buffer, (*nodeb)->rand4fingerprint, (*nodeb)->subtree_fingerprint);
        } else {
            *did_split = 0;
        }
        return 0;

    } else if (cmd->type == BRT_DELETE) {
        u_int32_t delta;
        int r = toku_pma_delete(node->u.l.buffer, cmd->u.id.key, 0, node->rand4fingerprint, &node->local_fingerprint, &delta);
        if (r == BRT_OK) {
            node->u.l.n_bytes_in_buffer -= delta;
            node->dirty = 1;
        }
        *did_split = 0;
        return BRT_OK;

    } else if (cmd->type == BRT_DELETE_BOTH) {
        u_int32_t delta;
        int r = toku_pma_delete(node->u.l.buffer, cmd->u.id.key, cmd->u.id.val, node->rand4fingerprint, &node->local_fingerprint, &delta);
        if (r == BRT_OK) {
            node->u.l.n_bytes_in_buffer -= delta;
            node->dirty = 1;
        }
        *did_split = 0;
        return BRT_OK;
        
    } else {
        return EINVAL;
    }
}

/* find the leftmost child that may contain the key */
static unsigned int brtnode_left_child (BRTNODE node , DBT *k, DBT *d, BRT t) {
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

static unsigned int brtnode_right_child (BRTNODE node, DBT *k, DBT *data, BRT t) {
    return brtnode_left_child(node, k, data, t);
}

/* put a cmd into a nodes child */
static int brt_nonleaf_put_cmd_child_node (BRT t, BRTNODE node, BRT_CMD cmd,
                                           int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, 
                                           int debug, TOKULOGGER logger, int childnum, int maybe) {
    int r;
    void *child_v;
    BRTNODE child;
    int child_did_split;
    BRTNODE childa, childb;
    DBT childsplitk;

    *did_split = 0;

    if (maybe) 
        r = toku_cachetable_maybe_get_and_pin(t->cf, BNC_DISKOFF(node, childnum), &child_v);
    else 
        r = toku_cachetable_get_and_pin(t->cf, BNC_DISKOFF(node, childnum), &child_v, NULL, 
					toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t);
    if (r != 0)
        return r;

    child = child_v;

    child_did_split = 0;
    r = brtnode_put_cmd(t, child, cmd,
                        &child_did_split, &childa, &childb, &childsplitk, debug, logger);
    if (r != 0) {
        /* putting to the child failed for some reason, so unpin the child and return the error code */
	int rr = unpin_brtnode(t, child);
        assert(rr == 0);
        return r;
    }
    if (child_did_split) {
        if (0) printf("brt_nonleaf_insert child_split %p\n", child);
        assert(cmd->type <= BRT_DELETE_BOTH);
        r = handle_split_of_child(t, node, childnum,
                                  childa, childb, &childsplitk,
                                  did_split, nodea, nodeb, splitk,
                                  logger);
        assert(r == 0);
    } else {
	//verify_local_fingerprint_nonleaf(child);
	fixup_child_fingerprint(node, childnum, child, t, logger);
	int rr = unpin_brtnode(t, child);
        assert(rr == 0);
    }
    return r;
}

int toku_brt_do_push_cmd = 1;

/* put a cmd into a node at childnum */
static int brt_nonleaf_put_cmd_child (BRT t, BRTNODE node, BRT_CMD cmd,
                                      int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
                                      int debug, TOKULOGGER logger, unsigned int childnum, int can_push, int *do_push_down) {
    //verify_local_fingerprint_nonleaf(node);

    /* try to push the cmd to the subtree if the buffer is empty and pushes are enabled */
    if (BNC_NBYTESINBUF(node, childnum) == 0 && can_push && toku_brt_do_push_cmd) {
        int r = brt_nonleaf_put_cmd_child_node(t, node, cmd, did_split, nodea, nodeb, splitk, debug, logger, childnum, 1);
        if (r == 0)
            return r;
    }
    //verify_local_fingerprint_nonleaf(node);

    /* append the cmd to the child buffer */
    {
        int type = cmd->type;
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;

	int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
        int r=toku_fifo_enq(BNC_BUFFER(node,childnum), k->data, k->size, v->data, v->size, type, cmd->xid);
	assert(r==0);
	node->local_fingerprint += node->rand4fingerprint * toku_calccrc32_cmd(type, cmd->xid, k->data, k->size, v->data, v->size);
	node->u.n.n_bytes_in_buffers += diff;
	BNC_NBYTESINBUF(node, childnum) += diff;
        node->dirty = 1;
    }
    *do_push_down = 1;
    return 0;
}

static int brt_nonleaf_insert_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
                                   int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
                                   int debug, TOKULOGGER logger) {
    //verify_local_fingerprint_nonleaf(node);
    unsigned int childnum;
    int r;

    /* find the right subtree */
    childnum = brtnode_right_child(node, cmd->u.id.key, cmd->u.id.val, t);

    /* put the cmd in the subtree */
    int do_push_down = 0;
    r = brt_nonleaf_put_cmd_child(t, node, cmd, did_split, nodea, nodeb, splitk, debug, logger, childnum, 1, &do_push_down);
    if (r != 0) return r;

    /* maybe push down */
    if (do_push_down) {
        if (debug) printf("%s:%d %*sDoing maybe_push_down\n", __FILE__, __LINE__, debug, "");
        //verify_local_fingerprint_nonleaf(node);
        r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, debugp1(debug), logger);
        if (r!=0) return r;
        if (debug) printf("%s:%d %*sDid maybe_push_down\n", __FILE__, __LINE__, debug, "");
        if (*did_split) {
            assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
            assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
            assert((*nodea)->u.n.n_children>0);
            assert((*nodeb)->u.n.n_children>0);
            assert(BNC_DISKOFF(*nodea, (*nodea)->u.n.n_children-1)!=0);
            assert(BNC_DISKOFF(*nodeb, (*nodeb)->u.n.n_children-1)!=0);
            toku_verify_counts(*nodea);
            toku_verify_counts(*nodeb);
        } else {
            assert(toku_serialize_brtnode_size(node)<=node->nodesize);
            toku_verify_counts(node);
        }
        //if (*did_split) {
        //	verify_local_fingerprint_nonleaf(*nodea);
        //	verify_local_fingerprint_nonleaf(*nodeb);
        //} else {
        //	verify_local_fingerprint_nonleaf(node);
        //}
    }
    return 0;
}

/* delete in all subtrees starting from the left most one which contains the key */
static int brt_nonleaf_delete_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
                                   int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
                                   int debug,
                                   TOKULOGGER logger) {
    int r;

    /* find all children that need a delete cmd */
    int delchild[TREE_FANOUT], delidx = 0;
    inline void delchild_append(int i) {
        if (delidx == 0 || delchild[delidx-1] != i) 
            delchild[delidx++] = i;
    }
    int i;
    for (i = 0; i < node->u.n.n_children-1; i++) {
        int cmp = brt_compare_pivot(t, cmd->u.id.key, 0, node->u.n.childkeys[i]);
        if (cmp > 0) {
            continue;
        } else if (cmp < 0) {
            delchild_append(i);
            break;
        } else if (t->flags & TOKU_DB_DUPSORT) {
            delchild_append(i);
            delchild_append(i+1);
        } else {
            delchild_append(i);
            break;
        }
    }

    if (delidx == 0)
        delchild_append(node->u.n.n_children-1);

    /* issue the delete cmd to all of the children found previously */
    int do_push_down = 0;
    for (i=0; i<delidx; i++) {
        r = brt_nonleaf_put_cmd_child(t, node, cmd, did_split, nodea, nodeb, splitk, debug, logger, delchild[i], delidx == 1, &do_push_down);
        assert(r == 0);
    }

    if (do_push_down) {
        /* maybe push down */
        if (debug) printf("%s:%d %*sDoing maybe_push_down\n", __FILE__, __LINE__, debug, "");
        //verify_local_fingerprint_nonleaf(node);
        r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, debugp1(debug), logger);
        if (r!=0) return r;
        if (debug) printf("%s:%d %*sDid maybe_push_down\n", __FILE__, __LINE__, debug, "");
        if (*did_split) {
            assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
            assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
            assert((*nodea)->u.n.n_children>0);
            assert((*nodeb)->u.n.n_children>0);
            assert(BNC_DISKOFF(*nodea,(*nodea)->u.n.n_children-1)!=0);
            assert(BNC_DISKOFF(*nodeb,(*nodeb)->u.n.n_children-1)!=0);
            toku_verify_counts(*nodea);
            toku_verify_counts(*nodeb);
        } else {
            assert(toku_serialize_brtnode_size(node)<=node->nodesize);
            toku_verify_counts(node);
        }
        //if (*did_split) {
        //	verify_local_fingerprint_nonleaf(*nodea);
        //	verify_local_fingerprint_nonleaf(*nodeb);
        //} else {
        //	verify_local_fingerprint_nonleaf(node);
        //}
    }
    return 0;
}
static int brt_nonleaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
				int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				DBT *splitk,
				int debug,
				TOKULOGGER logger) {
    if (cmd->type == BRT_INSERT || cmd->type == BRT_DELETE_BOTH) {
        return brt_nonleaf_insert_cmd(t, node, cmd, did_split, nodea, nodeb, splitk, debug, logger);
    } else if (cmd->type == BRT_DELETE) {
        return brt_nonleaf_delete_cmd(t, node, cmd, did_split, nodea, nodeb, splitk, debug, logger);
    } else
        return EINVAL;
}


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

static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			    int debug,
			    TOKULOGGER logger) {
    //static int counter=0; // FOO
    //static int oldcounter=0;
    //int        tmpcounter;
    //u_int32_t  oldfingerprint=node->local_fingerprint;
    int r;
    //counter++; tmpcounter=counter;
    if (node->height==0) {
//	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
	r = brt_leaf_put_cmd(t, node, cmd,
			     did_split, nodea, nodeb, splitk,
			     debug, logger);
    } else {
	r = brt_nonleaf_put_cmd(t, node, cmd,
				did_split, nodea, nodeb, splitk,
				debug, logger);
    }
    //oldcounter=tmpcounter;
    // Watch out.  If did_split then the original node is no longer allocated.
    if (*did_split) {
	assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
//	if ((*nodea)->height==0) {
//	    toku_pma_verify_fingerprint((*nodea)->u.l.buffer, (*nodea)->rand4fingerprint, (*nodea)->subtree_fingerprint);
//	    toku_pma_verify_fingerprint((*nodeb)->u.l.buffer, (*nodeb)->rand4fingerprint, (*nodeb)->subtree_fingerprint);
//	}
    } else {
	assert(toku_serialize_brtnode_size(node)<=node->nodesize);
//	if (node->height==0) {
//	    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->local_fingerprint);
//	} else {
//	    verify_local_fingerprint_nonleaf(node);
//	}
    }
    //if (node->local_fingerprint==3522421844U) {
//    if (*did_split) {
//	verify_local_fingerprint_nonleaf(*nodea);
//	verify_local_fingerprint_nonleaf(*nodeb);
//    }
    return r;
}

int toku_brt_create_cachetable(CACHETABLE *ct, long cachesize, LSN initial_lsn, TOKULOGGER logger) {
    if (cachesize == 0)
        cachesize = 128*1024*1024;
    return toku_create_cachetable(ct, cachesize, initial_lsn, logger);
}

static int setup_initial_brt_root_node (BRT t, DISKOFF offset, TOKULOGGER logger) {
    int r;
    TAGMALLOC(BRTNODE, node);
    assert(node);
    //printf("%s:%d\n", __FILE__, __LINE__);
    initialize_brtnode(t, node,
		       offset, /* the location is one nodesize offset from 0. */
		       0);
    //    node->brt = t;
    if (0) {
	printf("%s:%d for tree %p node %p mdict_create--> %p\n", __FILE__, __LINE__, t, node, node->u.l.buffer);
	printf("%s:%d put root at %lld\n", __FILE__, __LINE__, offset);
    }
    //printf("%s:%d putting %p (%lld)\n", __FILE__, __LINE__, node, node->thisnodename);
    r=toku_cachetable_put(t->cf, offset, node, brtnode_size(node),
			  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    toku_verify_counts(node);
//    verify_local_fingerprint_nonleaf(node);
    toku_log_newbrtnode(logger, toku_cachefile_filenum(t->cf), offset, 0, t->h->nodesize, (t->flags&TOKU_DB_DUPSORT)!=0, node->rand4fingerprint);
    toku_update_brtnode_loggerlsn(node, logger);
    r=unpin_brtnode(t, node);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    return 0;
}

//#define BRT_TRACE
#ifdef BRT_TRACE
#define WHEN_BRTTRACE(x) x
#else
#define WHEN_BRTTRACE(x) ((void)0)
#endif

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

int toku_brt_open(BRT t, const char *fname, const char *fname_in_env, const char *dbname, int is_create, int only_create, int load_flags, CACHETABLE cachetable, TOKUTXN txn, DB *db) {

    /* If dbname is NULL then we setup to hold a single tree.  Otherwise we setup an array. */
    int r;
    char *malloced_name=0;
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE: %s:%d toku_brt_open(%s, \"%s\", %d, %p, %d, %p)\n",
			  __FILE__, __LINE__, fname, dbname, is_create, newbrt, nodesize, cachetable));
    if (0) { died0:  assert(r); return r; }

    assert(is_create || !only_create);
    assert(!load_flags || !only_create);
    if (dbname) {
	malloced_name = toku_strdup(dbname);
	if (malloced_name==0) {
	    r = ENOMEM;
	    if (0) { died0a: if(malloced_name) toku_free(malloced_name); }
	    goto died0;
	}
    }
    t->database_name = malloced_name;
    t->db = db;
    {
	int fd = open(fname, O_RDWR, 0777);
	r = errno;
	if (fd==-1 && errno==ENOENT) {
	    if (!is_create) {
		t->database_name=0;
		goto died0a;
	    }
	    fd = open(fname, O_RDWR | O_CREAT, 0777);
	    r = errno;
	    if (fd==-1) {
		t->database_name=0;
		goto died0a;
	    }
	    toku_logger_log_fcreate(txn, fname_in_env, 0777);
	}
	r=toku_cachetable_openfd(&t->cf, cachetable, fd);
	toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(t->cf));
    }
    if (r!=0) {
	if (0) { died1: toku_cachefile_close(&t->cf); }
        t->database_name = 0;
	goto died0a;
    }
    assert(t->nodesize>0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    if (0) {
    died_after_read_and_pin:
	toku_cachetable_unpin(t->cf, 0, 0, 0); // unpin the header
	goto died1;
    }
    if (is_create) {
	r = toku_read_and_pin_brt_header(t->cf, &t->h);
	if (r==-1) {
	    /* construct a new header. */
	    if ((MALLOC(t->h))==0) {
		assert(errno==ENOMEM);
		r = ENOMEM;
		if (0) { died2: toku_free(t->h); }
		t->h=0;
		goto died_after_read_and_pin;
	    }
	    t->h->dirty=1;
            t->h->flags = t->flags;
	    t->h->nodesize=t->nodesize;
	    t->h->freelist=-1;
	    t->h->unused_memory=2*t->nodesize;
	    if (dbname) {
		t->h->unnamed_root = -1;
		t->h->n_named_roots = 1;
		if ((MALLOC_N(1, t->h->names))==0)             { assert(errno==ENOMEM); r=ENOMEM; if (0) { died3: toku_free(t->h->names); } goto died2; }
		if ((MALLOC_N(1, t->h->roots))==0)             { assert(errno==ENOMEM); r=ENOMEM; if (0) { died4: toku_free(t->h->roots); } goto died3; }
		if ((t->h->names[0] = toku_strdup(dbname))==0) { assert(errno==ENOMEM); r=ENOMEM; if (0) { died5: toku_free(t->h->names[0]); } goto died4; }
		t->h->roots[0] = t->nodesize;
	    } else {
		t->h->unnamed_root = t->nodesize;
		t->h->n_named_roots = -1;
		t->h->names=0;
		t->h->roots=0;
	    }
	    if ((r=toku_logger_log_header(txn, toku_cachefile_filenum(t->cf), t->h)))                               { goto died6; }
	    if ((r=setup_initial_brt_root_node(t, t->nodesize, toku_txn_logger(txn)))!=0) { died6: if (dbname) goto died5; else goto died2;	}
	    if ((r=toku_cachetable_put(t->cf, 0, t->h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0))) { goto died6; }
	}
	else if (r!=0) {
	    goto died_after_read_and_pin;
	}
	else {
	    int i;
	    assert(r==0);
	    assert(dbname);
	    if (t->h->unnamed_root!=-1) { r=EINVAL; goto died_after_read_and_pin; } // Cannot create a subdb in a file that is not enabled for subdbs
	    assert(t->h->n_named_roots>=0);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    if (only_create) {
			r = EEXIST;
			goto died_after_read_and_pin;
		    }
		    else goto found_it;
		}
	    }
	    if ((t->h->names = toku_realloc(t->h->names, (1+t->h->n_named_roots)*sizeof(*t->h->names))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto died_after_read_and_pin; }
	    if ((t->h->roots = toku_realloc(t->h->roots, (1+t->h->n_named_roots)*sizeof(*t->h->roots))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto died_after_read_and_pin; }
	    t->h->n_named_roots++;
	    if ((t->h->names[t->h->n_named_roots-1] = toku_strdup(dbname)) == 0)                                { assert(errno==ENOMEM); r=ENOMEM; goto died_after_read_and_pin; }
	    //printf("%s:%d t=%p\n", __FILE__, __LINE__, t);
	    r = malloc_diskblock_header_is_in_memory(&t->h->roots[t->h->n_named_roots-1], t, t->h->nodesize, toku_txn_logger(txn));
	    if (r!=0) goto died_after_read_and_pin;
	    t->h->dirty = 1;
	    if ((r=setup_initial_brt_root_node(t, t->h->roots[t->h->n_named_roots-1], toku_txn_logger(txn)))!=0) goto died_after_read_and_pin;
	}
    } else {
	if ((r = toku_read_and_pin_brt_header(t->cf, &t->h))!=0) goto died1;
	if (!dbname) {
	    if (t->h->n_named_roots!=-1) { r = EINVAL; goto died_after_read_and_pin; } // requires a subdb
	} else {
	    int i;
	    if (t->h->n_named_roots==-1) { r=EINVAL; goto died_after_read_and_pin; } // no suddbs in the db
	    // printf("%s:%d n_roots=%d\n", __FILE__, __LINE__, t->h->n_named_roots);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    goto found_it;
		}

	    }
	    r=ENOENT; /* the database doesn't exist */
	    goto died_after_read_and_pin;
	}
    found_it:
        t->nodesize = t->h->nodesize;                 /* inherit the pagesize from the file */
        if (t->flags != t->h->flags) {                /* flags must match */
            if (load_flags) t->flags = t->h->flags;
            else {r = EINVAL; goto died_after_read_and_pin;}
        }
    }
    assert(t->h);
    if ((r = toku_unpin_brt_header(t)) !=0) goto died1; // it's unpinned
    assert(t->h==0);
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE -> %p\n", t));
    return 0;
}

int toku_brt_remove_subdb(BRT brt, const char *dbname, u_int32_t flags) {
    int r;
    int r2 = 0;
    int i;
    int found = -1;

    assert(flags == 0);
    r = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    //TODO: What if r != 0? Is this possible?
    //  We just called toku_brt_open, so it should exist...
    assert(r==0);  

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
    if ((brt->h->names = toku_realloc(brt->h->names, (brt->h->n_named_roots)*sizeof(*brt->h->names))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto error; }
    if ((brt->h->roots = toku_realloc(brt->h->roots, (brt->h->n_named_roots)*sizeof(*brt->h->roots))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto error; }

error:
    r2 = toku_unpin_brt_header(brt);
    assert(r2==0);//TODO: Can r2 be non 0?
    assert(brt->h==0);
    return r ? r : r2;
}

// This one has no env
int toku_open_brt (const char *fname, const char *dbname, int is_create, BRT *newbrt, int nodesize, CACHETABLE cachetable, TOKUTXN txn,
	      int (*compare_fun)(DB*,const DBT*,const DBT*), DB *db) {
    BRT brt;
    int r;
    const int only_create = 0;
    const int load_flags  = 0;

    r = toku_brt_create(&brt);
    if (r != 0)
        return r;
    toku_brt_set_nodesize(brt, nodesize);
    toku_brt_set_bt_compare(brt, compare_fun);

    r = toku_brt_open(brt, fname, fname, dbname, is_create, only_create, load_flags, cachetable, txn, db);
    if (r != 0) {
        return r;
    }

    *newbrt = brt;
    return r;
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

int toku_brt_debug_mode = 0;//strcmp(key,"hello387")==0;

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

static int brt_init_new_root(BRT brt, BRTNODE nodea, BRTNODE nodeb, DBT splitk, CACHEKEY *rootp, TOKULOGGER logger, BRTNODE *newrootp) {
    TAGMALLOC(BRTNODE, newroot);
    int r;
    int new_height = nodea->height+1;
    int new_nodesize = brt->h->nodesize;
    DISKOFF newroot_diskoff;
    r=malloc_diskblock(&newroot_diskoff, brt, new_nodesize, logger);
    assert(r==0);
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
    //printf("new_root %lld %d %lld %lld\n", newroot_diskoff, newroot->height, nodea->thisnodename, nodeb->thisnodename);
    newroot->u.n.n_children=2;
    //printf("%s:%d Splitkey=%p %s\n", __FILE__, __LINE__, splitkey, splitkey);
    newroot->u.n.childkeys[0] = splitk.data;
    newroot->u.n.totalchildkeylens=splitk.size;
    BNC_DISKOFF(newroot,0)=nodea->thisnodename;
    BNC_DISKOFF(newroot,1)=nodeb->thisnodename;
    r=toku_fifo_create(&BNC_BUFFER(newroot,0)); if (r!=0) return r;
    r=toku_fifo_create(&BNC_BUFFER(newroot,1)); if (r!=0) return r;
    toku_verify_counts(newroot);
    //verify_local_fingerprint_nonleaf(nodea);
    //verify_local_fingerprint_nonleaf(nodeb);
    r=toku_log_newbrtnode(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, new_height, new_nodesize, (brt->flags&TOKU_DB_DUPSORT)!=0, newroot->rand4fingerprint);
    if (r!=0) return r;
    r=toku_log_addchild(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, nodea->thisnodename, 0);
    if (r!=0) return r;
    r=toku_log_addchild(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, 1, nodeb->thisnodename, 0);
    if (r!=0) return r;
    fixup_child_fingerprint(newroot, 0, nodea, brt, logger);
    fixup_child_fingerprint(newroot, 1, nodeb, brt, logger);
    {
	BYTESTRING bs = { .len = kv_pair_keylen(newroot->u.n.childkeys[0]),
			  .data = kv_pair_key(newroot->u.n.childkeys[0]) };
	r=toku_log_setpivot(logger, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, bs);
	if (r!=0) return r;
	toku_update_brtnode_loggerlsn(newroot, logger);
    }
    r=unpin_brtnode(brt, nodea);
    if (r!=0) return r;
    r=unpin_brtnode(brt, nodeb);
    if (r!=0) return r;
    //printf("%s:%d put %lld\n", __FILE__, __LINE__, brt->root);
    toku_cachetable_put(brt->cf, newroot_diskoff, newroot, brtnode_size(newroot),
                        toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    *newrootp = newroot;
    return 0;
}

static int brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger) {
    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
    int result;
    int r;
    int did_split; BRTNODE nodea=0, nodeb=0;
    DBT splitk;
    int debug = toku_brt_debug_mode;//strcmp(key,"hello387")==0;
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    if (debug) printf("%s:%d Getting %lld\n", __FILE__, __LINE__, *rootp);
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, 
				  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt))) {
	goto died0;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    if (debug) printf("%s:%d node inserting\n", __FILE__, __LINE__);
    did_split = 0;
    result = brtnode_put_cmd(brt, node, cmd,
			     &did_split, &nodea, &nodeb, &splitk,
			     debug,
			     logger);
    if (debug) printf("%s:%d did_insert\n", __FILE__, __LINE__);
    if (did_split) {
	// node is unpinned, so now we have to proceed to update the root with a new node.

	//printf("%s:%d did_split=%d nodeb=%p nodeb->thisnodename=%lld nodeb->nodesize=%d\n", __FILE__, __LINE__, did_split, nodeb, nodeb->thisnodename, nodeb->nodesize);
	//printf("Did split, splitkey=%s\n", splitkey);
	if (nodeb->height>0) assert(BNC_DISKOFF(nodeb,nodeb->u.n.n_children-1)!=0);
	assert(nodeb->nodesize>0);
        r = brt_init_new_root(brt, nodea, nodeb, splitk, rootp, logger, &node);
        assert(r == 0);
    } else {
	if (node->height>0)
	    assert(node->u.n.n_children<=TREE_FANOUT);
    }
    r = unpin_brtnode(brt, node);
    assert(r==0);
    r = toku_unpin_brt_header(brt);
    assert(r == 0);
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    return result;
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
    printf("split_count=%d\n", split_count);
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

typedef struct brt_split {
    int did_split;
    BRTNODE nodea;
    BRTNODE nodeb;
    DBT splitk;
} BRT_SPLIT;

static inline void brt_split_init(BRT_SPLIT *split) {
    split->did_split = 0;
    split->nodea = split->nodeb = 0;
    toku_init_dbt(&split->splitk);
}

static int brt_search_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, BRT_SPLIT *split, TOKULOGGER logger);

/* search in a node's child */
static int brt_search_child(BRT brt, BRTNODE node, int childnum, brt_search_t *search, DBT *newkey, DBT *newval, BRT_SPLIT *split, TOKULOGGER logger) {
    int r, rr;

    /* if the child's buffer is not empty then try to empty it */
    if (BNC_NBYTESINBUF(node, childnum) > 0) {
        rr = push_some_brt_cmds_down(brt, node, childnum, &split->did_split, &split->nodea, &split->nodeb, &split->splitk, 0, logger);
        assert(rr == 0);
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

static int brt_search_nonleaf_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, BRT_SPLIT *split, TOKULOGGER logger) {
    int r = DB_NOTFOUND;
    int c;

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
            r = brt_search_child(brt, node, child[c], search, newkey, newval, split, logger);
            if (r == 0 || r == EAGAIN) 
                break;
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

static int brt_search_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, BRT_SPLIT *split, TOKULOGGER logger) {
    if (node->height > 0)
        return brt_search_nonleaf_node(brt, node, search, newkey, newval, split, logger);
    else
        return brt_search_leaf_node(node, search, newkey, newval);
}

int toku_brt_search(BRT brt, brt_search_t *search, DBT *newkey, DBT *newval, TOKULOGGER logger) {
    int r, rr;

    rr = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    assert(rr == 0);

    CACHEKEY *rootp;
    rootp = toku_calculate_root_offset_pointer(brt);

    void *node_v;
    rr = toku_cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(rr == 0);

    BRTNODE node = node_v;

    for (;;) {
        BRT_SPLIT split; brt_split_init(&split);
        r = brt_search_node(brt, node, search, newkey, newval, &split, logger);

        if (split.did_split) {
            rr = brt_init_new_root(brt, split.nodea, split.nodeb, split.splitk, rootp, 0, &node);
            assert(rr == 0);
        } 

        if (r != EAGAIN)
            break;
    }

    rr = unpin_brtnode(brt, node);
    assert(rr == 0);

    rr = toku_unpin_brt_header(brt); 
    assert(rr == 0);

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

