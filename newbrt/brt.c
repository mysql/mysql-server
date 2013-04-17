/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


/*

Checkpoint and recovery notes:

After a checkpoint, on a the first write of a node, we are supposed to write the node to a new location on disk.

Q: During recovery, how do we find the root node without looking at every block on disk?
A: The root node is either the designated root near the front of the freelist.
   The freelist is updated infrequently.  Before updating the stable copy of the freelist, we make sure that
   the root is up-to-date.  We can make the freelist-and-root update be an arbitrarily small fraction of disk bandwidth.

*/

/*

Managing the tree shape:  How insertion, deletion, and querying work


When we insert a message into the BRT, here's what happens.

Insert_message_at_node(msg, node, BOOL *did_io)
  if (node is leaf) {
     apply message;
  } else {
     for each child i relevant to the msg (e.g., if it's a broadcast, then all children) {
        rs[i] := Insert_message_to_child(msg, node, i, did_io);
     for each i such that rs[i] is not stable, while !*did_io
        (from greatest i to smallest, so that the numbers remain valid)
        Split_or_merge(node, i, did_io);
  }
  return reaction_state(node)
}

Insert_message_to_child (msg, node, i, BOOL *did_io) {
  if (child i's queue is empty and
      child i is in main memory) {
    return Insert_message_at_node(msg, child[i], did_io)
  } else {
    Insert msg into the buffer[i]
    while (!*did_io && node is overfull) {
      rs,child_that_flushed = Flush_some_child(node, did_io);
      if (rs is reactive) Split_or_merge(node, child_that_flushed, did_io);
    }
  }
}

Flush_this_child (node, childnum, BOOL *did_io) {
  while (child i queue not empty) {
    if (child i is not in memory) *did_io = TRUE;
    rs = Insert_message_at_node(pop(queue), child i, did_io)
    // don't do any reshaping if a node becomes reactive.  Deal with it all at the end.
  }
  return rs, i; // the last rs is all that matters.  Also return the child that was the heaviest (the one that rs applies to)
}

Flush_some_child (node, BOOL *did_io) {
  i = pick heaviest child()
  assert(i>0); // there must be such a child
  return Flush_this_child (node, i, did_io)
}

Split_or_merge (node, childnum, BOOL *did_io) {
  if (child i is fissible) {
    fetch node and child i into main memory (set *did_io if we perform I/O)  (Fetch them in canonical order)
    split child i, producing two nodes A and B, and also a pivot.   Don't worry if the resulting child is still too big or too small.
    fixup node to point at the two new children.  Don't worry about the node become prolific.
    return;
  } else if (child i is fusible (and if there are at least two children)) {
    fetch node, child i, and a sibling of child i into main memory.  (Set *did_io if needed)  (Fetch them in canonical order)
    merge the two siblings.
    fixup the node to point at one child (which means merging the fifos for the two children)
    Split or merge again, pointing at the relevant child.
    // Claim:  The number of splits_or_merges is finite, since each time down the merge decrements the number of children
    //  and as soon as a node splits we return
  }
}


lookup (key, node) {
  if (node is a leaf) {
     return lookup in leaf.
  } else {
     find appropriate child, i, for key.
     BOOL did_io = FALSE;
     rs = Flush_this_child(node, i, &did_io);
     if (rs is reactive) Split_or_merge(node, i, &did_io);
     return lookup(node, child[i])
  }
}

When inserting a message, we send it as far down the tree as possible, but
 - we stop if it would require I/O (we bring in the root node even if does require I/O), and
 - we stop if we reach a node that is gorged
   (We don't want to further overfill a node, so we place the message in the gorged node's parent.  If the root is gorged, we place the message in the root.)
 - we stop if we find a FIFO that contains a message, since we must preserve the order of messages.  (We enqueue the message).

After putting the message into a node, we may need to adjust the tree.
Observe that the ancestors of the node into which we placed the
message are not gorged.  (But they may be hungry or too fat or too thin.)



 - If we put a message into a leaf node, and it is now gorged, then we

 - An gorged leaf node.   We split it.
 - An hungry leaf node.  We merge it with its neighbor (if there is such a neighbor).
 - An gorged nonleaf node.
    We pick the heaviest child, and push all the messages from the node to the child.
    If that child is an gorged leaf node, then


--------------------
*/

// Simple scheme with no eager promotions:
//
// Insert_message_in_tree:
//  Put a message in a node.
//  handle_node_that_maybe_the_wrong_shape(node)
//
// Handle_node_that_maybe_the_wrong_shape(node)
//  If the node is gorged
//    If it is a leaf node, split it.
//    else (it is an internal node),
//      pick the heaviest child, and push all that child's messages to that child.
//      Handle_node_that_maybe_the_wrong_shape(child)
//      If the node is now too fat:
//        split the node
//      else if the node is now too thin:
//        merge the node
//      else do nothing
//  If the node is an hungry leaf:
//     merge the node (or distribute the data with its neighbor if the merge would be too big)
//
//
// Note: When nodes a merged,  they may become gorged.  But we just let it be gorged.
//
// search()
//  To search a leaf node, we just do the lookup.
//  To search a nonleaf node:
//    Determine which child is the right one.
//    Push all data to that child.
//    search() the child, collecting whatever result is needed.
//    Then:
//      If the child is an gorged leaf or a too-fat nonleaf
//         split it
//      If the child is an hungry leaf or a too-thin nonleaf
//         merge it (or distribute the data with the neighbor if the merge would be too big.)
//    return from the search.
//
// Note: During search we may end up with gorged nonleaf nodes.
// (Nonleaf nodes can become gorged because of merging two thin nodes
// that had big buffers.)  We just let that happen, without worrying
// about it too much.
//
// --------------------
//
// Eager promotions make it only slightly tougher:
//
//
// Insert_message_in_tree:
//   It's the same routine, except that
//     if the fifo leading to the child is empty
//     and the child is in main memory
//     and the child is not gorged
//     then we place the message in the child (and we do this recursively, so we may place the message in the grandchild, or so forth.)
//       We simply leave any resulting gorged or hungry nodes alone, since the nodes can only be slightly gorged (and for hungryness we don't worry in this case.)
//   Otherewise put the message in the root and handle_node_that_is_maybe_the_wrong_shape().
//
//       An even more aggresive promotion scheme would be to go ahead and insert the new message in the gorged child, and then do the splits and merges resulting from that child getting gorged.
//
// */
//
//
#include "includes.h"
#include "leaflock.h"
#include "checkpoint.h"

// We invalidate all the OMTCURSORS any time we push into the root of the BRT for that OMT.
// We keep a counter on each brt header, but if the brt header is evicted from the cachetable
// then we lose that counter.  So we also keep a global counter.
// An alternative would be to keep only the global counter.  But that would invalidate all OMTCURSORS
// even from unrelated BRTs.  This way we only invalidate an OMTCURSOR if
static u_int64_t global_root_put_counter = 0;

enum reactivity { RE_STABLE, RE_FUSIBLE, RE_FISSIBLE };

static enum reactivity
get_leaf_reactivity (BRTNODE node) {
    assert(node->height==0);
    unsigned int size = toku_serialize_brtnode_size(node);
    if (size     > node->nodesize) return RE_FISSIBLE;
    if ((size*4) < node->nodesize) return RE_FUSIBLE;
    return RE_STABLE;
}

static enum reactivity
get_nonleaf_reactivity (BRTNODE node) {
    assert(node->height>0);
    int n_children = node->u.n.n_children;
    if (n_children > TREE_FANOUT) return RE_FISSIBLE;
    if (n_children*4 < TREE_FANOUT) return RE_FUSIBLE;
    return RE_STABLE;
}

static BOOL
nonleaf_node_is_gorged (BRTNODE node) {
    return (BOOL)(toku_serialize_brtnode_size(node) > node->nodesize);
}

static int
brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd, enum reactivity *re, BOOL *did_io);

static int
flush_this_child (BRT t, BRTNODE node, int childnum, enum reactivity *child_re, BOOL *did_io);


int toku_brt_debug_mode = 0;

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_counts(n), toku_verify_estimates(t,n))
#else
#define VERIFY_NODE(t,n) ((void)0)
#endif

//#define BRT_TRACE
#ifdef BRT_TRACE
#define WHEN_BRTTRACE(x) x
#else
#define WHEN_BRTTRACE(x) ((void)0)
#endif

static u_int32_t compute_child_fullhash (CACHEFILE cf, BRTNODE node, int childnum) {
    assert(node->height>0 && childnum<node->u.n.n_children);
    switch (BNC_HAVE_FULLHASH(node, childnum)) {
    case TRUE:
        {
            assert(BNC_FULLHASH(node, childnum)==toku_cachetable_hash(cf, BNC_BLOCKNUM(node, childnum)));
            return BNC_FULLHASH(node, childnum);
        }
    case FALSE:
        {
            u_int32_t child_fullhash = toku_cachetable_hash(cf, BNC_BLOCKNUM(node, childnum));
            BNC_HAVE_FULLHASH(node, childnum) = TRUE;
            BNC_FULLHASH(node, childnum) = child_fullhash;
            return child_fullhash;
        }
    }
    abort(); return 0;
}

struct fill_leafnode_estimates_state {
    struct subtree_estimates *e;
    OMTVALUE prevval;
    BRTNODE  node;
};

static int
fill_leafnode_estimates (OMTVALUE val, u_int32_t UU(idx), void *vs)
{
    LEAFENTRY le = val;
    struct fill_leafnode_estimates_state *s = vs;
    s->e->dsize += le_any_keylen(le) + le_any_vallen(le);
    s->e->ndata++;
    if ((s->prevval == NULL) ||
	(0 == (s->node->flags & TOKU_DB_DUPSORT)) ||
	(le_any_keylen(le) != le_any_keylen(s->prevval)) ||
	(memcmp(le_any_key(le), le_any_key(s->prevval), le_any_keylen(le))!=0)) { // really should use comparison function
	s->e->nkeys++;
    }
    s->prevval = le;
    return 0;
}

static struct subtree_estimates
calc_leaf_stats (BRTNODE node) {
    struct subtree_estimates e = zero_estimates;
    struct fill_leafnode_estimates_state f = {&e, (OMTVALUE)NULL, node};
    toku_omt_iterate(node->u.l.buffer, fill_leafnode_estimates, &f);
    return e;
}

static void __attribute__((__unused__))
brt_leaf_check_leaf_stats (BRTNODE node)
{
    static int count=0; count++;
    if (node->height>0) return;
    struct subtree_estimates e = calc_leaf_stats(node);
    assert(e.ndata == node->u.l.leaf_stats.ndata);
    assert(e.nkeys == node->u.l.leaf_stats.nkeys);
    assert(e.dsize == node->u.l.leaf_stats.dsize);
    assert(node->u.l.leaf_stats.exact);
}

// This should be done incrementally in most cases.
static void
fixup_child_fingerprint (BRTNODE node, int childnum_of_node, BRTNODE child)
// Effect:  Sum the child fingerprint (and leafentry estimates) and store them in NODE.
// Parameters:
//   node                The node to modify
//   childnum_of_node    Which child changed   (PERFORMANCE: Later we could compute this incrementally)
//   child               The child that changed.
//   brt                 The brt (not used now but it will be for logger)
//   logger              The logger (not used now but it will be for logger)
{
    struct subtree_estimates estimates = zero_estimates;
    u_int32_t sum = child->local_fingerprint;
    estimates.exact = TRUE;
    if (child->height>0) {
        int i;
        for (i=0; i<child->u.n.n_children; i++) {
            sum += BNC_SUBTREE_FINGERPRINT(child,i);
	    struct subtree_estimates *child_se = &BNC_SUBTREE_ESTIMATES(child,i);
	    estimates.nkeys += child_se->nkeys;
	    estimates.ndata += child_se->ndata;
	    estimates.dsize += child_se->dsize;
	    if (!child_se->exact) estimates.exact = FALSE;
	    if (toku_fifo_n_entries(BNC_BUFFER(child,i))!=0) estimates.exact=FALSE;
        }
    } else {
	estimates = child->u.l.leaf_stats;
#ifdef SLOWSLOW
	assert(estimates.ndata == child->u.l.leaf_stats.ndata);
	struct fill_leafnode_estimates_state s = {&estimates, (OMTVALUE)NULL};
	toku_omt_iterate(child->u.l.buffer, fill_leafnode_estimates, &s);
#endif
    }
    // Don't try to get fancy about not modifying the fingerprint if it didn't change.
    // We only call this function if we have reason to believe that the child's fingerprint did change.
    BNC_SUBTREE_FINGERPRINT(node,childnum_of_node)=sum;
    BNC_SUBTREE_ESTIMATES(node,childnum_of_node)=estimates;
    node->dirty=1;
}

static inline void
verify_local_fingerprint_nonleaf (BRTNODE node)
{
    if (0) {
	//brt_leaf_check_leaf_stats(node);
        static int count=0; count++;
        u_int32_t fp=0;
        int i;
        if (node->height==0) return;
        for (i=0; i<node->u.n.n_children; i++)
            FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
                         fp += toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
                         );
        fp *= node->rand4fingerprint;
        assert(fp==node->local_fingerprint);
    }
}

