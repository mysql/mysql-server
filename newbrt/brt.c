/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

 
/*

 We always write nodes to a new location on disk.
 The nodes themselves contain the information about the tree structure.

Q: During recovery, how do we find the root node without looking at every block on disk?
A: The root node is either the designated root near the front of the freelist.
   The freelist is updated infrequently.  Before updating the stable copy of the freelist, we make sure that
   the root is up-to-date.  We can make the freelist-and-root update be an arbitrarily small fraction of disk bandwidth.
     
*/

/*

How insertion, deletion, and querying work:

Every node in the tree has a target size.  (It might be 1MB).

Eveyr nonleaf node also has a target fanout.  (It might be 16).

We say that a node is _overfull_ if it is larger than its target size.

We say that a node is _underfull_ if it is less than 1/4 of its target size.

We say that a nonleaf node is _too fat_ if its fanout is larger than its target fanout.

We say that a nonleaf node is _too thin_ if its fanout is less than 1/4 of its target fanout.

We say that a node _should be split_ if 
 - it is an overfull leaf, or
 - it is a too-fat nonleaf.

We say that a node _should be merged_ if
 - it is a underfull leaf , or
 - it is a too-thin nonleaf.

When we insert a value into the BRT, we insert a message which travels down the tree.  Here's the rules for how far the message travels and what happens.

When inserting a message, we send it as far down the tree as possible, but
 - we stop if it would require I/O (we bring in the root node even if does require I/O), and
 - we stop if we reach a node that is overfull 
   (We don't want to further overfill a node, so we place the message in the overfull node's parent.  If the root is overfull, we place the message in the root.)
 - we stop if we find a FIFO that contains a message, since we must preserve the order of messages.  (We enqueue the message).

After putting the message into a node, we may need to adjust the tree.
Observe that the ancestors of the node into which we placed the
message are not overfull.  (But they may be underfull or too fat or too thin.)




 - If we put a message into a leaf node, and it is now overfull, then we 
 
 - An overfull leaf node.   We split it.
 - An underfull leaf node.  We merge it with its neighbor (if there is such a neighbor).
 - An overfull nonleaf node.  
    We pick the heaviest child, and push all the messages from the node to the child.
    If that child is an overfull leaf node, then 


--------------------
*/

// Simple scheme with no eager promotions:
// 
// Insert_message_in_tree:
//  Put a message in a node.
//  handle_node_that_maybe_the_wrong_shape(node)
// 
// Handle_node_that_maybe_the_wrong_shape(node)
//  If the node is overfull
//    If it is a leaf node, split it.
//    else (it is an internal node),
//      pick the heaviest child, and push all that child's messages to that child.
//      Handle_node_that_maybe_the_wrong_shape(child)
//      If the node is now too fat:  
//        split the node
//      else if the node is now too thin:
//        merge the node
//      else do nothing
//  If the node is an underfull leaf:
//     merge the node (or distribute the data with its neighbor if the merge would be too big)
// 
// 
// Note: When nodes a merged,  they may become overfull.  But we just let it be overfull.
// 
// search()
//  To search a leaf node, we just do the lookup.
//  To search a nonleaf node:
//    Determine which child is the right one.
//    Push all data to that child.
//    search() the child, collecting whatever result is needed.
//    Then:
//      If the child is an overfull leaf or a too-fat nonleaf
//         split it
//      If the child is an underfull leaf or a too-thin nonleaf
//         merge it (or distribute the data with the neighbor if the merge would be too big.)
//    return from the search.
// 
// Note: During search we may end up with overfull nonleaf nodes.
// (Nonleaf nodes can become overfull because of merging two thin nodes
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
//     and the child is not overfull
//     then we place the message in the child (and we do this recursively, so we may place the message in the grandchild, or so forth.)
//       We simply leave any resulting overfull or underfull nodes alone, since the nodes can only be slightly overfull (and for underfullness we don't worry in this case.)
//   Otherewise put the message in the root and handle_node_that_is_maybe_the_wrong_shape().
// 
//       An even more aggresive promotion scheme would be to go ahead and insert the new message in the overfull child, and then do the splits and merges resulting from that child getting overfull.
// 
// */
// 
// 
#include "includes.h"
 																     