static inline void
toku_verify_estimates (BRT t, BRTNODE node) {
    if (node->height>0) {
        int childnum;
        for (childnum=0; childnum<node->u.n.n_children; childnum++) {
            BLOCKNUM childblocknum = BNC_BLOCKNUM(node, childnum);
            u_int32_t fullhash = compute_child_fullhash(t->cf, node, childnum);
            void *childnode_v;
            int r = toku_cachetable_get_and_pin(t->cf, childblocknum, fullhash, &childnode_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
            assert(r==0);
            BRTNODE childnode = childnode_v;
	    // we'll just do this estimate
            u_int64_t child_estimate = 0;
            if (childnode->height==0) {
                child_estimate = toku_omt_size(childnode->u.l.buffer);
            } else {
                int i;
                for (i=0; i<childnode->u.n.n_children; i++) {
                    child_estimate += BNC_SUBTREE_ESTIMATES(childnode, i).ndata;
                }
            }
            assert(BNC_SUBTREE_ESTIMATES(node, childnum).ndata==child_estimate);
            toku_unpin_brtnode(t, childnode);
        }
    }
}

static u_int32_t
mp_pool_size_for_nodesize (u_int32_t nodesize)
// Effect: Calculate how big the mppool should be for a node of size NODESIZE.  Leave a little extra space for expansion.
{
    return nodesize+nodesize/4;
}

static long
brtnode_memory_size (BRTNODE node)
// Effect: Estimate how much main memory a node requires.
{
    if (node->height>0) {
        int n_children = node->u.n.n_children;
        int fifo_sum=0;
        int i;
        for (i=0; i<n_children; i++) {
            fifo_sum+=toku_fifo_memory_size(node->u.n.childinfos[i].buffer);
        }
        return sizeof(*node)
            +(1+n_children)*(sizeof(node->u.n.childinfos[0]))
            +(n_children)+(sizeof(node->u.n.childkeys[0]))
            +node->u.n.totalchildkeylens
            +fifo_sum;
    } else {
        return sizeof(*node)+toku_omt_memory_size(node->u.l.buffer)+toku_mempool_get_size(&node->u.l.buffer_mempool);
    }
}

int toku_unpin_brtnode (BRT brt, BRTNODE node) {
//    if (node->dirty && txn) {
//        // For now just update the log_lsn.  Later we'll have to deal with the checksums.
//        node->log_lsn = toku_txn_get_last_lsn(txn);
//        //if (node->log_lsn.lsn>33320) printf("%s:%d node%lld lsn=%lld\n", __FILE__, __LINE__, node->thisnodename, node->log_lsn.lsn);
//    }
    verify_local_fingerprint_nonleaf(node);
    VERIFY_NODE(brt,node);
    return toku_cachetable_unpin(brt->cf, node->thisnodename, node->fullhash, (enum cachetable_dirty) node->dirty, brtnode_memory_size(node));
}

void toku_brtnode_flush_callback (CACHEFILE cachefile, BLOCKNUM nodename, void *brtnode_v, void *extraargs, long size __attribute__((unused)), BOOL write_me, BOOL keep_me, BOOL for_checkpoint) {
    struct brt_header *h = extraargs;
    BRTNODE brtnode = brtnode_v;
//    if ((write_me || keep_me) && (brtnode->height==0)) {
//        toku_pma_verify_fingerprint(brtnode->u.l.buffer, brtnode->rand4fingerprint, brtnode->subtree_fingerprint);
//    }
    if (0) {
        printf("%s:%d toku_brtnode_flush_callback %p thisnodename=%" PRId64 " keep_me=%u height=%d", __FILE__, __LINE__, brtnode, brtnode->thisnodename.b, (unsigned)keep_me, brtnode->height);
        if (brtnode->height==0) printf(" buf=%p mempool-base=%p", brtnode->u.l.buffer, brtnode->u.l.buffer_mempool.base);
        printf("\n");
    }
    //if (modified_lsn.lsn > brtnode->lsn.lsn) brtnode->lsn=modified_lsn;
    assert(brtnode->thisnodename.b==nodename.b);
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (write_me) {
	if (!h->panic) { // if the brt panicked, stop writing, otherwise try to write it.
	    int n_workitems, n_threads; 
	    toku_cachefile_get_workqueue_load(cachefile, &n_workitems, &n_threads);
	    int r = toku_serialize_brtnode_to(toku_cachefile_fd(cachefile), brtnode->thisnodename, brtnode, h, n_workitems, n_threads, for_checkpoint);
	    if (r) {
		if (h->panic==0) {
		    char *e = strerror(r);
		    int   l = 200 + strlen(e);
		    char s[l];
		    h->panic=r;
		    snprintf(s, l-1, "While writing data to disk, error %d (%s)", r, e);
		    h->panic_string = toku_strdup(s);
		}
	    }
	}
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (!keep_me) {
        toku_brtnode_free(&brtnode);
    }
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
}

int toku_brtnode_fetch_callback (CACHEFILE cachefile, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, long *sizep, void*extraargs, LSN *written_lsn) {
    assert(extraargs);
    struct brt_header *h = extraargs;
    BRTNODE *result=(BRTNODE*)brtnode_pv;
    int r = toku_deserialize_brtnode_from(toku_cachefile_fd(cachefile), nodename, fullhash, result, h);
    if (r == 0) {
        *sizep = brtnode_memory_size(*result);
        *written_lsn = (*result)->disk_lsn;
    }
    //(*result)->parent_brtnode = 0; /* Don't know it right now. */
    //printf("%s:%d installed %p (offset=%lld)\n", __FILE__, __LINE__, *result, nodename);
    return r;
}

static int
leafval_heaviside_le_committed (u_int32_t klen, void *kval,
                             u_int32_t dlen, void *dval,
                             struct cmd_leafval_heaviside_extra *be) {
    BRT t = be->t;
    DBT dbt;
    int cmp = t->compare_fun(t->db,
                             toku_fill_dbt(&dbt, kval, klen),
                             be->cmd->u.id.key);
    if (cmp == 0 && be->compare_both_keys && be->cmd->u.id.val->data) {
        return t->dup_compare(t->db,
                              toku_fill_dbt(&dbt, dval, dlen),
                              be->cmd->u.id.val);
    } else {
        return cmp;
    }
}

static int
leafval_heaviside_le_both (TXNID xid __attribute__((__unused__)),
                        u_int32_t klen, void *kval,
                        u_int32_t clen __attribute__((__unused__)), void *cval __attribute__((__unused__)),
                        u_int32_t plen, void *pval,
                        struct cmd_leafval_heaviside_extra *be) {
    return leafval_heaviside_le_committed(klen, kval, plen, pval, be);
}

static int
leafval_heaviside_le_provdel (TXNID xid __attribute__((__unused__)),
                           u_int32_t klen, void *kval,
                           u_int32_t clen, void *cval,
                           struct cmd_leafval_heaviside_extra *be) {
    return leafval_heaviside_le_committed(klen, kval, clen, cval, be);
}

static int
leafval_heaviside_le_provpair (TXNID xid __attribute__((__unused__)),
                            u_int32_t klen, void *kval,
                            u_int32_t plen, void *pval,
                            struct cmd_leafval_heaviside_extra *be) {
    return leafval_heaviside_le_committed(klen, kval, plen, pval, be);
}

int toku_cmd_leafval_heaviside (OMTVALUE lev, void *extra) {
    LEAFENTRY le=lev;
    struct cmd_leafval_heaviside_extra *be = extra;
    LESWITCHCALL(le, leafval_heaviside, be);
    abort(); return 0; // make certain compilers happy
}

// If you pass in data==0 then it only compares the key, not the data (even if is a DUPSORT database)
static int
brt_compare_pivot(BRT brt, DBT *key, DBT *data, bytevec ck)
{
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

static int
verify_in_mempool (OMTVALUE lev, u_int32_t UU(idx), void *vmp)
{
    LEAFENTRY le=lev;
    struct mempool *mp=vmp;
    assert(toku_mempool_inrange(mp, le, leafentry_memsize(le)));
    return 0;
}

void
toku_verify_all_in_mempool (BRTNODE node)
{
    if (node->height==0) {
        toku_omt_iterate(node->u.l.buffer, verify_in_mempool, &node->u.l.buffer_mempool);
    }
}

/* Frees a node, including all the stuff in the hash table. */
void toku_brtnode_free (BRTNODE *nodep) {

    //TODO: #1378 Take omt lock (via brtnode) around call to toku_omt_destroy().
    //            After node is destroyed, release lock to free pool.

    BRTNODE node=*nodep;
    int i;
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, node, node->mdicts[0]);
    if (node->height>0) {
        for (i=0; i<node->u.n.n_children-1; i++) {
            toku_free(node->u.n.childkeys[i]);
        }
        for (i=0; i<node->u.n.n_children; i++) {
            if (BNC_BUFFER(node,i)) {
                toku_fifo_free(&BNC_BUFFER(node,i));
            }
        }
        toku_free(node->u.n.childkeys);
        toku_free(node->u.n.childinfos);
    } else {
        toku_leaflock_lock_by_leaf(node->u.l.leaflock);
        if (node->u.l.buffer) // The buffer may have been freed already, in some cases.
            toku_omt_destroy(&node->u.l.buffer);
        toku_leaflock_unlock_and_return(&node->u.l.leaflock);
        void *mpbase = toku_mempool_get_base(&node->u.l.buffer_mempool);
        toku_mempool_fini(&node->u.l.buffer_mempool);
        toku_free(mpbase);

    }

    toku_free(node);
    *nodep=0;
}

static void
brtheader_partial_destroy(struct brt_header *h) {
    if (h->type == BRTHEADER_CHECKPOINT_INPROGRESS) {
        //Share fifo till #1603
        h->fifo = NULL;
    }
    else {
        assert(h->type == BRTHEADER_CURRENT);
        toku_fifo_free(&h->fifo); //TODO: #1603 delete
    }
}

static void
brtheader_destroy(struct brt_header *h) {
    if (!h->panic) assert(!h->checkpoint_header);

    brtheader_partial_destroy(h);

    //header and checkpoint_header have same Blocktable pointer
    //cannot destroy since it is still in use by CURRENT
    if (h->type == BRTHEADER_CHECKPOINT_INPROGRESS) h->blocktable = NULL; 
    else {
        assert(h->type == BRTHEADER_CURRENT);
        toku_blocktable_destroy(&h->blocktable);
        if (h->descriptor.sdbt.data) toku_free(h->descriptor.sdbt.data);
    }
}

static int
brtheader_alloc(struct brt_header **hh) {
    int r = 0;
    if ((CALLOC(*hh))==0) {
        assert(errno==ENOMEM);
        r = ENOMEM;
    }
    return r;
}

// Make a copy of the header for the purpose of a checkpoint
static void
brtheader_copy_for_checkpoint(struct brt_header *h, LSN checkpoint_lsn) {
    assert(h->type == BRTHEADER_CURRENT);
    assert(h->checkpoint_header == NULL);
    assert(h->panic==0);

    struct brt_header* XMALLOC(ch);
    *ch = *h; //Do a shallow copy
    ch->type = BRTHEADER_CHECKPOINT_INPROGRESS; //Different type
    ch->checkpoint_lsn = checkpoint_lsn;
    ch->panic_string = NULL;
    
    //ch->blocktable is SHARED between the two headers
    h->checkpoint_header = ch;
}

static void
brtheader_free(struct brt_header *h)
{
    brtheader_destroy(h);
    toku_free(h);
}
        
void
toku_brtheader_free (struct brt_header *h) {
    brtheader_free(h);
}

static void
initialize_empty_brtnode (BRT t, BRTNODE n, BLOCKNUM nodename, int height)
// Effect: Fill in N as an empty brtnode.
{
    n->tag = TYP_BRTNODE;
    n->nodesize = t->h->nodesize;
    n->flags = t->flags;
    n->thisnodename = nodename;
    n->disk_lsn.lsn = 0; // a new one can always be 0.
    n->log_lsn = n->disk_lsn;
    assert(t->h->layout_version != 0);
    n->layout_version = t->h->layout_version;
    n->height       = height;
    n->rand4fingerprint = random();
    n->local_fingerprint = 0;
    n->dirty = 1;
    assert(height>=0);
    if (height>0) {
        n->u.n.n_children   = 0;
        n->u.n.totalchildkeylens = 0;
        n->u.n.n_bytes_in_buffers = 0;
        n->u.n.childinfos=0;
        n->u.n.childkeys=0;
    } else {
	n->u.l.leaf_stats = zero_estimates;
        int r;
        r = toku_omt_create(&n->u.l.buffer);
        assert(r==0);
        r = toku_leaflock_borrow(&n->u.l.leaflock);
        assert(r==0);
        {
            u_int32_t mpsize = mp_pool_size_for_nodesize(n->nodesize);
            void *mp = toku_malloc(mpsize);
            assert(mp);
            toku_mempool_init(&n->u.l.buffer_mempool, mp, mpsize);
        }

        static int rcount=0;
        //printf("%s:%d n PMA= %p (rcount=%d)\n", __FILE__, __LINE__, n->u.l.buffer, rcount); 
        rcount++;
        n->u.l.n_bytes_in_buffer = 0;
        n->u.l.seqinsert = 0;
    }
}

static int
brt_init_new_root(BRT brt, BRTNODE nodea, BRTNODE nodeb, DBT splitk, CACHEKEY *rootp, TOKULOGGER logger, BRTNODE *newrootp)
// Effect:  Create a new root node whose two children are NODEA and NODEB, an dthe pivotkey is SPLITK.
//  Store the new root's identity in *ROOTP, and the node in *NEWROOTP.
//  Unpin nodea and nodeb.
//  Leave the new root pinned.
//  Stores the sum of the fingerprints of the children into the new node.  (LAZY:  Later we'll only store the fingerprints when evicting.)
{
    TAGMALLOC(BRTNODE, newroot);
    int r;
    int new_height = nodea->height+1;
    BLOCKNUM newroot_diskoff;
    toku_allocate_blocknum(brt->h->blocktable, &newroot_diskoff, brt->h);
    assert(newroot);
    newroot->ever_been_written = 0;
    toku_log_changeunnamedroot(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), *rootp, newroot_diskoff);
    *rootp=newroot_diskoff;
    initialize_empty_brtnode (brt, newroot, newroot_diskoff, new_height);
    //printf("new_root %lld %d %lld %lld\n", newroot_diskoff, newroot->height, nodea->thisnodename, nodeb->thisnodename);
    newroot->u.n.n_children=2;
    MALLOC_N(3, newroot->u.n.childinfos);
    MALLOC_N(2, newroot->u.n.childkeys);
    //printf("%s:%d Splitkey=%p %s\n", __FILE__, __LINE__, splitkey, splitkey);
    newroot->u.n.childkeys[0] = splitk.data;
    newroot->u.n.totalchildkeylens=splitk.size;
    BNC_BLOCKNUM(newroot,0)=nodea->thisnodename;
    BNC_BLOCKNUM(newroot,1)=nodeb->thisnodename;
    BNC_HAVE_FULLHASH(newroot, 0) = FALSE;
    BNC_HAVE_FULLHASH(newroot, 1) = FALSE;
    r=toku_fifo_create(&BNC_BUFFER(newroot,0)); if (r!=0) return r;
    r=toku_fifo_create(&BNC_BUFFER(newroot,1)); if (r!=0) return r;
    BNC_NBYTESINBUF(newroot, 0)=0;
    BNC_NBYTESINBUF(newroot, 1)=0;
    BNC_SUBTREE_FINGERPRINT(newroot, 0)=0;
    BNC_SUBTREE_FINGERPRINT(newroot, 1)=0;
    BNC_SUBTREE_ESTIMATES(newroot, 0)=zero_estimates;
    BNC_SUBTREE_ESTIMATES(newroot, 1)=zero_estimates;
    verify_local_fingerprint_nonleaf(nodea);
    verify_local_fingerprint_nonleaf(nodeb);
    fixup_child_fingerprint(newroot, 0, nodea);
    fixup_child_fingerprint(newroot, 1, nodeb);
    r = toku_unpin_brtnode(brt, nodea);
    if (r!=0) return r;
    r = toku_unpin_brtnode(brt, nodeb);
    if (r!=0) return r;
    //printf("%s:%d put %lld\n", __FILE__, __LINE__, newroot_diskoff);
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, newroot_diskoff);
    newroot->fullhash = fullhash;
    toku_cachetable_put(brt->cf, newroot_diskoff, fullhash, newroot, brtnode_memory_size(newroot),
                        toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
    *newrootp = newroot;
    return 0;
}

// logs the memory allocation, but not the creation of the new node
int toku_create_new_brtnode (BRT t, BRTNODE *result, int height) {
    TAGMALLOC(BRTNODE, n);
    int r;
    BLOCKNUM name;
    toku_allocate_blocknum(t->h->blocktable, &name, t->h);
    assert(n);
    assert(t->h->nodesize>0);
    n->ever_been_written = 0;
    initialize_empty_brtnode(t, n, name, height);
    *result = n;
    assert(n->nodesize>0);
    //    n->brt            = t;
    //printf("%s:%d putting %p (%lld)\n", __FILE__, __LINE__, n, n->thisnodename);
    u_int32_t fullhash = toku_cachetable_hash(t->cf, n->thisnodename);
    n->fullhash = fullhash;
    r=toku_cachetable_put(t->cf, n->thisnodename, fullhash,
                          n, brtnode_memory_size(n),
                          toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    assert(r==0);
    return 0;
}

static int
fill_buf (OMTVALUE lev, u_int32_t idx, void *varray)
{
    LEAFENTRY le=lev;
    LEAFENTRY *array=varray;
    array[idx]=le;
    return 0;
}

static int
brtleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk)
// Effect: Split a leaf node.
{
    BRTNODE B;
    int r;

    //printf("%s:%d splitting leaf %" PRIu64 " which is size %u (targetsize = %u)\", __FILE__, __LINE__, node->thisnodename.b, toku_serialize_brtnode_size(node), node->nodesize);

    assert(node->height==0);
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    toku_create_new_brtnode(t, &B, 0);
    assert(B->nodesize>0);
    assert(node->nodesize>0);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    //printf("%s:%d B is at %lld nodesize=%d\n", __FILE__, __LINE__, B->thisnodename, B->nodesize);
    assert(node->height>0 || node->u.l.buffer!=0);

    toku_verify_all_in_mempool(node);

    u_int32_t n_leafentries = toku_omt_size(node->u.l.buffer);
    u_int32_t break_at = 0;
    {
        OMTVALUE *MALLOC_N(n_leafentries, leafentries);
        assert(leafentries);
        toku_omt_iterate(node->u.l.buffer, fill_buf, leafentries);
        break_at = 0;
        {
            u_int32_t i;
            u_int32_t sumlesizes=0;
            for (i=0; i<n_leafentries; i++) sumlesizes += leafentry_disksize(leafentries[i]);
            u_int32_t sumsofar=0;
            // split in half if not sequentially inserted
            // otherwise put 1/128th in the new node
            u_int32_t f = 2; // 1/2
            if (node->u.l.seqinsert*2 >= n_leafentries) f = 128; // 1/128
            node->u.l.seqinsert = 0;
            for (i=n_leafentries-1; i>0; i--) {
                assert(toku_mempool_inrange(&node->u.l.buffer_mempool, leafentries[i], leafentry_memsize(leafentries[i])));
                sumsofar += leafentry_disksize(leafentries[i]);
                if (sumsofar*f >= sumlesizes) {
                    break_at = i;
                    break;
                }
            }
        }
        // Now we know where we are going to break it
        OMT old_omt = node->u.l.buffer;
        toku_omt_destroy(&B->u.l.buffer); // Destroy B's empty OMT, so I can rebuild it from an array
        {
            u_int32_t i;
            u_int32_t diff_fp = 0;
            u_int32_t diff_size = 0;
	    struct subtree_estimates diff_est = zero_estimates;
	    LEAFENTRY *MALLOC_N(n_leafentries-break_at, free_us);
            for (i=break_at; i<n_leafentries; i++) {
		LEAFENTRY prevle = (i>0) ? leafentries[i-1] : 0;
                LEAFENTRY oldle = leafentries[i];
                LEAFENTRY newle = toku_mempool_malloc(&B->u.l.buffer_mempool, leafentry_memsize(oldle), 1);
                assert(newle!=0); // it's a fresh mpool, so this should always work.
		BOOL key_is_unique;
		{
		    DBT xdbt,ydbt;
		    if (t->flags & TOKU_DB_DUPSORT) key_is_unique=TRUE;
		    else if (prevle==NULL)          key_is_unique=TRUE;
		    else if (t->compare_fun(t->db,
					    toku_fill_dbt(&xdbt, le_any_key(prevle), le_any_keylen(prevle)),
					    toku_fill_dbt(&ydbt, le_any_key(oldle),   le_any_keylen(oldle)))
			     ==0) {
			key_is_unique=FALSE;
		    } else {
			key_is_unique=TRUE;
		    }
		}
		if (key_is_unique) diff_est.nkeys++;
		diff_est.ndata++;
		diff_est.dsize += le_any_keylen(oldle) + le_any_vallen(oldle);
		//printf("%s:%d Added %u got %lu\n", __FILE__, __LINE__, le_any_keylen(oldle)+ le_any_vallen(oldle), diff_est.dsize);
                diff_fp += toku_le_crc(oldle);
                diff_size += OMT_ITEM_OVERHEAD + leafentry_disksize(oldle);
                memcpy(newle, oldle, leafentry_memsize(oldle));
		free_us[i-break_at] = oldle; // don't free the old leafentries yet, since we compare them in the other iterations of the loops
                leafentries[i] = newle;
	    }
            for (i=break_at; i<n_leafentries; i++) {
		LEAFENTRY oldle = free_us[i-break_at];
                toku_mempool_mfree(&node->u.l.buffer_mempool, oldle, leafentry_memsize(oldle));
	    }
	    toku_free(free_us);
            node->local_fingerprint -= node->rand4fingerprint * diff_fp;
            B   ->local_fingerprint += B   ->rand4fingerprint * diff_fp;
            node->u.l.n_bytes_in_buffer -= diff_size;
            B   ->u.l.n_bytes_in_buffer += diff_size;
	    subtract_estimates(&node->u.l.leaf_stats, &diff_est);
	    add_estimates     (&B->u.l.leaf_stats,    &diff_est);
	    //printf("%s:%d After subtracint and adding got %lu and %lu\n", __FILE__, __LINE__, node->u.l.leaf_stats.dsize, B->u.l.leaf_stats.dsize);
        }
        if ((r = toku_omt_create_from_sorted_array(&B->u.l.buffer,    leafentries+break_at, n_leafentries-break_at))) return r;
        if ((r = toku_omt_create_steal_sorted_array(&node->u.l.buffer, &leafentries,          break_at, n_leafentries))) return r;
        assert(leafentries==NULL);

        toku_verify_all_in_mempool(node);
        toku_verify_all_in_mempool(B);

        toku_omt_destroy(&old_omt);

	node->u.l.leaf_stats = calc_leaf_stats(node);
	B   ->u.l.leaf_stats = calc_leaf_stats(B   );
    }

    //toku_verify_gpma(node->u.l.buffer);
    //toku_verify_gpma(B->u.l.buffer);
    if (splitk) {
        memset(splitk, 0, sizeof *splitk);
        OMTVALUE lev = 0;
        r=toku_omt_fetch(node->u.l.buffer, toku_omt_size(node->u.l.buffer)-1, &lev, NULL);
        assert(r==0); // that fetch should have worked.
        LEAFENTRY le=lev;
        if (node->flags&TOKU_DB_DUPSORT) {
            splitk->size = le_any_keylen(le)+le_any_vallen(le);
            splitk->data = kv_pair_malloc(le_any_key(le), le_any_keylen(le), le_any_val(le), le_any_vallen(le));
        } else {
            splitk->size = le_any_keylen(le);
            splitk->data = kv_pair_malloc(le_any_key(le), le_any_keylen(le), 0, 0);
        }
        splitk->flags=0;
    }
    assert(r == 0);
    assert(node->height>0 || node->u.l.buffer!=0);
    /* Remove it from the cache table, and free its storage. */
    //printf("%s:%d old pma = %p\n", __FILE__, __LINE__, node->u.l.buffer);

    node->dirty = 1;
    B   ->dirty = 1;

    *nodea = node;
    *nodeb = B;

    //printf("%s:%d new sizes Node %" PRIu64 " size=%u omtsize=%d dirty=%d; Node %" PRIu64 " size=%u omtsize=%d dirty=%d\n", __FILE__, __LINE__,
    //           node->thisnodename.b, toku_serialize_brtnode_size(node), node->height==0 ? (int)(toku_omt_size(node->u.l.buffer)) : -1, node->dirty,
    //           B   ->thisnodename.b, toku_serialize_brtnode_size(B   ), B   ->height==0 ? (int)(toku_omt_size(B   ->u.l.buffer)) : -1, B->dirty);
    //toku_dump_brtnode(t, node->thisnodename, 0, NULL, 0, NULL, 0);
    //toku_dump_brtnode(t, B   ->thisnodename, 0, NULL, 0, NULL, 0);
    return 0;
}

static int
brt_nonleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk)
// Effect: node must be a node-leaf node.  It is split into two nodes, and the fanout is split between them.
//    Sets splitk->data pointer to a malloc'd value
//    Sets nodea, and nodeb to the two new nodes.
//    The caller must replace the old node with the two new nodes.
{
    int old_n_children = node->u.n.n_children;
    int n_children_in_a = old_n_children/2;
    int n_children_in_b = old_n_children-n_children_in_a;
    BRTNODE B;
    assert(node->height>0);
    assert(node->u.n.n_children>=2); // Otherwise, how do we split?  We need at least two children to split. */
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    toku_create_new_brtnode(t, &B, node->height);
    MALLOC_N(n_children_in_b+1, B->u.n.childinfos);
    MALLOC_N(n_children_in_b, B->u.n.childkeys);
    B->u.n.n_children   =n_children_in_b;
    if (0) {
        printf("%s:%d %p (%" PRId64 ") splits, old estimates:", __FILE__, __LINE__, node, node->thisnodename.b);
        //int i;
        //for (i=0; i<node->u.n.n_children; i++) printf(" %" PRIu64, BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, i));
        printf("\n");
    }

    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    {
        /* The first n_children_in_a go into node a.
         * That means that the first n_children_in_a-1 keys go into node a.
         * The splitter key is key number n_children_in_a */
        int i;

        for (i=0; i<n_children_in_b; i++) {
            int r = toku_fifo_create(&BNC_BUFFER(B,i));
            if (r!=0) return r;
            BNC_NBYTESINBUF(B,i)=0;
            BNC_SUBTREE_FINGERPRINT(B,i)=0;
            BNC_SUBTREE_ESTIMATES(B,i)=zero_estimates;
        }

        verify_local_fingerprint_nonleaf(node);

        for (i=n_children_in_a; i<old_n_children; i++) {

            int targchild = i-n_children_in_a;
            FIFO from_htab     = BNC_BUFFER(node,i);
            FIFO to_htab       = BNC_BUFFER(B,   targchild);
            BLOCKNUM thischildblocknum = BNC_BLOCKNUM(node, i);

            BNC_BLOCKNUM(B, targchild) = thischildblocknum;
            BNC_HAVE_FULLHASH(B,targchild) = BNC_HAVE_FULLHASH(node,i);
            BNC_FULLHASH(B,targchild)      = BNC_FULLHASH(node, i);

            while (1) {
                bytevec key, data;
                unsigned int keylen, datalen;
                u_int32_t type;
                TXNID xid;
                int fr = toku_fifo_peek(from_htab, &key, &keylen, &data, &datalen, &type, &xid);
                if (fr!=0) break;
                int n_bytes_moved = keylen+datalen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
                u_int32_t old_from_fingerprint = node->local_fingerprint;
                u_int32_t delta = toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
                u_int32_t new_from_fingerprint = old_from_fingerprint - node->rand4fingerprint*delta;
		B->local_fingerprint += B->rand4fingerprint*delta;
                int r = toku_fifo_enq(to_htab, key, keylen, data, datalen, type, xid);
                if (r!=0) return r;
                toku_fifo_deq(from_htab);
                // key and data will no longer be valid
                node->local_fingerprint = new_from_fingerprint;

                B->u.n.n_bytes_in_buffers     += n_bytes_moved;
                BNC_NBYTESINBUF(B, targchild) += n_bytes_moved;
                node->u.n.n_bytes_in_buffers  -= n_bytes_moved;
                BNC_NBYTESINBUF(node, i)      -= n_bytes_moved;
                // verify_local_fingerprint_nonleaf(B);
                // verify_local_fingerprint_nonleaf(node);
            }

            // Delete a child, removing it's fingerprint, and also the preceeding pivot key.  The child number must be > 0
            {
                assert(i>0);
                if (i>n_children_in_a) {
                    B->u.n.childkeys[targchild-1] = node->u.n.childkeys[i-1];
                    B->u.n.totalchildkeylens += toku_brt_pivot_key_len(t, node->u.n.childkeys[i-1]);
                    node->u.n.totalchildkeylens -= toku_brt_pivot_key_len(t, node->u.n.childkeys[i-1]);
                    node->u.n.childkeys[i-1] = 0;
                }
            }
            BNC_BLOCKNUM(node, i) = make_blocknum(0);
            BNC_HAVE_FULLHASH(node, i) = FALSE; 
            
            BNC_SUBTREE_FINGERPRINT(B, targchild)  = BNC_SUBTREE_FINGERPRINT(node, i);
            BNC_SUBTREE_FINGERPRINT(node, i)       = 0;

            BNC_SUBTREE_ESTIMATES(B,    targchild) = BNC_SUBTREE_ESTIMATES(node, i);
            BNC_SUBTREE_ESTIMATES(node, i)         = zero_estimates;

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

        REALLOC_N(n_children_in_a+1,   node->u.n.childinfos);
        REALLOC_N(n_children_in_a, node->u.n.childkeys);

        verify_local_fingerprint_nonleaf(node);
        verify_local_fingerprint_nonleaf(B);
    }

    node->dirty = 1;
    B   ->dirty = 1;

    *nodea = node;
    *nodeb = B;

    assert(toku_serialize_brtnode_size(node) <= node->nodesize);
    assert(toku_serialize_brtnode_size(B) <= B->nodesize);
    return 0;
}

/* NODE is a node with a child.
 * childnum was split into two nodes childa, and childb.  childa is the same as the original child.  childb is a new child.
 * We must slide things around, & move things from the old table to the new tables.
 * Requires: the CHILDNUMth buffer of node is empty.
 * We don't push anything down to children.  We split the node, and things land wherever they land.
 * We must delete the old buffer (but the old child is already deleted.)
 * On return, the new children are unpinned.
 */
static int
handle_split_of_child (BRT t, BRTNODE node, int childnum,
                       BRTNODE childa, BRTNODE childb,
                       DBT *splitk /* the data in the childsplitk is alloc'd and is consumed by this call. */
		       )
{
    assert(node->height>0);
    assert(0 <= childnum && childnum < node->u.n.n_children);
    FIFO      old_h = BNC_BUFFER(node,childnum);
    int       old_count = BNC_NBYTESINBUF(node, childnum);
    assert(old_count==0);
    int cnum;
    int r;

    WHEN_NOT_GCOV(
    if (toku_brt_debug_mode) {
        int i;
        printf("%s:%d Child %d splitting on %s\n", __FILE__, __LINE__, childnum, (char*)splitk->data);
        printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
        for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
        printf("\n");
    }
                  )

    node->dirty = 1;

    verify_local_fingerprint_nonleaf(node);

    XREALLOC_N(node->u.n.n_children+2, node->u.n.childinfos);
    XREALLOC_N(node->u.n.n_children+1, node->u.n.childkeys);
    // Slide the children over.
    BNC_SUBTREE_FINGERPRINT (node, node->u.n.n_children+1)=0;
    BNC_SUBTREE_ESTIMATES   (node, node->u.n.n_children+1)=zero_estimates;
    for (cnum=node->u.n.n_children; cnum>childnum+1; cnum--) {
        node->u.n.childinfos[cnum] = node->u.n.childinfos[cnum-1];
    }
    node->u.n.n_children++;

    assert(BNC_BLOCKNUM(node, childnum).b==childa->thisnodename.b); // use the same child
    BNC_BLOCKNUM(node, childnum+1) = childb->thisnodename;
    BNC_HAVE_FULLHASH(node, childnum+1) = TRUE;
    BNC_FULLHASH(node, childnum+1) = childb->fullhash;
    // BNC_SUBTREE_FINGERPRINT(node, childnum)=0; // leave the subtreefingerprint alone for the child, so we can log the change
    BNC_SUBTREE_FINGERPRINT(node, childnum+1)=0;
    BNC_SUBTREE_ESTIMATES  (node, childnum+1)=zero_estimates;
    fixup_child_fingerprint(node, childnum,   childa);
    fixup_child_fingerprint(node, childnum+1, childb);
    r=toku_fifo_create(&BNC_BUFFER(node,childnum+1)); assert(r==0);
    verify_local_fingerprint_nonleaf(node);    // The fingerprint hasn't changed and everhything is still there.
    r=toku_fifo_create(&BNC_BUFFER(node,childnum));   assert(r==0); // ??? SHould handle this error case
    BNC_NBYTESINBUF(node, childnum) = 0;
    BNC_NBYTESINBUF(node, childnum+1) = 0;

    // Remove all the cmds from the local fingerprint.  Some may get added in again when we try to push to the child.

    verify_local_fingerprint_nonleaf(node);

    // Slide the keys over
    {
        struct kv_pair *pivot = splitk->data;

        for (cnum=node->u.n.n_children-2; cnum>childnum; cnum--) {
            node->u.n.childkeys[cnum] = node->u.n.childkeys[cnum-1];
        }
        //if (logger) assert((t->flags&TOKU_DB_DUPSORT)==0); // the setpivot is wrong for TOKU_DB_DUPSORT, so recovery will be broken.
        node->u.n.childkeys[childnum]= pivot;
        node->u.n.totalchildkeylens += toku_brt_pivot_key_len(t, pivot);
    }

    WHEN_NOT_GCOV(
    if (toku_brt_debug_mode) {
        int i;
        printf("%s:%d splitkeys:", __FILE__, __LINE__);
        for(i=0; i<node->u.n.n_children-2; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
        printf("\n");
    }
                  )

    verify_local_fingerprint_nonleaf(node);

    node->u.n.n_bytes_in_buffers -= old_count; /* By default, they are all removed.  We might add them back in. */
    /* Keep pushing to the children, but not if the children would require a pushdown */

    toku_fifo_free(&old_h);

    verify_local_fingerprint_nonleaf(childa);
    verify_local_fingerprint_nonleaf(childb);
    verify_local_fingerprint_nonleaf(node);

    VERIFY_NODE(t, node);
    VERIFY_NODE(t, childa);
    VERIFY_NODE(t, childb);

    r=toku_unpin_brtnode(t, childa);
    assert(r==0);
    r=toku_unpin_brtnode(t, childb);
    assert(r==0);
                
    return 0;
}

static int
brt_split_child (BRT t, BRTNODE node, int childnum, BOOL *did_react)
{
    if (0) {
        printf("%s:%d Node %" PRId64 "->u.n.n_children=%d estimates=", __FILE__, __LINE__, node->thisnodename.b, node->u.n.n_children);
        //int i;
        //for (i=0; i<node->u.n.n_children; i++) printf(" %" PRIu64, BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, i));
        printf("\n");
    }
    assert(node->height>0);
    BRTNODE child;
    if (BNC_NBYTESINBUF(node, childnum)>0) {
        // I don't think this can happen, but it's easy to handle.  Flush the child, and if no longer fissible, then return.
        enum reactivity re = RE_STABLE;
        BOOL did_io = FALSE;
        int r = flush_this_child(t, node, childnum, &re, &did_io);
        if (r != 0) return r;
        if (re != RE_FISSIBLE) return 0;
    }
    {
	void *childnode_v;
	int r = toku_cachetable_get_and_pin(t->cf,
					    BNC_BLOCKNUM(node, childnum),
					    compute_child_fullhash(t->cf, node, childnum),
					    &childnode_v,
					    NULL,
					    toku_brtnode_flush_callback, toku_brtnode_fetch_callback,
					    t->h);
	if (r!=0) return r;
	child = childnode_v;
	assert(child->thisnodename.b!=0);
	VERIFY_NODE(t,child);
    }

    verify_local_fingerprint_nonleaf(node);
    verify_local_fingerprint_nonleaf(child);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // printf("%s:%d node %" PRIu64 "->u.n.n_children=%d height=%d\n", __FILE__, __LINE__, node->thisnodename.b, node->u.n.n_children, node->height);
    if (child->height==0) {
        int r = brtleaf_split(t, child, &nodea, &nodeb, &splitk);
        assert(r==0); // REMOVE LATER
        if (r!=0) return r;
    } else {
        int r = brt_nonleaf_split(t, child, &nodea, &nodeb, &splitk);
        assert(r==0); // REMOVE LATER
        if (r!=0) return r;
    }
    // printf("%s:%d child did split\n", __FILE__, __LINE__);
    *did_react = TRUE;
    {
        int r = handle_split_of_child (t, node, childnum, nodea, nodeb, &splitk);
        if (0) {
            printf("%s:%d Node %" PRId64 "->u.n.n_children=%d estimates=", __FILE__, __LINE__, node->thisnodename.b, node->u.n.n_children);
            //int i;
            //for (i=0; i<node->u.n.n_children; i++) printf(" %" PRIu64, BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, i));
            printf("\n");
        }
        return r;
    }
}

static int
should_compare_both_keys (BRTNODE node, BRT_CMD cmd)
// Effect: Return nonzero if we need to compare both the key and the value.
{
    switch (cmd->type) {
    case BRT_INSERT:
        return node->flags & TOKU_DB_DUPSORT;
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:
        return 1;
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
        return 0;
    case BRT_NONE:
        break;
    }
    abort(); return 0;
}

static int apply_cmd_to_le_committed (u_int32_t klen, void *kval,
                                      u_int32_t dlen, void *dval,
                                      BRT_CMD cmd,
                                      u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data,
                                      OMT omt, struct mempool *mp, void **maybe_free) {
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
        return le_both(cmd->xid,
                       klen, kval,
                       dlen, dval, 
                       cmd->u.id.val->size, cmd->u.id.val->data,
                       newlen, disksize, new_data,
                       omt, mp, maybe_free);
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
        return le_provdel(cmd->xid,
                          klen, kval,
                          dlen, dval,
                          newlen, disksize, new_data,
                          omt, mp, maybe_free);
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
        // Just return the original committed record
        return le_committed(klen, kval, dlen, dval,
                            newlen, disksize, new_data,
                            omt, mp, maybe_free);
    case BRT_NONE: break;
    }
    abort(); return 0;
}

static int apply_cmd_to_le_both (TXNID xid,
                                 u_int32_t klen, void *kval,
                                 u_int32_t clen, void *cval,
                                 u_int32_t plen, void *pval,
                                 BRT_CMD cmd,
                                 u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data,
                                 OMT omt, struct mempool *mp, void *maybe_free) {
    u_int32_t prev_len;
    void     *prev_val;
    if (xid==cmd->xid) {
        // The xids match, so throw away the provisional value.
        prev_len = clen;  prev_val = cval;
    } else {
        // If the xids don't match, then we are moving the provisional value to committed status.
        prev_len = plen;  prev_val = pval;
    }
    // keep the committed value for rollback.
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
        return le_both(cmd->xid,
                       klen, kval,
                       prev_len, prev_val,
                       cmd->u.id.val->size, cmd->u.id.val->data,
                       newlen, disksize, new_data,
                       omt, mp, maybe_free);
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
        return le_provdel(cmd->xid,
                          klen, kval,
                          prev_len, prev_val,
                          newlen, disksize, new_data,
                          omt, mp, maybe_free);
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
        // I don't see how you could have an abort where the xids don't match.  But do it anyway.
        return le_committed(klen, kval,
                            prev_len, prev_val,
                            newlen, disksize, new_data,
                            omt, mp, maybe_free);
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
        // In the future we won't even have these commit messages.
        return le_committed(klen, kval,
                            plen, pval,
                            newlen, disksize, new_data,
                            omt, mp, maybe_free);
    case BRT_NONE: break;
    }
    abort(); return 0;
}

static int apply_cmd_to_le_provdel (TXNID xid,
                                    u_int32_t klen, void *kval,
                                    u_int32_t clen, void *cval,
                                    BRT_CMD cmd,
                                    u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data,
                                    OMT omt, struct mempool *mp, void *maybe_free)
{
    // keep the committed value for rollback
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
        if (cmd->xid == xid) {
            return le_both(cmd->xid,
                           klen, kval,
                           clen, cval,
                           cmd->u.id.val->size, cmd->u.id.val->data,
                           newlen, disksize, new_data,
                           omt, mp, maybe_free);
        } else {
            // It's an insert, but the committed value is deleted (since the xids don't match, we assume the delete took effect)
            return le_provpair(cmd->xid,
                               klen, kval,
                               cmd->u.id.val->size, cmd->u.id.val->data,
                               newlen, disksize, new_data,
                               omt, mp, maybe_free);
        }
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
        if (cmd->xid == xid) {
            // A delete of a delete could conceivably return the identical value, saving a malloc and a free, but to simplify things we just reallocate it
            // because othewise we have to notice not to free() the olditem.
            return le_provdel(cmd->xid,
                              klen, kval,
                              clen, cval,
                              newlen, disksize, new_data,
                              omt, mp, maybe_free);
        } else {
            // The commited value is deleted, and we are deleting, so treat as a delete.
            *new_data = 0;
            return 0;
        }
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
        // I don't see how the xids could not match...
        return le_committed(klen, kval,
                            clen, cval,
                            newlen, disksize, new_data,
                            omt, mp, maybe_free);
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
        *new_data = 0;
        return 0;
    case BRT_NONE: break;
    }
    abort(); return 0;
}

static int apply_cmd_to_le_provpair (TXNID xid,
                                     u_int32_t klen, void *kval,
                                     u_int32_t plen , void *pval,
                                     BRT_CMD cmd,
                                     u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data,
                                     OMT omt, struct mempool *mp, void **maybe_free) {
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
        if (cmd->xid == xid) {
            // it's still a provpair (the old prov value is lost)
            return le_provpair(cmd->xid,
                               klen, kval,
                               cmd->u.id.val->size, cmd->u.id.val->data,
                               newlen, disksize, new_data,
                               omt, mp, maybe_free);
        } else {
            // the old prov was actually committed.
            return le_both(cmd->xid,
                           klen, kval,
                           plen, pval,
                           cmd->u.id.val->size, cmd->u.id.val->data,
                           newlen, disksize, new_data,
                           omt, mp, maybe_free);
        }
    case BRT_DELETE_BOTH:
    case BRT_DELETE_ANY:
        if (cmd->xid == xid) {
            // A delete of a provisional pair is nothign
            *new_data = 0;
            return 0;
        } else {
            // The prov pair is actually a committed value.
            return le_provdel(cmd->xid,
                              klen, kval,
                              plen, pval,
                              newlen, disksize, new_data,
                              omt, mp, maybe_free);
        }
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
        // An abort of a provisional pair is nothing.
        *new_data = 0;
        return 0;
    case BRT_COMMIT_ANY:
    case BRT_COMMIT_BOTH:
        return le_committed(klen, kval,
                            plen, pval,
                            newlen, disksize, new_data,
                            omt, mp, maybe_free);
    case BRT_NONE: break;
    }
    abort(); return 0;
}

static int
apply_cmd_to_leaf (BRT_CMD cmd,
                   void *stored_data, // NULL if there was no stored data.
                   u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data,
                   OMT omt, struct mempool *mp, void **maybe_free)
{
    if (stored_data==0) {
        switch (cmd->type) {
        case BRT_INSERT:
            {
                LEAFENTRY le;
                int r = le_provpair(cmd->xid,
                                    cmd->u.id.key->size, cmd->u.id.key->data,
                                    cmd->u.id.val->size, cmd->u.id.val->data,
                                    newlen, disksize, &le,
                                    omt, mp, maybe_free);
                if (r==0) *new_data=le;
                return r;
            }
        case BRT_DELETE_BOTH:
        case BRT_DELETE_ANY:
        case BRT_ABORT_BOTH:
        case BRT_ABORT_ANY:
        case BRT_COMMIT_BOTH:
        case BRT_COMMIT_ANY:
            *new_data = 0;
            return 0; // Don't have to insert anything.
        case BRT_NONE:
            break;
        }
        abort();
    } else {
        LESWITCHCALL(stored_data, apply_cmd_to, cmd,
                     newlen, disksize, new_data,
                     omt, mp, maybe_free);
    }
    abort(); return 0;
}

static int
other_key_matches (BRTNODE node, u_int32_t idx, LEAFENTRY le)
{
    OMTVALUE other_lev = 0;
    int r = toku_omt_fetch(node->u.l.buffer, idx, &other_lev, (OMTCURSOR)NULL);
    assert(r==0);
    LEAFENTRY other_le = other_lev;
    u_int32_t other_keylen = le_any_keylen(other_le);
    if (other_keylen == le_any_keylen(le)
	&& memcmp(le_any_key(other_le), le_any_key(le), other_keylen)==0)   // really should use comparison function
	return 1;
    else
	return 0;
}

static void
maybe_bump_nkeys (BRTNODE node, u_int32_t idx, LEAFENTRY le, int direction) {
    int keybump=direction;

    if (0 != (node->flags & TOKU_DB_DUPSORT)) {
	if (idx>0) {
	    if (other_key_matches(node, idx-1, le)) keybump=0;
	}
	if (idx+1<toku_omt_size(node->u.l.buffer)) {
	    if (other_key_matches(node, idx+1, le)) keybump=0;
	}
    }
    node->u.l.leaf_stats.nkeys += keybump;;
    assert(node->u.l.leaf_stats.exact);
}

static int
brt_leaf_apply_cmd_once (BRTNODE node, BRT_CMD cmd,
                         u_int32_t idx, LEAFENTRY le)
// Effect: Apply cmd to leafentry
//   idx is the location where it goes
//   le is old leafentry
{
    // brt_leaf_check_leaf_stats(node);

    u_int32_t newlen=0, newdisksize=0;
    LEAFENTRY new_le=0;
    void *maybe_free = 0;
    // This function may call mempool_malloc_dont_release() to allocate more space.
    // That means the old pointers are guaranteed to still be good, but the data may have been copied into a new mempool.
    // We'll have to release the old mempool later.
    int r = apply_cmd_to_leaf(cmd, le, &newlen, &newdisksize, &new_le, node->u.l.buffer, &node->u.l.buffer_mempool, &maybe_free);
    if (r!=0) return r;
    if (new_le) assert(newdisksize == leafentry_disksize(new_le));

    //printf("Applying command: %s xid=%lld ", unparse_cmd_type(cmd->type), (long long)cmd->xid);
    //toku_print_BYTESTRING(stdout, cmd->u.id.key->size, cmd->u.id.key->data);
    //printf(" ");
    //toku_print_BYTESTRING(stdout, cmd->u.id.val->size, cmd->u.id.val->data);
    //printf(" to \n");
    //print_leafentry(stdout, le); printf("\n");
    //printf(" got "); print_leafentry(stdout, new_le); printf("\n");

    if (le && new_le) {

	// If we are replacing a leafentry, then the counts on the estimates remain unchanged, but the size might change
	{
	    u_int32_t oldlen = le_any_vallen(le);
	    assert(node->u.l.leaf_stats.dsize >= oldlen);
	    assert(node->u.l.leaf_stats.dsize < (1U<<31)); // make sure we didn't underflow
	    node->u.l.leaf_stats.dsize -= oldlen;
	    node->u.l.leaf_stats.dsize += le_any_vallen(new_le); // add it in two pieces to avoid ugly overflow
	    assert(node->u.l.leaf_stats.dsize < (1U<<31)); // make sure we didn't underflow
	}

        node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(le);
        node->local_fingerprint     -= node->rand4fingerprint * toku_le_crc(le);
        
	//printf("%s:%d Added %u-%u got %lu\n", __FILE__, __LINE__, le_any_keylen(new_le), le_any_vallen(le), node->u.l.leaf_stats.dsize);
	// the ndata and nkeys remains unchanged

        u_int32_t size = leafentry_memsize(le);

        // This mfree must occur after the mempool_malloc so that when the mempool is compressed everything is accounted for.
        // But we must compute the size before doing the mempool malloc because otherwise the le pointer is no good.
        toku_mempool_mfree(&node->u.l.buffer_mempool, 0, size); // Must pass 0, since le may be no good any more.
        
        node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + newdisksize;
        node->local_fingerprint += node->rand4fingerprint*toku_le_crc(new_le);

        if ((r = toku_omt_set_at(node->u.l.buffer, new_le, idx))) goto return_r;

    } else {
        if (le) {
            // It's there, note that it's gone and remove it from the mempool

	    // Figure out if one of the other keys is the same key
	    maybe_bump_nkeys(node, idx, le, -1);

            if ((r = toku_omt_delete_at(node->u.l.buffer, idx))) goto return_r;

            node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(le);
            node->local_fingerprint     -= node->rand4fingerprint * toku_le_crc(le);

	    {
		u_int32_t oldlen = le_any_vallen(le) + le_any_keylen(le);
		assert(node->u.l.leaf_stats.dsize >= oldlen);
		node->u.l.leaf_stats.dsize -= oldlen;
	    }
	    assert(node->u.l.leaf_stats.dsize < (1U<<31)); // make sure we didn't underflow
	    node->u.l.leaf_stats.ndata --;

            toku_mempool_mfree(&node->u.l.buffer_mempool, 0, leafentry_memsize(le)); // Must pass 0, since le may be no good any more.

        }
        if (new_le) {


            if ((r = toku_omt_insert_at(node->u.l.buffer, new_le, idx))) goto return_r;

            node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + newdisksize;
            node->local_fingerprint += node->rand4fingerprint*toku_le_crc(new_le);

	    node->u.l.leaf_stats.dsize += le_any_vallen(new_le) + le_any_keylen(new_le);
	    assert(node->u.l.leaf_stats.dsize < (1U<<31)); // make sure we didn't underflow
	    node->u.l.leaf_stats.ndata ++;
	    // Look at the key to the left and the one to the right.  If both are different then increment nkeys.
	    maybe_bump_nkeys(node, idx, new_le, +1);
        }
    }
    r=0;
//        printf("%s:%d rand4=%08x local_fingerprint=%08x this=%08x\n", __FILE__, __LINE__, node->rand4fingerprint, node->local_fingerprint, toku_calccrc32_kvpair_struct(kv));
 return_r:

    if (maybe_free) toku_free(maybe_free); //

    // brt_leaf_check_leaf_stats(node);

    return r;
}

static int
brt_leaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
                  enum reactivity *re /*OUT*/
                  )
// Effect: Put a cmd into a leaf.
//  Return the serialization size in *new_size.
// The leaf could end up "too big" or "too small".  It is up to the caller to fix that up.
{
//    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
    VERIFY_NODE(t, node);
    assert(node->height==0);

    LEAFENTRY storeddata;
    OMTVALUE storeddatav=NULL;

    u_int32_t idx;
    int r;
    int compare_both = should_compare_both_keys(node, cmd);
    struct cmd_leafval_heaviside_extra be = {t, cmd, compare_both};

    //static int counter=0;
    //counter++;
    //printf("counter=%d\n", counter);

    switch (cmd->type) {
    case BRT_INSERT:
        if (node->u.l.seqinsert) {
            idx = toku_omt_size(node->u.l.buffer);
            r = toku_omt_fetch(node->u.l.buffer, idx-1, &storeddatav, NULL);
            if (r != 0) goto fz;
            storeddata = storeddatav;
            int cmp = toku_cmd_leafval_heaviside(storeddata, &be);
            if (cmp >= 0) goto fz;
            r = DB_NOTFOUND;
        } else {
        fz:
            r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_heaviside, &be,
                                   &storeddatav, &idx, NULL);
        }
        if (r==DB_NOTFOUND) {
            storeddata = 0;
        } else if (r!=0) {
            return r;
        } else {
            storeddata=storeddatav;
        }
        
        r = brt_leaf_apply_cmd_once(node, cmd, idx, storeddata);
        if (r!=0) return r;

        // if the insertion point is within a window of the right edge of
        // the leaf then it is sequential
        // window = min(32, number of leaf entries/16)
        {
        u_int32_t s = toku_omt_size(node->u.l.buffer);
        u_int32_t w = s / 16;
        if (w == 0) w = 1;
        if (w > 32) w = 32;

        // within the window?
        if (s - idx <= w) {
            node->u.l.seqinsert += 1;
        } else {
            node->u.l.seqinsert = 0;
        }
        }
        break;
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:

        // Delete the one item
        r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_heaviside,  &be,
                               &storeddatav, &idx, NULL);
        if (r == DB_NOTFOUND) break;
        if (r != 0) return r;
        storeddata=storeddatav;

        VERIFY_NODE(t, node);

        //static int count=0; count++;
        r = brt_leaf_apply_cmd_once(node, cmd, idx, storeddata);
        if (r!=0) return r;

        VERIFY_NODE(t, node);
        break;

    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
        // Delete all the matches

        r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_heaviside, &be,
                               &storeddatav, &idx, NULL);
        if (r == DB_NOTFOUND) break;
        if (r != 0) return r;
        storeddata=storeddatav;
            
        while (1) {
            int   vallen   = le_any_vallen(storeddata);
            void *save_val = toku_memdup(le_any_val(storeddata), vallen);

            r = brt_leaf_apply_cmd_once(node, cmd, idx, storeddata);
            if (r!=0) return r;

            // Now we must find the next one.
            DBT valdbt;
            BRT_CMD_S ncmd = { cmd->type, cmd->xid, .u.id={cmd->u.id.key, toku_fill_dbt(&valdbt, save_val, vallen)}};
            struct cmd_leafval_heaviside_extra nbe = {t, &ncmd, 1};
            r = toku_omt_find(node->u.l.buffer, toku_cmd_leafval_heaviside,  &nbe, +1,
                              &storeddatav, &idx, NULL);
            
            toku_free(save_val);
            if (r!=0) break;
            storeddata=storeddatav;
            {   // Continue only if the next record that we found has the same key.
                DBT adbt;
                if (t->compare_fun(t->db,
                                   toku_fill_dbt(&adbt, le_any_key(storeddata), le_any_keylen(storeddata)),
                                   cmd->u.id.key) != 0)
                    break;
            }
        }

        break;

    case BRT_NONE: return EINVAL;
    }
    /// All done doing the work

    node->dirty = 1;
        
//        toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
    *re = get_leaf_reactivity(node);
    if (cmd->type == BRT_INSERT && *re == RE_FUSIBLE)
        *re = RE_STABLE;
    VERIFY_NODE(t, node);
    return 0;
}

static int brt_nonleaf_cmd_once_to_child (BRT t, BRTNODE node, unsigned int childnum, BRT_CMD cmd,
                                          enum reactivity re_array[], BOOL *did_io)
{

    verify_local_fingerprint_nonleaf(node);

    // if the fifo is empty and the child is in main memory and the child isn't gorged, then put it in the child
    if (BNC_NBYTESINBUF(node, childnum) == 0) {
        BLOCKNUM childblocknum  = BNC_BLOCKNUM(node, childnum); 
        u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnum);
        void *child_v = NULL;
        int r = toku_cachetable_maybe_get_and_pin(t->cf, childblocknum, childfullhash, &child_v);
        if (r!=0) {
            // It's not in main memory, so
            goto put_in_fifo;
        }
        // The child is in main memory.
        BRTNODE child = child_v;