// We invalidate all the OMTCURSORS any time we push into the root of the BRT for that OMT.
// We keep a counter on each brt header, but if the brt header is evicted from the cachetable
// then we lose that counter.  So we also keep a global counter.
// An alternative would be to keep only the global counter.  But that would invalidate all OMTCURSORS
// even from unrelated BRTs.  This way we only invalidate an OMTCURSOR if
static u_int64_t global_root_put_counter = 0;

enum should_status { SHOULD_OK, SHOULD_MERGE, SHOULD_SPLIT };

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_counts(n), toku_verify_estimates(t,n))
#else
#define VERIFY_NODE(t,n) ((void)0)
#endif

static void
fixup_child_fingerprint (BRTNODE node, int childnum_of_node, BRTNODE child, BRT UU(brt), TOKULOGGER UU(logger))
// Effect:  Sum the child fingerprint (and leafentry estimates) and store them in NODE.
// Parameters:
//   node                The node to modify
//   childnum_of_node    Which child changed   (PERFORMANCE: Later we could compute this incrementally)
//   child               The child that changed.
//   brt                 The brt (not used now but it will be for logger)
//   logger              The logger (not used now but it will be for logger)
{
    u_int64_t leafentry_estimate = 0;
    u_int32_t sum = child->local_fingerprint;
    if (child->height>0) {
	int i;
	for (i=0; i<child->u.n.n_children; i++) {
	    sum += BNC_SUBTREE_FINGERPRINT(child,i);
	    leafentry_estimate += BNC_SUBTREE_LEAFENTRY_ESTIMATE(child,i);
	}
    } else {
	leafentry_estimate = toku_omt_size(child->u.l.buffer);
    }
    // Don't try to get fancy about not modifying the fingerprint if it didn't change.
    // We only call this function if we have reason to believe that the child's fingerprint did change.
    BNC_SUBTREE_FINGERPRINT(node,childnum_of_node)=sum;
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(node,childnum_of_node)=leafentry_estimate;
    node->dirty=1;
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

static int
allocate_diskblocknumber (BLOCKNUM *res, BRT brt, TOKULOGGER logger __attribute__((__unused__))) {
    assert(brt->h->free_blocks.b == -1); // no blocks in the free list
    BLOCKNUM result = brt->h->unused_blocks;
    brt->h->unused_blocks.b++;
    brt->h->dirty = 1;
    *res = result;
    return 0;
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
    n->layout_version = BRT_LAYOUT_VERSION;
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
	int r = toku_omt_create(&n->u.l.buffer);
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
    int new_nodesize = brt->h->nodesize;
    BLOCKNUM newroot_diskoff;
    r = allocate_diskblocknumber(&newroot_diskoff, brt, logger);
    assert(r==0);
    assert(newroot);
    newroot->ever_been_written = 0;
    if (brt->database_name==0) {
	toku_log_changeunnamedroot(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), *rootp, newroot_diskoff);
    } else {
	BYTESTRING bs;
	bs.len = 1+strlen(brt->database_name);
	bs.data = brt->database_name;
	toku_log_changenamedroot(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), bs, *rootp, newroot_diskoff);
    }
    *rootp=newroot_diskoff;
    brt->h->dirty=1;
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
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(newroot, 0)=0; 
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(newroot, 1)=0; 
    //verify_local_fingerprint_nonleaf(nodea);
    //verify_local_fingerprint_nonleaf(nodeb);
    r=toku_log_newbrtnode(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), newroot_diskoff, new_height, new_nodesize, (unsigned char)((brt->flags&TOKU_DB_DUPSORT)!=0), newroot->rand4fingerprint);
    if (r!=0) return r;
    r=toku_log_addchild(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, nodea->thisnodename, 0);
    if (r!=0) return r;
    r=toku_log_addchild(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), newroot_diskoff, 1, nodeb->thisnodename, 0);
    if (r!=0) return r;
    fixup_child_fingerprint(newroot, 0, nodea, brt, logger);
    fixup_child_fingerprint(newroot, 1, nodeb, brt, logger);
    {
	BYTESTRING bs = { .len = kv_pair_keylen(newroot->u.n.childkeys[0]),
			  .data = kv_pair_key(newroot->u.n.childkeys[0]) };
	r=toku_log_setpivot(logger, &newroot->log_lsn, 0, toku_cachefile_filenum(brt->cf), newroot_diskoff, 0, bs);
	if (r!=0) return r;
    }
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
    assert(0);
    return 0;
}