        verify_local_fingerprint_nonleaf(child);

        r = brtnode_put_cmd (t, child, cmd, &re_array[childnum], did_io);
        fixup_child_fingerprint(node, childnum, child);
        VERIFY_NODE(t, node);

        verify_local_fingerprint_nonleaf(child);

        int rr = toku_unpin_brtnode(t, child);
        assert(rr==0);

        verify_local_fingerprint_nonleaf(node);

        return r;
    }

 put_in_fifo:

    {
        int type = cmd->type;
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;

	node->local_fingerprint += node->rand4fingerprint * toku_calc_fingerprint_cmd(type, cmd->xid, k->data, k->size, v->data, v->size);
        int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
        int r=toku_fifo_enq(BNC_BUFFER(node,childnum), k->data, k->size, v->data, v->size, type, cmd->xid);
        assert(r==0);
        node->u.n.n_bytes_in_buffers += diff;
        BNC_NBYTESINBUF(node, childnum) += diff;
        node->dirty = 1;
    }

    verify_local_fingerprint_nonleaf(node);

    return 0;
}

/* find the leftmost child that may contain the key */
unsigned int toku_brtnode_which_child (BRTNODE node , DBT *k, DBT *d, BRT t) {
    assert(node->height>0);
#define DO_PIVOT_SEARCH_LR 0
#if DO_PIVOT_SEARCH_LR
    int i;
    for (i=0; i<node->u.n.n_children-1; i++) {
        int cmp = brt_compare_pivot(t, k, d, node->u.n.childkeys[i]);
        if (cmp > 0) continue;
        if (cmp < 0) return i;
        return i;
    }
    return node->u.n.n_children-1;
#else
#endif
#define DO_PIVOT_SEARCH_RL 0
#if DO_PIVOT_SEARCH_RL
    // give preference for appending to the dictionary.  no change for
    // random keys
    int i;
    for (i = node->u.n.n_children-2; i >= 0; i--) {
        int cmp = brt_compare_pivot(t, k, d, node->u.n.childkeys[i]);
        if (cmp > 0) return i+1;
    }
    return 0;
#endif
#define DO_PIVOT_BIN_SEARCH 1
#if DO_PIVOT_BIN_SEARCH
    // a funny case of no pivots
    if (node->u.n.n_children <= 1) return 0;

    // check the last key to optimize seq insertions
    int n = node->u.n.n_children-1;
    int cmp = brt_compare_pivot(t, k, d, node->u.n.childkeys[n-1]);
    if (cmp > 0) return n;

    // binary search the pivots
    int lo = 0;
    int hi = n-1; // skip the last one, we checked it above
    int mi;
    while (lo < hi) {
        mi = (lo + hi) / 2;
        cmp = brt_compare_pivot(t, k, d, node->u.n.childkeys[mi]);
        if (cmp > 0) {
            lo = mi+1;
            continue;
        } 
        if (cmp < 0) {
            hi = mi;
            continue;
        }
        return mi;
    }
    return lo;
#endif
}

static int brt_nonleaf_cmd_once (BRT t, BRTNODE node, BRT_CMD cmd,
                                 enum reactivity re_array[], BOOL *did_io)
// Effect: Insert a message into a nonleaf.  We may put it into a child, possibly causing the child to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to reactivity of any modified child.
{

    verify_local_fingerprint_nonleaf(node);

    verify_local_fingerprint_nonleaf(node);
    /* find the right subtree */
    unsigned int childnum = toku_brtnode_which_child(node, cmd->u.id.key, cmd->u.id.val, t);

    int r = brt_nonleaf_cmd_once_to_child (t, node, childnum, cmd, re_array, did_io);

    verify_local_fingerprint_nonleaf(node);

    return r;
}

static int
brt_nonleaf_cmd_many (BRT t, BRTNODE node, BRT_CMD cmd,
                      enum reactivity re_array[], BOOL *did_io)
// Effect: Put the cmd into a nonleaf node.  We may put it into several children, possibly causing the children to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.  (And there may be several such children.)
{
    /* find all children that need a copy of the command */
    unsigned int sendchild[node->u.n.n_children];
    unsigned int delidx = 0;
#define sendchild_append(i) \
        if (delidx == 0 || sendchild[delidx-1] != i) sendchild[delidx++] = i;
    unsigned int i;
    for (i = 0; i+1 < (unsigned int)node->u.n.n_children; i++) {
        int cmp = brt_compare_pivot(t, cmd->u.id.key, 0, node->u.n.childkeys[i]);
        if (cmp > 0) {
            continue;
        } else if (cmp < 0) {
            sendchild_append(i);
            break;
        } else if (t->flags & TOKU_DB_DUPSORT) {
            sendchild_append(i);
            sendchild_append(i+1);
        } else {
            sendchild_append(i);
            break;
        }
    }

    if (delidx == 0)
        sendchild_append((unsigned int)(node->u.n.n_children-1));
#undef sendchild_append

    /* issue the cmd to all of the children found previously */
    int r;
    for (i=0; i<delidx; i++) {
        /* Append the cmd to the appropriate child buffer. */
        int childnum = sendchild[i];

        r = brt_nonleaf_cmd_once_to_child(t, node, childnum, cmd, re_array, did_io);
        if (r!=0)  goto return_r;
    }
    r=0;
 return_r:
    return r;
}

static int
brt_nonleaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
                     enum reactivity re_array[], BOOL *did_io)
// Effect: Put the cmd into a nonleaf node.  We may put it into a child, possibly causing the child to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.  (And there may be several such children.)
//
{

    verify_local_fingerprint_nonleaf(node);

    switch (cmd->type) {
    case BRT_INSERT:
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:
    do_once:
        return brt_nonleaf_cmd_once(t, node, cmd, re_array, did_io);
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
        if (0 == (node->flags & TOKU_DB_DUPSORT)) goto do_once; // nondupsort delete_any is just do once.
        return brt_nonleaf_cmd_many(t, node, cmd, re_array, did_io);
    case BRT_NONE:
        break;
    }
    abort(); return 0;
}

static LEAFENTRY
fetch_from_buf (OMT omt, u_int32_t idx) {
    OMTVALUE v = 0;
    int r = toku_omt_fetch(omt, idx, &v, NULL);
    assert(r==0);
    return (LEAFENTRY)v;
}

static int
merge_leaf_nodes (BRTNODE a, BRTNODE b) {
    OMT omta = a->u.l.buffer;
    OMT omtb = b->u.l.buffer;
    while (toku_omt_size(omtb)>0) {
        LEAFENTRY le = fetch_from_buf(omtb, 0);
        u_int32_t le_size = leafentry_memsize(le);
        u_int32_t le_crc  = toku_le_crc(le);
        {
            LEAFENTRY new_le = mempool_malloc_from_omt(omta, &a->u.l.buffer_mempool, le_size, 0);
            assert(new_le);
            memcpy(new_le, le, le_size);
	    int idx = toku_omt_size(a->u.l.buffer);
            int r = toku_omt_insert_at(omta, new_le, idx);
            assert(r==0);
            a->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + le_size;
            a->local_fingerprint     += a->rand4fingerprint * le_crc;

	    a->u.l.leaf_stats.ndata++;
	    maybe_bump_nkeys(a, idx, new_le, +1);
	    a->u.l.leaf_stats.dsize+= le_any_keylen(le) + le_any_vallen(le);
	    //printf("%s:%d Added %u got %lu\n", __FILE__, __LINE__, le_any_keylen(le)+le_any_vallen(le), a->u.l.leaf_stats.dsize);
        }
        {
	    maybe_bump_nkeys(b, 0, le, -1);
            int r = toku_omt_delete_at(omtb, 0);
            assert(r==0);
            b->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + le_size;
            b->local_fingerprint     -= b->rand4fingerprint * le_crc;

	    b->u.l.leaf_stats.ndata--;
	    b->u.l.leaf_stats.dsize-= le_any_keylen(le) + le_any_vallen(le);
	    //printf("%s:%d Subed %u got %lu\n", __FILE__, __LINE__, le_any_keylen(le)+le_any_vallen(le), b->u.l.leaf_stats.dsize);
	    assert(b->u.l.leaf_stats.ndata < 1U<<31);
	    assert(b->u.l.leaf_stats.nkeys < 1U<<31);
	    assert(b->u.l.leaf_stats.dsize < 1U<<31);

            toku_mempool_mfree(&b->u.l.buffer_mempool, 0, le_size);
        }
    }
    a->dirty = 1;
    b->dirty = 1;
    return 0;
}

static int
balance_leaf_nodes (BRTNODE a, BRTNODE b, struct kv_pair **splitk)
// Effect:
//  If b is bigger then move stuff from b to a until b is the smaller.
//  If a is bigger then move stuff from a to b until a is the smaller.
{
    BOOL move_from_right = (BOOL)(toku_serialize_brtnode_size(a) < toku_serialize_brtnode_size(b));
    BRTNODE from = move_from_right ? b : a;
    BRTNODE to   = move_from_right ? a : b;
    OMT  omtfrom = from->u.l.buffer;
    OMT  omtto   = to  ->u.l.buffer;
    assert(toku_serialize_brtnode_size(to) <= toku_serialize_brtnode_size(from)); // Could be equal in some screwy cases.
    while (toku_serialize_brtnode_size(to) <  toku_serialize_brtnode_size(from)) {
        int from_idx = move_from_right ? 0                    : toku_omt_size(omtfrom)-1;
        int to_idx   = move_from_right ? toku_omt_size(omtto) : 0;
        LEAFENTRY le = fetch_from_buf(omtfrom, from_idx);
        u_int32_t le_size = leafentry_memsize(le);
        u_int32_t le_crc  = toku_le_crc(le);
        {
            LEAFENTRY new_le = mempool_malloc_from_omt(omtto, &to->u.l.buffer_mempool, le_size, 0);
            assert(new_le);
            memcpy(new_le, le, le_size);
            int r = toku_omt_insert_at(omtto, new_le, to_idx);
            assert(r==0);
	    maybe_bump_nkeys(to, to_idx, le, +1);
            to  ->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + le_size;
            to  ->local_fingerprint     += to->rand4fingerprint * le_crc;

	    to->u.l.leaf_stats.ndata++;
	    to->u.l.leaf_stats.dsize+= le_any_keylen(le) + le_any_vallen(le);
	    //printf("%s:%d Added %u got %lu\n", __FILE__, __LINE__, le_any_keylen(le)+ le_any_vallen(le), to->u.l.leaf_stats.dsize);
        }
        {
	    maybe_bump_nkeys(from, from_idx, le, -1);
            int r = toku_omt_delete_at(omtfrom, from_idx);
            assert(r==0);
            from->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + le_size;
            from->local_fingerprint     -= from->rand4fingerprint * le_crc;

	    from->u.l.leaf_stats.ndata--;
	    from->u.l.leaf_stats.dsize-= le_any_keylen(le) + le_any_vallen(le);
	    assert(from->u.l.leaf_stats.ndata < 1U<<31);
	    assert(from->u.l.leaf_stats.nkeys < 1U<<31);
	    //printf("%s:%d Removed %u  get %lu\n", __FILE__, __LINE__, le_any_keylen(le)+ le_any_vallen(le), from->u.l.leaf_stats.dsize);

            toku_mempool_mfree(&from->u.l.buffer_mempool, 0, le_size);
        }
    }
    assert(from->u.l.leaf_stats.dsize < 1U<<31);
    assert(toku_omt_size(a->u.l.buffer)>0);
    {
        LEAFENTRY le = fetch_from_buf(a->u.l.buffer, toku_omt_size(a->u.l.buffer)-1);
        if (a->flags&TOKU_DB_DUPSORT) {
            *splitk = kv_pair_malloc(le_any_key(le), le_any_keylen(le), le_any_val(le), le_any_vallen(le));
        } else {
            *splitk = kv_pair_malloc(le_any_key(le), le_any_keylen(le), 0, 0);
        }
    }
    a->dirty = 1; // make them dirty even if nothing actually happened.
    b->dirty = 1;
    // Boundary case: If both were empty then the loop will fall through.  (Generally if they are the same size the loop falls through.)
    return 0;
}


static int
maybe_merge_pinned_leaf_nodes (BRTNODE parent, int childnum_of_parent,
			       BRTNODE a, BRTNODE b, struct kv_pair *parent_splitk, BOOL *did_merge, struct kv_pair **splitk)
// Effect: Either merge a and b into one one node (merge them into a) and set *did_merge = TRUE.    (We do this if the resulting node is not fissible)
//         or distribute the leafentries evenly between a and b.   (If a and be are already evenly distributed, we may do nothing.)
{
    unsigned int sizea = toku_serialize_brtnode_size(a);
    unsigned int sizeb = toku_serialize_brtnode_size(b);
    if ((sizea + sizeb)*4 > (a->nodesize*3)) {
        // the combined size is more than 3/4 of a node, so don't merge them.
        *did_merge = FALSE;
        if (sizea*4 > a->nodesize && sizeb*4 > a->nodesize) {
            // no need to do anything if both are more than 1/4 of a node.
            *splitk = parent_splitk;
            return 0;
        }
        // one is less than 1/4 of a node, and together they are more than 3/4 of a node.
        toku_free(parent_splitk); // We don't need the parent_splitk any more.  If we need a splitk (if we don't merge) we'll malloc a new one.
        int r = balance_leaf_nodes(a, b, splitk);
	if (r != 0) return r;
    } else {
        // we are merging them.
        *did_merge = TRUE;
        *splitk = 0;
        toku_free(parent_splitk); // if we are merging, the splitk gets freed.
        int r = merge_leaf_nodes(a, b);
	if (r != 0) return r;
    }
    fixup_child_fingerprint(parent, childnum_of_parent,   a);
    fixup_child_fingerprint(parent, childnum_of_parent+1, b);
    return 0;
}

static int
maybe_merge_pinned_nonleaf_nodes (BRT t,
                                  BRTNODE parent, int childnum_of_parent, struct kv_pair *parent_splitk,
                                  BRTNODE a, BRTNODE b,
                                  BOOL *did_merge,
                                  struct kv_pair **splitk)
{
    verify_local_fingerprint_nonleaf(a);
    assert(parent_splitk);
    int old_n_children = a->u.n.n_children;
    int new_n_children = old_n_children + b->u.n.n_children;
    XREALLOC_N(new_n_children, a->u.n.childinfos);
    memcpy(a->u.n.childinfos + old_n_children,
           b->u.n.childinfos,
           b->u.n.n_children*sizeof(b->u.n.childinfos[0]));
    XREALLOC_N(new_n_children-1, a->u.n.childkeys);
    a->u.n.childkeys[old_n_children-1] = parent_splitk;
    memcpy(a->u.n.childkeys + old_n_children,
           b->u.n.childkeys,
           (b->u.n.n_children-1)*sizeof(b->u.n.childkeys[0]));
    a->u.n.totalchildkeylens += b->u.n.totalchildkeylens + toku_brt_pivot_key_len(t, parent_splitk);
    a->u.n.n_bytes_in_buffers += b->u.n.n_bytes_in_buffers;
    a->u.n.n_children = new_n_children;

    b->u.n.totalchildkeylens = 0;
    b->u.n.n_children = 0;
    b->u.n.n_bytes_in_buffers = 0;

    {
        static int count=0; count++;
        u_int32_t fp = 0;
        int i;
        for (i=0; i<a->u.n.n_children; i++)
            FIFO_ITERATE(BNC_BUFFER(a,i), key, keylen, data, datalen, type, xid,
                         fp += toku_calc_fingerprint_cmd(type, xid, key, keylen, data, datalen);
                         );
        a->local_fingerprint = a->rand4fingerprint * fp;
        //printf("%s:%d fp=%u\n", __FILE__, __LINE__, a->local_fingerprint);
        verify_local_fingerprint_nonleaf(a);
    }
    b->local_fingerprint = 0;

    a->dirty = 1;
    b->dirty = 1;

    fixup_child_fingerprint(parent, childnum_of_parent, a);
//    abort(); // don't forget to reuse blocknums
    *did_merge = TRUE;
    *splitk    = NULL;
    verify_local_fingerprint_nonleaf(a);
    return 0;
}

static int
maybe_merge_pinned_nodes (BRT t,
                          BRTNODE parent, int childnum_of_parent, struct kv_pair *parent_splitk,
                          BRTNODE a, BRTNODE b, BOOL *did_merge, struct kv_pair **splitk)
// Effect: either merge a and b into one node (merge them into a) and set *did_merge = TRUE.  (We do this if the resulting node is not fissible)
//             or distribute a and b evenly and set *did_merge = FALSE  (If a and be are already evenly distributed, we may do nothing.)
//  If we distribute:
//    For leaf nodes, we distribute the leafentries evenly.
//    For nonleaf nodes, we distribute the children evenly.  That may leave one or both of the nodes overfull, but that's OK.
//  If we distribute, we set *splitk to a malloced pivot key.
// Parameters:
//  t                   The BRT.
//  parent              The parent of the two nodes to be split.
//  childnum_of_parent  Which child of the parent is a?  (b is the next child.)
//  parent_splitk       The pivot key between a and b.   This is either free()'d or returned in *splitk.
//  a                   The first node to merge.
//  b                   The second node to merge.
//  logger              The logger.
//  did_merge           (OUT):  Did the two nodes actually get merged?
//  splitk              (OUT):  If the two nodes did not get merged, the new pivot key between the two nodes.
{
    assert(a->height == b->height);
    verify_local_fingerprint_nonleaf(a);
    parent->dirty = 1; // just to make sure 
    if (a->height == 0) {
        return maybe_merge_pinned_leaf_nodes(parent, childnum_of_parent, a, b, parent_splitk, did_merge, splitk);
    } else {
        int r = maybe_merge_pinned_nonleaf_nodes(t, parent, childnum_of_parent, parent_splitk, a, b, did_merge, splitk);
        verify_local_fingerprint_nonleaf(a);
        return r;
    }
}

static int
brt_merge_child (BRT t, BRTNODE node, int childnum_to_merge, BOOL *did_io, BOOL *did_react)
{
    verify_local_fingerprint_nonleaf(node);
    if (node->u.n.n_children < 2) return 0; // if no siblings, we are merged as best we can.

    int childnuma,childnumb;
    if (childnum_to_merge > 0) {
        childnuma = childnum_to_merge-1;
        childnumb = childnum_to_merge;
    } else {
        childnuma = childnum_to_merge;
        childnumb = childnum_to_merge+1;
    }
    assert(0 <= childnuma);
    assert(childnuma+1 == childnumb);
    assert(childnumb < node->u.n.n_children);

    assert(node->height>0);

    if (toku_fifo_n_entries(BNC_BUFFER(node,childnuma))>0) {
        enum reactivity re = RE_STABLE;
        int r = flush_this_child(t, node, childnuma, &re, did_io);
        if (r!=0) return r;
    }
    if (toku_fifo_n_entries(BNC_BUFFER(node,childnumb))>0) {
        enum reactivity re = RE_STABLE;
        int r = flush_this_child(t, node, childnumb, &re, did_io);
        if (r!=0) return r;
    }

    // We suspect that at least one of the children is fusible, but they might not be.

    BRTNODE childa, childb;
    {
        void *childnode_v;
        u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnuma);
        int r = toku_cachetable_get_and_pin(t->cf, BNC_BLOCKNUM(node, childnuma), childfullhash, &childnode_v, NULL,
                                            toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
        if (r!=0) return r;
        childa = childnode_v;
        verify_local_fingerprint_nonleaf(childa);
    }
    {
        void *childnode_v;
        u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnumb);
        int r = toku_cachetable_get_and_pin(t->cf, BNC_BLOCKNUM(node, childnumb), childfullhash, &childnode_v, NULL,
                                            toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
        if (r!=0) {
            toku_unpin_brtnode(t, childa); // ignore the result
            return r;
        }
        childb = childnode_v;
    }

    // now we have both children pinned in main memory.

    int r;
    BOOL did_merge;
    {
        struct kv_pair *splitk_kvpair = 0;
        struct kv_pair *old_split_key = node->u.n.childkeys[childnuma];
        unsigned int deleted_size = toku_brt_pivot_key_len(t, old_split_key);
        verify_local_fingerprint_nonleaf(childa);
        r = maybe_merge_pinned_nodes(t, node, childnuma, node->u.n.childkeys[childnuma], childa, childb, &did_merge, &splitk_kvpair);
        verify_local_fingerprint_nonleaf(childa);
        if (childa->height>0) { int i; for (i=0; i+1<childa->u.n.n_children; i++) assert(childa->u.n.childkeys[i]); }
        //(toku_verify_counts(childa), toku_verify_estimates(t,childa));
        *did_react = did_merge;
        if (did_merge) assert(!splitk_kvpair); else assert(splitk_kvpair);
        if (r!=0) goto return_r;

        node->u.n.totalchildkeylens -= deleted_size; // The key was free()'d inside the maybe_merge_pinned_nodes.

        verify_local_fingerprint_nonleaf(node);
        verify_local_fingerprint_nonleaf(childa);
        if (did_merge) {
            toku_fifo_free(&BNC_BUFFER(node, childnumb));
            node->u.n.n_children--;
            memmove(&node->u.n.childinfos[childnumb],
                    &node->u.n.childinfos[childnumb+1],
                    (node->u.n.n_children-childnumb)*sizeof(node->u.n.childinfos[0]));
            REALLOC_N(node->u.n.n_children, node->u.n.childinfos);
            memmove(&node->u.n.childkeys[childnuma],
                    &node->u.n.childkeys[childnuma+1],
                    (node->u.n.n_children-childnumb)*sizeof(node->u.n.childkeys[0]));
            REALLOC_N(node->u.n.n_children-1, node->u.n.childkeys);
            fixup_child_fingerprint(node, childnuma, childa);
            assert(node->u.n.childinfos[childnuma].blocknum.b == childa->thisnodename.b);
            verify_local_fingerprint_nonleaf(node);
            verify_local_fingerprint_nonleaf(childa);
	    childa->dirty = 1; // just to make sure
	    childb->dirty = 1; // just to make sure
        } else {
            assert(splitk_kvpair);
            // If we didn't merge the nodes, then we need the correct pivot.
            node->u.n.childkeys[childnuma] = splitk_kvpair;
            node->u.n.totalchildkeylens += toku_brt_pivot_key_len(t, node->u.n.childkeys[childnuma]);
            verify_local_fingerprint_nonleaf(node);
            node->dirty = 1;
        }
    }
    assert(node->dirty);
 return_r:
    // Unpin both, and return the first nonzero error code that is found
    {
        int rra = toku_unpin_brtnode(t, childa);
        int rrb;
        if (did_merge) {
            BLOCKNUM bn = childb->thisnodename;
            rrb = toku_cachetable_unpin_and_remove(t->cf, bn);
            toku_free_blocknum(t->h->blocktable, &bn, t->h);
        } else {
            rrb = toku_unpin_brtnode(t, childb);
        }

        if (rra) return rra;
        if (rrb) return rrb;
    }
    verify_local_fingerprint_nonleaf(node);
    return r;
}
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
static int STRADDLE_HACK_disable_merges_and_splits = 0;
#endif

static int
brt_handle_maybe_reactive_child(BRT t, BRTNODE node, int childnum, enum reactivity re, BOOL *did_io, BOOL *did_react) {
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
    if (STRADDLE_HACK_disable_merges_and_splits) {
        *did_react = FALSE;
        return 0;
    }
#endif
    switch (re) {
    case RE_STABLE:
        *did_react = FALSE;
        return 0;
    case RE_FISSIBLE:
        return brt_split_child(t, node, childnum, did_react);
    case RE_FUSIBLE:
        return brt_merge_child(t, node, childnum, did_io, did_react);
    }
    abort(); return 0; // this cannot happen
}

static int
brt_handle_maybe_reactive_child_at_root (BRT brt, CACHEKEY *rootp, BRTNODE *nodep, enum reactivity re, TOKULOGGER logger) {
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
    if (STRADDLE_HACK_disable_merges_and_splits) {
        return 0;
    }
#endif
    BRTNODE node = *nodep;
    switch (re) {
    case RE_STABLE:
        return 0;
    case RE_FISSIBLE:
        // The root node should split, so make a new root.
        {
            BRTNODE nodea,nodeb;
            DBT splitk;
            if (node->height==0) {
                int r = brtleaf_split(brt, node, &nodea, &nodeb, &splitk);
                if (r!=0) return r;
            } else {
                int r = brt_nonleaf_split(brt, node, &nodea, &nodeb, &splitk);
                if (r!=0) return r;
            }
	    //verify_local_fingerprint_nonleaf(nodea);
	    //verify_local_fingerprint_nonleaf(nodeb);
            return brt_init_new_root(brt, nodea, nodeb, splitk, rootp, logger, nodep);
        }
    case RE_FUSIBLE:
        return 0; // Cannot merge anything at the root, so return happy.
    }
    abort(); return 0;

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

static int
flush_this_child (BRT t, BRTNODE node, int childnum, enum reactivity *child_re, BOOL *did_io)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
// The child could end up reactive, and this function doesn't fix that.
{
    assert(node->height>0);
    BLOCKNUM targetchild = BNC_BLOCKNUM(node, childnum);
    toku_verify_blocknum_allocated(t->h->blocktable, targetchild);
    u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnum);
    BRTNODE child;
    {
        void *childnode_v;
        int r = toku_cachetable_get_and_pin(t->cf, targetchild, childfullhash, &childnode_v, NULL, 
                                            toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
        if (r!=0) return r;
        child = childnode_v;
    }
    assert(child->thisnodename.b!=0);
    VERIFY_NODE(t, child);

    int r = 0;
    {
        bytevec key,val;
        ITEMLEN keylen, vallen;
        //printf("%s:%d Try random_pick, weight=%d \n", __FILE__, __LINE__, BNC_NBYTESINBUF(node, childnum));
        assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))>0);
        u_int32_t type;
        TXNID xid;
        while(0==toku_fifo_peek(BNC_BUFFER(node,childnum), &key, &keylen, &val, &vallen, &type, &xid)) {
            DBT hk,hv;

            BRT_CMD_S brtcmd = { (enum brt_cmd_type)type, xid, .u.id= {toku_fill_dbt(&hk, key, keylen),
                                                                       toku_fill_dbt(&hv, val, vallen)} };

            int n_bytes_removed = (hk.size + hv.size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD);
            u_int32_t old_from_fingerprint = node->local_fingerprint;
            u_int32_t delta = toku_calc_fingerprint_cmd(type, xid, key, keylen, val, vallen);
            u_int32_t new_from_fingerprint = old_from_fingerprint - node->rand4fingerprint*delta;

            //printf("%s:%d random_picked\n", __FILE__, __LINE__);
            r = brtnode_put_cmd (t, child, &brtcmd, child_re, did_io);

            //printf("%s:%d %d=push_a_brt_cmd_down=();  child_did_split=%d (weight=%d)\n", __FILE__, __LINE__, r, child_did_split, BNC_NBYTESINBUF(node, childnum));
            if (r!=0) goto return_r;

            r = toku_fifo_deq(BNC_BUFFER(node,childnum));
            //printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
            if (r!=0) goto return_r;

            node->local_fingerprint = new_from_fingerprint;
            node->u.n.n_bytes_in_buffers -= n_bytes_removed;
            BNC_NBYTESINBUF(node, childnum) -= n_bytes_removed;
            node->dirty = 1;

        }
        if (0) printf("%s:%d done random picking\n", __FILE__, __LINE__);
    }
    verify_local_fingerprint_nonleaf(node);
 return_r:
    fixup_child_fingerprint(node, childnum, child);
    {
        int rr=toku_unpin_brtnode(t, child);
        if (rr!=0) return rr;
    }
    return r;

}

static int
flush_some_child (BRT t, BRTNODE node, enum reactivity re_array[], BOOL *did_io)
{
    assert(node->height>0);
    int childnum;
    find_heaviest_child(node, &childnum);
    assert(toku_fifo_n_entries(BNC_BUFFER(node, childnum))>0);
    return flush_this_child (t, node, childnum, &re_array[childnum], did_io);
}


static int
brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd, enum reactivity *re, BOOL *did_io)
// Effect: Push CMD into the subtree rooted at NODE, and indicate whether as a result NODE should split or should merge.
//   If NODE is a leaf, then
//      put CMD into leaf, applying it to the leafentries
//   If NODE is a nonleaf, then push the cmd in the relevant child (or children).  That may entail putting it into FIFOs or
//      actually putting it into the child.
//   Set *re to the reactivity of the node.   If node becomes reactive, we don't change its shape (but if a child becomes reactive, we fix it.)
//   If we perform I/O then set *did_io to true.
//   If a nonleaf node becomes overfull then we will flush some child.
{

    verify_local_fingerprint_nonleaf(node);

    if (node->height==0) {
        return brt_leaf_put_cmd(t, node, cmd, re);
    } else {
        verify_local_fingerprint_nonleaf(node);
        enum reactivity child_re[node->u.n.n_children];
        { int i; for (i=0; i<node->u.n.n_children; i++) child_re[i]=RE_STABLE; }
        int r = brt_nonleaf_put_cmd(t, node, cmd, child_re, did_io);
        if (r!=0) goto return_r;
        verify_local_fingerprint_nonleaf(node);
        // Now we may have overfilled node.  So we'll flush the heaviest child until we are happy.
        while (!*did_io                              // Don't flush if we've done I/O.
               && nonleaf_node_is_gorged(node)       // Don't flush if the node is small enough.
               && (node->u.n.n_bytes_in_buffers > 0) // Don't try to flush if everything is flushed.
               ) {
            r = flush_some_child(t, node, child_re, did_io);
            if (r!=0) goto return_r;
        }
        // Now all those children may need fixing.
        {
        int i;
        int original_n_children = node->u.n.n_children;
        for (i=0; i<original_n_children; i++) {
            int childnum = original_n_children - 1 -i;
            BOOL did_react; // ignore the result.
            r = brt_handle_maybe_reactive_child(t, node, childnum, child_re[childnum], did_io, &did_react);
            if (r!=0) break;
            if (*did_io) break;
        }
        }
    return_r:
        *re = get_nonleaf_reactivity(node);
        return r;
    }
}

static int push_something_at_root (BRT brt, BRTNODE *nodep, CACHEKEY *rootp, BRT_CMD cmd, TOKULOGGER logger)
// Effect:  Put CMD into brt by descending into the tree as deeply as we can
//   without performing I/O (but we must fetch the root),
//   bypassing only empty FIFOs
//   If the cmd is a broadcast message, we copy the message as needed as we descend the tree so that each relevant subtree receives the message.
//   At the end of the descent, we are either at a leaf, or we hit a nonempty FIFO.
//     If it's a leaf, and the leaf is gorged or hungry, then we split the leaf or merge it with the neighbor.
//      Note: for split operations, no disk I/O is required.  For merges, I/O may be required, so for a broadcast delete, quite a bit
//       of I/O could be induced in the worst case.
//     If it's a nonleaf, and the node is gorged or hungry, then we flush everything in the heaviest fifo to the child.
//       During flushing, we allow the child to become gorged.
//         (And for broadcast messages, we simply place the messages into all the relevant fifos of the child, rather than trying to descend.)
//       After flushing to a child, if the child is gorged (underful), then
//           if the child is leaf, we split (merge) it
//           if the child is a nonleaf, we flush the heaviest child recursively.
//       Note: After flushing, a node could still be gorged (or possibly hungry.)  We let it remain so.
//       Note: During the initial descent, we may gorged many nonleaf nodes.  We wish to flush only one nonleaf node at each level.
{
    BRTNODE node = *nodep;
    enum   reactivity re = RE_STABLE;
    BOOL   did_io = FALSE;
    {
        int r = brtnode_put_cmd(brt, node, cmd, &re, &did_io);
        verify_local_fingerprint_nonleaf(node);
        if (r!=0) return r;
        //if (should_split) printf("%s:%d Pushed something simple, should_split=1\n", __FILE__, __LINE__); 
    }
    //printf("%s:%d should_split=%d node_size=%" PRIu64 "\n", __FILE__, __LINE__, should_split, brtnode_memory_size(node));

    {
        int r = brt_handle_maybe_reactive_child_at_root(brt, rootp, nodep, re, logger);
        verify_local_fingerprint_nonleaf(*nodep);
        return r;
    }
}

static void compute_and_fill_remembered_hash (BRT brt) {
    struct remembered_hash *rh = &brt->h->root_hash;
    assert(brt->cf); // if cf is null, we'll be hosed.
    rh->valid = TRUE;
    rh->fnum=toku_cachefile_filenum(brt->cf);
    rh->root=brt->h->root;
    rh->fullhash = toku_cachetable_hash(brt->cf, rh->root);
}

static u_int32_t get_roothash (BRT brt) {
    struct remembered_hash *rh = &brt->h->root_hash;
    BLOCKNUM root = brt->h->root;
    // compare cf first, since cf is NULL for invalid entries.
    assert(rh);
    //printf("v=%d\n", rh->valid);
    if (rh->valid) {
        //printf("f=%d\n", rh->fnum.fileid); 
        //printf("cf=%d\n", toku_cachefile_filenum(brt->cf).fileid);
        if (rh->fnum.fileid == toku_cachefile_filenum(brt->cf).fileid)
            if (rh->root.b == root.b)
                return rh->fullhash;
    }
    compute_and_fill_remembered_hash(brt);
    return rh->fullhash;
}

CACHEKEY* toku_calculate_root_offset_pointer (BRT brt, u_int32_t *roothash) {
    *roothash = get_roothash(brt);
    return &brt->h->root;
}

int toku_brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger)
// Effect:  Flush the root fifo into the brt, and then push the cmd into the brt.
{
    if (logger) {
        BYTESTRING keybs = {.len=cmd->u.id.key->size, .data=cmd->u.id.key->data};
        BYTESTRING valbs = {.len=cmd->u.id.val->size, .data=cmd->u.id.val->data};
        int r = toku_log_enqrootentry(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), cmd->xid, cmd->type, keybs, valbs);
        if (r!=0) return r;
    }

    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
    int r;
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    assert(brt->h);

    brt->h->root_put_counter = global_root_put_counter++;
    u_int32_t fullhash;
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    //assert(fullhash==toku_cachetable_hash(brt->cf, *rootp));
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, fullhash, &node_v, NULL,
                                       toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h))) {
        return r;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;

    VERIFY_NODE(brt, node);

    assert(node->fullhash==fullhash);
    // push the fifo stuff
    {
        DBT okey,odata;
        BRT_CMD_S ocmd;
        while (0==toku_fifo_peek_cmdstruct(brt->h->fifo, &ocmd,  &okey, &odata)) {
            if ((r = push_something_at_root(brt, &node, rootp, &ocmd, logger))) return r;
            r = toku_fifo_deq(brt->h->fifo);
            assert(r==0);
        }
    }

    VERIFY_NODE(brt, node);
    verify_local_fingerprint_nonleaf(node);
    if ((r = push_something_at_root(brt, &node, rootp, cmd, logger))) {
	toku_unpin_brtnode(brt, node); // ignore any error code on the unpin.
	return r;
    }
    verify_local_fingerprint_nonleaf(node);
    r = toku_unpin_brtnode(brt, node);
    assert(r == 0);
    return 0;
}

int toku_cachefile_root_put_cmd (CACHEFILE cf, BRT_CMD cmd, TOKULOGGER logger) {
    int r;
    struct brt_header *h = toku_cachefile_get_userdata(cf);
    assert(h);
    r = toku_fifo_enq_cmdstruct(h->fifo, cmd);
    if (r!=0) return r;
    {
        BYTESTRING keybs = {.len=cmd->u.id.key->size, .data=cmd->u.id.key->data};
        BYTESTRING valbs = {.len=cmd->u.id.val->size, .data=cmd->u.id.val->data};
        r = toku_log_enqrootentry(logger, (LSN*)0, 0, toku_cachefile_filenum(cf), cmd->xid, cmd->type, keybs, valbs);
        if (r!=0) return r;
    }
    return 0;
}