static int apply_cmd_to_le_committed (u_int32_t klen, void *kval,
				      u_int32_t dlen, void *dval,
				      BRT_CMD cmd,
				      u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data) {
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
	return le_both(cmd->xid,
		       klen, kval,
		       dlen, dval, 
		       cmd->u.id.val->size, cmd->u.id.val->data,
		       newlen, disksize, new_data);
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
	return le_provdel(cmd->xid,
			  klen, kval,
			  dlen, dval,
			  newlen, disksize, new_data);
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
	// Just return the original committed record
	return le_committed(klen, kval, dlen, dval,
			    newlen, disksize, new_data);
    case BRT_NONE: break;
    }
    assert(0);
    return 0;
}

static int apply_cmd_to_le_both (TXNID xid,
				 u_int32_t klen, void *kval,
				 u_int32_t clen, void *cval,
				 u_int32_t plen, void *pval,
				 BRT_CMD cmd,
				 u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data) {
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
		       newlen, disksize, new_data);
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
	return le_provdel(cmd->xid,
			  klen, kval,
			  prev_len, prev_val,
			  newlen, disksize, new_data);
    case BRT_ABORT_BOTH:
    case BRT_ABORT_ANY:
	// I don't see how you could have an abort where the xids don't match.  But do it anyway.
	return le_committed(klen, kval,
			    prev_len, prev_val,
			    newlen, disksize, new_data);
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
	// In the future we won't even have these commit messages.
	return le_committed(klen, kval,
			    plen, pval,
			    newlen, disksize, new_data);
    case BRT_NONE: break;
    }
    assert(0);
    return 0;
}

static int apply_cmd_to_le_provdel (TXNID xid,
				    u_int32_t klen, void *kval,
				    u_int32_t clen, void *cval,
				    BRT_CMD cmd,
				    u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data) {
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
			   newlen, disksize, new_data);
	} else {
	    // It's an insert, but the committed value is deleted (since the xids don't match, we assume the delete took effect)
	    return le_provpair(cmd->xid,
			       klen, kval,
			       cmd->u.id.val->size, cmd->u.id.val->data,
			       newlen, disksize, new_data);
	}
    case BRT_DELETE_ANY:
    case BRT_DELETE_BOTH:
	if (cmd->xid == xid) {
	    // A delete of a delete could conceivably return the identical value, saving a malloc and a free, but to simplify things we just reallocate it
	    // because othewise we have to notice not to free() the olditem.
	    return le_provdel(cmd->xid,
			      klen, kval,
			      clen, cval,
			      newlen, disksize, new_data);
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
			    newlen, disksize, new_data);
    case BRT_COMMIT_BOTH:
    case BRT_COMMIT_ANY:
	*new_data = 0;
	return 0;
    case BRT_NONE: break;
    }
    assert(0);
    return 0;
}

static int apply_cmd_to_le_provpair (TXNID xid,
				     u_int32_t klen, void *kval,
				     u_int32_t plen , void *pval,
				     BRT_CMD cmd,
				     u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data) {
    //assert(cmd->u.id.key->size == klen);
    //assert(memcmp(cmd->u.id.key->data, kval, klen)==0);
    switch (cmd->type) {
    case BRT_INSERT:
	if (cmd->xid == xid) {
	    // it's still a provpair (the old prov value is lost)
	    return le_provpair(cmd->xid,
			       klen, kval,
			       cmd->u.id.val->size, cmd->u.id.val->data,
			       newlen, disksize, new_data);
	} else {
	    // the old prov was actually committed.
	    return le_both(cmd->xid,
			   klen, kval,
			   plen, pval,
			   cmd->u.id.val->size, cmd->u.id.val->data,
			   newlen, disksize, new_data);
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
			      newlen, disksize, new_data);
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
			    newlen, disksize, new_data);
    case BRT_NONE: break;
    }
    assert(0);
    return 0;
}