int toku_brt_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn)
// Effect: Insert the key-val pair into brt.
{
    int r;
    if (txn && (brt->txnid_that_created_or_locked_when_empty != toku_txn_get_txnid(txn))) {
        toku_cachefile_refup(brt->cf);
        BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
        int need_data = (brt->flags&TOKU_DB_DUPSORT)!=0; // dupsorts don't need the data part
        if (need_data) {
            BYTESTRING databs = {val->size, toku_memdup_in_rollback(txn, val->data, val->size)};
            r = toku_logger_save_rollback_cmdinsertboth(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs, databs);
        } else {
            r = toku_logger_save_rollback_cmdinsert    (txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs);
        }
        if (r!=0) return r;
        r = toku_txn_note_brt(txn, brt);
        if (r!=0) return r;
    }
    BRT_CMD_S brtcmd = { BRT_INSERT, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    if (r!=0) return r;
    return r;
}

int toku_brt_delete(BRT brt, DBT *key, TOKUTXN txn) {
    int r;
    if (txn && (brt->txnid_that_created_or_locked_when_empty != toku_txn_get_txnid(txn))) {
        BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
        toku_cachefile_refup(brt->cf);
        r = toku_logger_save_rollback_cmddelete(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs);
        if (r!=0) return r;
        r = toku_txn_note_brt(txn, brt);
        if (r!=0) return r;
    }
    DBT val;
    BRT_CMD_S brtcmd = { BRT_DELETE_ANY, toku_txn_get_txnid(txn), .u.id={key, toku_init_dbt(&val)}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

struct omt_compressor_state {
    struct mempool *new_kvspace;
    OMT omt;
};

static int move_it (OMTVALUE lev, u_int32_t idx, void *v) {
    LEAFENTRY le=lev;
    struct omt_compressor_state *oc = v;
    u_int32_t size = leafentry_memsize(le);
    LEAFENTRY newdata = toku_mempool_malloc(oc->new_kvspace, size, 1);
    assert(newdata); // we do this on a fresh mempool, so nothing bad shouldhapepn
    memcpy(newdata, le, size);
    toku_omt_set_at(oc->omt, newdata, idx);
    return 0;
}

// Compress things, and grow the mempool if needed.
static int omt_compress_kvspace (OMT omt, struct mempool *memp, size_t added_size, void **maybe_free) {
    u_int32_t total_size_needed = memp->free_offset-memp->frag_size + added_size;
    if (total_size_needed+total_size_needed/4 >= memp->size) {
        memp->size = total_size_needed+total_size_needed/4;
    }
    void *newmem = toku_malloc(memp->size);
    if (newmem == 0)
        return ENOMEM;
    struct mempool new_kvspace;
    toku_mempool_init(&new_kvspace, newmem, memp->size);
    struct omt_compressor_state oc = { &new_kvspace, omt };
    toku_omt_iterate(omt, move_it, &oc);

    if (maybe_free) {
        *maybe_free = memp->base;
    } else {
        toku_free(memp->base);
    }
    *memp = new_kvspace;
    return 0;
}

void *
mempool_malloc_from_omt(OMT omt, struct mempool *mp, size_t size, void **maybe_free) {
    void *v = toku_mempool_malloc(mp, size, 1);
    if (v==0) {
        if (0 == omt_compress_kvspace(omt, mp, size, maybe_free)) {
            v = toku_mempool_malloc(mp, size, 1);
            assert(v);
        }
    }
    return v;
}

/* ******************** open,close and create  ********************** */

// This one has no env
int toku_open_brt (const char *fname, int is_create, BRT *newbrt, int nodesize, CACHETABLE cachetable, TOKUTXN txn,
                   int (*compare_fun)(DB*,const DBT*,const DBT*), DB *db) {
    BRT brt;
    int r;
    const int only_create = 0;

    r = toku_brt_create(&brt);
    if (r != 0)
        return r;
    toku_brt_set_nodesize(brt, nodesize);
    toku_brt_set_bt_compare(brt, compare_fun);

    r = toku_brt_open(brt, fname, fname, is_create, only_create, cachetable, txn, db);
    if (r != 0) {
        return r;
    }

    *newbrt = brt;
    return r;
}

static int setup_initial_brt_root_node (BRT t, BLOCKNUM blocknum) {
    int r;
    TAGMALLOC(BRTNODE, node);
    assert(node);
    node->ever_been_written = 0;
    //printf("%s:%d\n", __FILE__, __LINE__);
    initialize_empty_brtnode(t, node, blocknum, 0);
    //    node->brt = t;
    if (0) {
        printf("%s:%d for tree %p node %p mdict_create--> %p\n", __FILE__, __LINE__, t, node, node->u.l.buffer);
        printf("%s:%d put root at %" PRId64 "\n", __FILE__, __LINE__, blocknum.b);
    }
    //printf("%s:%d putting %p (%lld)\n", __FILE__, __LINE__, node, node->thisnodename);
    u_int32_t fullhash = toku_cachetable_hash(t->cf, blocknum);
    node->fullhash = fullhash;
    r=toku_cachetable_put(t->cf, blocknum, fullhash,
                          node, brtnode_memory_size(node),
                          toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    if (r!=0) {
        toku_free(node);
        return r;
    }
//    verify_local_fingerprint_nonleaf(node);
    r = toku_unpin_brtnode(t, node);
    if (r!=0) {
        toku_free(node);
        return r;
    }
    return 0;
}

// open a file for use by the brt.  if the file does not exist, create it.
static int brt_open_file(BRT brt, const char *fname, int is_create, int *fdp, BOOL *did_create) {
    brt = brt;
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r;
    int fd;
    *did_create = FALSE;
    fd = open(fname, O_RDWR | O_BINARY, mode);
    if (fd==-1) {
        r = errno;
        if (errno == ENOENT) {
            if (!is_create) {
                return r;
            }
            fd = open(fname, O_RDWR | O_CREAT | O_BINARY, mode);
            if (fd == -1) {
                r = errno; return r;
            }
            *did_create = TRUE;
        } else
            return r;
    }
    *fdp = fd;
    return 0;
}

static int 
brt_init_header_partial (BRT t) {
    int r;
    t->h->flags = t->flags;
    t->h->nodesize=t->nodesize;

    compute_and_fill_remembered_hash(t);

    toku_fifo_create(&t->h->fifo);
    t->h->root_put_counter = global_root_put_counter++; 
            
#if 0 //TODO: logged header logic //TODO: #1605
    {
	u_int64_t n_blocks_translated = toku_block_get_translated_blocknum_limit(t->h->blocktable);
	assert(n_blocks_translated==0);
        LOGGEDBRTHEADER lh = {.size= toku_serialize_brt_header_size(t->h),
                              .flags = t->flags,
                              .nodesize = t->h->nodesize,
                              .n_named_roots = t->h->n_named_roots,
			      .btt_size = make_blocknum(n_blocks_translated),
			      .btt_diskoff = 0, // No diskoffset yet allocated, since it's a new blocktable.
			      .btt_pairs = 0};
        if (t->h->n_named_roots>=0) {
            lh.u.many.names = t->h->names;
            lh.u.many.roots = t->h->roots;
        } else {
            lh.u.one.root = t->h->roots[0];
        }
#ifdef __ICL
        lh = lh; //Satisfy icl (variable set but unused)
#endif
        //if ((r=toku_log_fheader(toku_txn_logger(txn), (LSN*)0, 0, toku_txn_get_txnid(txn), toku_cachefile_filenum(t->cf), lh))) { return r; }
    }
#endif
    BLOCKNUM root = t->h->root;
    if ((r=setup_initial_brt_root_node(t, root))!=0) { return r; }
    //printf("%s:%d putting %p (%d)\n", __FILE__, __LINE__, t->h, 0);
    toku_block_verify_no_free_blocknums(t->h->blocktable);
    toku_cachefile_set_userdata(t->cf, t->h, toku_brtheader_close, toku_brtheader_checkpoint, toku_brtheader_begin_checkpoint, toku_brtheader_end_checkpoint);

    return r;
}

static int
brt_init_header (BRT t) {
    t->h->type = BRTHEADER_CURRENT;
    t->h->checkpoint_header = NULL;
    toku_blocktable_create_new(&t->h->blocktable);
    BLOCKNUM root;
    //Assign blocknum for root block, also dirty the header
    toku_allocate_blocknum(t->h->blocktable, &root, t->h);
    t->h->root = root;

    int r = brt_init_header_partial(t);
    return r;
}


// allocate and initialize a brt header.
// t->cf is not set to anything.
int toku_brt_alloc_init_header(BRT t) {
    int r;

    r = brtheader_alloc(&t->h);
    if (r != 0) {
        if (0) { died2: toku_free(t->h); }
        t->h=0;
        return r;
    }

    t->h->layout_version = BRT_LAYOUT_VERSION;

    memset(&t->h->descriptor, 0, sizeof(t->h->descriptor));

    r = brt_init_header(t);
    if (r != 0) goto died2;
    return r;
}

int toku_read_brt_header_and_store_in_cachefile (CACHEFILE cf, struct brt_header **header)
// If the cachefile already has the header, then just get it.
// If the cachefile has not been initialized, then don't modify anything.
{
    {
        struct brt_header *h;
        if ((h=toku_cachefile_get_userdata(cf))!=0) {
            *header = h;
            return 0;
        }
    }
    struct brt_header *h;
    int r = toku_deserialize_brtheader_from(toku_cachefile_fd(cf), &h);
    if (r!=0) return r;
    h->root_put_counter = global_root_put_counter++;
    toku_cachefile_set_userdata(cf, (void*)h, toku_brtheader_close, toku_brtheader_checkpoint, toku_brtheader_begin_checkpoint, toku_brtheader_end_checkpoint);
    *header = h;
    return 0;
}

int toku_brt_open(BRT t, const char *fname, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, DB *db) {

    int r;

    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE: %s:%d toku_brt_open(%s, \"%s\", %d, %p, %d, %p)\n",
                          __FILE__, __LINE__, fname, dbname, is_create, newbrt, nodesize, cachetable));
    if (0) { died0:  assert(r); return r; }

    assert(is_create || !only_create);
    t->fname = toku_strdup(fname_in_env);
    if (t->fname==0) {
        r = errno;
        if (0) { died00: if (t->fname) toku_free(t->fname); t->fname=0; }
        goto died0;
    }
    t->db = db;
    t->txnid_that_created_or_locked_when_empty = 0; // Uses 0 for no transaction.
    {
        int fd = -1;
        BOOL did_create = FALSE;
        r = brt_open_file(t, fname, is_create, &fd, &did_create);
        if (r != 0) goto died00;
        r=toku_cachetable_openfd(&t->cf, cachetable, fd, fname);
        if (r != 0) goto died00;
        if (did_create) {
            mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
            r = toku_logger_log_fcreate(txn, fname_in_env, toku_cachefile_filenum(t->cf), mode);
            if (r != 0) goto died_after_open;
            t->txnid_that_created_or_locked_when_empty = toku_txn_get_txnid(txn);
        } else {
	    r = toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(t->cf));
	}
    }
    if (r!=0) {
        died_after_open: 
        toku_cachefile_close(&t->cf, toku_txn_logger(txn), 0);
        goto died00;
    }
    assert(t->nodesize>0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    if (0) {
    died_after_read_and_pin:
        goto died_after_open;
    }
    if (is_create) {
        r = toku_read_brt_header_and_store_in_cachefile(t->cf, &t->h);
        if (r==-1) {
            r = toku_brt_alloc_init_header(t);
            if (r != 0) goto died_after_read_and_pin;
        }
        else if (r!=0) {
            goto died_after_read_and_pin;
        }
        else if (only_create) {
            assert(r==0);
            r = EEXIST;
            goto died_after_read_and_pin;
        }
        else goto found_it;
    } else {
        if ((r = toku_read_brt_header_and_store_in_cachefile(t->cf, &t->h))!=0) goto died_after_open;
        found_it:
        t->nodesize = t->h->nodesize;                 /* inherit the pagesize from the file */
        if (!t->did_set_flags) {
            t->flags = t->h->flags;
        } else {
            if (t->flags != t->h->flags) {                /* if flags have been set then flags must match */
                r = EINVAL; goto died_after_read_and_pin;
            }
        }
    }
    if (t->did_set_descriptor) {
        if (t->h->descriptor.sdbt.len!=t->temp_descriptor.size ||
            memcmp(t->h->descriptor.sdbt.data, t->temp_descriptor.data, t->temp_descriptor.size)) {
            if (t->h->descriptor.b.b <= 0) toku_allocate_blocknum(t->h->blocktable, &t->h->descriptor.b, t->h);
            DISKOFF offset;
            //4 for checksum
            toku_blocknum_realloc_on_disk(t->h->blocktable, t->h->descriptor.b, t->temp_descriptor.size+4, &offset, t->h, FALSE);
            r = toku_serialize_descriptor_contents_to_fd(toku_cachefile_fd(t->cf), &t->temp_descriptor, offset);
            if (r!=0) goto died_after_read_and_pin;
            if (t->h->descriptor.sdbt.data) toku_free(t->h->descriptor.sdbt.data);
            t->h->descriptor.sdbt.data = t->temp_descriptor.data;
            t->h->descriptor.sdbt.len = t->temp_descriptor.size;
            t->temp_descriptor.data = NULL;
        }
        t->did_set_descriptor = 0;
    }
    if (t->db) {
        toku_fill_dbt(&t->db->descriptor, t->h->descriptor.sdbt.data, t->h->descriptor.sdbt.len);
    }
    assert(t->h);
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE -> %p\n", t));
    return 0;
}

int toku_brt_get_fd(BRT brt, int *fdp) {
    *fdp = toku_cachefile_fd(brt->cf);
    return 0;
}

int toku_brt_set_flags(BRT brt, unsigned int flags) {
    brt->did_set_flags = 1;
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

int toku_brt_create_cachetable(CACHETABLE *ct, long cachesize, LSN initial_lsn, TOKULOGGER logger) {
    if (cachesize == 0)
        cachesize = 128*1024*1024;
    return toku_create_cachetable(ct, cachesize, initial_lsn, logger);
}

// Create checkpoint-in-progress versions of header and translation (btt) (and fifo for now...).
int
toku_brtheader_begin_checkpoint (CACHEFILE UU(cachefile), LSN checkpoint_lsn, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
        // hold lock around copying and clearing of dirty bit
        toku_block_lock_for_multiple_operations (h->blocktable);
        assert(h->type == BRTHEADER_CURRENT);
        assert(h->checkpoint_header == NULL);
        brtheader_copy_for_checkpoint(h, checkpoint_lsn);
        h->dirty = 0;        // this is only place this bit is cleared  (in currentheader)
        toku_block_translation_note_start_checkpoint_unlocked(h->blocktable);

        //FIFO handling
        toku_off_t write_to;
        struct brt_header *ch = h->checkpoint_header;
        //TODO: #1616 Delete code handling root fifo.
        //We would want retrieving 'write_to' and writing to that point to be
        //atomic.  This is only done during checkpoint of a BRT, so we allow it (not good?).
         //fifo
        u_int64_t fifo_size = toku_fifo_get_serialized_size (ch->fifo);
        toku_realloc_fifo_on_disk_unlocked (ch->blocktable, fifo_size, &write_to);
        //printf("%s:%d fifo written to %lu\n", __FILE__, __LINE__, write_to);
        toku_block_unlock_for_multiple_operations (h->blocktable);
        r = toku_serialize_fifo_at(toku_cachefile_fd(cachefile), write_to, ch->fifo, fifo_size);
    }
    return r;
}

// Write checkpoint-in-progress versions of header and translation to disk (really to OS internal buffer).
int
toku_brtheader_checkpoint (CACHEFILE cachefile, void *header_v)
{
    struct brt_header *h = header_v;
    struct brt_header *ch = h->checkpoint_header;
    int r = 0;
    if (h->panic!=0) goto handle_error;
    //printf("%s:%d allocated_limit=%lu writing queue to %lu\n", __FILE__, __LINE__,
    //       block_allocator_allocated_limit(h->block_allocator), h->unused_blocks.b*h->nodesize);
    assert(ch);
    if (ch->panic!=0) goto handle_error;
    assert(ch->type == BRTHEADER_CHECKPOINT_INPROGRESS);
    if (ch->dirty) {	// this is only place this bit is tested (in checkpoint_header)
	{
	    ch->checkpoint_count++;
	    // write translation and header to disk (or at least to OS internal buffer)
	    r = toku_serialize_brt_header_to(toku_cachefile_fd(cachefile), ch);
	    if (r!=0) goto handle_error;
	}
	ch->dirty = 0;		// this is only place this bit is cleared (in checkpoint_header)
    }
    else toku_block_translation_note_skipped_checkpoint(ch->blocktable);
    if (0) {
handle_error:
        if (h->panic) r = h->panic;
        else if (ch->panic) {
            r = ch->panic;
            //Steal panic string.  Cannot afford to malloc.
            h->panic         = ch->panic;
            h->panic_string  = ch->panic_string;
        }
        else toku_block_translation_note_failed_checkpoint(ch->blocktable);
    }
    return r;

}

// Really write everything to disk (fsync dictionary), then free unused disk space 
// (i.e. tell BlockAllocator to liberate blocks used by previous checkpoint).
int
toku_brtheader_end_checkpoint (CACHEFILE cachefile, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
        assert(h->type == BRTHEADER_CURRENT);
	struct brt_header *ch = h->checkpoint_header;
	BOOL checkpoint_success_so_far = (ch->checkpoint_count==h->checkpoint_count+1 && ch->dirty==0);
	if (checkpoint_success_so_far) {
            r = toku_cachefile_fsync(cachefile);
	    if (r!=0) 
		toku_block_translation_note_failed_checkpoint(h->blocktable);
	    else
		h->checkpoint_count++;	// checkpoint succeeded, next checkpoint will save to alternate header location
	}
        toku_block_translation_note_end_checkpoint(h->blocktable);
        brtheader_free(h->checkpoint_header);
        h->checkpoint_header = NULL;
    }
    return r;
}

int
toku_brtheader_close (CACHEFILE cachefile, void *header_v, char **malloced_error_string)
{
    struct brt_header *h = header_v;
    assert(h->type == BRTHEADER_CURRENT);
    int r = 0;
    if (h->dirty) {	// this is the only place this bit is tested (in currentheader)
        //TODO: #1627 put meaningful LSN in for begin_checkpoint
        int r2;
        r2 = toku_brtheader_begin_checkpoint(cachefile, ZERO_LSN, header_v);
        if (r==0) r = r2;
        r2 = toku_brtheader_checkpoint(cachefile, h);
        if (r==0) r = r2;
        r2 = toku_brtheader_end_checkpoint(cachefile, header_v);
        if (r==0) r = r2;
	if (!h->panic) assert(!h->dirty);	// dirty bit should be cleared by begin_checkpoint and never set again (because we're closing the dictionary)
    }
    if (malloced_error_string) *malloced_error_string = h->panic_string;
    if (r==0) {
	r=h->panic;
    }
    toku_brtheader_free(h);
    return r;
}

int toku_close_brt (BRT brt, TOKULOGGER logger, char **error_string) {
    int r;
    while (!list_empty(&brt->cursors)) {
        BRT_CURSOR c = list_struct(list_pop(&brt->cursors), struct brt_cursor, cursors_link);
        r=toku_brt_cursor_close(c);
        if (r!=0) return r;
    }

    // Must do this work before closing the cf
    r=toku_txn_note_close_brt(brt);
    assert(r==0);
    toku_omt_destroy(&brt->txns);

    if (brt->cf) {
        if (logger) {
            assert(brt->fname);
            BYTESTRING bs = {.len=strlen(brt->fname), .data=brt->fname};
            LSN lsn;
            r = toku_log_brtclose(logger, &lsn, 1, bs, toku_cachefile_filenum(brt->cf)); // flush the log on close, otherwise it might not make it out.
            if (r!=0) return r;
        }
	if (!brt->h->panic)
	    assert(0==toku_cachefile_count_pinned(brt->cf, 1)); // For the brt, the pinned count should be zero (but if panic, don't worry)
        //printf("%s:%d closing cachetable\n", __FILE__, __LINE__);
        // printf("%s:%d brt=%p ,brt->h=%p\n", __FILE__, __LINE__, brt, brt->h);
	if (error_string) assert(*error_string == 0);
        r = toku_cachefile_close(&brt->cf, logger, error_string);
	if (r==0 && error_string) assert(*error_string == 0);
    }
    if (brt->fname) toku_free(brt->fname);
    if (brt->temp_descriptor.data) toku_free(brt->temp_descriptor.data);
    toku_free(brt);
    return r;
}

int toku_brt_create(BRT *brt_ptr) {
    BRT brt = toku_malloc(sizeof *brt);
    if (brt == 0)
        return ENOMEM;
    memset(brt, 0, sizeof *brt);
    list_init(&brt->cursors);
    brt->flags = 0;
    brt->did_set_flags = 0;
    brt->did_set_descriptor = 0;
    brt->nodesize = BRT_DEFAULT_NODE_SIZE;
    brt->compare_fun = toku_default_compare_fun;
    brt->dup_compare = toku_default_compare_fun;
    int r = toku_omt_create(&brt->txns);
    if (r!=0) { toku_free(brt); return r; }
    *brt_ptr = brt;
    return 0;
}

int
toku_brt_set_descriptor (BRT t, const DBT *descriptor) {
    int r;
    if (t->did_set_descriptor) r = EINVAL;
    else {
        void *copy = toku_memdup(descriptor->data, descriptor->size);
        if (!copy) r = ENOMEM;
        else {
            if (t->temp_descriptor.data) toku_free(t->temp_descriptor.data);
            toku_fill_dbt(&t->temp_descriptor, copy, descriptor->size);
            t->did_set_descriptor = 1;
            r = 0;
        }
    }
    return r;
}

int toku_brt_flush (BRT brt) {
    return toku_cachefile_flush(brt->cf);
}

/* ************* CURSORS ********************* */

static inline void
brt_cursor_cleanup_dbts(BRT_CURSOR c) {
    if (!c->current_in_omt) {
        if (c->key.data) toku_free(c->key.data);
        if (c->val.data) toku_free(c->val.data);
        memset(&c->key, 0, sizeof(c->key));
        memset(&c->val, 0, sizeof(c->val));
    }
}

static inline void load_dbts_from_omt(BRT_CURSOR c, DBT *key, DBT *val) {
    OMTVALUE le = 0;
    int r = toku_omt_cursor_current(c->omtcursor, &le);
    assert(r==0);
    if (key) {
        key->data = le_latest_key(le);
        key->size = le_latest_keylen(le);
    }
    if (val) {
        val->data = le_latest_val(le);
        val->size = le_latest_vallen(le);
    }
}

// When an omt cursor is invalidated, this is the brt-level function
// that is called.  This function is only called by the omt logic.
// This callback is called when either (a) the brt logic invalidates one
// cursor (see brt_cursor_invalidate()) or (b) when the omt logic invalidates
// all the cursors for an omt.
static void
brt_cursor_invalidate_callback(OMTCURSOR UU(omt_c), void *extra) {
    BRT_CURSOR cursor = extra;

    //TODO: #1378 assert that this thread owns omt lock in brtcursor

    if (cursor->current_in_omt) {
        DBT key,val;
        load_dbts_from_omt(cursor, toku_init_dbt(&key), toku_init_dbt(&val));
	cursor->key.data = toku_memdup(key.data, key.size);
	cursor->val.data = toku_memdup(val.data, val.size);
	cursor->key.size = key.size;
	cursor->val.size = val.size;
        //TODO: Find some way to deal with ENOMEM here.
        //Until then, just assert that the memdups worked.
	assert(cursor->key.data && cursor->val.data);
        cursor->current_in_omt = FALSE;
    }
}

// Called at start of every slow query, and only from slow queries.
// When all cursors are invalidated (from writer thread, or insert/delete),
// this function is not used.
static void
brt_cursor_invalidate(BRT_CURSOR brtcursor) {
    if (brtcursor->leaf_info.leaflock) {
        toku_leaflock_lock_by_cursor(brtcursor->leaf_info.leaflock);
        toku_omt_cursor_invalidate(brtcursor->omtcursor); // will call brt_cursor_invalidate_callback()
        toku_leaflock_unlock_by_cursor(brtcursor->leaf_info.leaflock);
    }
}

int toku_brt_cursor (BRT brt, BRT_CURSOR *cursorptr) {
    BRT_CURSOR cursor = toku_malloc(sizeof *cursor);
    if (cursor == 0)
        return ENOMEM;
    memset(cursor, 0, sizeof(*cursor));
    cursor->brt = brt;
    cursor->current_in_omt = FALSE;
    cursor->prefetching = FALSE;
    list_push(&brt->cursors, &cursor->cursors_link);
    int r = toku_omt_cursor_create(&cursor->omtcursor);
    assert(r==0);
    toku_omt_cursor_set_invalidate_callback(cursor->omtcursor,
                                            brt_cursor_invalidate_callback, cursor);
    cursor->root_put_counter=0;
    *cursorptr = cursor;
    return 0;
}

// Called during cursor destruction
// It is the same as brt_cursor_invalidate, except that
// we make sure the callback function is never called.
static void
brt_cursor_invalidate_no_callback(BRT_CURSOR brtcursor) {
    if (brtcursor->leaf_info.leaflock) {
        toku_leaflock_lock_by_cursor(brtcursor->leaf_info.leaflock);
        toku_omt_cursor_set_invalidate_callback(brtcursor->omtcursor, NULL, NULL);
        toku_omt_cursor_invalidate(brtcursor->omtcursor); // will NOT call brt_cursor_invalidate_callback()
        toku_leaflock_unlock_by_cursor(brtcursor->leaf_info.leaflock);
    }
}

//TODO: #1378 When we split the ydb lock, touching cursor->cursors_link
//is not thread safe.
int toku_brt_cursor_close(BRT_CURSOR cursor) {
    brt_cursor_invalidate_no_callback(cursor);
    brt_cursor_cleanup_dbts(cursor);
    list_remove(&cursor->cursors_link);
    toku_omt_cursor_destroy(&cursor->omtcursor);
    toku_free_n(cursor, sizeof *cursor);
    return 0;
}

static inline void brt_cursor_set_prefetching(BRT_CURSOR cursor) {
    cursor->prefetching = TRUE;
}

static inline BOOL brt_cursor_prefetching(BRT_CURSOR cursor) {
    return cursor->prefetching;
}

//Return TRUE if cursor is uninitialized.  FALSE otherwise.
static BOOL
brt_cursor_not_set(BRT_CURSOR cursor) {
    assert((cursor->key.data==NULL) == (cursor->val.data==NULL));
    return (BOOL)(!cursor->current_in_omt && cursor->key.data == NULL);
}

static int
pair_leafval_heaviside_le_committed (u_int32_t klen, void *kval,
                                  u_int32_t dlen, void *dval,
                                  brt_search_t *search) {
    DBT x,y;
    int cmp = search->compare(search,
                              search->k ? toku_fill_dbt(&x, kval, klen) : 0, 
                              search->v ? toku_fill_dbt(&y, dval, dlen) : 0);
    // The search->compare function returns only 0 or 1
    switch (search->direction) {
    case BRT_SEARCH_LEFT:   return cmp==0 ? -1 : +1;
    case BRT_SEARCH_RIGHT:  return cmp==0 ? +1 : -1; // Because the comparison runs backwards for right searches.
    }
    abort(); return 0;
}


static int
pair_leafval_heaviside_le_both (TXNID xid __attribute__((__unused__)),
                             u_int32_t klen, void *kval,
                             u_int32_t clen __attribute__((__unused__)), void *cval __attribute__((__unused__)),
                             u_int32_t plen, void *pval,
                             brt_search_t *search) {
    return pair_leafval_heaviside_le_committed(klen, kval, plen, pval, search);
}

static int
pair_leafval_heaviside_le_provdel (TXNID xid __attribute__((__unused__)),
                                u_int32_t klen, void *kval,
                                u_int32_t clen, void *cval,
                                brt_search_t *be) {
    return pair_leafval_heaviside_le_committed(klen, kval, clen, cval, be);
}

static int
pair_leafval_heaviside_le_provpair (TXNID xid __attribute__((__unused__)),
                                 u_int32_t klen, void *kval,
                                 u_int32_t plen, void *pval,
                                 brt_search_t *be) {
    return pair_leafval_heaviside_le_committed(klen, kval, plen, pval, be);
}

static int heaviside_from_search_t (OMTVALUE lev, void *extra) {
    LEAFENTRY leafval=lev;
    brt_search_t *search = extra;
    LESWITCHCALL(leafval, pair_leafval_heaviside, search);
    abort(); return 0;
}

// This is the only function that associates a brt cursor (and its contained
// omt cursor) with a brt node (and its associated omt).  This is different
// from older code because the old code associated the omt cursor with the
// omt when the search found a match.  In this new design, the omt cursor
// will not be associated with the omt until after the application-level
// callback accepts the search result.
// The lock is necessary because we don't want two threads modifying
// the omt's list of cursors simultaneously.
// Note, this is only place in brt code that calls toku_omt_cursor_set_index().
// Requires: cursor->omtcursor is valid
static inline void
brt_cursor_update(BRT_CURSOR brtcursor) {
    //Free old version if it is using local memory.
    OMTCURSOR omtcursor = brtcursor->omtcursor;
    if (!brtcursor->current_in_omt) {
        brt_cursor_cleanup_dbts(brtcursor);
        brtcursor->current_in_omt = TRUE;
        toku_leaflock_lock_by_cursor(brtcursor->leaf_info.leaflock);
        toku_omt_cursor_associate(brtcursor->leaf_info.to_be.omt, omtcursor);
        toku_leaflock_unlock_by_cursor(brtcursor->leaf_info.leaflock);
        //no longer touching linked list, and
        //only one thread can touch cursor at a time
    }
    toku_omt_cursor_set_index(omtcursor, brtcursor->leaf_info.to_be.index);
}

// This is a bottom layer of the search functions.
static int
brt_search_leaf_node(BRT brt, BRTNODE node, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, enum reactivity *re, BOOL *doprefetch, BRT_CURSOR brtcursor)
{
    // Now we have to convert from brt_search_t to the heaviside function with a direction.  What a pain...

    *re = get_leaf_reactivity(node); // searching doesn't change the reactivity, so we can calculate it here.

    int direction;
    switch (search->direction) {
    case BRT_SEARCH_LEFT:   direction = +1; goto ok;
    case BRT_SEARCH_RIGHT:  direction = -1; goto ok;
    }
    return EINVAL;  // This return and the goto are a hack to get both compile-time and run-time checking on enum
 ok: ;
    OMTVALUE datav;
    u_int32_t idx = 0;
    int r = toku_omt_find(node->u.l.buffer,
                          heaviside_from_search_t,
                          search,
                          direction,
                          &datav, &idx, NULL);
    if (r!=0) return r;

    LEAFENTRY le = datav;
    if (le_is_provdel(le)) {
        TXNID xid = le_any_xid(le);
        TOKUTXN txn = 0;
        toku_txn_find_by_xid(brt, xid, &txn);

        // Provisionally deleted stuff is gone.
        // So we need to scan in the direction to see if we can find something
        while (1) {
            // see if the transaction is alive
            TXNID newxid = le_any_xid(le);
            if (newxid != xid) {
                xid = newxid;
                txn = 0;
                toku_txn_find_by_xid(brt, xid, &txn);
            }

            switch (search->direction) {
            case BRT_SEARCH_LEFT:
                if (txn) {
                    // printf("xid %llu -> %p\n", (unsigned long long) xid, txn);
                    idx++;
                } else {
                    // apply a commit message for this leafentry to the node
                    // printf("apply commit_both %llu\n", (unsigned long long) xid);
                    DBT key, val;
                    BRT_CMD_S brtcmd = { BRT_COMMIT_BOTH, xid, .u.id= {toku_fill_dbt(&key, le_latest_key(le), le_latest_keylen(le)),
                                                                       toku_fill_dbt(&val, le_latest_val(le), le_latest_vallen(le))} };
                    r = brt_leaf_apply_cmd_once(node, &brtcmd, idx, le);
                    assert(r == 0);
                }
                if (idx>=toku_omt_size(node->u.l.buffer)) return DB_NOTFOUND;
                break;
            case BRT_SEARCH_RIGHT:
                if (idx==0) return DB_NOTFOUND;
                idx--;
                break;
            }
            if (idx>=toku_omt_size(node->u.l.buffer)) continue;
            r = toku_omt_fetch(node->u.l.buffer, idx, &datav, NULL);
            assert(r==0); // we just validated the index
            le = datav;
            if (!le_is_provdel(le)) goto got_a_good_value;
        }
    }
got_a_good_value:
    {
        u_int32_t keylen = le_latest_keylen(le);
        bytevec   key    = le_latest_key(le);
        u_int32_t vallen = le_latest_vallen(le);
        bytevec   val    = le_latest_val(le);

        assert(brtcursor->current_in_omt == FALSE);
        r = getf(keylen, key,
                 vallen, val,
                 0, NULL, //TODO: Put actual values here.
                 0, NULL, //TODO: Put actual values here.
                 getf_v);
        if (r==0) {
            // Leave the omtcursor alone above (pass NULL to omt_find/fetch)
            // This prevents the omt from calling associate(), which would
            // require a lock to keep the list of cursors safe when the omt
	    // is used by the brt.  (We don't want to impose the locking requirement
	    // on the omt for non-brt uses.)
            //
            // Instead, all associating of omtcursors with omts (for leaf nodes)
            // is done in brt_cursor_update.
            brtcursor->leaf_info.fullhash    = node->fullhash;
            brtcursor->leaf_info.blocknumber = node->thisnodename;
            brtcursor->leaf_info.leaflock    = node->u.l.leaflock;
            brtcursor->leaf_info.to_be.omt   = node->u.l.buffer;
            brtcursor->leaf_info.to_be.index = idx;
            brt_cursor_update(brtcursor);
            //The search was successful.  Prefetching can continue.
            *doprefetch = TRUE;
        }
    }
    return r;
}

static int
brt_search_node (BRT brt, BRTNODE node, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, enum reactivity *re, BOOL *doprefetch, BRT_CURSOR brtcursor);

// the number of nodes to prefetch
#define TOKU_DO_PREFETCH 2
#if TOKU_DO_PREFETCH

static void
brt_node_maybe_prefetch(BRT brt, BRTNODE node, int childnum, BRT_CURSOR brtcursor, BOOL *doprefetch) {

    // if we want to prefetch in the tree 
    // then prefetch the next children if there are any
    if (*doprefetch && brt_cursor_prefetching(brtcursor)) {
        int i;
        for (i=0; i<TOKU_DO_PREFETCH; i++) {
            int nextchildnum = childnum+i+1;
            if (nextchildnum >= node->u.n.n_children) 
                break;
            BLOCKNUM nextchildblocknum = BNC_BLOCKNUM(node, nextchildnum);
            u_int32_t nextfullhash =  compute_child_fullhash(brt->cf, node, nextchildnum);
            toku_cachefile_prefetch(brt->cf, nextchildblocknum, nextfullhash, 
                                    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
            *doprefetch = FALSE;
        }
    }
}

#endif

/* search in a node's child */
static int
brt_search_child(BRT brt, BRTNODE node, int childnum, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, enum reactivity *parent_re, BOOL *doprefetch, BRT_CURSOR brtcursor, BOOL *did_react)
// Effect: Search in a node's child.
//  If we change the shape, set *did_react = TRUE.  Else set *did_react = FALSE.
{

    verify_local_fingerprint_nonleaf(node);

    /* if the child's buffer is not empty then empty it */
    if (BNC_NBYTESINBUF(node, childnum) > 0) {
        BOOL did_io = FALSE;
        enum reactivity child_re = RE_STABLE;
        int rr = flush_this_child(brt, node, childnum, &child_re, &did_io);
        assert(rr == 0);
        /* push down may cause the child to be overfull, but that's OK.  We'll search the child anyway, and recompute the ractivity. */
    }

    void *node_v;
    BLOCKNUM childblocknum = BNC_BLOCKNUM(node,childnum);
    u_int32_t fullhash =  compute_child_fullhash(brt->cf, node, childnum);
    {
        int rr = toku_cachetable_get_and_pin(brt->cf, childblocknum, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
        assert(rr == 0);
    }

    BRTNODE childnode = node_v;
    verify_local_fingerprint_nonleaf(node);
    verify_local_fingerprint_nonleaf(childnode);
    enum reactivity child_re = RE_STABLE;
    int r = brt_search_node(brt, childnode, search, getf, getf_v, &child_re, doprefetch, brtcursor);
    // Even if r is reactive, we want to handle the maybe reactive child.
    verify_local_fingerprint_nonleaf(node);
    verify_local_fingerprint_nonleaf(childnode);

#if TOKU_DO_PREFETCH
    // maybe prefetch the next child
    if (r == 0)
        brt_node_maybe_prefetch(brt, node, childnum, brtcursor, doprefetch);
#endif

    {
        int rr = toku_unpin_brtnode(brt, childnode); // unpin the childnode before handling the reactive child (because that may make the childnode disappear.)
        if (rr!=0) r = rr;
    }

    {
        BOOL did_io = FALSE;
        int rr = brt_handle_maybe_reactive_child(brt, node, childnum, child_re, &did_io, did_react);
        if (rr!=0) r = rr; // if we got an error, then return rr.  Else we will return the r from brt_search_node().
    }

    *parent_re = get_nonleaf_reactivity(node);

    verify_local_fingerprint_nonleaf(node);

    return r;
}

static int
brt_search_nonleaf_node(BRT brt, BRTNODE node, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, enum reactivity *re, BOOL *doprefetch, BRT_CURSOR brtcursor)
{
    int count=0;
 again:
    count++;
    verify_local_fingerprint_nonleaf(node);

    {
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
                BOOL did_change_shape = FALSE;
                verify_local_fingerprint_nonleaf(node);
                int r = brt_search_child(brt, node, child[c], search, getf, getf_v, re, doprefetch, brtcursor, &did_change_shape);
                assert(r != EAGAIN);
                if (r == 0) return r;           //Success
                if (r != DB_NOTFOUND) return r; //Error (or message to quit early, such as TOKUDB_FOUND_BUT_REJECTED)
                if (did_change_shape) goto again;
            }
        }

        /* check the first (left) or last (right) node if nothing has been found */
        BOOL ignore_did_change_shape; // ignore this
        verify_local_fingerprint_nonleaf(node);
        return brt_search_child(brt, node, child[c], search, getf, getf_v, re, doprefetch, brtcursor, &ignore_did_change_shape);
    }
}

static int
brt_search_node (BRT brt, BRTNODE node, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, enum reactivity *re, BOOL *doprefetch, BRT_CURSOR brtcursor)
{
    verify_local_fingerprint_nonleaf(node);
    if (node->height > 0)
        return brt_search_nonleaf_node(brt, node, search, getf, getf_v, re, doprefetch, brtcursor);
    else {
        return brt_search_leaf_node(brt, node, search, getf, getf_v, re, doprefetch, brtcursor);
    }
}

static int
toku_brt_search (BRT brt, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger, BRT_CURSOR brtcursor, u_int64_t *root_put_counter)
// Effect: Perform a search.  Associate cursor with a leaf if possible.
// All searches are performed through this function.
{
    int r, rr;

    assert(brt->h);

    *root_put_counter = brt->h->root_put_counter;

    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    void *node_v;

    //assert(fullhash == toku_cachetable_hash(brt->cf, *rootp));
    rr = toku_cachetable_get_and_pin(brt->cf, *rootp, fullhash,
                                     &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
    assert(rr == 0);

    BRTNODE node = node_v;

    // push the fifo stuff
    {
        DBT okey,odata;
        BRT_CMD_S ocmd;
        while (0==toku_fifo_peek_cmdstruct(brt->h->fifo, &ocmd,  &okey, &odata)) {
            if ((r = push_something_at_root(brt, &node, rootp, &ocmd, logger))) return r;
            r = toku_fifo_deq(brt->h->fifo);
            assert(r==0);
        }
    }

    {
        enum reactivity re = RE_STABLE;
        BOOL doprefetch = FALSE;
        //static int counter = 0;        counter++;
        r = brt_search_node(brt, node, search, getf, getf_v, &re, &doprefetch, brtcursor);
        if (r!=0) goto return_r;

        r = brt_handle_maybe_reactive_child_at_root(brt, rootp, &node, re, logger);
    }

 return_r:
    rr = toku_unpin_brtnode(brt, node);
    assert(rr == 0);

    //Heaviside function (+direction) queries define only a lower or upper
    //bound.  Some queries require both an upper and lower bound.
    //They do this by wrapping the BRT_GET_CALLBACK_FUNCTION with another
    //test that checks for the other bound.  If the other bound fails,
    //it returns TOKUDB_FOUND_BUT_REJECTED which means not found, but
    //stop searching immediately, as opposed to DB_NOTFOUND
    //which can mean not found, but keep looking in another leaf.
    if (r==TOKUDB_FOUND_BUT_REJECTED) r = DB_NOTFOUND;
    else if (r==DB_NOTFOUND) {
        //We truly did not find an answer to the query.
        //Therefore, the BRT_GET_STRADDLE_CALLBACK_FUNCTION has NOT been called.
        //The contract specifies that the callback function must be called
        //for 'r= (0|DB_NOTFOUND|TOKUDB_FOUND_BUT_REJECTED)'
        //TODO: #1378 This is not the ultimate location of this call to the
        //callback.  It is surely wrong for node-level locking, and probably
        //wrong for the STRADDLE callback for heaviside function(two sets of key/vals)
        int r2 = getf(0,NULL, 0,NULL,
                      0,NULL, 0,NULL,
                      getf_v);
        if (r2!=0) r = r2;
    }
    return r;
}

struct brt_cursor_search_struct {
    BRT_GET_CALLBACK_FUNCTION getf;
    void *getf_v;
    BRT_CURSOR cursor;
    brt_search_t *search;
};

static int
brt_cursor_search_getf(ITEMLEN keylen,          bytevec key,
                       ITEMLEN vallen,          bytevec val,
                       ITEMLEN UU(next_keylen), bytevec UU(next_key),
                       ITEMLEN UU(next_vallen), bytevec UU(next_val),
                       void *v) {
    struct brt_cursor_search_struct *bcss = v;
    int r = bcss->getf(keylen, key, vallen, val, bcss->getf_v);
    return r;
}

/* search for the first kv pair that matches the search object */
static int
brt_cursor_search(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_cursor_invalidate(cursor);
    struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, search};
    int r = toku_brt_search(cursor->brt, search, brt_cursor_search_getf, &bcss, logger, cursor, &cursor->root_put_counter);
    return r;
}

static inline int compare_kv_xy(BRT brt, DBT *k, DBT *v, DBT *x, DBT *y) {
    int cmp = brt->compare_fun(brt->db, k, x);
    if (cmp == 0 && v && y)
        cmp = brt->dup_compare(brt->db, v, y);
    return cmp;
}

static inline int compare_k_x(BRT brt, DBT *k, DBT *x) {
    return brt->compare_fun(brt->db, k, x);
}

static inline int compare_v_y(BRT brt, DBT *v, DBT *y) {
    return brt->dup_compare(brt->db, v, y);
}

static int
brt_cursor_compare_one(brt_search_t *search, DBT *x, DBT *y)
{
    search = search; x = x; y = y;
    return 1;
}

static int
brt_cursor_search_eq_kv_xy_getf(ITEMLEN keylen,          bytevec key,
                                ITEMLEN vallen,          bytevec val,
                                ITEMLEN UU(next_keylen), bytevec UU(next_key),
                                ITEMLEN UU(next_vallen), bytevec UU(next_val),
                                void *v) {
    struct brt_cursor_search_struct *bcss = v;
    int r;
    if (key==NULL) {
	r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
    } else {
	BRT_CURSOR cursor = bcss->cursor;
	brt_search_t *search = bcss->search;
	DBT newkey = {.size = keylen, .data=(void*)key};
	DBT newval = {.size = vallen, .data=(void*)val};
	if (compare_kv_xy(cursor->brt, search->k, search->v, &newkey, &newval) == 0) {
	    r = bcss->getf(keylen, key, vallen, val, bcss->getf_v);
	} else {
	    r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
            if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
        }
    }
    return r;
}

/* search for the kv pair that matches the search object and is equal to kv */
static int
brt_cursor_search_eq_kv_xy(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_cursor_invalidate(cursor);
    struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, search};
    int r = toku_brt_search(cursor->brt, search, brt_cursor_search_eq_kv_xy_getf, &bcss, logger, cursor, &cursor->root_put_counter);
    return r;
}

static int brt_cursor_compare_set(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) <= 0; /* return min xy: kv <= xy */
}