static int
apply_cmd_to_leaf (BRT_CMD cmd,
		   void *stored_data, // NULL if there was no stored data.
		   u_int32_t *newlen, u_int32_t *disksize, LEAFENTRY *new_data)
{
    if (stored_data==0) {
	switch (cmd->type) {
	case BRT_INSERT:
	    {
		LEAFENTRY le;
		int r = le_provpair(cmd->xid,
				    cmd->u.id.key->size, cmd->u.id.key->data,
				    cmd->u.id.val->size, cmd->u.id.val->data,
				    newlen, disksize, &le);
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
	assert(0);
	return 0;
    } else {
	LESWITCHCALL(stored_data, apply_cmd_to, cmd,
		     newlen, disksize, new_data);
    }
    abort(); return 0; // make certain compilers happy    
}

static int
brt_leaf_apply_cmd_once (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger,
			 u_int32_t idx, LEAFENTRY le)
// Effect: Apply cmd to leafentry
//   idx is the location where it goes
//   le is old leafentry
{
    FILENUM filenum = toku_cachefile_filenum(t->cf);
    u_int32_t newlen=0, newdisksize=0;
    LEAFENTRY newdata=0;
    int r = apply_cmd_to_leaf(cmd, le, &newlen, &newdisksize, &newdata);
    if (r!=0) return r;
    if (newdata) assert(newdisksize == leafentry_disksize(newdata));
    
    //printf("Applying command: %s xid=%lld ", unparse_cmd_type(cmd->type), (long long)cmd->xid);
    //toku_print_BYTESTRING(stdout, cmd->u.id.key->size, cmd->u.id.key->data);
    //printf(" ");
    //toku_print_BYTESTRING(stdout, cmd->u.id.val->size, cmd->u.id.val->data);
    //printf(" to \n");
    //print_leafentry(stdout, le); printf("\n");
    //printf(" got "); print_leafentry(stdout, newdata); printf("\n");

    if (le && newdata) {
	if (t->txn_that_created != cmd->xid) {
	    if ((r = toku_log_deleteleafentry(logger, &node->log_lsn, 0, filenum, node->thisnodename, idx))) return r;
	    if ((r = toku_log_insertleafentry(logger, &node->log_lsn, 0, toku_cachefile_filenum(t->cf), node->thisnodename, idx, newdata))) return r;
	}

	node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(le);
	node->local_fingerprint     -= node->rand4fingerprint * toku_le_crc(le);
	
	u_int32_t size = leafentry_memsize(le);

	LEAFENTRY new_le = mempool_malloc_from_omt(node->u.l.buffer, &node->u.l.buffer_mempool, newlen);
	assert(new_le);
	memcpy(new_le, newdata, newlen);

	// This mfree must occur after the mempool_malloc so that when the mempool is compressed everything is accounted for.
	// But we must compute the size before doing the mempool malloc because otherwise the le pointer is no good.
	toku_mempool_mfree(&node->u.l.buffer_mempool, 0, size); // Must pass 0, since le may be no good any more.
	
	node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + newdisksize;
	node->local_fingerprint += node->rand4fingerprint*toku_le_crc(newdata);
	toku_free(newdata);

	if ((r = toku_omt_set_at(node->u.l.buffer, new_le, idx))) return r;

    } else {
	if (le) {
	    // It's there, note that it's gone and remove it from the mempool

	    if (t->txn_that_created != cmd->xid) {
		if ((r = toku_log_deleteleafentry(logger, &node->log_lsn, 0, filenum, node->thisnodename, idx))) return r;
	    }

	    if ((r = toku_omt_delete_at(node->u.l.buffer, idx))) return r;

	    node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(le);
	    node->local_fingerprint     -= node->rand4fingerprint * toku_le_crc(le);

	    toku_mempool_mfree(&node->u.l.buffer_mempool, 0, leafentry_memsize(le)); // Must pass 0, since le may be no good any more.

	}
	if (newdata) {
	    LEAFENTRY new_le = mempool_malloc_from_omt(node->u.l.buffer, &node->u.l.buffer_mempool, newlen);
	    assert(new_le);
	    memcpy(new_le, newdata, newlen);
	    if ((r = toku_omt_insert_at(node->u.l.buffer, new_le, idx))) return r;

	    if (t->txn_that_created != cmd->xid) {
		if ((r = toku_log_insertleafentry(logger, &node->log_lsn, 0, toku_cachefile_filenum(t->cf), node->thisnodename, idx, newdata))) return r;
	    }

	    node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + newdisksize;
	    node->local_fingerprint += node->rand4fingerprint*toku_le_crc(newdata);
	    toku_free(newdata);
	}
    }
//	printf("%s:%d rand4=%08x local_fingerprint=%08x this=%08x\n", __FILE__, __LINE__, node->rand4fingerprint, node->local_fingerprint, toku_calccrc32_kvpair_struct(kv));
    return 0;
}

static int
brt_leaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger,
		  u_int64_t *new_size /*OUT*/
		  )
// Effect: Put a cmd into a leaf.
//  Return the serialization size in *new_size.
// The leaf could end up "too big".  It is up to the caller to fix that up.
{
//    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
    VERIFY_NODE(t, node);
    assert(node->height==0);

    LEAFENTRY storeddata;
    OMTVALUE storeddatav=NULL;

    u_int32_t idx;
    int r;
    int compare_both = should_compare_both_keys(node, cmd);
    struct cmd_leafval_bessel_extra be = {t, cmd, compare_both};

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
            int cmp = toku_cmd_leafval_bessel(storeddata, &be);
            if (cmp >= 0) goto fz;
            r = DB_NOTFOUND;
        } else {
        fz:
            r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel, &be,
                                   &storeddatav, &idx, NULL);
        }
	if (r==DB_NOTFOUND) {
	    storeddata = 0;
	} else if (r!=0) {
	    return r;
	} else {
	    storeddata=storeddatav;
	}
	
	r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	if (r!=0) return r;

        // if the insertion point is within a window of the right edge of
        // the leaf then it is sequential

        // window = min(32, number of leaf entries/16)
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
	break;
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:

	// Delete the one item
	r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel,  &be,
			       &storeddatav, &idx, NULL);
	if (r == DB_NOTFOUND) break;
	if (r != 0) return r;
	storeddata=storeddatav;

	VERIFY_NODE(t, node);

	static int count=0;
	count++;
	r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	if (r!=0) return r;

	VERIFY_NODE(t, node);
	break;

    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
	// Delete all the matches

	r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel, &be,
			       &storeddatav, &idx, NULL);
	if (r == DB_NOTFOUND) break;
	if (r != 0) return r;
	storeddata=storeddatav;
	    
	while (1) {
	    int   vallen   = le_any_vallen(storeddata);
	    void *save_val = toku_memdup(le_any_val(storeddata), vallen);

	    r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	    if (r!=0) return r;

	    // Now we must find the next one.
	    DBT valdbt;
	    BRT_CMD_S ncmd = { cmd->type, cmd->xid, .u.id={cmd->u.id.key, toku_fill_dbt(&valdbt, save_val, vallen)}};
	    struct cmd_leafval_bessel_extra nbe = {t, &ncmd, 1};
	    r = toku_omt_find(node->u.l.buffer, toku_cmd_leafval_bessel,  &nbe, +1,
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
	
//	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);

    VERIFY_NODE(t, node);
    *new_size = toku_serialize_brtnode_size(node);
    return 0;
}

static int
brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger, enum should_status *should, int *io_count)
// Effect: Push CMD into the subtree rooted at NODE, and indicate whether as a result NODE should split or should merge.
//   If NODE is a leaf, then
//      put CMD into leaf, applying it to the leafentries
//   If NODE is a nonleaf, then copy the cmd into the relevant child fifos.
//      For each child fifo that is empty and where the child is in main memory put the command into the child (using this same algorithm)
//      Use *io_count to determine whether I/O has already been performed.  Once I/O has occured, we avoid additional I/O by leaving nodes that are overfull, underfull, overfat, or underfat.
//   Set *should as follows:
//                { SHOULD_SPLIT if the node is overfull
//      *should = { SHOULD_MERGE if the node is underfull
//                { SHOULD_OK    if the node is ok.   (Those cases are mutually exclusive.)
//   For every I/O increment *io_count
{
    if (node->height==0) {
	int r;
	u_int64_t new_size MAYBE_INIT(0);
	r = brt_leaf_put_cmd(t, node, cmd, logger, &new_size);
	if (r!=0) return r;
	if (new_size > node->nodesize)          *should = SHOULD_SPLIT;
	else if ((new_size*4) < node->nodesize) *should = SHOULD_MERGE;
	else                                    *should = SHOULD_OK;
    } else {
	int r;
	u_int32_t new_fanout = 0; // Some compiler bug in gcc is complaining that this is uninitialized.
	r = brt_nonleaf_put_cmd(t, node, cmd, logger, &new_fanout, io_count);
	if (r!=0) return 0;
	if (new_fanout > TREE_FANOUT)        *should = SHOULD_SPLIT;
	else if (new_fanout*4 < TREE_FANOUT) *should = SHOULD_MERGE;
	else                                 *should = SHOULD_OK;
    }
    return 0;
}