static int
brt_cursor_current_getf(ITEMLEN keylen,          bytevec key,
                        ITEMLEN vallen,          bytevec val,
                        ITEMLEN UU(next_keylen), bytevec UU(next_key),
                        ITEMLEN UU(next_vallen), bytevec UU(next_val),
                        void *v) {
    struct brt_cursor_search_struct *bcss = v;
    int r;
    if (key==NULL) {
	r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
    } else {
	BRT_CURSOR cursor = bcss->cursor;
	DBT newkey = {.size=keylen, .data=(void*)key}; // initializes other fields to zero
	DBT newval = {.size=vallen, .data=(void*)val};
        //Safe to access cursor->key/val because current_in_omt is FALSE
	if (compare_kv_xy(cursor->brt, &cursor->key, &cursor->val, &newkey, &newval) != 0) {
	    r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v); // This was once DB_KEYEMPTY
            if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
        }
	else
	    r = bcss->getf(keylen, key, vallen, val, bcss->getf_v);
    }
    return r;
}

int
toku_brt_cursor_current(BRT_CURSOR cursor, int op, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    if (brt_cursor_not_set(cursor))
        return EINVAL;
    if (op == DB_CURRENT) {
        brt_cursor_invalidate(cursor);
	struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, 0};
        brt_search_t search; brt_search_init(&search, brt_cursor_compare_set, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
        return toku_brt_search(cursor->brt, &search, brt_cursor_current_getf, &bcss, logger, cursor, &cursor->root_put_counter);
    }
    brt_cursor_invalidate(cursor);
    return getf(cursor->key.size, cursor->key.data, cursor->val.size, cursor->val.data, getf_v); // brt_cursor_copyout(cursor, outkey, outval);
}

int
toku_brt_cursor_first(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_LEFT, 0, 0, cursor->brt);
    return brt_cursor_search(cursor, &search, getf, getf_v, logger);
}

int
toku_brt_cursor_last(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_RIGHT, 0, 0, cursor->brt);
    return brt_cursor_search(cursor, &search, getf, getf_v, logger);
}

static int brt_cursor_compare_next(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) < 0; /* return min xy: kv < xy */
}

static int
brt_cursor_shortcut (BRT_CURSOR cursor, int direction, u_int32_t limit, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v) {
    int r;
    OMTCURSOR omtcursor     = cursor->omtcursor;
    OMT       omt           = toku_omt_cursor_get_omt(omtcursor);
    u_int64_t h_put_counter = cursor->brt->h->root_put_counter;
    u_int64_t c_put_counter = cursor->root_put_counter;
    BOOL found = FALSE;

    //Verify that no messages have been inserted
    //since the last time the cursor's pointer was set.
    //Also verify the omt cursor is still valid.
    //(Necessary to recheck after the maybe_get_and_pin)
    if (c_put_counter==h_put_counter && toku_omt_cursor_is_valid(cursor->omtcursor)) {
        u_int32_t index = 0;
        r = toku_omt_cursor_current_index(omtcursor, &index);
        assert(r==0);

        //Starting with the prev, find the first real (non-provdel) leafentry.
        while (index != limit) {
            OMTVALUE le = NULL;
            index += direction;
            r = toku_omt_fetch(omt, index, &le, NULL);
            assert(r==0);

            if (!le_is_provdel(le)) {
                u_int32_t keylen = le_latest_keylen(le);
                bytevec   key    = le_latest_key(le);
                u_int32_t vallen = le_latest_vallen(le);
                bytevec   val    = le_latest_val(le);

                r = getf(keylen, key, vallen, val, getf_v);
                if (r==0) {
                    //Update cursor.
                    cursor->leaf_info.to_be.index = index;
                    brt_cursor_update(cursor);
                    found = TRUE;
                }
                break;
            }
        }
        if (r==0 && !found) r = DB_NOTFOUND;
    }
    else r = EINVAL;

    return r;
}

//TODO: #1485 once we have multiple main threads, restore this code, analyze performance.
#ifndef TOKU_MULTIPLE_MAIN_THREADS
#define TOKU_MULTIPLE_MAIN_THREADS 0
#endif

#if TOKU_MULTIPLE_MAIN_THREADS
static int
brt_cursor_maybe_get_and_pin_leaf(BRT_CURSOR brtcursor, BRTNODE* leafp) {
    void * nodep = NULL;
    int r = toku_cachetable_maybe_get_and_pin(brtcursor->brt->cf,
                                              brtcursor->leaf_info.blocknumber,
                                              brtcursor->leaf_info.fullhash,
                                              &nodep);
    if (r == 0) {
	BRTNODE leaf = nodep;
	assert(leaf->height == 0);	// verify that returned node is leaf...
	assert(leaf->u.l.buffer == toku_omt_cursor_get_omt(brtcursor->omtcursor));  // ... and has right omt
	*leafp = leaf;
    }
    return r;
}

static int
brt_cursor_unpin_leaf(BRT_CURSOR brtcursor, BRTNODE leaf) {
    int r = toku_unpin_brtnode(brtcursor->brt, leaf);
    return r;
}
#else
static int
brt_cursor_maybe_get_and_pin_leaf(BRT_CURSOR UU(brtcursor), BRTNODE* UU(leafp)) {
    return 0;
}
static int
brt_cursor_unpin_leaf(BRT_CURSOR UU(brtcursor), BRTNODE UU(leaf)) {
    return 0;
}
#endif


static int
brt_cursor_next_shortcut (BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
// Effect: If possible, increment the cursor and return the key-value pair
//  (i.e., the next one from what the cursor pointed to before.)
// That is, do DB_NEXT on DUP databases, and do DB_NEXT_NODUP on NODUP databases.
{
    int r;
    int r2;
    if (toku_omt_cursor_is_valid(cursor->omtcursor)) {
	BRTNODE leaf;
	r = brt_cursor_maybe_get_and_pin_leaf(cursor, &leaf);
	if (r == 0) {
	    u_int32_t limit = toku_omt_size(toku_omt_cursor_get_omt(cursor->omtcursor)) - 1;
	    r = brt_cursor_shortcut(cursor, 1, limit, getf, getf_v);
	    r2 = brt_cursor_unpin_leaf(cursor, leaf);
	}
    }
    else r = EINVAL;
    return r ? r : r2;
}

int
toku_brt_cursor_next(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    int r;

    if ((cursor->brt->flags & TOKU_DB_DUP) && brt_cursor_next_shortcut(cursor, getf, getf_v)==0) {
	r = 0;
    }
    else {
        brt_search_t search; brt_search_init(&search, brt_cursor_compare_next, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
        r = brt_cursor_search(cursor, &search, getf, getf_v, logger);
    }
    if (r == 0) brt_cursor_set_prefetching(cursor);
    return r;
}

static int brt_cursor_compare_next_nodup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context; y = y;
    return compare_k_x(brt, search->k, x) < 0; /* return min x: k < x */
}

int
toku_brt_cursor_next_nodup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    int r;

    if (!(cursor->brt->flags & TOKU_DB_DUP) && brt_cursor_next_shortcut(cursor, getf, getf_v)==0) {
	r = 0;
    }
    else {
        brt_search_t search; brt_search_init(&search, brt_cursor_compare_next_nodup, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
        r = brt_cursor_search(cursor, &search, getf, getf_v, logger);
    }
    if (r == 0) brt_cursor_set_prefetching(cursor);
    return r;
}

static int brt_cursor_compare_next_dup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp < 0)
        return 1;
    else
        return keycmp == 0 && y && compare_v_y(brt, search->v, y) < 0; /* return min xy: k <= x && v < y */
}

static int
brt_cursor_search_eq_k_x_getf(ITEMLEN keylen,          bytevec key,
                              ITEMLEN vallen,          bytevec val,
                              ITEMLEN UU(next_keylen), bytevec UU(next_key),
                              ITEMLEN UU(next_vallen), bytevec UU(next_val),
                              void *v) {
    struct brt_cursor_search_struct *bcss = v;
    int r;
    if (key==NULL) {
        r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
    } else {
	BRT_CURSOR cursor = bcss->cursor;
	DBT newkey = {.size=keylen, .data=(void*)key}; // initializes other fields to zero
	if (compare_k_x(cursor->brt, bcss->search->k, &newkey) == 0) {
	    r = bcss->getf(keylen, key, vallen, val, bcss->getf_v);
	} else {
	    r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
            if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
	}
    }
    return r;
}

/* search for the kv pair that matches the search object and is equal to k */
static int
brt_cursor_search_eq_k_x(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_cursor_invalidate(cursor);
    struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, search};
    int r = toku_brt_search(cursor->brt, search, brt_cursor_search_eq_k_x_getf, &bcss, logger, cursor, &cursor->root_put_counter);
    return r;
}

int
toku_brt_cursor_next_dup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    int r;
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next_dup, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->brt);
    r = brt_cursor_search_eq_k_x(cursor, &search, getf, getf_v, logger);
    if (r == 0) brt_cursor_set_prefetching(cursor);
    return r;
}

static int brt_cursor_compare_get_both_range(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp < 0)
        return 1;
    else
        return keycmp == 0 && (y == 0 || compare_v_y(brt, search->v, y) <= 0); /* return min xy: k <= x && v <= y */
}

int
toku_brt_cursor_get_both_range(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_get_both_range, BRT_SEARCH_LEFT, key, val, cursor->brt);
    return brt_cursor_search_eq_k_x(cursor, &search, getf, getf_v, logger);
}


static int
brt_cursor_prev_shortcut (BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
// Effect: If possible, decrement the cursor and return the key-value pair
//  (i.e., the previous one from what the cursor pointed to before.)
// That is, do DB_PREV on DUP databases, and do DB_PREV_NODUP on NODUP databases.
{
    int r;
    int r2;
    if (toku_omt_cursor_is_valid(cursor->omtcursor)) {
	BRTNODE leaf;
	r = brt_cursor_maybe_get_and_pin_leaf(cursor, &leaf);
	if (r == 0) {
	    r = brt_cursor_shortcut(cursor, -1, 0, getf, getf_v);
	    r2 = brt_cursor_unpin_leaf(cursor, leaf);
	}
    }
    else r = EINVAL;
    return r ? r : r2;
}



static int brt_cursor_compare_prev(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) > 0; /* return max xy: kv > xy */
}

int
toku_brt_cursor_prev(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    if (0!=(cursor->brt->flags & TOKU_DB_DUP) &&
	brt_cursor_prev_shortcut(cursor, getf, getf_v)==0)
	return 0;
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, getf, getf_v, logger);
}

static int brt_cursor_compare_prev_nodup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context; y = y;
    return compare_k_x(brt, search->k, x) > 0; /* return max x: k > x */
}

int
toku_brt_cursor_prev_nodup(BRT_CURSOR cursor,  BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    if (0==(cursor->brt->flags & TOKU_DB_DUP) &&
	brt_cursor_prev_shortcut(cursor, getf, getf_v)==0)
	return 0;
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev_nodup, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search(cursor, &search, getf, getf_v, logger);
}

static int brt_cursor_compare_prev_dup(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    int keycmp = compare_k_x(brt, search->k, x);
    if (keycmp > 0)
        return 1;
    else
        return keycmp == 0 && y && compare_v_y(brt, search->v, y) > 0; /* return max xy: k >= x && v > y */
}

int
toku_brt_cursor_prev_dup(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev_dup, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->brt);
    return brt_cursor_search_eq_k_x(cursor, &search, getf, getf_v, logger);
}


static int brt_cursor_compare_set_range(brt_search_t *search, DBT *x, DBT *y) {
    BRT brt = search->context;
    return compare_kv_xy(brt, search->k, search->v, x, y) <= 0; /* return kv <= xy */
}

int
toku_brt_cursor_set(BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, val, cursor->brt);
    return brt_cursor_search_eq_kv_xy(cursor, &search, getf, getf_v, logger);
}

int
toku_brt_cursor_set_range(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, NULL, cursor->brt);
    return brt_cursor_search(cursor, &search, getf, getf_v, logger);
}

//TODO: When tests have been rewritten, get rid of this function.
//Only used by tests.
int
toku_brt_cursor_get (BRT_CURSOR cursor, DBT *key, DBT *val, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags, TOKUTXN txn)
{
    int op = get_flags & DB_OPFLAGS_MASK;
    TOKULOGGER logger = toku_txn_logger(txn);
    if (get_flags & ~DB_OPFLAGS_MASK)
        return EINVAL;

    switch (op) {
    case DB_CURRENT:
    case DB_CURRENT_BINDING:
        return toku_brt_cursor_current(cursor, op, getf, getf_v, logger);
    case DB_FIRST:
        return toku_brt_cursor_first(cursor, getf, getf_v, logger);
    case DB_LAST:
        return toku_brt_cursor_last(cursor, getf, getf_v, logger);
    case DB_NEXT:
        if (brt_cursor_not_set(cursor))
            return toku_brt_cursor_first(cursor, getf, getf_v, logger);
        else
            return toku_brt_cursor_next(cursor, getf, getf_v, logger);
    case DB_NEXT_DUP:
        if (brt_cursor_not_set(cursor))
            return EINVAL;
        else
            return toku_brt_cursor_next_dup(cursor, getf, getf_v, logger);
    case DB_NEXT_NODUP:
        if (brt_cursor_not_set(cursor))
            return toku_brt_cursor_first(cursor, getf, getf_v, logger);
        else
            return toku_brt_cursor_next_nodup(cursor, getf, getf_v, logger);
    case DB_PREV:
        if (brt_cursor_not_set(cursor))
            return toku_brt_cursor_last(cursor, getf, getf_v, logger);
        else
            return toku_brt_cursor_prev(cursor, getf, getf_v, logger);
#if defined(DB_PREV_DUP)
    case DB_PREV_DUP:
        if (brt_cursor_not_set(cursor))
            return EINVAL;
        else
            return toku_brt_cursor_prev_dup(cursor, getf, getf_v, logger);
#endif
    case DB_PREV_NODUP:
        if (brt_cursor_not_set(cursor))
            return toku_brt_cursor_last(cursor, getf, getf_v, logger);
        else
            return toku_brt_cursor_prev_nodup(cursor, getf, getf_v, logger);
    case DB_SET:
        return toku_brt_cursor_set(cursor, key, 0, getf, getf_v, logger);
    case DB_SET_RANGE:
        return toku_brt_cursor_set_range(cursor, key, getf, getf_v, logger);
    case DB_GET_BOTH:
        return toku_brt_cursor_set(cursor, key, val, getf, getf_v, logger);
    case DB_GET_BOTH_RANGE:
        return toku_brt_cursor_get_both_range(cursor, key, val, getf, getf_v, logger);
    default: ;// Fall through
    }
    return EINVAL;
}

void
toku_brt_cursor_peek(BRT_CURSOR cursor, const DBT **pkey, const DBT **pval)
// Effect: Retrieves a pointer to the DBTs for the current key and value.
// Requires:  The caller may not modify the DBTs or the memory at which they points.
// Requires:  The caller must be in the context of a
// BRT_GET_(STRADDLE_)CALLBACK_FUNCTION
{
    if (cursor->current_in_omt) load_dbts_from_omt(cursor, &cursor->key, &cursor->val);
    *pkey = &cursor->key;
    *pval = &cursor->val;
}

static int brt_cursor_compare_heaviside(brt_search_t *search, DBT *x, DBT *y) {
    HEAVI_WRAPPER wrapper = search->context;
    int r = wrapper->h(x, y, wrapper->extra_h);
    // wrapper->r_h must have the same signus as the final chosen element.
    // it is initialized to -1 or 1.  0's are closer to the min (max) that we
    // want so once we hit 0 we keep it.
    if (r==0) wrapper->r_h = 0;
    //direction>0 means BRT_SEARCH_LEFT
    //direction<0 means BRT_SEARCH_RIGHT
    //direction==0 is not allowed
    r = (wrapper->direction>0) ? r>=0 : r<=0;
    return r;
}

//We pass in toku_dbt_fake to the search functions, since it will not pass the
//key(or val) to the heaviside function if key(or val) is NULL.
//It is not used for anything else,
//the actual 'extra' information for the heaviside function is inside the
//wrapper.
static const DBT __toku_dbt_fake;
static const DBT* const toku_dbt_fake = &__toku_dbt_fake;

#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
struct brt_cursor_straddle_search_struct {
    BRT_GET_STRADDLE_CALLBACK_FUNCTION getf;
    void *getf_v;
    BRT_CURSOR cursor;
    brt_search_t *search;
};

static int
straddle_hack_getf(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val,
                   ITEMLEN next_keylen, bytevec next_key, ITEMLEN next_vallen, bytevec next_val, void* v) {
    struct brt_cursor_straddle_search_struct *bcsss = v;
    int old_hack_value = STRADDLE_HACK_disable_merges_and_splits;
    STRADDLE_HACK_disable_merges_and_splits = 1;
    int r = bcsss->getf(keylen, key, vallen, val, next_keylen, next_key, next_vallen, next_val, bcsss->getf_v);
    STRADDLE_HACK_disable_merges_and_splits = old_hack_value;
    return r;
}
#endif

/* search for the first kv pair that matches the search object */
static int
brt_cursor_straddle_search(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger)
{
    brt_cursor_invalidate(cursor);
#ifdef  BRT_LEVEL_STRADDLE_CALLBACK_LOGIC_NOT_READY
    struct brt_cursor_straddle_search_struct bcsss = {getf, getf_v, cursor, search};
    int r = toku_brt_search(cursor->brt, search, straddle_hack_getf, &bcsss, logger, cursor, &cursor->root_put_counter);
#else
    int r = toku_brt_search(cursor->brt, search, getf, getf_v, logger, cursor, &cursor->root_put_counter);
#endif
    return r;
}

int
toku_brt_cursor_heaviside(BRT_CURSOR cursor, BRT_GET_STRADDLE_CALLBACK_FUNCTION getf, void *getf_v, TOKULOGGER logger, HEAVI_WRAPPER wrapper)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_heaviside,
                                         wrapper->direction < 0 ? BRT_SEARCH_RIGHT : BRT_SEARCH_LEFT,
                                         (DBT*)toku_dbt_fake,
                                         cursor->brt->flags & TOKU_DB_DUPSORT ? (DBT*)toku_dbt_fake : NULL,
                                         wrapper);
    return brt_cursor_straddle_search(cursor, &search, getf, getf_v, logger);
}

BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c) {
    return brt_cursor_not_set(c);
}

int toku_brt_get_cursor_count (BRT brt) {
    int n = 0;
    struct list *list;
    for (list = brt->cursors.next; list != &brt->cursors; list = list->next)
        n += 1;
    return n;
}

// TODO: Get rid of this
int toku_brt_dbt_set(DBT* key, DBT* key_source) {
    int r = toku_dbt_set(key_source->size, key_source->data, key, NULL);
    return r;
}

/* ********************************* lookup **************************************/

int
toku_brt_lookup (BRT brt, DBT *k, DBT *v, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    int r, rr;
    BRT_CURSOR cursor;

    rr = toku_brt_cursor(brt, &cursor);
    if (rr != 0) return rr;

    int op = brt->flags & TOKU_DB_DUPSORT ? DB_GET_BOTH : DB_SET;
    r = toku_brt_cursor_get(cursor, k, v, getf, getf_v, op, 0);

    rr = toku_brt_cursor_close(cursor); assert(rr == 0);

    return r;
}

/* ********************************* delete **************************************/

int toku_brt_delete_both(BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    //{ unsigned i; printf("del %p keylen=%d key={", brt->db, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", val->size); for(i=0; i<val->size; i++) printf("%d,", ((char*)val->data)[i]); printf("}\n"); }
    int r;
    if (txn && (brt->txnid_that_created_or_locked_when_empty != toku_txn_get_txnid(txn))) {
        BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
        BYTESTRING databs = {val->size, toku_memdup_in_rollback(txn, val->data, val->size)};
        toku_cachefile_refup(brt->cf);
        r = toku_logger_save_rollback_cmddeleteboth(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs, databs);
        if (r!=0) return r;
        r = toku_txn_note_brt(txn, brt);
        if (r!=0) return r;
    }
    BRT_CMD_S brtcmd = { BRT_DELETE_BOTH, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

static int
getf_nothing (ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN UU(vallen), bytevec UU(val), void *UU(pair_v)) {
    return 0;
}

int
toku_brt_cursor_delete(BRT_CURSOR cursor, int flags, TOKUTXN txn) {
    int r;

    int unchecked_flags = flags;
    BOOL error_if_missing = (BOOL) !(flags&DB_DELETE_ANY);
    unchecked_flags &= ~DB_DELETE_ANY;
    if (unchecked_flags!=0) r = EINVAL;
    else if (brt_cursor_not_set(cursor)) r = EINVAL;
    else {
        r = 0;
        if (error_if_missing) {
            r = toku_brt_cursor_current(cursor, DB_CURRENT, getf_nothing, NULL, toku_txn_logger(txn));
        }
        if (r == 0) {
            //We need to have access to the (key,val) that the cursor points to.
            //By invalidating the cursor we guarantee we have a local copy.
            //
            //If we try to use the omtcursor, there exists a race condition
            //(node could be evicted), but maybe_get_and_pin() prevents delete.
            brt_cursor_invalidate(cursor);
            r = toku_brt_delete_both(cursor->brt, &cursor->key, &cursor->val, txn);
        }
    }
    return r;
}

/* ********************* keyrange ************************ */


static void toku_brt_keyrange_internal (BRT brt, CACHEKEY nodename, u_int32_t fullhash, DBT *key, u_int64_t *less,  u_int64_t *equal,  u_int64_t *greater) {
    BRTNODE node;
    {
        void *node_v;
        //assert(fullhash == toku_cachetable_hash(brt->cf, nodename));
        int rr = toku_cachetable_get_and_pin(brt->cf, nodename, fullhash,
                                             &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
        assert(rr == 0);
        node = node_v;
        assert(node->fullhash==fullhash);
    }
    if (node->height>0) {
        int n_keys = node->u.n.n_children-1;
        int compares[n_keys];
        int i;
        for (i=0; i<n_keys; i++) {
            struct kv_pair *pivot = node->u.n.childkeys[i];
            DBT dbt;
            compares[i] = brt->compare_fun(brt->db, toku_fill_dbt(&dbt, kv_pair_key(pivot), kv_pair_keylen(pivot)), key);
        }
        for (i=0; i<node->u.n.n_children; i++) {
            int prevcomp = (i==0) ? -1 : compares[i-1];
            int nextcomp = (i+1 >= n_keys) ? 1 : compares[i];
            int subest = BNC_SUBTREE_ESTIMATES(node, i).ndata;
            if (nextcomp < 0) {
                // We're definitely looking too far to the left
                *less += subest;
            } else if (prevcomp > 0) {
                // We're definitely looking too far to the right
                *greater += subest;
            } else if (prevcomp == 0 && nextcomp == 0) {
                // We're looking at a subtree that contains all zeros
                *equal   += subest;
            } else {
                // nextcomp>=0 and prevcomp<=0, so something in the subtree could match
                // but they are not both zero, so it's not the whole subtree, so we need to recurse
                toku_brt_keyrange_internal(brt, BNC_BLOCKNUM(node, i), compute_child_fullhash(brt->cf, node, i), key, less, equal, greater);
            }
        }
    } else {
        BRT_CMD_S cmd = { BRT_INSERT, 0, .u.id={key,0}};
        struct cmd_leafval_heaviside_extra be = {brt, &cmd, 0};
        u_int32_t idx;
        int r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_heaviside, &be, 0, &idx, NULL);
        *less += idx;
        if (r==0 && (brt->flags & TOKU_DB_DUP)) {
            // There is something, and so we now want to find the rightmost extent.
            u_int32_t idx2;
            r = toku_omt_find(node->u.l.buffer, toku_cmd_leafval_heaviside, &be, +1, 0, &idx2, NULL);
            if (r==0) {
                *greater += toku_omt_size(node->u.l.buffer)-idx2;
                *equal   += idx2-idx;
            } else {
                *equal   += toku_omt_size(node->u.l.buffer)-idx;
            }
            //printf("%s:%d (%llu, %llu, %llu)\n", __FILE__, __LINE__, (unsigned long long)*less, (unsigned long long)*equal, (unsigned long long)*greater);
        } else {
            *greater += toku_omt_size(node->u.l.buffer)-idx;
            if (r==0) {
                (*greater)--;
                (*equal)++;
            }
        }
    }
    {
        int rr = toku_unpin_brtnode(brt, node);
        assert(rr == 0);
    }
}

int toku_brt_keyrange (BRT brt, DBT *key, u_int64_t *less,  u_int64_t *equal,  u_int64_t *greater) {
    assert(brt->h);
    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    *less = *equal = *greater = 0;
    toku_brt_keyrange_internal (brt, *rootp, fullhash, key, less, equal, greater);
    return 0;
}

int toku_brt_stat64 (BRT brt, TOKUTXN UU(txn), u_int64_t *nkeys, u_int64_t *ndata, u_int64_t *dsize, u_int64_t *fsize) {
    {
	int64_t file_size;
	int r = toku_os_get_file_size(toku_cachefile_fd(brt->cf), &file_size);
	assert(r==0);
	*fsize = file_size + toku_cachefile_size_in_memory(brt->cf);
    }

    assert(brt->h);
    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    CACHEKEY root = *rootp;
    void *node_v;
    int r = toku_cachetable_get_and_pin(brt->cf, root, fullhash,
					&node_v, NULL,
					toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
    if (r!=0) return r;
    BRTNODE node = node_v;

    if (node->height==0) {
	*nkeys = node->u.l.leaf_stats.nkeys;
	*ndata = node->u.l.leaf_stats.ndata;
	*dsize = node->u.l.leaf_stats.dsize;
    } else {
	*nkeys = *ndata = *dsize = 0;
	int i;
	for (i=0; i<node->u.n.n_children; i++) {
	    struct subtree_estimates *se = &BNC_SUBTREE_ESTIMATES(node, i);
	    *nkeys += se->nkeys;
	    *ndata += se->ndata;
	    *dsize += se->dsize;
	}
    }
    
    r = toku_cachetable_unpin(brt->cf, root, fullhash, CACHETABLE_CLEAN, 0);
    if (r!=0) return r;
    return 0;
}

/* ********************* debugging dump ************************ */
static int
toku_dump_brtnode (FILE *file, BRT brt, BLOCKNUM blocknum, int depth, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen) {
    int result=0;
    BRTNODE node;
    void *node_v;
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    int r = toku_cachetable_get_and_pin(brt->cf, blocknum, fullhash,
                                        &node_v, NULL,
                                        toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h);
    assert(r==0);
    fprintf(file, "%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    assert(node->fullhash==fullhash);
    result=toku_verify_brtnode(brt, blocknum, lorange, lolen, hirange, hilen, 0);
    fprintf(file, "%*sNode=%p\n", depth, "", node);
    if (node->height>0) {
        fprintf(file, "%*sNode %"PRId64" nodesize=%u height=%d n_children=%d  n_bytes_in_buffers=%u keyrange=%s %s\n",
               depth, "", blocknum.b, node->nodesize, node->height, node->u.n.n_children, node->u.n.n_bytes_in_buffers, (char*)lorange, (char*)hirange);
        //printf("%s %s\n", lorange ? lorange : "NULL", hirange ? hirange : "NULL");
        {
            int i;
            for (i=0; i+1< node->u.n.n_children; i++) {
                fprintf(file, "%*spivotkey %d =", depth+1, "", i);
                toku_print_BYTESTRING(file, toku_brt_pivot_key_len(brt, node->u.n.childkeys[i]), node->u.n.childkeys[i]->key);
                fprintf(file, "\n");
            }
            for (i=0; i< node->u.n.n_children; i++) {
                fprintf(file, "%*schild %d buffered (%d entries):", depth+1, "", i, toku_fifo_n_entries(BNC_BUFFER(node,i)));
		{
		    struct subtree_estimates *e = &BNC_SUBTREE_ESTIMATES(node, i);
		    fprintf(file, " est={n=%" PRIu64 " k=%" PRIu64 " s=%" PRIu64 " e=%d}",
			    e->ndata, e->nkeys, e->dsize, e->exact);
		}
		fprintf(file, "\n");
                FIFO_ITERATE(BNC_BUFFER(node,i), key, keylen, data, datalen, type, xid,
                                  {
                                      data=data; datalen=datalen; keylen=keylen;
                                      fprintf(file, "%*s xid=%"PRIu64" %u (type=%d)\n", depth+2, "", xid, (unsigned)toku_dtoh32(*(int*)key), type);
                                      //assert(strlen((char*)key)+1==keylen);
                                      //assert(strlen((char*)data)+1==datalen);
                                  });
            }
            for (i=0; i<node->u.n.n_children; i++) {
                fprintf(file, "%*schild %d\n", depth, "", i);
                if (i>0) {
                    fprintf(file, "%*spivot %d len=%u %u\n", depth+1, "", i-1, node->u.n.childkeys[i-1]->keylen, (unsigned)toku_dtoh32(*(int*)&node->u.n.childkeys[i-1]->key));
                }
                toku_dump_brtnode(file, brt, BNC_BLOCKNUM(node, i), depth+4,
                                  (i==0) ? lorange : node->u.n.childkeys[i-1]->key,
                                  (i==0) ? lolen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i-1]),
                                  (i==node->u.n.n_children-1) ? hirange : node->u.n.childkeys[i]->key,
                                  (i==node->u.n.n_children-1) ? hilen   : toku_brt_pivot_key_len(brt, node->u.n.childkeys[i])
                                  );
            }
        }
    } else {
        fprintf(file, "%*sNode %" PRId64 " nodesize=%u height=%d n_bytes_in_buffer=%u keyrange (key only)=",
                depth, "", blocknum.b, node->nodesize, node->height, node->u.l.n_bytes_in_buffer);
        if (lorange) { toku_print_BYTESTRING(file, lolen, (void*)lorange); } else { fprintf(file, "-\\infty"); } fprintf(file, " ");
        if (hirange) { toku_print_BYTESTRING(file, hilen, (void*)hirange); } else { fprintf(file, "\\infty"); }
	fprintf(file, " est={n=%" PRIu64 " k=%" PRIu64 " s=%" PRIu64 " e=%d}",
		node->u.l.leaf_stats.ndata, node->u.l.leaf_stats.nkeys, node->u.l.leaf_stats.dsize, node->u.l.leaf_stats.exact);
	fprintf(file, "\n");
        int size = toku_omt_size(node->u.l.buffer);
        int i;
	if (0)
        for (i=0; i<size; i++) {
            OMTVALUE v = 0;
            r = toku_omt_fetch(node->u.l.buffer, i, &v, 0);
            assert(r==0);
            fprintf(file, " [%d]=", i);
            print_leafentry(file, v);
            fprintf(file, "\n");
        }
        //             printf(" (%d)%u ", len, *(int*)le_any_key(data)));
        fprintf(file, "\n");
    }
    r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, 0);
    assert(r==0);
    return result;
}

int toku_dump_brt (FILE *f, BRT brt) {
    CACHEKEY *rootp;
    assert(brt->h);
    u_int32_t fullhash;
    toku_dump_translation_table(f, brt->h->blocktable);
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    return toku_dump_brtnode(f, brt, *rootp, 0, 0, 0, 0, 0);
}

int toku_brt_truncate (BRT brt) {
    int r;

    // flush the cached tree blocks
    r = toku_brt_flush(brt);

    // TODO log the truncate?

    toku_block_lock_for_multiple_operations(brt->h->blocktable);
    if (r==0) {
        // reinit the header
        toku_block_translation_truncate_unlocked(brt->h->blocktable, brt->h);
        //Assign blocknum for root block, also dirty the header
        toku_allocate_blocknum_unlocked(brt->h->blocktable, &brt->h->root, brt->h);

        brtheader_partial_destroy(brt->h);
        r = brt_init_header_partial(brt);
    }

    toku_block_unlock_for_multiple_operations(brt->h->blocktable);

    return r;
}

static void toku_brt_lock_init(void) {
    toku_pwrite_lock_init();
    toku_logger_lock_init();
    toku_leaflock_init();
}

static void toku_brt_lock_destroy(void) {
    toku_pwrite_lock_destroy();
    toku_logger_lock_destroy();
    toku_leaflock_destroy();
}

void toku_brt_init(void (*ydb_lock_callback)(void), void (*ydb_unlock_callback)(void)) {
    toku_brt_lock_init();
    toku_checkpoint_init(ydb_lock_callback, ydb_unlock_callback);
}

void toku_brt_destroy(void) {
    toku_brt_lock_destroy();
}

//Return TRUE if empty, FALSE if not empty.
static BOOL
brt_is_empty (BRT brt, TOKULOGGER logger) {
    BRT_CURSOR cursor;
    int r, r2;
    BOOL is_empty;
    r = toku_brt_cursor(brt, &cursor);
    if (r == 0) {
        r = toku_brt_cursor_first(cursor, getf_nothing, NULL, logger);
        r2 = toku_brt_cursor_close(cursor);
        is_empty = (BOOL)(r2==0 && r==DB_NOTFOUND);
    }
    else is_empty = FALSE; //Declare it "not empty" on error.
    return is_empty;
}

int
toku_brt_note_table_lock (BRT brt, TOKUTXN txn)
{
    if (brt_is_empty(brt, toku_txn_logger(txn))) {
        brt->txnid_that_created_or_locked_when_empty = toku_txn_get_txnid(txn);
        toku_cachefile_refup(brt->cf);
        return toku_logger_save_rollback_tablelock_on_empty_table(txn, toku_cachefile_filenum(brt->cf));
    }
    return 0;
}