static int push_something_at_root (BRT brt, BRTNODE *nodep, CACHEKEY *rootp, BRT_CMD cmd, TOKULOGGER logger)
// Effect:  Put CMD into brt by descending into the tree as deeply as we can
//   without performing I/O (but we must fetch the root), 
//   bypassing only empty FIFOs
//   If the cmd is a broadcast message, we copy the message as needed as we descend the tree so that each relevant subtree receives the message.
//   At the end of the descent, we are either at a leaf, or we hit a nonempty FIFO.
//     If it's a leaf, and the leaf is overfull or underfull, then we split the leaf or merge it with the neighbor.
//      Note: for split operations, no disk I/O is required.  For merges, I/O may be required, so for a broadcast delete, quite a bit
//       of I/O could be induced in the worst case.
//     If it's a nonleaf, and the node is overfull or underfull, then we flush everything in the heaviest fifo to the child.
//       During flushing, we allow the child to become overfull. 
//         (And for broadcast messages, we simply place the messages into all the relevant fifos of the child, rather than trying to descend.)
//       After flushing to a child, if the child is overfull (underful), then
//           if the child is leaf, we split (merge) it
//           if the child is a nonleaf, we flush the heaviest child recursively.
//       Note: After flushing, a node could still be overfull (or possibly underfull.)  We let it remain so.
//       Note: During the initial descent, we may overfull many nonleaf nodes.  We wish to flush only one nonleaf node at each level.
{
    BRTNODE node = *nodep;
    enum should_status should;
    BOOL   did_io = FALSE;
    BOOL should_split =-1;
    BOOL should_merge =-1;
    {
	int r = brtnode_put_cmd(brt, node, cmd, logger, &should, &did_io);
	if (r!=0) return r;
	//if (should_split) printf("%s:%d Pushed something simple, should_split=1\n", __FILE__, __LINE__); 

    }
    assert(should_split!=(BOOL)-1 && should_merge!=(BOOL)-1);
    assert(!(should_split && should_merge));
    //printf("%s:%d should_split=%d node_size=%" PRIu64 "\n", __FILE__, __LINE__, should_split, brtnode_memory_size(node));

    if (should_split) {
	// The root node should split, so make a new root.
	BRTNODE nodea,nodeb;
	DBT splitk;
	if (node->height==0) {
	    int r = brtleaf_split(logger, toku_cachefile_filenum(brt->cf), brt, node, &nodea, &nodeb, &splitk);
	    if (r!=0) return r;
	} else {
	    int r = brt_nonleaf_split(brt, node, &nodea, &nodeb, &splitk, logger);
	    if (r!=0) return r;
	}
	return brt_init_new_root(brt, nodea, nodeb, splitk, rootp, logger, nodep);
    } else if (should_merge) {
	return 0; // Cannot merge anything at the root, so return happy.
    } else {
	return 0;
    }
}


int toku_brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger)
// Effect:  Flush the root fifo into the brt, and then push the cmd into the brt.
{
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

    if ((r = push_something_at_root(brt, &node, rootp, cmd, logger))) return r;
    r = toku_unpin_brtnode(brt, node);
    assert(r == 0);
    return 0;
}

int toku_brt_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn)
// Effect: Insert the key-val pair into brt.
{
    int r;
    if (txn && (brt->txn_that_created != toku_txn_get_txnid(txn))) {
	toku_cachefile_refup(brt->cf);
	BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
	BYTESTRING databs = {val->size, toku_memdup_in_rollback(txn, val->data, val->size)};
	r = toku_logger_save_rollback_cmdinsert(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs, databs);
	if (r!=0) return r;
	r = toku_txn_note_brt(txn, brt);
	if (r!=0) return r;
    }
    BRT_CMD_S brtcmd = { BRT_INSERT, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    if (r!=0) return r;
    return r;
}
 
