/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


/*

Managing the tree shape:  How insertion, deletion, and querying work

When we insert a message into the BRT, here's what happens.

Insert_a_message_at_root (msg)
   root = find the root
   insert_the_message_into_the_buffers_of(msg, root)
   If the root is way too full then process the root ourself.  "Way too full" means something like twice as much messages as it's supposed to have.
   else If the root needs to be split, then split it 
   else if the root's buffers are too full then (it must be a nonleaf)
      create a work item to process the root.	The workitem specifies a height and a key (the height is the height of the root, and the key can be any key)
   endif
   If the brt file is fragmented, and the file isn't being shrunk, then set file->being_shrunk and schedule a work item to shrink the file.

To process a nonleaf node (height, key)
   Note: Height is always > 0.
   Note: This process occurs asynchrnously, but we get the YDB lock at the beginning.
   Descend the tree following KEY until	 a node of HEIGHT is found.
      While the node is too full then 
	 pick the heaviest child
	 bring that child into memory (use nonblocking get_and_pin, which means that if we get a try-again, we go back up and restart the process_a_node job.
	 move all messages for that child from the node to the child.
	 If the child needs to be split or merged, then split or merge the child.
	 If the resulting child's (or children's) buffers are too full then create a work item for each such child to process the child.  (This can only happen
	       for nonleaf children, since otherwise there are no buffers to be too full).

We also have a background thread that traverses the tree (relatively slowly) to flatten the tree.
Background_flattener:
   It's state is a height and a key and a child number
   Repeat:
      sleep (say 1s)
      grab the ydb lock
      descend the tree to find the height and key
      while the node is not empty:
	 bring the child into memory (possibly causing a TRY_AGAIN)
	 move all messages from the node into the child
	 if the child needs to be split or merged then split or merge the child
	 set the state to operate on the next relevant node in the depth-first order
	   That is: if there are more children, increment the child number, and return.
		    if there are no more children, then return with an error code that says "next".  At the first point at the descent is not to the ultimate
			     child, then set the state to visit that node and that child.  
		    if we get back up to the root then the state goes to "root" and "child 0" so the whole background flattener can run again the next BRT.   
		      Probably only open BRTs get flattened.
		      It may be important for the flattener not to run if there've been no message insertions since the last time it ran.
      The background flattener should also garbage collect MVCC versions.  The flattener should remember the MVCC versions it has encountered
	so that if any of those are no longer live, it can run again.
			 

To shrink a file: Let X be the size of the reachable data.  
    We define an acceptable bloat constant of C.  For example we set C=2 if we are willing to allow the file to be as much as 2X in size.
    The goal is to find the smallest amount of stuff we can move to get the file down to size CX.
    That seems like a difficult problem, so we use the following heuristics:
       If we can relocate the last block to an lower location, then do so immediately.	(The file gets smaller right away, so even though the new location
	 may even not be in the first CX bytes, we are making the file smaller.)
       Otherwise all of the earlier blocks are smaller than the last block (of size L).	 So find the smallest region that has L free bytes in it.
	 (This can be computed in one pass)
	 Move the first allocated block in that region to some location not in the interior of the region.
	       (Outside of the region is OK, and reallocating the block at the edge of the region is OK).
	    This has the effect of creating a smaller region with at least L free bytes in it.
	 Go back to the top (because by now some other block may have been allocated or freed).
    Claim: if there are no other allocations going on concurrently, then this algorithm will shrink the file reasonably efficiently.  By this I mean that
       each block of shrinkage does the smallest amount of work possible.  That doesn't mean that the work overall is minimized.
    Note: If there are other allocations and deallocations going on concurrently, we might never get enough space to move the last block.  But it takes a lot
      of allocations and deallocations to make that happen, and it's probably reasonable for the file not to shrink in this case.

To split or merge a child of a node:
Split_or_merge (node, childnum) {
  If the child needs to be split (it's a leaf with too much stuff or a nonleaf with too much fanout)
    fetch the node and the child into main memory.
    split the child, producing two nodes A and B, and also a pivot.   Don't worry if the resulting child is still too big or too small.	 Fix it on the next pass.
    fixup node to point at the two new children.  Don't worry about the node getting too much fanout.
    return;
  If the child needs to be merged (it's a leaf with too little stuff (less than 1/4 full) or a nonleaf with too little fanout (less than 1/4)
    fetch node, the child  and a sibling of the child into main memory.
    move all messages from the node to the two children (so that the FIFOs are empty)
    If the two siblings together fit into one node then
      merge the two siblings. 
      fixup the node to point at one child
    Otherwise
      load balance the content of the two nodes
    Don't worry about the resulting children having too many messages or otherwise being too big or too small.	Fix it on the next pass.
  }
}


Lookup:
 As of #3312, we don't do any tree shaping on lookup.
 We don't promote eagerly or use aggressive promotion or passive-aggressive promotion.	We just push messages down according to the traditional BRT algorithm
  on insertions.
 For lookups, we maintain the invariant that the in-memory leaf nodes have a soft copy which reflects all the messages above it in the tree.
 So when a leaf node is brought into memory, we apply all messages above it.
 When a message is inserted into the tree, we apply it to all the leaf nodes to which it is applicable.
 When flushing to a leaf, we flush to the hard copy not to the soft copy.
*/

#include "includes.h"
#include "checkpoint.h"
#include "mempool.h"
// Access to nested transaction logic
#include "ule.h"
#include "xids.h"
#include "roll.h"
#include "sub_block.h"
#include "sort.h"

#if defined(HAVE_CILK)
#include <cilk/cilk.h>
#define cilk_worker_count (__cilkrts_get_nworkers())
#else
#define cilk_spawn
#define cilk_sync
#define cilk_for for
#define cilk_worker_count 1
#endif

static const uint32_t this_version = BRT_LAYOUT_VERSION;

static BRT_STATUS_S brt_status;

void 
toku_brt_get_status(BRT_STATUS s) {
    *s = brt_status;
}

void
toku_brt_header_suppress_rollbacks(struct brt_header *h, TOKUTXN txn) {
    TXNID txnid = toku_txn_get_txnid(txn);
    assert(h->txnid_that_created_or_locked_when_empty == TXNID_NONE ||
	   h->txnid_that_created_or_locked_when_empty == txnid);
    h->txnid_that_created_or_locked_when_empty = txnid;
    TXNID rootid = toku_txn_get_root_txnid(txn);
    assert(h->root_that_created_or_locked_when_empty == TXNID_NONE ||
	   h->root_that_created_or_locked_when_empty == rootid);
    h->root_that_created_or_locked_when_empty  = rootid;
}


enum reactivity { RE_STABLE, RE_FUSIBLE, RE_FISSIBLE };

void
toku_assert_entire_node_in_memory(BRTNODE node) {
    for (int i = 0; i < node->n_children; i++) {
        assert(BP_STATE(node,i) == PT_AVAIL);
    }
}

static u_int32_t
get_leaf_num_entries(BRTNODE node) {
    u_int32_t result = 0;
    int i;
    toku_assert_entire_node_in_memory(node);
    for ( i = 0; i < node->n_children; i++) {
        result += toku_omt_size(BLB_BUFFER(node, i));
    }
    return result;
}

static enum reactivity
get_leaf_reactivity (BRTNODE node) {
    enum reactivity re = RE_STABLE;
    assert(node->height==0);
    if (node->dirty) {
	unsigned int size = toku_serialize_brtnode_size(node);
	if (size > node->nodesize && get_leaf_num_entries(node) > 1) {
	    re = RE_FISSIBLE;
	}
	else if ((size*4) < node->nodesize && !BLB_SEQINSERT(node, node->n_children-1)) {
	    re = RE_FUSIBLE;
	}
    }
    return re;
}

static enum reactivity
get_nonleaf_reactivity (BRTNODE node) {
    assert(node->height>0);
    int n_children = node->n_children;
    if (n_children > TREE_FANOUT) return RE_FISSIBLE;
    if (n_children*4 < TREE_FANOUT) return RE_FUSIBLE;
    return RE_STABLE;
}

static enum reactivity
get_node_reactivity (BRTNODE node) {
    toku_assert_entire_node_in_memory(node);
    if (node->height==0)
	return get_leaf_reactivity(node);
    else
	return get_nonleaf_reactivity(node);
}

unsigned int
toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc)
{
    return bnc->n_bytes_in_buffer;
}

// return TRUE if the size of the buffers plus the amount of work done is large enough.   (But return false if there is nothing to be flushed (the buffers empty)).
static bool
nonleaf_node_is_gorged (BRTNODE node) {
    u_int64_t size = toku_serialize_brtnode_size(node);

    bool buffers_are_empty = TRUE;
    toku_assert_entire_node_in_memory(node);
    //
    // the nonleaf node is gorged if the following holds true:
    //  - the buffers are non-empty
    //  - the total workdone by the buffers PLUS the size of the buffers
    //     is greater than node->nodesize (which as of Maxwell should be
    //     4MB)
    //
    assert(node->height > 0);
    for (int child = 0; child < node->n_children; ++child) {
        size += BP_WORKDONE(node, child);
    }
    for (int child = 0; child < node->n_children; ++child) {
        if (toku_bnc_nbytesinbuf(BNC(node, child)) > 0) {
            buffers_are_empty = FALSE;
            break;
        }
    }
    return ((size > node->nodesize)
            &&
            (!buffers_are_empty));
}

static inline void add_to_brt_status(u_int64_t* val, u_int64_t data) {
    (*val) += data;
}

static void brtnode_put_cmd (
    brt_compare_func compare_fun,
    brt_update_func update_fun,
    DESCRIPTOR desc,
    BRTNODE node, 
    BRT_MSG cmd, 
    bool is_fresh, 
    OMT snapshot_txnids, 
    OMT live_list_reverse
    );


static void
flush_this_child (struct brt_header* h, BRTNODE node, BRTNODE child, int childnum, bool started_at_root);

static void brt_verify_flags(BRT brt, BRTNODE node) {
    assert(brt->flags == node->flags);
}

int toku_brt_debug_mode = 0;

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_or_set_counts(n), toku_verify_estimates(t,n))
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
    assert(node->height>0 && childnum<node->n_children);
    return toku_cachetable_hash(cf, BP_BLOCKNUM(node, childnum));
}

//
// The intent of toku_pin_brtnode(_holding_lock) is to abstract the process of retrieving a node from
// the rest of brt.c, so that there is only one place where we need to worry applying ancestor 
// messages to a leaf node. The idea is for all of brt.c (search, splits, merges, flushes, etc)
// to access a node via toku_pin_brtnode(_holding_lock)
//
int toku_pin_brtnode (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
                      UNLOCKERS unlockers,
                      ANCESTORS ancestors, struct pivot_bounds const * const bounds,
                      struct brtnode_fetch_extra *bfe,
                      BOOL apply_ancestor_messages, // this BOOL is probably temporary, for #3972, once we know how range query estimates work, will revisit this
                      BRTNODE *node_p) {
    void *node_v;
    int r = toku_cachetable_get_and_pin_nonblocking(
            brt->cf, 
            blocknum, 
            fullhash, 
            &node_v, 
            NULL, 
            toku_brtnode_flush_callback, 
            toku_brtnode_fetch_callback, 
            toku_brtnode_pe_est_callback,
            toku_brtnode_pe_callback,
            toku_brtnode_pf_req_callback,
            toku_brtnode_pf_callback,
            toku_brtnode_cleaner_callback,
            bfe, //read_extraargs 
            brt->h, //write_extraargs
            unlockers);
    if (r==0) {
        BRTNODE node = node_v;
        if (apply_ancestor_messages) {
            maybe_apply_ancestors_messages_to_node(brt, node, ancestors, bounds);
        }
        *node_p = node;
        // printf("%*sPin %ld\n", 8-node->height, "", blocknum.b);
    } else {
        assert(r==TOKUDB_TRY_AGAIN); // Any other error and we should bomb out ASAP.
        // printf("%*sPin %ld try again\n", 8, "", blocknum.b);
    }
    return r;
}

// see comments for toku_pin_brtnode
void toku_pin_brtnode_holding_lock (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
				   ANCESTORS ancestors, struct pivot_bounds const * const bounds,
                                   struct brtnode_fetch_extra *bfe, 
                                   BOOL apply_ancestor_messages, // this BOOL is probably temporary, for #3972, once we know how range query estimates work, will revisit this
				   BRTNODE *node_p) {
    void *node_v;
    int r = toku_cachetable_get_and_pin(
        brt->cf, 
        blocknum, 
        fullhash, 
        &node_v, 
        NULL, 
        toku_brtnode_flush_callback, 
        toku_brtnode_fetch_callback, 
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        toku_brtnode_cleaner_callback,
        bfe,
        brt->h
        );
    assert(r==0);
    BRTNODE node = node_v;
    if (apply_ancestor_messages) maybe_apply_ancestors_messages_to_node(brt, node, ancestors, bounds);
    *node_p = node;
}

static void toku_pin_brtnode_off_client_thread (
    struct brt_header* h, 
    BLOCKNUM blocknum, 
    u_int32_t fullhash,
    struct brtnode_fetch_extra *bfe,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes,
    BRTNODE *node_p
    ) 
{
    void *node_v;
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }
    
    int r = toku_cachetable_get_and_pin_with_dep_pairs(
        h->cf, 
        blocknum, 
        fullhash, 
        &node_v, 
        NULL, 
        toku_brtnode_flush_callback, 
        toku_brtnode_fetch_callback, 
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        toku_brtnode_cleaner_callback,
        bfe,
        h,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits
        );
    assert(r==0);
    BRTNODE node = node_v;
    *node_p = node;
}

static void checkpoint_nodes(struct brt_header* h, u_int32_t num_dependent_nodes, BRTNODE* dependent_nodes)
{
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }
    toku_checkpoint_pairs(
        h->cf,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits
        );
}

static void toku_unpin_brtnode_off_client_thread (struct brt_header* h, BRTNODE node) 
// Effect: Unpin a brt node.
{
    int r = toku_cachetable_unpin(
        h->cf, 
        node->thisnodename, 
        node->fullhash, 
        (enum cachetable_dirty) node->dirty, 
        make_brtnode_pair_attr(node)
        );
    assert(r==0);
}


void toku_unpin_brtnode (BRT brt, BRTNODE node) 
// Effect: Unpin a brt node.
{
    // printf("%*sUnpin %ld\n", 8-node->height, "", node->thisnodename.b);
    VERIFY_NODE(brt,node);
    toku_unpin_brtnode_off_client_thread(brt->h, node);
}

// TODO: (Zardosht) look into this and possibly fix and use
static void __attribute__((__unused__))
brt_leaf_check_leaf_stats (BRTNODE node)
{
    assert(node);
    assert(FALSE);
    // static int count=0; count++;
    // if (node->height>0) return;
    // struct subtree_estimates e = calc_leaf_stats(node);
    // assert(e.ndata == node->u.l.leaf_stats.ndata);
    // assert(e.nkeys == node->u.l.leaf_stats.nkeys);
    // assert(e.dsize == node->u.l.leaf_stats.dsize);
    // assert(node->u.l.leaf_stats.exact);
}

int
toku_bnc_n_entries(NONLEAF_CHILDINFO bnc)
{
    return toku_fifo_n_entries(bnc->buffer);
}

static struct kv_pair const *prepivotkey (BRTNODE node, int childnum, struct kv_pair const * const lower_bound_exclusive) {
    if (childnum==0)
	return lower_bound_exclusive;
    else {
	return node->childkeys[childnum-1];
    }
}

static struct kv_pair const *postpivotkey (BRTNODE node, int childnum, struct kv_pair const * const upper_bound_inclusive) {
    if (childnum+1 == node->n_children)
	return upper_bound_inclusive;
    else {
	return node->childkeys[childnum];
    }
}
static struct pivot_bounds next_pivot_keys (BRTNODE node, int childnum, struct pivot_bounds const * const old_pb) {
    struct pivot_bounds pb = {.lower_bound_exclusive = prepivotkey(node, childnum, old_pb->lower_bound_exclusive),
			      .upper_bound_inclusive = postpivotkey(node, childnum, old_pb->upper_bound_inclusive)};
    return pb;
}

static LEAFENTRY
fetch_from_buf (OMT omt, u_int32_t idx) {
    OMTVALUE v = 0;
    int r = toku_omt_fetch(omt, idx, &v);
    assert_zero(r);
    return (LEAFENTRY)v;
}

// how much memory does this child buffer consume?
long  
toku_bnc_memory_size(NONLEAF_CHILDINFO bnc)
{
    return (sizeof(*bnc) +
            toku_fifo_memory_footprint(bnc->buffer) +
            toku_omt_memory_size(bnc->fresh_message_tree) +
            toku_omt_memory_size(bnc->stale_message_tree) +
            toku_omt_memory_size(bnc->broadcast_list));
}

// how much memory in this child buffer holds useful data?
// originally created solely for use by test program(s).
long
toku_bnc_memory_used(NONLEAF_CHILDINFO bnc)
{
    return (sizeof(*bnc) +
            toku_fifo_memory_size_in_use(bnc->buffer) +
            toku_omt_memory_size(bnc->fresh_message_tree) +
            toku_omt_memory_size(bnc->stale_message_tree) +
            toku_omt_memory_size(bnc->broadcast_list));
}

static long
get_avail_internal_node_partition_size(BRTNODE node, int i)
{
    assert(node->height > 0);
    return toku_bnc_memory_size(BNC(node, i));
}


static long 
brtnode_cachepressure_size(BRTNODE node)
{
    long retval = 0;
    bool totally_empty = true;
    if (node->height == 0) {
        goto exit;
    }
    else {
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node,i) == PT_INVALID || BP_STATE(node,i) == PT_ON_DISK) {
                continue;
            }
            else if (BP_STATE(node,i) == PT_COMPRESSED) {
                SUB_BLOCK sb = BSB(node, i);
                totally_empty = false;
                retval += sb->compressed_size;
            }
            else if (BP_STATE(node,i) == PT_AVAIL) {
                totally_empty = totally_empty && (toku_bnc_n_entries(BNC(node, i)) == 0);
                retval += get_avail_internal_node_partition_size(node, i);
                retval += BP_WORKDONE(node, i);
            }
            else {
                assert(FALSE);
            }
        }
    }
exit:
    if (totally_empty) {
        return 0;
    }
    return retval;
}

long
brtnode_memory_size (BRTNODE node)
// Effect: Estimate how much main memory a node requires.
{
    long retval = 0;
    int n_children = node->n_children;
    retval += sizeof(*node);
    retval += (n_children)*(sizeof(node->bp[0]));
    retval += node->totalchildkeylens;

    // now calculate the sizes of the partitions
    for (int i = 0; i < n_children; i++) {
        if (BP_STATE(node,i) == PT_INVALID || BP_STATE(node,i) == PT_ON_DISK) {
            continue;
        }
        else if (BP_STATE(node,i) == PT_COMPRESSED) {
            SUB_BLOCK sb = BSB(node, i);
            retval += sizeof(*sb);
            retval += sb->compressed_size;
        }
        else if (BP_STATE(node,i) == PT_AVAIL) {
            if (node->height > 0) {
                retval += get_avail_internal_node_partition_size(node, i);
            }
            else {
                BASEMENTNODE bn = BLB(node, i);
                retval += sizeof(*bn);
		{
		    // include fragmentation overhead but do not include space in the 
		    // mempool that has not yet been allocated for leaf entries
		    size_t poolsize = toku_mempool_footprint(&bn->buffer_mempool);  
		    invariant (poolsize >= BLB_NBYTESINBUF(node,i));
		    retval += poolsize;
		}
                OMT curr_omt = BLB_BUFFER(node, i);
                retval += (toku_omt_memory_size(curr_omt));
            }
        }
        else {
            assert(FALSE);
        }
    }
    return retval;
}

PAIR_ATTR make_brtnode_pair_attr(BRTNODE node) { 
    long size = brtnode_memory_size(node);
    long cachepressure_size = brtnode_cachepressure_size(node);
    PAIR_ATTR result={
     .size = size, 
     .nonleaf_size = (node->height > 0) ? size : 0, 
     .leaf_size = (node->height > 0) ? 0 : size, 
     .rollback_size = 0, 
     .cache_pressure_size = cachepressure_size
    }; 
    return result; 
}



// assign unique dictionary id
static uint64_t dict_id_serial = 1;
static DICTIONARY_ID
next_dict_id(void) {
    uint64_t i = __sync_fetch_and_add(&dict_id_serial, 1);
    assert(i);	// guarantee unique dictionary id by asserting 64-bit counter never wraps
    DICTIONARY_ID d = {.dictid = i};
    return d;
}

//
// Given a bfe and a childnum, returns whether the query that constructed the bfe 
// wants the child available.
// Requires: bfe->child_to_read to have been set
//
bool 
toku_bfe_wants_child_available (struct brtnode_fetch_extra* bfe, int childnum)
{
    if (bfe->type == brtnode_fetch_all || 
        (bfe->type == brtnode_fetch_subset && bfe->child_to_read == childnum))
    {
        return true;
    }
    else {
        return false;
    }
}

int
toku_bfe_leftmost_child_wanted(struct brtnode_fetch_extra *bfe, BRTNODE node)
{
    lazy_assert(bfe->type == brtnode_fetch_subset || bfe->type == brtnode_fetch_prefetch);
    if (bfe->left_is_neg_infty) {
        return 0;
    } else if (bfe->range_lock_left_key == NULL) {
        return -1;
    } else {
        return toku_brtnode_which_child(node, bfe->range_lock_left_key, &bfe->h->descriptor, bfe->h->compare_fun);
    }
}

int
toku_bfe_rightmost_child_wanted(struct brtnode_fetch_extra *bfe, BRTNODE node)
{
    lazy_assert(bfe->type == brtnode_fetch_subset || bfe->type == brtnode_fetch_prefetch);
    if (bfe->right_is_pos_infty) {
        return node->n_children - 1;
    } else if (bfe->range_lock_right_key == NULL) {
        return -1;
    } else {
        return toku_brtnode_which_child(node, bfe->range_lock_right_key, &bfe->h->descriptor, bfe->h->compare_fun);
    }
}

static int
brt_cursor_rightmost_child_wanted(BRT_CURSOR cursor, BRT brt, BRTNODE node)
{
    if (cursor->right_is_pos_infty) {
        return node->n_children - 1;
    } else if (cursor->range_lock_right_key.data == NULL) {
        return -1;
    } else {
        return toku_brtnode_which_child(node, &cursor->range_lock_right_key, &brt->h->descriptor, brt->h->compare_fun);
    }
}

//fd is protected (must be holding fdlock)
void toku_brtnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *brtnode_v, void *extraargs, PAIR_ATTR size __attribute__((unused)), PAIR_ATTR* new_size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint) {
    struct brt_header *h = extraargs;
    BRTNODE brtnode = brtnode_v;
    assert(brtnode->thisnodename.b==nodename.b);
    int height = brtnode->height;
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (write_me) {
        if (!h->panic) { // if the brt panicked, stop writing, otherwise try to write it.
            toku_assert_entire_node_in_memory(brtnode);
            int n_workitems, n_threads;
            toku_cachefile_get_workqueue_load(cachefile, &n_workitems, &n_threads);
            int r = toku_serialize_brtnode_to(fd, brtnode->thisnodename, brtnode, h, n_workitems, n_threads, for_checkpoint);
            if (r) {
                if (h->panic==0) {
                    char *e = strerror(r);
                    int	  l = 200 + strlen(e);
                    char s[l];
                    h->panic=r;
                    snprintf(s, l-1, "While writing data to disk, error %d (%s)", r, e);
                    h->panic_string = toku_strdup(s);
                }
            }
        }
        if (height == 0) {  // statistics incremented only when disk I/O is done, so worth the threadsafe count
            if (for_checkpoint)
                brt_status.disk_flush_leaf_for_checkpoint++;
            else
                brt_status.disk_flush_leaf++;
        }
        else {
            if (for_checkpoint)
                brt_status.disk_flush_nonleaf_for_checkpoint++;
            else
                brt_status.disk_flush_nonleaf++;
        }
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    *new_size = make_brtnode_pair_attr(brtnode);
    if (!keep_me) {
        toku_brtnode_free(&brtnode);
    }
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
}


//fd is protected (must be holding fdlock)
int toku_brtnode_fetch_callback (CACHEFILE UU(cachefile), int fd, BLOCKNUM nodename, u_int32_t fullhash, 
				 void **brtnode_pv, PAIR_ATTR *sizep, int *dirtyp, void *extraargs) {
    assert(extraargs);
    assert(*brtnode_pv == NULL);
    struct brtnode_fetch_extra *bfe = (struct brtnode_fetch_extra *)extraargs;
    BRTNODE *result=(BRTNODE*)brtnode_pv;
    // deserialize the node, must pass the bfe in because we cannot evaluate what piece of the 
    // the node is necessary until we get it at least partially into memory
    int r = toku_deserialize_brtnode_from(fd, nodename, fullhash, result, bfe);
    if (r == 0) {
	*sizep = make_brtnode_pair_attr(*result);
	*dirtyp = (*result)->dirty;
    }
    return r;
}


void toku_brtnode_pe_est_callback(
    void* brtnode_pv, 
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    assert(brtnode_pv != NULL);
    long bytes_to_free = 0;
    BRTNODE node = (BRTNODE)brtnode_pv;
    if (node->dirty || node->height == 0) {
        *bytes_freed_estimate = 0;
        *cost = PE_CHEAP;
        goto exit;
    }

    //
    // we are dealing with a clean internal node
    //
    *cost = PE_EXPENSIVE;
    // now lets get an estimate for how much data we can free up
    // we estimate the compressed size of data to be how large
    // the compressed data is on disk
    for (int i = 0; i < node->n_children; i++) {
        if (BP_STATE(node,i) == PT_AVAIL && BP_SHOULD_EVICT(node,i)) {
            // calculate how much data would be freed if
            // we compress this node and add it to
            // bytes_to_free

            // first get an estimate for how much space will be taken 
            // after compression, it is simply the size of compressed
            // data on disk plus the size of the struct that holds it
            u_int32_t compressed_data_size = BP_SIZE(node, i);
            compressed_data_size += sizeof(struct sub_block);

            // now get the space taken now
            u_int32_t decompressed_data_size = get_avail_internal_node_partition_size(node,i);
            bytes_to_free += (decompressed_data_size - compressed_data_size);
        }
    }

    *bytes_freed_estimate = bytes_to_free;
exit:
    return;
}


static void
compress_internal_node_partition(BRTNODE node, int i)
{
    // if we should evict, compress the
    // message buffer into a sub_block
    assert(BP_STATE(node, i) == PT_AVAIL);
    assert(node->height > 0);
    SUB_BLOCK sb = NULL;
    sb = toku_xmalloc(sizeof(struct sub_block));
    sub_block_init(sb);
    toku_create_compressed_partition_from_available(node, i, sb);
    
    // now free the old partition and replace it with this
    destroy_nonleaf_childinfo(BNC(node,i));
    set_BSB(node, i, sb);
    BP_STATE(node,i) = PT_COMPRESSED;
}

// callback for partially evicting a node
int toku_brtnode_pe_callback (void *brtnode_pv, PAIR_ATTR UU(old_attr), PAIR_ATTR* new_attr, void* UU(extraargs)) {
    BRTNODE node = (BRTNODE)brtnode_pv;
    // 
    // nothing on internal nodes for now
    //
    if (node->dirty) {
        goto exit;
    }
    //
    // partial eviction for internal nodes
    //
    if (node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node,i) == PT_AVAIL) {
                if (BP_SHOULD_EVICT(node,i)) {
                    brt_status.partial_evictions_nonleaf++;
                    cilk_spawn compress_internal_node_partition(node, i);
                }
                else {
                    BP_SWEEP_CLOCK(node,i);
                }
            }
            else {
                continue;
            }
        }
        cilk_sync;
    }
    //
    // partial eviction strategy for basement nodes:
    //  if the bn is compressed, evict it
    //  else: check if it requires eviction, if it does, evict it, if not, sweep the clock count
    //
    else {
        for (int i = 0; i < node->n_children; i++) {
            // Get rid of compressed stuff no matter what.
            if (BP_STATE(node,i) == PT_COMPRESSED) {
                brt_status.partial_evictions_leaf++;
                SUB_BLOCK sb = BSB(node, i);
                toku_free(sb->compressed_ptr);
                toku_free(sb);
                set_BNULL(node, i);
                BP_STATE(node,i) = PT_ON_DISK;
            }
            else if (BP_STATE(node,i) == PT_AVAIL) {
                if (BP_SHOULD_EVICT(node,i)) {
                    brt_status.partial_evictions_leaf++;
                    // free the basement node
                    BASEMENTNODE bn = BLB(node, i);
                    struct mempool * mp = &bn->buffer_mempool;
                    toku_mempool_destroy(mp);
                    destroy_basement_node(bn);
                    set_BNULL(node,i);
                    BP_STATE(node,i) = PT_ON_DISK;
                }
                else {
                    BP_SWEEP_CLOCK(node,i);
                }
            }
            else if (BP_STATE(node,i) == PT_ON_DISK) {
                continue;
            }
            else {
                assert(FALSE);
            }
        }
    }

exit:
    *new_attr = make_brtnode_pair_attr(node);
    return 0;
}


// The parameter "started_at_root" is needed to resolve #4147 and #4160,
// which are subtle interactions of background flushing (cleaner and
// flusher threads) and MSN logic.
//
// When we rebalance basement nodes to write out a leaf, we can't have two
// basement nodes with different max_msn_applieds.  When we flush to a
// basement node, it may have stale ancestors' messages applied already.
//
// If we've flushed everything down from the root recursively, then there
// is no problem.  Anything that was applied to the leaf node by a query
// already must be in the batch of stuff we're flushing, so it's okay to
// do whatever we want, the MSNs will be consistent.
//
// But if we started somewhere in the middle (as a cleaner thread does),
// then we might not have all the messages that were applied to the leaf,
// and some basement nodes may be in a different state than others.  So
// before we flush to it, we have to destroy and re-read (off disk) the
// basement nodes which have messages applied.  Similarly, if a flush
// started in the middle wants to merge two leaf nodes, we can't do that
// because we might create a leaf node in a bad state.
//
// We use "started_at_root" to decide what to do about this problem in
// code further down.  For now, anything started by the cleaner thread
// will have started_at_root==false and anything started by the flusher
// thread will have started_at_root==true, but future mechanisms need to
// be mindful of this issue.
static void flush_some_child(struct brt_header* h, BRTNODE node, int *n_dirtied, int cascades, bool started_at_root);
static void bring_node_fully_into_memory(BRTNODE node, struct brt_header *h);

// TODO 3988 Leif set cleaner_nodes_dirtied
static void
update_cleaner_status(BRTNODE node, int childnum)
{
    brt_status.cleaner_total_nodes++;
    if (node->height == 1) {
        brt_status.cleaner_h1_nodes++;
    } else {
        brt_status.cleaner_hgt1_nodes++;
    }

    unsigned int nbytesinbuf = toku_bnc_nbytesinbuf(BNC(node, childnum));
    if (nbytesinbuf == 0) {
        brt_status.cleaner_empty_nodes++;
    } else {
        if (nbytesinbuf > brt_status.cleaner_max_buffer_size) {
            brt_status.cleaner_max_buffer_size = nbytesinbuf;
        }
        if (nbytesinbuf < brt_status.cleaner_min_buffer_size) {
            brt_status.cleaner_min_buffer_size = nbytesinbuf;
        }
        brt_status.cleaner_total_buffer_size += nbytesinbuf;

        uint64_t workdone = BP_WORKDONE(node, childnum);
        if (workdone > brt_status.cleaner_max_buffer_workdone) {
            brt_status.cleaner_max_buffer_workdone = workdone;
        }
        if (workdone < brt_status.cleaner_min_buffer_workdone) {
            brt_status.cleaner_min_buffer_workdone = workdone;
        }
        brt_status.cleaner_total_buffer_workdone += workdone;
    }
}

static void find_heaviest_child (BRTNODE node, int *childnum);

int
toku_brtnode_cleaner_callback(void *brtnode_pv, BLOCKNUM blocknum, u_int32_t fullhash, void *extraargs)
{
    BRTNODE node = brtnode_pv;
    invariant(node->thisnodename.b == blocknum.b);
    invariant(node->fullhash == fullhash);
    invariant(node->height > 0);   // we should never pick a leaf node (for now at least)
    struct brt_header *h = extraargs;
    bring_node_fully_into_memory(node, h);
    int childnum;
    find_heaviest_child(node, &childnum);
    update_cleaner_status(node, childnum);

    // Either flush_some_child will unlock the node, or we do it here.
    if (toku_bnc_nbytesinbuf(BNC(node, childnum)) > 0) {
        int n_dirtied = 0;
        flush_some_child(h, node, &n_dirtied, 0, false);
        brt_status.cleaner_nodes_dirtied += n_dirtied;
    } else {
        toku_unpin_brtnode_off_client_thread(h, node);
    }
    return 0;
}


static inline void
brt_status_update_partial_fetch(u_int8_t state)
{
    if (state == PT_AVAIL) {
        brt_status.partial_fetch_hit++;
    }
    else if (state == PT_COMPRESSED) {
        brt_status.partial_fetch_compressed++;
    }
    else if (state == PT_ON_DISK){
        brt_status.partial_fetch_miss++;
    }
    else {
        invariant(FALSE);
    }
}

// Callback that states if a partial fetch of the node is necessary
// Currently, this function is responsible for the following things:
//  - reporting to the cachetable whether a partial fetch is required (as required by the contract of the callback)
//  - A couple of things that are NOT required by the callback, but we do for efficiency and simplicity reasons:
//   - for queries, set the value of bfe->child_to_read so that the query that called this can proceed with the query
//      as opposed to having to evaluate toku_brt_search_which_child again. This is done to make the in-memory query faster
//   - touch the necessary partition's clock. The reason we do it here is so that there is one central place it is done, and not done
//      by all the various callers
//
BOOL toku_brtnode_pf_req_callback(void* brtnode_pv, void* read_extraargs) {
    // placeholder for now
    BOOL retval = FALSE;
    BRTNODE node = brtnode_pv;
    struct brtnode_fetch_extra *bfe = read_extraargs;
    //
    // The three types of fetches that the brt layer may request are:
    //  - brtnode_fetch_none: no partitions are necessary (example use: stat64)
    //  - brtnode_fetch_subset: some subset is necessary (example use: toku_brt_search)
    //  - brtnode_fetch_all: entire node is necessary (example use: flush, split, merge)
    // The code below checks if the necessary partitions are already in memory, 
    // and if they are, return FALSE, and if not, return TRUE
    //
    if (bfe->type == brtnode_fetch_none) {
        retval = FALSE;
    }
    else if (bfe->type == brtnode_fetch_all) {
        retval = FALSE;
        for (int i = 0; i < node->n_children; i++) {
            BP_TOUCH_CLOCK(node,i);
            // if we find a partition that is not available,
            // then a partial fetch is required because
            // the entire node must be made available
            if (BP_STATE(node,i) != PT_AVAIL) {
                retval = TRUE;
            }
            brt_status_update_partial_fetch(BP_STATE(node, i));
        }
    }
    else if (bfe->type == brtnode_fetch_subset) {
        // we do not take into account prefetching yet
        // as of now, if we need a subset, the only thing
        // we can possibly require is a single basement node
        // we find out what basement node the query cares about
        // and check if it is available
        assert(bfe->h->compare_fun);
        assert(bfe->search);
        bfe->child_to_read = toku_brt_search_which_child(
            &bfe->h->descriptor,
            bfe->h->compare_fun,
            node,
            bfe->search
            );
        BP_TOUCH_CLOCK(node,bfe->child_to_read);
        // child we want to read is not available, must set retval to TRUE
        retval = (BP_STATE(node, bfe->child_to_read) != PT_AVAIL);
        brt_status_update_partial_fetch(BP_STATE(node, bfe->child_to_read));
    }
    else if (bfe->type == brtnode_fetch_prefetch) {
        int lc = toku_bfe_leftmost_child_wanted(bfe, node);
        int rc = toku_bfe_rightmost_child_wanted(bfe, node);
        for (int i = lc; i <= rc; ++i) {
            if (BP_STATE(node, i) != PT_AVAIL) {
                retval = TRUE;
            }
            brt_status_update_partial_fetch(BP_STATE(node, i));
        }
    }
    else {
        // we have a bug. The type should be known
        assert(FALSE);
    }
    return retval;
}

// callback for partially reading a node
// could have just used toku_brtnode_fetch_callback, but wanted to separate the two cases to separate functions
int toku_brtnode_pf_callback(void* brtnode_pv, void* read_extraargs, int fd, PAIR_ATTR* sizep) {
    BRTNODE node = brtnode_pv;
    struct brtnode_fetch_extra *bfe = read_extraargs;
    // there must be a reason this is being called. If we get a garbage type or the type is brtnode_fetch_none,
    // then something went wrong
    assert((bfe->type == brtnode_fetch_subset) || (bfe->type == brtnode_fetch_all) || (bfe->type == brtnode_fetch_prefetch));
    // determine the range to prefetch
    int lc, rc;
    if (bfe->type == brtnode_fetch_subset || bfe->type == brtnode_fetch_prefetch) {
        lc = toku_bfe_leftmost_child_wanted(bfe, node);
        rc = toku_bfe_rightmost_child_wanted(bfe, node);
    } else {
        lc = -1;
        rc = -1;
    }
    // TODO: possibly cilkify expensive operations in this loop
    // TODO: review this with others to see if it can be made faster
    for (int i = 0; i < node->n_children; i++) {
        if (BP_STATE(node,i) == PT_AVAIL) {
            continue;
        }
        if ((lc <= i && i <= rc) || toku_bfe_wants_child_available(bfe, i)) {
            if (BP_STATE(node,i) == PT_COMPRESSED) {
                cilk_spawn toku_deserialize_bp_from_compressed(node, i, &bfe->h->descriptor, bfe->h->compare_fun);
            }
            else if (BP_STATE(node,i) == PT_ON_DISK) {
                cilk_spawn toku_deserialize_bp_from_disk(node, i, fd, bfe);
            }
            else {
                assert(FALSE);
            }
        }
    }
    cilk_sync;
    *sizep = make_brtnode_pair_attr(node);
    return 0;
}


#define FAKE_DB(db, desc) struct __toku_db db = {.descriptor= desc}

static int
leafval_heaviside_le (u_int32_t klen, void *kval,
		      struct cmd_leafval_heaviside_extra *be) {
    DBT dbt;
    DBT const * const key = be->key;
    FAKE_DB(db, be->desc);
    return be->compare_fun(&db,
			   toku_fill_dbt(&dbt, kval, klen),
			   key);
}

//TODO: #1125 optimize
int
toku_cmd_leafval_heaviside (OMTVALUE lev, void *extra) {
    LEAFENTRY le=lev;
    struct cmd_leafval_heaviside_extra *be = extra;
    u_int32_t keylen;
    void*     key = le_key_and_len(le, &keylen);
    return leafval_heaviside_le(keylen, key,
				be);
}

static int
brt_compare_pivot(DESCRIPTOR desc, brt_compare_func cmp, const DBT *key, bytevec ck)
{
    int r;
    DBT mydbt;
    struct kv_pair *kv = (struct kv_pair *) ck;
    FAKE_DB(db, desc);
    r = cmp(&db, key, toku_fill_dbt(&mydbt, kv_pair_key(kv), kv_pair_keylen(kv)));
    return r;
}

static int
verify_in_mempool (OMTVALUE lev, u_int32_t UU(idx), void *vmp)
{
    LEAFENTRY le=lev;
    struct mempool *mp=vmp;
    lazy_assert(toku_mempool_inrange(mp, le, leafentry_memsize(le)));
    return 0;
}

static void
verify_all_in_mempool (BRTNODE node)
{
    if (node->height==0) {
	for (int i = 0; i < node->n_children; i++) {
	    invariant(BP_STATE(node,i) == PT_AVAIL);
	    BASEMENTNODE bn = BLB(node, i);
	    toku_omt_iterate(bn->buffer, verify_in_mempool, &bn->buffer_mempool);
	}
    }
}


// destroys the internals of the brtnode, but it does not free the values
// that are stored
// this is common functionality for toku_brtnode_free and rebalance_brtnode_leaf
// MUST NOT do anything besides free the structures that have been allocated
void toku_destroy_brtnode_internals(BRTNODE node)
{
    for (int i=0; i<node->n_children-1; i++) {
	toku_free(node->childkeys[i]);
    }
    toku_free(node->childkeys);
    node->childkeys = NULL;

    for (int i=0; i < node->n_children; i++) {
        if (BP_STATE(node,i) == PT_AVAIL) {
            if (node->height > 0) {
		destroy_nonleaf_childinfo(BNC(node,i));
            } else {
		destroy_basement_node(BLB(node, i));
            }
        } else if (BP_STATE(node,i) == PT_COMPRESSED) {
            SUB_BLOCK sb = BSB(node,i);
            toku_free(sb->compressed_ptr);
	    toku_free(sb);
        } else {
	    assert(is_BNULL(node, i));
        }
	set_BNULL(node, i);
    }
    toku_free(node->bp);
    node->bp = NULL;

}


/* Frees a node, including all the stuff in the hash table. */
void toku_brtnode_free (BRTNODE *nodep) {

    //TODO: #1378 Take omt lock (via brtnode) around call to toku_omt_destroy().

    BRTNODE node=*nodep;
    if (node->height == 0) {
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node,i) == PT_AVAIL) {
                struct mempool * mp = &(BLB_BUFFER_MEMPOOL(node, i));
                toku_mempool_destroy(mp);
            }
        }
        brt_status.destroy_leaf++;
    } else {
        brt_status.destroy_nonleaf++;
    }
    toku_destroy_brtnode_internals(node);
    toku_free(node);
    *nodep=0;
}

static void
brtheader_destroy(struct brt_header *h) {
    if (!h->panic) assert(!h->checkpoint_header);

    //header and checkpoint_header have same Blocktable pointer
    //cannot destroy since it is still in use by CURRENT
    if (h->type == BRTHEADER_CHECKPOINT_INPROGRESS) h->blocktable = NULL; 
    else {
	assert(h->type == BRTHEADER_CURRENT);
	toku_blocktable_destroy(&h->blocktable);
	if (h->descriptor.dbt.data) toku_free(h->descriptor.dbt.data);
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
    //printf("checkpoint_lsn=%" PRIu64 "\n", checkpoint_lsn.lsn);
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

void
toku_initialize_empty_brtnode (BRTNODE n, BLOCKNUM nodename, int height, int num_children, int layout_version, unsigned int nodesize, unsigned int flags)
// Effect: Fill in N as an empty brtnode.
{
    assert(layout_version != 0);
    assert(height >= 0);

    n->max_msn_applied_to_node_on_disk = MIN_MSN;    // correct value for root node, harmless for others
    n->nodesize = nodesize;
    n->flags = flags;
    n->thisnodename = nodename;
    n->layout_version	       = layout_version;
    n->layout_version_original = layout_version;
    n->layout_version_read_from_disk = layout_version;
    n->height = height;
    n->optimized_for_upgrade = 0;
    n->dirty = 1;
    n->totalchildkeylens = 0;
    n->childkeys = 0;
    n->bp = 0;
    n->n_children = num_children; 

    if (num_children > 0) {
        XMALLOC_N(num_children-1, n->childkeys);
        XMALLOC_N(num_children, n->bp);
	for (int i = 0; i < num_children; i++) {
            BP_BLOCKNUM(n,i).b=0;
            BP_STATE(n,i) = PT_INVALID;
            BP_START(n,i) = 0;
            BP_SIZE (n,i) = 0;
	    BP_WORKDONE(n,i) = 0;
            BP_INIT_TOUCHED_CLOCK(n, i);
            set_BNULL(n,i);
            if (height > 0) {
		set_BNC(n, i, toku_create_empty_nl());
	    } else {
		set_BLB(n, i, toku_create_empty_bn());
            }
	}
    }
}

static void
brt_init_new_root(BRT brt, BRTNODE nodea, BRTNODE nodeb, DBT splitk, CACHEKEY *rootp, BRTNODE *newrootp)
// Effect:  Create a new root node whose two children are NODEA and NODEB, and the pivotkey is SPLITK.
//  Store the new root's identity in *ROOTP, and the node in *NEWROOTP.
//  Unpin nodea and nodeb.
//  Leave the new root pinned.
{
    BRTNODE XMALLOC(newroot);
    int new_height = nodea->height+1;
    BLOCKNUM newroot_diskoff;
    toku_allocate_blocknum(brt->h->blocktable, &newroot_diskoff, brt->h);
    assert(newroot);
    *rootp=newroot_diskoff;
    assert(new_height > 0);
    toku_initialize_empty_brtnode (newroot, newroot_diskoff, new_height, 2, brt->h->layout_version, brt->h->nodesize, brt->flags);
    //printf("new_root %lld %d %lld %lld\n", newroot_diskoff, newroot->height, nodea->thisnodename, nodeb->thisnodename);
    //printf("%s:%d Splitkey=%p %s\n", __FILE__, __LINE__, splitkey, splitkey);
    newroot->childkeys[0] = splitk.data;
    newroot->totalchildkeylens=splitk.size;
    BP_BLOCKNUM(newroot,0)=nodea->thisnodename;
    BP_BLOCKNUM(newroot,1)=nodeb->thisnodename;
    {
	MSN msna = nodea->max_msn_applied_to_node_on_disk;
	MSN msnb = nodeb->max_msn_applied_to_node_on_disk;
	invariant(msna.msn == msnb.msn);
	newroot->max_msn_applied_to_node_on_disk = msna;
    }
    BP_STATE(newroot,0) = PT_AVAIL;
    BP_STATE(newroot,1) = PT_AVAIL;
    newroot->dirty = 1;
    toku_unpin_brtnode(brt, nodea);
    toku_unpin_brtnode(brt, nodeb);
    //printf("%s:%d put %lld\n", __FILE__, __LINE__, newroot_diskoff);
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, newroot_diskoff);
    newroot->fullhash = fullhash;
    toku_cachetable_put(brt->cf, newroot_diskoff, fullhash, newroot, make_brtnode_pair_attr(newroot),
			toku_brtnode_flush_callback, toku_brtnode_pe_est_callback, toku_brtnode_pe_callback, toku_brtnode_cleaner_callback, brt->h);
    *newrootp = newroot;
}


static void brtnode_get_key_and_fullhash (BLOCKNUM* cachekey, u_int32_t* fullhash, void* extra)
{    
    struct brt_header* h = extra;
    BLOCKNUM name;
    toku_allocate_blocknum(h->blocktable, &name, h);
    *cachekey = name;
    *fullhash = toku_cachetable_hash(h->cf, name);
}


static void
cachetable_put_empty_node_with_dep_nodes(
    struct brt_header* h, 
    u_int32_t num_dependent_nodes, 
    BRTNODE* dependent_nodes,
    BLOCKNUM* name, //output
    u_int32_t* fullhash, //output
    BRTNODE* result
)
{
    BRTNODE XMALLOC(new_node);
    CACHEFILE dependent_cf[num_dependent_nodes];
    BLOCKNUM dependent_keys[num_dependent_nodes];
    u_int32_t dependent_fullhash[num_dependent_nodes];
    enum cachetable_dirty dependent_dirty_bits[num_dependent_nodes];
    for (u_int32_t i = 0; i < num_dependent_nodes; i++) {
        dependent_cf[i] = h->cf;
        dependent_keys[i] = dependent_nodes[i]->thisnodename;
        dependent_fullhash[i] = toku_cachetable_hash(h->cf, dependent_nodes[i]->thisnodename);
        dependent_dirty_bits[i] = (enum cachetable_dirty) dependent_nodes[i]->dirty;
    }
    
    
    int r = toku_cachetable_put_with_dep_pairs(
        h->cf,
        brtnode_get_key_and_fullhash,
        new_node, 
        make_pair_attr(sizeof(BRTNODE)),
        toku_brtnode_flush_callback,
        toku_brtnode_pe_est_callback,
        toku_brtnode_pe_callback,
        toku_brtnode_cleaner_callback,
        h,
        h,
        num_dependent_nodes,
        dependent_cf,
        dependent_keys,
        dependent_fullhash,
        dependent_dirty_bits,
        name,
        fullhash
        );
    assert_zero(r);

    *result = new_node;
}

static void 
create_new_brtnode_with_dep_nodes (
    struct brt_header* h,
    BRTNODE *result, 
    int height, 
    int n_children, 
    u_int32_t num_dependent_nodes, 
    BRTNODE* dependent_nodes
    ) 
{
    u_int32_t fullhash = 0;
    BLOCKNUM name;

    cachetable_put_empty_node_with_dep_nodes(h, num_dependent_nodes, dependent_nodes, &name, &fullhash, result);

    assert(h->nodesize > 0);
    assert(h->basementnodesize > 0);
    if (height == 0) {
        assert(n_children > 0);
    }

    toku_initialize_empty_brtnode(*result, name, height, n_children, h->layout_version, h->nodesize, h->flags);
    assert((*result)->nodesize > 0);
    (*result)->fullhash = fullhash;

}

void 
toku_create_new_brtnode (
    BRT t, 
    BRTNODE *result, 
    int height, 
    int n_children
    ) 
{
    return create_new_brtnode_with_dep_nodes(
        t->h,
        result,
        height,
        n_children,
        0,
        NULL
        );
}



static void
init_childinfo(BRTNODE node, int childnum, BRTNODE child) {
    BP_BLOCKNUM(node,childnum) = child->thisnodename;
    BP_STATE(node,childnum) = PT_AVAIL;
    BP_START(node,childnum) = 0;
    BP_SIZE (node,childnum) = 0;
    BP_WORKDONE(node, childnum)   = 0;
    set_BNC(node, childnum, toku_create_empty_nl());
}

static void
init_childkey(BRTNODE node, int childnum, struct kv_pair *pivotkey, size_t pivotkeysize) {
    node->childkeys[childnum] = pivotkey;
    node->totalchildkeylens += pivotkeysize;
}

// Used only by test programs: append a child node to a parent node
void
toku_brt_nonleaf_append_child(BRTNODE node, BRTNODE child, struct kv_pair *pivotkey, size_t pivotkeysize) {
    int childnum = node->n_children;
    node->n_children++;
    XREALLOC_N(node->n_children, node->bp);
    init_childinfo(node, childnum, child);
    XREALLOC_N(node->n_children-1, node->childkeys);
    if (pivotkey) {
	invariant(childnum > 0);
	init_childkey(node, childnum-1, pivotkey, pivotkeysize);
    }
    node->dirty = 1;
}

static u_int64_t
brtleaf_disk_size(BRTNODE node)
// Effect: get the disk size of a leafentry
{
    assert(node->height == 0);
    toku_assert_entire_node_in_memory(node);
    u_int64_t retval = 0;
    int i;
    for (i = 0; i < node->n_children; i++) {
	OMT curr_buffer = BLB_BUFFER(node, i);
	u_int32_t n_leafentries = toku_omt_size(curr_buffer);
	u_int32_t j;
	for (j=0; j < n_leafentries; j++) {
	    OMTVALUE v;
	    LEAFENTRY curr_le = NULL;
	    int r = toku_omt_fetch(curr_buffer, j, &v);
	    curr_le = v;
	    assert_zero(r);
	    retval += leafentry_disksize(curr_le);
	}
    }
    return retval;
}

static void
brtleaf_get_split_loc(
    BRTNODE node, 
    u_int64_t sumlesizes, 
    int* bn_index,   // which basement within leaf
    int* le_index    // which key within basement
    )
// Effect: Find the location within a leaf node where we want to perform a split
// bn_index is which basement node (which OMT) should be split.
// le_index is index into OMT of the last key that should be on the left side of the split.
{
    assert(node->height == 0);
    u_int32_t size_so_far = 0;
    int i;
    for (i = 0; i < node->n_children; i++) {
        OMT curr_buffer = BLB_BUFFER(node, i);
        u_int32_t n_leafentries = toku_omt_size(curr_buffer);
        u_int32_t j;
        for (j=0; j < n_leafentries; j++) {
            LEAFENTRY curr_le = NULL;
            OMTVALUE v;
            int r = toku_omt_fetch(curr_buffer, j, &v);
            curr_le = v;
            assert_zero(r);
            size_so_far += leafentry_disksize(curr_le);
            if (size_so_far >= sumlesizes/2) {
                *bn_index = i;
                *le_index = j;
                if ((*bn_index == node->n_children - 1) &&
                    ((unsigned int) *le_index == n_leafentries - 1)) {
                    // need to correct for when we're splitting after the
                    // last element, that makes no sense
                    if (*le_index > 0) {
                        (*le_index)--;
                    } else if (*bn_index > 0) {
                        (*bn_index)--;
                        *le_index = toku_omt_size(BLB_BUFFER(node, *bn_index)) - 1;
                    } else {
                        // we are trying to split a leaf with only one
                        // leafentry in it
                        assert(FALSE);
                    }
                }
                goto exit;
            }
        }
    }
exit:
    return;
}

// TODO: (Zardosht) possibly get rid of this function and use toku_omt_split_at in
// brtleaf_split
static void
move_leafentries(
    BASEMENTNODE dest_bn,    
    BASEMENTNODE src_bn,
    u_int32_t lbi, //lower bound inclusive
    u_int32_t ube, //upper bound exclusive
    u_int32_t* num_bytes_moved
    )
//Effect: move leafentries in the range [lbi, upe) from src_omt to newly created dest_omt
{
    assert(lbi < ube);
    OMTVALUE *XMALLOC_N(ube-lbi, newleafpointers);    // create new omt

    size_t mpsize = toku_mempool_get_used_space(&src_bn->buffer_mempool);   // overkill, but safe
    struct mempool * dest_mp = &dest_bn->buffer_mempool;
    struct mempool * src_mp  = &src_bn->buffer_mempool;
    toku_mempool_construct(dest_mp, mpsize);   

    u_int32_t i = 0;
    *num_bytes_moved = 0;
    for (i = lbi; i < ube; i++) {
	LEAFENTRY curr_le = NULL;
	curr_le = fetch_from_buf(src_bn->buffer, i);
	size_t le_size = leafentry_memsize(curr_le);
	*num_bytes_moved += leafentry_disksize(curr_le);
	LEAFENTRY new_le = toku_mempool_malloc(dest_mp, le_size, 1);
	memcpy(new_le, curr_le, le_size);
	newleafpointers[i-lbi] = new_le;
	toku_mempool_mfree(src_mp, curr_le, le_size);
    }

    int r = toku_omt_create_steal_sorted_array(
	&dest_bn->buffer,
	&newleafpointers,
	ube-lbi,
	ube-lbi
	);
    assert_zero(r);
    // now remove the elements from src_omt
    for (i=ube-1; i >= lbi; i--) {
	toku_omt_delete_at(src_bn->buffer,i);
    }
}

void
brtleaf_split (struct brt_header* h, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, BOOL create_new_node, u_int32_t num_dependent_nodes, BRTNODE* dependent_nodes)
// Effect: Split a leaf node.
// Argument "node" is node to be split.
// Upon return:
//   nodea and nodeb point to new nodes that result from split of "node"
//   nodea is the left node that results from the split
//   splitk is the right-most key of nodea
{

    //    printf("###### brtleaf_split():  create_new_node = %d, num_dependent_nodes = %d\n", create_new_node, num_dependent_nodes);
    BRTNODE B;

    u_int32_t fullhash;
    BLOCKNUM name;
    if (create_new_node) {
        // put value in cachetable and do checkpointing
        // of dependent nodes
        //
        // We do this here, before evaluating the last_bn_on_left
        // and last_le_on_left_within_bn because this operation
        // may write to disk the dependent nodes.
        // While doing so, we may rebalance the leaf node
        // we are splitting, thereby invalidating the
        // values of last_bn_on_left and last_le_on_left_within_bn.
        // So, we must call this before evaluating
        // those two values
        cachetable_put_empty_node_with_dep_nodes(
            h,
            num_dependent_nodes,
            dependent_nodes,
            &name,
            &fullhash,
            &B
            );
    }
    
    //printf("%s:%d splitting leaf %" PRIu64 " which is size %u (targetsize = %u)\n", __FILE__, __LINE__, node->thisnodename.b, toku_serialize_brtnode_size(node), node->nodesize);

    assert(node->height==0);
    assert(node->nodesize>0);
    toku_assert_entire_node_in_memory(node);
    verify_all_in_mempool(node);
    MSN max_msn_applied_to_node = node->max_msn_applied_to_node_on_disk;

    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    //printf("%s:%d B is at %lld nodesize=%d\n", __FILE__, __LINE__, B->thisnodename, B->nodesize);


    // variables that say where we will do the split. We do it in the basement node indexed at
    // at last_bn_on_left and at the index last_le_on_left_within_bn within that basement node.
    int last_bn_on_left = 0;               // last_bn_on_left may or may not be fully included
    int last_le_on_left_within_bn = 0;
    {
        {
            // TODO: (Zardosht) see if we can/should make this faster, we iterate over the rows twice
            u_int64_t sumlesizes=0;
            sumlesizes = brtleaf_disk_size(node);
            // TODO: (Zardosht) #3537, figure out serial insertion optimization again later
            // split in half
            brtleaf_get_split_loc(
                node,
                sumlesizes,
                &last_bn_on_left,
                &last_le_on_left_within_bn
                );
        }
        // did we split right on the boundary between basement nodes?
        BOOL split_on_boundary = (last_le_on_left_within_bn == ((int) toku_omt_size(BLB_BUFFER(node, last_bn_on_left)) - 1));
        // Now we know where we are going to break it
        // the two nodes will have a total of n_children+1 basement nodes
        // and n_children-1 pivots
        // the left node, node, will have last_bn_on_left+1 basement nodes
        // the right node, B, will have n_children-last_bn_on_left basement nodes
        // the pivots of node will be the first last_bn_on_left pivots that originally exist
        // the pivots of B will be the last (n_children - 1 - last_bn_on_left) pivots that originally exist

	// Note: The basements will not be rebalanced.  Only the mempool of the basement that is split 
	//       (if split_on_boundary is false) will be affected.  All other mempools will remain intact. ???

        //set up the basement nodes in the new node
        int num_children_in_node = last_bn_on_left + 1;
        int num_children_in_b = node->n_children - last_bn_on_left - (split_on_boundary ? 1 : 0);
        if (create_new_node) {
            toku_initialize_empty_brtnode(
                B, 
                name, 
                0, 
                num_children_in_b, 
                h->layout_version, 
                h->nodesize, 
                h->flags
                );
            assert(B->nodesize > 0);
            B->fullhash = fullhash;
        }
        else {
            B = *nodeb;
            REALLOC_N(num_children_in_b-1, B->childkeys);
            REALLOC_N(num_children_in_b,   B->bp);
            B->n_children = num_children_in_b;
            for (int i = 0; i < num_children_in_b; i++) {
                BP_BLOCKNUM(B,i).b = 0;
                BP_STATE(B,i) = PT_AVAIL;
		BP_START(B,i) = 0;
                BP_SIZE(B,i) = 0;
                BP_WORKDONE(B,i) = 0;
                set_BLB(B, i, toku_create_empty_bn());
            }
        }
        //
        // first move all the data
        //

        int curr_src_bn_index = last_bn_on_left;
        int curr_dest_bn_index = 0;

        // handle the move of a subset of data in last_bn_on_left from node to B
        if (!split_on_boundary) {
            BP_STATE(B,curr_dest_bn_index) = PT_AVAIL;
            u_int32_t diff_size = 0;
            destroy_basement_node (BLB(B, curr_dest_bn_index)); // Destroy B's empty OMT, so I can rebuild it from an array
            set_BNULL(B, curr_dest_bn_index);
            set_BLB(B, curr_dest_bn_index, toku_create_empty_bn_no_buffer());
            move_leafentries(BLB(B, curr_dest_bn_index),
                             BLB(node, curr_src_bn_index),
                             last_le_on_left_within_bn+1,         // first row to be moved to B
                             toku_omt_size(BLB_BUFFER(node, curr_src_bn_index)),    // number of rows in basement to be split
                             &diff_size);
            BLB_MAX_MSN_APPLIED(B, curr_dest_bn_index) = BLB_MAX_MSN_APPLIED(node, curr_src_bn_index);
            BLB_NBYTESINBUF(node, curr_src_bn_index) -= diff_size;
            BLB_NBYTESINBUF(B, curr_dest_bn_index) += diff_size;
            curr_dest_bn_index++;
        }
        curr_src_bn_index++;

        assert(B->n_children - curr_dest_bn_index == node->n_children - curr_src_bn_index);
        // move the rest of the basement nodes
        for ( ; curr_src_bn_index < node->n_children; curr_src_bn_index++, curr_dest_bn_index++) {
            destroy_basement_node(BLB(B, curr_dest_bn_index));
            set_BNULL(B, curr_dest_bn_index);
            B->bp[curr_dest_bn_index] = node->bp[curr_src_bn_index];
        }
        node->n_children = num_children_in_node;

        //
        // now handle the pivots
        //

        // the child index in the original node that corresponds to the
        // first node in the right node of the split
        int base_index = (split_on_boundary ? last_bn_on_left + 1 : last_bn_on_left);
        // make pivots in B
        for (int i=0; i < num_children_in_b-1; i++) {
            B->childkeys[i] = node->childkeys[i+base_index];
            B->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[i+base_index]);
            node->totalchildkeylens -= toku_brt_pivot_key_len(node->childkeys[i+base_index]);
            node->childkeys[i+base_index] = NULL;
        }
        if (split_on_boundary) {
            // destroy the extra childkey between the nodes, we'll
            // recreate it in splitk below
            toku_free(node->childkeys[last_bn_on_left]);
        }
        REALLOC_N(num_children_in_node, node->bp);
        REALLOC_N(num_children_in_node-1, node->childkeys);

    }
    if (splitk) {
        memset(splitk, 0, sizeof *splitk);
        OMTVALUE lev = 0;
        int r=toku_omt_fetch(BLB_BUFFER(node, last_bn_on_left), toku_omt_size(BLB_BUFFER(node, last_bn_on_left))-1, &lev);
        assert_zero(r); // that fetch should have worked.
        LEAFENTRY le=lev;
        splitk->size = le_keylen(le);
        splitk->data = kv_pair_malloc(le_key(le), le_keylen(le), 0, 0);
        splitk->flags=0;
    }

    node->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;
    B->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;

    node->dirty = 1;
    B->dirty = 1;

    verify_all_in_mempool(node);
    verify_all_in_mempool(B);

    *nodea = node;
    *nodeb = B;

    //printf("%s:%d new sizes Node %" PRIu64 " size=%u omtsize=%d dirty=%d; Node %" PRIu64 " size=%u omtsize=%d dirty=%d\n", __FILE__, __LINE__,
    //		 node->thisnodename.b, toku_serialize_brtnode_size(node), node->height==0 ? (int)(toku_omt_size(node->u.l.buffer)) : -1, node->dirty,
    //		 B   ->thisnodename.b, toku_serialize_brtnode_size(B   ), B   ->height==0 ? (int)(toku_omt_size(B   ->u.l.buffer)) : -1, B->dirty);
    //toku_dump_brtnode(t, node->thisnodename, 0, NULL, 0, NULL, 0);
    //toku_dump_brtnode(t, B   ->thisnodename, 0, NULL, 0, NULL, 0);

}    // end of brtleaf_split()



static void
brt_nonleaf_split (struct brt_header* h, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, u_int32_t num_dependent_nodes, BRTNODE* dependent_nodes)
// Effect: node must be a node-leaf node.  It is split into two nodes, and the fanout is split between them.
//    Sets splitk->data pointer to a malloc'd value
//    Sets nodea, and nodeb to the two new nodes.
//    The caller must replace the old node with the two new nodes.
//    This function will definitely reduce the number of children for the node,
//    but it does not guarantee that the resulting nodes are smaller than nodesize.
{
    //VERIFY_NODE(t,node);
    toku_assert_entire_node_in_memory(node);
    int old_n_children = node->n_children;
    int n_children_in_a = old_n_children/2;
    int n_children_in_b = old_n_children-n_children_in_a;
    MSN max_msn_applied_to_node = node->max_msn_applied_to_node_on_disk;
    BRTNODE B;
    assert(node->height>0);
    assert(node->n_children>=2); // Otherwise, how do we split?	 We need at least two children to split. */
    create_new_brtnode_with_dep_nodes(h, &B, node->height, n_children_in_b, num_dependent_nodes, dependent_nodes);
    {
	/* The first n_children_in_a go into node a.
	 * That means that the first n_children_in_a-1 keys go into node a.
	 * The splitter key is key number n_children_in_a */
	int i;

	for (i=n_children_in_a; i<old_n_children; i++) {

	    int targchild = i-n_children_in_a;
            // TODO: Figure out better way to handle this
            // the problem is that create_new_brtnode_with_dep_nodes for B creates
            // all the data structures, whereas we really don't want it to fill
            // in anything for the bp's.
            // Now we have to go free what it just created so we can
            // slide the bp over            
	    destroy_nonleaf_childinfo(BNC(B, targchild));
            // now move the bp over
            B->bp[targchild] = node->bp[i];
            memset(&node->bp[i], 0, sizeof(node->bp[0]));
            
	    // Delete a child, removing the preceeding pivot key.  The child number must be > 0
	    {
		assert(i>0);
		if (i>n_children_in_a) {
		    B->childkeys[targchild-1] = node->childkeys[i-1];
		    B->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[i-1]);
		    node->totalchildkeylens -= toku_brt_pivot_key_len(node->childkeys[i-1]);
		    node->childkeys[i-1] = 0;
		}
	    }
	}

	node->n_children=n_children_in_a;

	splitk->data = (void*)(node->childkeys[n_children_in_a-1]);
	splitk->size = toku_brt_pivot_key_len(node->childkeys[n_children_in_a-1]);
	node->totalchildkeylens -= toku_brt_pivot_key_len(node->childkeys[n_children_in_a-1]);

	REALLOC_N(n_children_in_a,   node->bp);
	REALLOC_N(n_children_in_a-1, node->childkeys);

    }

    node->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;
    B	->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;

    node->dirty = 1;
    B	->dirty = 1;
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(B);
    //VERIFY_NODE(t,node);
    //VERIFY_NODE(t,B);
    *nodea = node;
    *nodeb = B;
}




/* NODE is a node with a child.
 * childnum was split into two nodes childa, and childb.  childa is the same as the original child.  childb is a new child.
 * We must slide things around, & move things from the old table to the new tables.
 * Requires: the CHILDNUMth buffer of node is empty.
 * We don't push anything down to children.  We split the node, and things land wherever they land.
 * We must delete the old buffer (but the old child is already deleted.)
 * On return, the new children and node STAY PINNED.
 */
static void
handle_split_of_child (BRTNODE node, int childnum,
		       BRTNODE childa, BRTNODE childb,
		       DBT *splitk /* the data in the childsplitk is alloc'd and is consumed by this call. */
		       )
{
    assert(node->height>0);
    assert(0 <= childnum && childnum < node->n_children);
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(childa);
    toku_assert_entire_node_in_memory(childb);
    int old_count = toku_bnc_nbytesinbuf(BNC(node, childnum));
    assert(old_count==0);
    int cnum;
    WHEN_NOT_GCOV(
    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d Child %d splitting on %s\n", __FILE__, __LINE__, childnum, (char*)splitk->data);
	printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->n_children-1; i++) printf(" %s", (char*)node->childkeys[i]);
	printf("\n");
    }
		  )

    node->dirty = 1;

    XREALLOC_N(node->n_children+1, node->bp);
    XREALLOC_N(node->n_children, node->childkeys);
    // Slide the children over.
    // suppose n_children is 10 and childnum is 5, meaning node->childnum[5] just got split
    // this moves node->bp[6] through node->bp[9] over to
    // node->bp[7] through node->bp[10]
    for (cnum=node->n_children; cnum>childnum+1; cnum--) {
        node->bp[cnum] = node->bp[cnum-1];
    }
    memset(&node->bp[childnum+1],0,sizeof(node->bp[0]));
    node->n_children++;

    assert(BP_BLOCKNUM(node, childnum).b==childa->thisnodename.b); // use the same child

    BP_BLOCKNUM(node, childnum+1) = childb->thisnodename;
    BP_WORKDONE(node, childnum+1)  = 0;
    BP_STATE(node,childnum+1) = PT_AVAIL;
    BP_START(node,childnum+1) = 0;
    BP_SIZE(node,childnum+1) = 0;

    set_BNC(node, childnum+1, toku_create_empty_nl());

    // Slide the keys over
    {
	struct kv_pair *pivot = splitk->data;

	for (cnum=node->n_children-2; cnum>childnum; cnum--) {
	    node->childkeys[cnum] = node->childkeys[cnum-1];
	}
	//if (logger) assert((t->flags&TOKU_DB_DUPSORT)==0); // the setpivot is wrong for TOKU_DB_DUPSORT, so recovery will be broken.
	node->childkeys[childnum]= pivot;
	node->totalchildkeylens += toku_brt_pivot_key_len(pivot);
    }

    WHEN_NOT_GCOV(
    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d splitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->n_children-2; i++) printf(" %s", (char*)node->childkeys[i]);
	printf("\n");
    }
		  )

    /* Keep pushing to the children, but not if the children would require a pushdown */
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(childa);
    toku_assert_entire_node_in_memory(childb);

    VERIFY_NODE(t, node);
    VERIFY_NODE(t, childa);
    VERIFY_NODE(t, childb);
}


#define ft_flush_before_applying_inbox 1  
#define ft_flush_before_child_pin 2
#define ft_flush_after_child_pin 3
#define ft_flush_before_split 4
#define ft_flush_during_split 5
#define ft_flush_before_merge 6
#define ft_flush_after_merge 7
#define ft_flush_after_rebalance 8


// For test purposes only.
// These callbacks are never used in production code, only as a way to test the system
// (for example, by causing crashes at predictable times).
static void (*flusher_thread_callback)(int, void*) = NULL;
static void* flusher_thread_callback_extra = NULL;

void toku_flusher_thread_set_callback(void (*callback_f)(int, void*), void* extra) {
    flusher_thread_callback = callback_f;
    flusher_thread_callback_extra = extra;
}

static void call_flusher_thread_callback(int ft_state) {
    if (flusher_thread_callback) {
        flusher_thread_callback(ft_state, flusher_thread_callback_extra);
    }
}

//
// responsibility of brt_split_child is to take locked BRTNODEs node and child
// and do the following:
//  - split child, 
//  - fix node,
//  - release lock on node
//  - possibly flush either new children created from split, otherwise unlock children
//
static void
brt_split_child (struct brt_header* h, BRTNODE node, int childnum, BRTNODE child, bool started_at_root)
{
    assert(node->height>0);
    assert(toku_bnc_nbytesinbuf(BNC(node, childnum))==0); // require that the buffer for this child is empty
    BRTNODE nodea, nodeb;
    DBT splitk;
    // printf("%s:%d node %" PRIu64 "->u.n.n_children=%d height=%d\n", __FILE__, __LINE__, node->thisnodename.b, node->u.n.n_children, node->height);
    assert(h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    
    // for test
    call_flusher_thread_callback(ft_flush_before_split);

    BRTNODE dep_nodes[2];
    dep_nodes[0] = node;
    dep_nodes[1] = child;
    if (child->height==0) {
        brtleaf_split(h, child, &nodea, &nodeb, &splitk, TRUE, 2, dep_nodes);
    } else {
        brt_nonleaf_split(h, child, &nodea, &nodeb, &splitk, 2, dep_nodes);
    }
    // printf("%s:%d child did split\n", __FILE__, __LINE__);
    handle_split_of_child (node, childnum, nodea, nodeb, &splitk);

    // for test
    call_flusher_thread_callback(ft_flush_during_split);

    // at this point, the split is complete
    // now we need to unlock node,
    // and possibly continue 
    // flushing one of the children
    toku_unpin_brtnode_off_client_thread(h, node);
    if (nodea->height > 0 && nonleaf_node_is_gorged(nodea)) {
        toku_unpin_brtnode_off_client_thread(h, nodeb);
        flush_some_child(h, nodea, NULL, 0, started_at_root);
    }
    else if (nodeb->height > 0 && nonleaf_node_is_gorged(nodeb)) {
        toku_unpin_brtnode_off_client_thread(h, nodea);
        flush_some_child(h, nodeb, NULL, 0, started_at_root);
    }
    else {
        toku_unpin_brtnode_off_client_thread(h, nodea);
        toku_unpin_brtnode_off_client_thread(h, nodeb);
    }
}

static void
brt_leaf_delete_leafentry (
    BASEMENTNODE bn,
    u_int32_t idx, 
    LEAFENTRY le
    )
// Effect: Delete leafentry
//   idx is the location where it is
//   le is the leafentry to be deleted
{
    // Figure out if one of the other keys is the same key

    {
	int r = toku_omt_delete_at(bn->buffer, idx);
	assert(r==0);
    }

    bn->n_bytes_in_buffer -= leafentry_disksize(le);

    toku_mempool_mfree(&bn->buffer_mempool, 0, leafentry_memsize(le)); // Must pass 0, since le is no good any more.
}

void
brt_leaf_apply_cmd_once (
    BASEMENTNODE bn,
    const BRT_MSG cmd,
    u_int32_t idx,
    LEAFENTRY le,
    OMT snapshot_txnids,
    OMT live_list_reverse,
    uint64_t *workdone
    )
// Effect: Apply cmd to leafentry (msn is ignored)
//         Calculate work done by message on leafentry and add it to caller's workdone counter.
//   idx is the location where it goes
//   le is old leafentry
{
    // brt_leaf_check_leaf_stats(node);

    size_t newsize=0, oldsize=0, workdone_this_le=0;
    LEAFENTRY new_le=0;
    void *maybe_free = 0;

    if (le)
	oldsize = leafentry_memsize(le);

    // This function may call mempool_malloc_from_omt() to allocate more space.
    // That means le is guaranteed to not cause a sigsegv but it may point to a mempool that is 
    // no longer in use.  We'll have to release the old mempool later.
    {
	int r = apply_msg_to_leafentry(cmd, le, &newsize, &new_le, bn->buffer, &bn->buffer_mempool, &maybe_free, snapshot_txnids, live_list_reverse);
	invariant(r==0);
    }
    if (new_le) assert(newsize == leafentry_disksize(new_le));

    if (le && new_le) {
	bn->n_bytes_in_buffer -= oldsize;
	bn->n_bytes_in_buffer += newsize;

        // This mfree must occur after the mempool_malloc so that when the mempool is compressed everything is accounted for.
        // But we must compute the size before doing the mempool mfree because otherwise the le pointer is no good.
        toku_mempool_mfree(&bn->buffer_mempool, 0, oldsize); // Must pass 0, since le may be no good any more.
        
	{ 
	    int r = toku_omt_set_at(bn->buffer, new_le, idx); 
	    invariant(r==0); 
	}

	workdone_this_le = (oldsize > newsize ? oldsize : newsize);  // work done is max of le size before and after message application

    } else {           // we did not just replace a row, so ...
	if (le) {      
	    //            ... we just deleted a row ...
            // It was there, note that it's gone and remove it from the mempool
	    brt_leaf_delete_leafentry (bn, idx, le);
	    workdone_this_le = oldsize;
	}
	if (new_le) {  
	    //            ... or we just added a row
	    int r = toku_omt_insert_at(bn->buffer, new_le, idx);
	    invariant(r==0);
            bn->n_bytes_in_buffer += newsize;
	    workdone_this_le = newsize;
        }
    }
    if (workdone) {  // test programs may call with NULL
	*workdone += workdone_this_le;
	if (*workdone > brt_status.max_workdone)
	    brt_status.max_workdone = *workdone;
    }

    // if we created a new mempool buffer, free the old one
    if (maybe_free) toku_free(maybe_free);

    // brt_leaf_check_leaf_stats(node);

}

static const uint32_t setval_tag = 0xee0ccb99; // this was gotten by doing "cat /dev/random|head -c4|od -x" to get a random number.  We want to make sure that the user actually passes us the setval_extra_s that we passed in.
struct setval_extra_s {
    u_int32_t  tag;
    BOOL did_set_val;
    int	 setval_r;    // any error code that setval_fun wants to return goes here.
    // need arguments for brt_leaf_apply_cmd_once
    BASEMENTNODE bn;
    MSN msn;	      // captured from original message, not currently used
    XIDS xids;
    const DBT *key;
    u_int32_t idx;
    LEAFENTRY le;
    OMT snapshot_txnids;
    OMT live_list_reverse;
    bool made_change;
    uint64_t * workdone;  // set by brt_leaf_apply_cmd_once()
};

/*
 * If new_val == NULL, we send a delete message instead of an insert.
 * This happens here instead of in do_delete() for consistency.
 * setval_fun() is called from handlerton, passing in svextra_v
 * from setval_extra_s input arg to brt->update_fun().
 */
static void setval_fun (const DBT *new_val, void *svextra_v) {
    struct setval_extra_s *svextra = svextra_v;
    assert(svextra->tag==setval_tag);
    assert(!svextra->did_set_val);
    svextra->did_set_val = TRUE;

    {
        // can't leave scope until brt_leaf_apply_cmd_once if
        // this is a delete
        DBT val;
        BRT_MSG_S msg = { BRT_NONE, svextra->msn, svextra->xids,
                          .u.id={svextra->key, NULL} };
        if (new_val) {
            msg.type = BRT_INSERT;
            msg.u.id.val = new_val;
        } else {
            msg.type = BRT_DELETE_ANY;
            toku_init_dbt(&val);
            msg.u.id.val = &val;
        }
        brt_leaf_apply_cmd_once(svextra->bn, &msg,
                                svextra->idx, svextra->le,
                                svextra->snapshot_txnids, svextra->live_list_reverse,
                                svextra->workdone);
        svextra->setval_r = 0;
    }
    svextra->made_change = TRUE;
}

// We are already past the msn filter (in brt_leaf_put_cmd(), which calls do_update()),
// so capturing the msn in the setval_extra_s is not strictly required.	 The alternative
// would be to put a dummy msn in the messages created by setval_fun(), but preserving
// the original msn seems cleaner and it preserves accountability at a lower layer.
static int do_update(brt_update_func update_fun, DESCRIPTOR desc, BASEMENTNODE bn, BRT_MSG cmd, int idx,
                     LEAFENTRY le, OMT snapshot_txnids, OMT live_list_reverse, bool* made_change,
                     uint64_t * workdone) {
    LEAFENTRY le_for_update;
    DBT key;
    const DBT *keyp;
    const DBT *update_function_extra;
    DBT vdbt;
    const DBT *vdbtp;

    // the location of data depends whether this is a regular or
    // broadcast update
    if (cmd->type == BRT_UPDATE) {
        // key is passed in with command (should be same as from le)
        // update function extra is passed in with command
        brt_status.updates++;
        keyp = cmd->u.id.key;
        update_function_extra = cmd->u.id.val;
    } else if (cmd->type == BRT_UPDATE_BROADCAST_ALL) {
        // key is not passed in with broadcast, it comes from le
        // update function extra is passed in with command
        assert(le);  // for broadcast updates, we just hit all leafentries
                     // so this cannot be null
        assert(cmd->u.id.key->size == 0);
        brt_status.updates_broadcast++;
        keyp = toku_fill_dbt(&key, le_key(le), le_keylen(le));
        update_function_extra = cmd->u.id.val;
    } else {
        assert(FALSE);
    }

    if (le && !le_latest_is_del(le)) {
        // if the latest val exists, use it, and we'll use the leafentry later
        u_int32_t vallen;
        void *valp = le_latest_val_and_len(le, &vallen);
        vdbtp = toku_fill_dbt(&vdbt, valp, vallen);
        le_for_update = le;
    } else {
        // otherwise, the val and leafentry are both going to be null
        vdbtp = NULL;
        le_for_update = NULL;
    }

    struct setval_extra_s setval_extra = {setval_tag, FALSE, 0, bn, cmd->msn, cmd->xids,
                                          keyp, idx, le_for_update, snapshot_txnids, live_list_reverse, 0, workdone};
    // call handlerton's brt->update_fun(), which passes setval_extra to setval_fun()
    FAKE_DB(db, desc);
    int r = update_fun(
        &db,
        keyp,
        vdbtp,
        update_function_extra,
        setval_fun, &setval_extra
        );

    *made_change = setval_extra.made_change;

    // TODO(leif): ensure that really bad return codes actually cause a
    // crash higher up the stack somewhere
    if (r == 0) { r = setval_extra.setval_r; }
    return r;
}

// Should be renamed as something like "apply_cmd_to_basement()."
void
brt_leaf_put_cmd (
    brt_compare_func compare_fun,
    brt_update_func update_fun,
    DESCRIPTOR desc,
    BASEMENTNODE bn, 
    BRT_MSG cmd, 
    bool* made_change,
    uint64_t *workdone,
    OMT snapshot_txnids,
    OMT live_list_reverse
    )
// Effect: 
//   Put a cmd into a leaf.
//   Calculate work done by message on leafnode and add it to caller's workdone counter.
// The leaf could end up "too big" or "too small".  The caller must fix that up.
{
    LEAFENTRY storeddata;
    OMTVALUE storeddatav=NULL;

    u_int32_t omt_size;
    int r;
    struct cmd_leafval_heaviside_extra be = {compare_fun, desc, cmd->u.id.key};
    *made_change = 0;

    unsigned int doing_seqinsert = bn->seqinsert;
    bn->seqinsert = 0;

    switch (cmd->type) {
    case BRT_INSERT_NO_OVERWRITE:
    case BRT_INSERT: {
	u_int32_t idx;
	*made_change = 1;
	if (doing_seqinsert) {
	    idx = toku_omt_size(bn->buffer);
	    r = toku_omt_fetch(bn->buffer, idx-1, &storeddatav);
	    if (r != 0) goto fz;
	    storeddata = storeddatav;
	    int cmp = toku_cmd_leafval_heaviside(storeddata, &be);
	    if (cmp >= 0) goto fz;
	    r = DB_NOTFOUND;
	} else {
	fz:
	    r = toku_omt_find_zero(bn->buffer, toku_cmd_leafval_heaviside, &be,
				   &storeddatav, &idx);
	}
	if (r==DB_NOTFOUND) {
	    storeddata = 0;
	} else {
	    assert(r==0);
	    storeddata=storeddatav;
	}
	brt_leaf_apply_cmd_once(bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, workdone);

	// if the insertion point is within a window of the right edge of
	// the leaf then it is sequential
	// window = min(32, number of leaf entries/16)
	{
	u_int32_t s = toku_omt_size(bn->buffer);
	u_int32_t w = s / 16;
	if (w == 0) w = 1;
	if (w > 32) w = 32;

	// within the window?
	if (s - idx <= w)
	    bn->seqinsert = doing_seqinsert + 1;
	}
	break;
    }
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY: {
	u_int32_t idx;
	// Apply to all the matches

	r = toku_omt_find_zero(bn->buffer, toku_cmd_leafval_heaviside, &be,
			       &storeddatav, &idx);
	if (r == DB_NOTFOUND) break;
	assert(r==0);
	storeddata=storeddatav;

	while (1) {
	    u_int32_t num_leafentries_before = toku_omt_size(bn->buffer);

	    brt_leaf_apply_cmd_once(bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, workdone);
	    *made_change = 1;

	    { 
		// Now we must find the next leafentry. 
		u_int32_t num_leafentries_after = toku_omt_size(bn->buffer); 
		//idx is the index of the leafentry we just modified.
		//If the leafentry was deleted, we will have one less leafentry in 
		//the omt than we started with and the next leafentry will be at the 
		//same index as the deleted one. Otherwise, the next leafentry will 
		//be at the next index (+1). 
		assert(num_leafentries_before	== num_leafentries_after || 
		       num_leafentries_before-1 == num_leafentries_after); 
		if (num_leafentries_after==num_leafentries_before) idx++; //Not deleted, advance index.

		assert(idx <= num_leafentries_after);
		if (idx == num_leafentries_after) break; //Reached the end of the leaf
		r = toku_omt_fetch(bn->buffer, idx, &storeddatav); 
		assert_zero(r);
	    } 
	    storeddata=storeddatav;
	    {	// Continue only if the next record that we found has the same key.
		DBT adbt;
		u_int32_t keylen;
		void *keyp = le_key_and_len(storeddata, &keylen);
		FAKE_DB(db, desc);
		if (compare_fun(&db,
				toku_fill_dbt(&adbt, keyp, keylen),
				cmd->u.id.key) != 0)
		    break;
	    }
	}

	break;
    }
    case BRT_OPTIMIZE_FOR_UPGRADE:
	*made_change = 1;
	// TODO 4053: Record version of software that sent the optimize_for_upgrade message, but that goes in the
	//       node's optimize_for_upgrade field, not in the basement.
	// fall through so that optimize_for_upgrade performs rest of the optimize logic
    case BRT_COMMIT_BROADCAST_ALL:
    case BRT_OPTIMIZE:
	// Apply to all leafentries
	omt_size = toku_omt_size(bn->buffer);
	for (u_int32_t idx = 0; idx < omt_size; ) {
	    r = toku_omt_fetch(bn->buffer, idx, &storeddatav);
	    assert_zero(r);
	    storeddata=storeddatav;
	    int deleted = 0;
	    if (!le_is_clean(storeddata)) { //If already clean, nothing to do.
		brt_leaf_apply_cmd_once(bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, workdone);
		u_int32_t new_omt_size = toku_omt_size(bn->buffer);
		if (new_omt_size != omt_size) {
		    assert(new_omt_size+1 == omt_size);
		    //Item was deleted.
		    deleted = 1;
		}
		*made_change = 1;
	    }
	    if (deleted)
		omt_size--;
	    else
		idx++;
	}
	assert(toku_omt_size(bn->buffer) == omt_size);

	break;
    case BRT_COMMIT_BROADCAST_TXN:
    case BRT_ABORT_BROADCAST_TXN:
	// Apply to all leafentries if txn is represented
	omt_size = toku_omt_size(bn->buffer);
	for (u_int32_t idx = 0; idx < omt_size; ) {
	    r = toku_omt_fetch(bn->buffer, idx, &storeddatav);
	    assert_zero(r);
	    storeddata=storeddatav;
	    int deleted = 0;
	    if (le_has_xids(storeddata, cmd->xids)) {
		brt_leaf_apply_cmd_once(bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, workdone);
		u_int32_t new_omt_size = toku_omt_size(bn->buffer);
		if (new_omt_size != omt_size) {
		    assert(new_omt_size+1 == omt_size);
		    //Item was deleted.
		    deleted = 1;
		}
		*made_change = 1;
	    }
	    if (deleted)
		omt_size--;
	    else
		idx++;
	}
	assert(toku_omt_size(bn->buffer) == omt_size);

	break;
    case BRT_UPDATE: {
	u_int32_t idx;
	r = toku_omt_find_zero(bn->buffer, toku_cmd_leafval_heaviside, &be,
			       &storeddatav, &idx);
	if (r==DB_NOTFOUND) {
	    r = do_update(update_fun, desc, bn, cmd, idx, NULL, snapshot_txnids, live_list_reverse, made_change, workdone);
	} else if (r==0) {
	    storeddata=storeddatav;
	    r = do_update(update_fun, desc, bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, made_change, workdone);
	} // otherwise, a worse error, just return it
	break;
    }
    case BRT_UPDATE_BROADCAST_ALL: {
	// apply to all leafentries.
	u_int32_t idx = 0;
	u_int32_t num_leafentries_before;
	while (idx < (num_leafentries_before = toku_omt_size(bn->buffer))) {
	    r = toku_omt_fetch(bn->buffer, idx, &storeddatav);
	    assert(r==0);
	    storeddata=storeddatav;
	    r = do_update(update_fun, desc, bn, cmd, idx, storeddata, snapshot_txnids, live_list_reverse, made_change, workdone);
	    // TODO(leif): This early return means get_leaf_reactivity()
	    // and VERIFY_NODE() never get called.  Is this a problem?
	    assert(r==0);

	    if (num_leafentries_before == toku_omt_size(bn->buffer)) {
		// we didn't delete something, so increment the index.
		idx++;
	    }
	}
	break;
    }
    case BRT_NONE: break; // don't do anything
    }

    return;
}

static inline int
key_msn_cmp(const DBT *a, const DBT *b, const MSN amsn, const MSN bmsn,
            DESCRIPTOR descriptor, brt_compare_func key_cmp)
{
    FAKE_DB(db, descriptor);
    int r = key_cmp(&db, a, b);
    if (r == 0) {
        r = (amsn.msn > bmsn.msn) - (amsn.msn < bmsn.msn);
    }
    return r;
}

int
toku_fifo_entry_key_msn_heaviside(OMTVALUE v, void *extrap)
{
    const struct toku_fifo_entry_key_msn_heaviside_extra *extra = extrap;
    const long offset = (long) v;
    const struct fifo_entry *query = toku_fifo_get_entry(extra->fifo, offset);
    DBT qdbt, tdbt;
    const DBT *query_key = fill_dbt_for_fifo_entry(&qdbt, query);
    const DBT *target_key = toku_fill_dbt(&tdbt, extra->key, extra->keylen);
    return key_msn_cmp(query_key, target_key, query->msn, extra->msn,
                       extra->desc, extra->cmp);
}

int
toku_fifo_entry_key_msn_cmp(void *extrap, const void *ap, const void *bp)
{
    const struct toku_fifo_entry_key_msn_cmp_extra *extra = extrap;
    const long ao = *(long *) ap;
    const long bo = *(long *) bp;
    const struct fifo_entry *a = toku_fifo_get_entry(extra->fifo, ao);
    const struct fifo_entry *b = toku_fifo_get_entry(extra->fifo, bo);
    DBT adbt, bdbt;
    const DBT *akey = fill_dbt_for_fifo_entry(&adbt, a);
    const DBT *bkey = fill_dbt_for_fifo_entry(&bdbt, b);
    return key_msn_cmp(akey, bkey, a->msn, b->msn,
                       extra->desc, extra->cmp);
}

int
toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type, MSN msn, XIDS xids, bool is_fresh, DESCRIPTOR desc, brt_compare_func cmp)
{
    int diff = keylen + datalen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids);
    long offset;
    int r = toku_fifo_enq(bnc->buffer, key, keylen, data, datalen, type, msn, xids, is_fresh, &offset); assert_zero(r);
    enum brt_msg_type etype = (enum brt_msg_type) type;
    if (brt_msg_type_applies_once(etype)) {
        struct toku_fifo_entry_key_msn_heaviside_extra extra = { .desc= desc, .cmp = cmp, .fifo = bnc->buffer, .key = key, .keylen = keylen, .msn = msn };
        if (is_fresh) {
            r = toku_omt_insert(bnc->fresh_message_tree, (OMTVALUE) offset, toku_fifo_entry_key_msn_heaviside, &extra, NULL); assert_zero(r);
        } else {
            r = toku_omt_insert(bnc->stale_message_tree, (OMTVALUE) offset, toku_fifo_entry_key_msn_heaviside, &extra, NULL); assert_zero(r);
        }
    } else if (brt_msg_type_applies_all(etype) || brt_msg_type_does_nothing(etype)) {
        u_int32_t idx = toku_omt_size(bnc->broadcast_list);
        r = toku_omt_insert_at(bnc->broadcast_list, (OMTVALUE) offset, idx); assert_zero(r);
    } else {
        assert(FALSE);
    }
    bnc->n_bytes_in_buffer += diff;
    return r;
}

// append a cmd to a nonleaf node's child buffer
// should be static, but used by test programs
void
toku_brt_append_to_child_buffer(brt_compare_func compare_fun, DESCRIPTOR desc, BRTNODE node, int childnum, int type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val) {
    assert(BP_STATE(node,childnum) == PT_AVAIL);
    int r = toku_bnc_insert_msg(BNC(node, childnum), key->data, key->size, val->data, val->size, type, msn, xids, is_fresh, desc, compare_fun); 
    invariant_zero(r);
    node->dirty = 1;
}

static void brt_nonleaf_cmd_once_to_child (brt_compare_func compare_fun, DESCRIPTOR desc,  BRTNODE node, unsigned int childnum, BRT_MSG cmd, bool is_fresh)
// Previously we had passive aggressive promotion, but that causes a lot of I/O a the checkpoint.  So now we are just putting it in the buffer here.
// Also we don't worry about the node getting overfull here.  It's the caller's problem.
{
    toku_brt_append_to_child_buffer(compare_fun, desc, node, childnum, cmd->type, cmd->msn, cmd->xids, is_fresh, cmd->u.id.key, cmd->u.id.val);
}

/* Find the leftmost child that may contain the key.
 * If the key exists it will be in the child whose number
 * is the return value of this function.
 */
unsigned int toku_brtnode_which_child(BRTNODE node, const DBT *k,
                                      DESCRIPTOR desc, brt_compare_func cmp) {
#define DO_PIVOT_SEARCH_LR 0
#if DO_PIVOT_SEARCH_LR
    int i;
    for (i=0; i<node->n_children-1; i++) {
	int c = brt_compare_pivot(desc, cmp, k, d, node->childkeys[i]);
	if (c > 0) continue;
	if (c < 0) return i;
	return i;
    }
    return node->n_children-1;
#else
#endif
#define DO_PIVOT_SEARCH_RL 0
#if DO_PIVOT_SEARCH_RL
    // give preference for appending to the dictionary.	 no change for
    // random keys
    int i;
    for (i = node->n_children-2; i >= 0; i--) {
	int c = brt_compare_pivot(desc, cmp, k, d, node->childkeys[i]);
	if (c > 0) return i+1;
    }
    return 0;
#endif
#define DO_PIVOT_BIN_SEARCH 1
#if DO_PIVOT_BIN_SEARCH
    // a funny case of no pivots
    if (node->n_children <= 1) return 0;

    // check the last key to optimize seq insertions
    int n = node->n_children-1;
    int c = brt_compare_pivot(desc, cmp, k, node->childkeys[n-1]);
    if (c > 0) return n;

    // binary search the pivots
    int lo = 0;
    int hi = n-1; // skip the last one, we checked it above
    int mi;
    while (lo < hi) {
	mi = (lo + hi) / 2;
	c = brt_compare_pivot(desc, cmp, k, node->childkeys[mi]);
	if (c > 0) {
	    lo = mi+1;
	    continue;
	} 
	if (c < 0) {
	    hi = mi;
	    continue;
	}
	return mi;
    }
    return lo;
#endif
}

// TODO Use this function to clean up other places where bits of messages are passed around
//      such as toku_bnc_insert_msg() and the call stack above it.
static size_t
brt_msg_size(BRT_MSG msg) {
    size_t keylen = msg->u.id.key->size;
    size_t vallen = msg->u.id.val->size;
    size_t xids_size = xids_get_serialize_size(msg->xids);
    size_t rval = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_size;
    return rval;
}


static void brt_nonleaf_cmd_once(brt_compare_func compare_fun, DESCRIPTOR desc, BRTNODE node, BRT_MSG cmd, bool is_fresh)
// Effect: Insert a message into a nonleaf.  We may put it into a child, possibly causing the child to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to reactivity of any modified child.
{
    /* find the right subtree */
    //TODO: accesses key, val directly
    unsigned int childnum = toku_brtnode_which_child(node, cmd->u.id.key, desc, compare_fun);

    brt_nonleaf_cmd_once_to_child (compare_fun, desc, node, childnum, cmd, is_fresh);
}

static void
brt_nonleaf_cmd_all (brt_compare_func compare_fun, DESCRIPTOR desc, BRTNODE node, BRT_MSG cmd, bool is_fresh)
// Effect: Put the cmd into a nonleaf node.  We put it into all children, possibly causing the children to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.	 (And there may be several such children.)
{
    int i;
    for (i = 0; i < node->n_children; i++) {
	brt_nonleaf_cmd_once_to_child(compare_fun, desc, node, i, cmd, is_fresh);
    }
}

static BOOL
brt_msg_applies_once(BRT_MSG cmd)
{
    return brt_msg_type_applies_once(cmd->type);
}

static BOOL
brt_msg_applies_all(BRT_MSG cmd)
{
    return brt_msg_type_applies_all(cmd->type);
}

static BOOL
brt_msg_does_nothing(BRT_MSG cmd)
{
    return brt_msg_type_does_nothing(cmd->type);
}

static void
brt_nonleaf_put_cmd (brt_compare_func compare_fun, DESCRIPTOR desc, BRTNODE node, BRT_MSG cmd, bool is_fresh)
// Effect: Put the cmd into a nonleaf node.  We may put it into a child, possibly causing the child to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.	 (And there may be several such children.)
//
{
    MSN cmd_msn = cmd->msn;
    invariant(cmd_msn.msn > node->max_msn_applied_to_node_on_disk.msn);
    node->max_msn_applied_to_node_on_disk = cmd_msn;

    //TODO: Accessing type directly
    switch (cmd->type) {
    case BRT_INSERT_NO_OVERWRITE: 
    case BRT_INSERT:
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
    case BRT_UPDATE:
	brt_nonleaf_cmd_once(compare_fun, desc, node, cmd, is_fresh);
	return;
    case BRT_COMMIT_BROADCAST_ALL:
    case BRT_COMMIT_BROADCAST_TXN:
    case BRT_ABORT_BROADCAST_TXN:
    case BRT_OPTIMIZE:
    case BRT_OPTIMIZE_FOR_UPGRADE:
    case BRT_UPDATE_BROADCAST_ALL:
	brt_nonleaf_cmd_all (compare_fun, desc, node, cmd, is_fresh);  // send message to all children
	return;
    case BRT_NONE:
	return;
    }
    abort(); // cannot happen
}

static void
merge_leaf_nodes (BRTNODE a, BRTNODE b) {
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    assert(a->height == 0);
    assert(b->height == 0);
    assert(a->n_children > 0);
    assert(b->n_children > 0);

    // this BOOL states if the last basement node in a has any items or not
    // If it does, then it stays in the merge. If it does not, the last basement node
    // of a gets eliminated because we do not have a pivot to store for it (because it has no elements)
    BOOL a_has_tail = toku_omt_size(BLB_BUFFER(a, a->n_children-1));
    
    // move each basement node from b to a
    // move the pivots, adding one of what used to be max(a)
    // move the estimates
    int num_children = a->n_children + b->n_children;
    if (!a_has_tail) {
	uint lastchild = a->n_children-1;
	BASEMENTNODE bn = BLB(a, lastchild);
	{
	    // verify that last basement in a is empty, then destroy mempool
	    struct mempool * mp = &bn->buffer_mempool;
	    size_t used_space = toku_mempool_get_used_space(mp);
	    invariant_zero(used_space);
	    toku_mempool_destroy(mp);
	}
	destroy_basement_node(bn);
	set_BNULL(a, a->n_children-1);
	num_children--;
    }

    //realloc pivots and basement nodes in a
    REALLOC_N(num_children, a->bp);
    REALLOC_N(num_children-1, a->childkeys);

    // fill in pivot for what used to be max of node 'a', if it is needed
    if (a_has_tail) {
	LEAFENTRY le = fetch_from_buf(
	    BLB_BUFFER(a, a->n_children-1), 
	    toku_omt_size(BLB_BUFFER(a, a->n_children-1))-1
	    );
	a->childkeys[a->n_children-1] = kv_pair_malloc(le_key(le), le_keylen(le), 0, 0);
	a->totalchildkeylens += le_keylen(le);
    }

    u_int32_t offset = a_has_tail ? a->n_children : a->n_children - 1;
    for (int i = 0; i < b->n_children; i++) {
	a->bp[i+offset] = b->bp[i];
        memset(&b->bp[i],0,sizeof(b->bp[0]));
	if (i < (b->n_children-1)) {
	    a->childkeys[i+offset] = b->childkeys[i];
	    b->childkeys[i] = NULL;
	}
    }
    a->totalchildkeylens += b->totalchildkeylens;
    a->n_children = num_children;

    // now that all the data has been moved from b to a, we can destroy the data in b
    // b can remain untouched, as it will be destroyed later
    b->totalchildkeylens = 0;
    b->n_children = 0;
    a->dirty = 1;
    b->dirty = 1;

}

static int
balance_leaf_nodes (BRTNODE a, BRTNODE b, struct kv_pair **splitk)
// Effect:
//  If b is bigger then move stuff from b to a until b is the smaller.
//  If a is bigger then move stuff from a to b until a is the smaller.
{
    DBT splitk_dbt;
    // first merge all the data into a
    merge_leaf_nodes(a,b);
    // now split them	 
    // because we are not creating a new node, we can pass in no dependent nodes
    brtleaf_split(NULL, a, &a, &b, &splitk_dbt, FALSE, 0, NULL);
    *splitk = splitk_dbt.data;

    return 0;
}


static void
maybe_merge_pinned_leaf_nodes (BRTNODE a, BRTNODE b, struct kv_pair *parent_splitk, 
			       BOOL *did_merge, BOOL *did_rebalance, struct kv_pair **splitk)
// Effect: Either merge a and b into one one node (merge them into a) and set *did_merge = TRUE.    
//	   (We do this if the resulting node is not fissible)
//	   or distribute the leafentries evenly between a and b, and set *did_rebalance = TRUE.	  
//	   (If a and be are already evenly distributed, we may do nothing.)
{
    unsigned int sizea = toku_serialize_brtnode_size(a);
    unsigned int sizeb = toku_serialize_brtnode_size(b);
    if ((sizea + sizeb)*4 > (a->nodesize*3)) {
	// the combined size is more than 3/4 of a node, so don't merge them.
	*did_merge = FALSE;
	if (sizea*4 > a->nodesize && sizeb*4 > a->nodesize) {
	    // no need to do anything if both are more than 1/4 of a node.
	    *did_rebalance = FALSE;
	    *splitk = parent_splitk;
	    return;
	}
	// one is less than 1/4 of a node, and together they are more than 3/4 of a node.
	toku_free(parent_splitk); // We don't need the parent_splitk any more.	If we need a splitk (if we don't merge) we'll malloc a new one.
	*did_rebalance = TRUE;
	int r = balance_leaf_nodes(a, b, splitk);
	assert(r==0);
    } else {
	// we are merging them.
	*did_merge = TRUE;
	*did_rebalance = FALSE;
	*splitk = 0;
	toku_free(parent_splitk); // if we are merging, the splitk gets freed.
	merge_leaf_nodes(a, b);
    }
}

static void
maybe_merge_pinned_nonleaf_nodes (struct kv_pair *parent_splitk,
				  BRTNODE a, BRTNODE b,
				  BOOL *did_merge, BOOL *did_rebalance, struct kv_pair **splitk)
{
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    assert(parent_splitk);
    int old_n_children = a->n_children;
    int new_n_children = old_n_children + b->n_children;
    XREALLOC_N(new_n_children, a->bp);
    memcpy(a->bp + old_n_children,
	   b->bp,
	   b->n_children*sizeof(b->bp[0]));
    memset(b->bp,0,b->n_children*sizeof(b->bp[0]));
    
    XREALLOC_N(new_n_children-1, a->childkeys);
    a->childkeys[old_n_children-1] = parent_splitk;
    memcpy(a->childkeys + old_n_children,
	   b->childkeys,
	   (b->n_children-1)*sizeof(b->childkeys[0]));
    a->totalchildkeylens += b->totalchildkeylens + toku_brt_pivot_key_len(parent_splitk);
    a->n_children = new_n_children;

    b->totalchildkeylens = 0;
    b->n_children = 0;

    a->dirty = 1;
    b->dirty = 1;

    *did_merge = TRUE;
    *did_rebalance = FALSE;
    *splitk    = NULL;

}

static void
maybe_merge_pinned_nodes (BRTNODE parent, struct kv_pair *parent_splitk,
			  BRTNODE a, BRTNODE b, 
			  BOOL *did_merge, BOOL *did_rebalance, struct kv_pair **splitk)
// Effect: either merge a and b into one node (merge them into a) and set *did_merge = TRUE.  
//	   (We do this if the resulting node is not fissible)
//	   or distribute a and b evenly and set *did_merge = FALSE and *did_rebalance = TRUE  
//	   (If a and be are already evenly distributed, we may do nothing.)
//  If we distribute:
//    For leaf nodes, we distribute the leafentries evenly.
//    For nonleaf nodes, we distribute the children evenly.  That may leave one or both of the nodes overfull, but that's OK.
//  If we distribute, we set *splitk to a malloced pivot key.
// Parameters:
//  t			The BRT.
//  parent		The parent of the two nodes to be split.
//  parent_splitk	The pivot key between a and b.	 This is either free()'d or returned in *splitk.
//  a			The first node to merge.
//  b			The second node to merge.
//  logger		The logger.
//  did_merge		(OUT):	Did the two nodes actually get merged?
//  splitk		(OUT):	If the two nodes did not get merged, the new pivot key between the two nodes.
{
    MSN msn_max;
    assert(a->height == b->height);
    toku_assert_entire_node_in_memory(parent);
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    parent->dirty = 1; // just to make sure 
    {
	MSN msna = a->max_msn_applied_to_node_on_disk;
	MSN msnb = b->max_msn_applied_to_node_on_disk;
	msn_max = (msna.msn > msnb.msn) ? msna : msnb;
	if (a->height > 0) {
	    invariant(msn_max.msn <= parent->max_msn_applied_to_node_on_disk.msn);  // parent msn must be >= children's msn
	}
    }
    if (a->height == 0) {
	maybe_merge_pinned_leaf_nodes(a, b, parent_splitk, did_merge, did_rebalance, splitk);
    } else {
	maybe_merge_pinned_nonleaf_nodes(parent_splitk, a, b, did_merge, did_rebalance, splitk);
    }
    if (*did_merge || *did_rebalance) {	 
	// accurate for leaf nodes because all msgs above have been applied,
	// accurate for non-leaf nodes because buffer immediately above each node has been flushed
	a->max_msn_applied_to_node_on_disk = msn_max;
	b->max_msn_applied_to_node_on_disk = msn_max;
    }
}


//
// Takes as input a locked node and a childnum_to_merge
// As output, two of node's children are merged or rebalanced, and node is unlocked
//
static void
brt_merge_child (struct brt_header* h, BRTNODE node, int childnum_to_merge, BOOL *did_react, bool started_at_root)
{
    if (node->n_children < 2) {
        toku_unpin_brtnode_off_client_thread(h, node);
        return; // if no siblings, we are merged as best we can.
    }
    toku_assert_entire_node_in_memory(node);

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
    assert(childnumb < node->n_children);

    assert(node->height>0);


    // We suspect that at least one of the children is fusible, but they might not be.
    // for test
    call_flusher_thread_callback(ft_flush_before_merge);

    BRTNODE childa, childb;
    {
	u_int32_t childfullhash = compute_child_fullhash(h->cf, node, childnuma);
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, h);
	toku_pin_brtnode_off_client_thread(h, BP_BLOCKNUM(node, childnuma), childfullhash, &bfe, 1, &node, &childa);
    }
    {
        BRTNODE dep_nodes[2];
        dep_nodes[0] = node;
        dep_nodes[1] = childa;
	u_int32_t childfullhash = compute_child_fullhash(h->cf, node, childnumb);
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, h);
	toku_pin_brtnode_off_client_thread(h, BP_BLOCKNUM(node, childnumb), childfullhash, &bfe, 2, dep_nodes, &childb);
    }

    if (toku_bnc_n_entries(BNC(node,childnuma))>0) {
        flush_this_child(h, node, childa, childnuma, started_at_root);
    }
    if (toku_bnc_n_entries(BNC(node,childnumb))>0) {
        flush_this_child(h, node, childb, childnumb, started_at_root);
    }


    //
    //prelock cachetable, do checkpointing
    //
    toku_cachetable_prelock(h->cf);
    BRTNODE dependent_nodes[3];
    dependent_nodes[0] = node;
    dependent_nodes[1] = childa;
    dependent_nodes[2] = childb;
    checkpoint_nodes(h, 3, dependent_nodes);
    
    // now we have both children pinned in main memory, and cachetable locked,
    // so no checkpoints will occur.

    BOOL did_merge, did_rebalance;
    {
	struct kv_pair *splitk_kvpair = 0;
	struct kv_pair *old_split_key = node->childkeys[childnuma];
	unsigned int deleted_size = toku_brt_pivot_key_len(old_split_key);
	maybe_merge_pinned_nodes(node, node->childkeys[childnuma], childa, childb, &did_merge, &did_rebalance, &splitk_kvpair);
	if (childa->height>0) { int i; for (i=0; i+1<childa->n_children; i++) assert(childa->childkeys[i]); }
	//toku_verify_estimates(t,childa);
	// the tree did react if a merge (did_merge) or rebalance (new spkit key) occurred
	*did_react = (BOOL)(did_merge || did_rebalance);
	if (did_merge) assert(!splitk_kvpair); else assert(splitk_kvpair);

	node->totalchildkeylens -= deleted_size; // The key was free()'d inside the maybe_merge_pinned_nodes.

	if (did_merge) {
            destroy_nonleaf_childinfo(BNC(node, childnumb));
	    set_BNULL(node, childnumb);
	    node->n_children--;
	    memmove(&node->bp[childnumb],
		    &node->bp[childnumb+1],
		    (node->n_children-childnumb)*sizeof(node->bp[0]));
	    REALLOC_N(node->n_children, node->bp);
	    memmove(&node->childkeys[childnuma],
		    &node->childkeys[childnuma+1],
		    (node->n_children-childnumb)*sizeof(node->childkeys[0]));
	    REALLOC_N(node->n_children-1, node->childkeys);
	    assert(BP_BLOCKNUM(node, childnuma).b == childa->thisnodename.b);
	    childa->dirty = 1; // just to make sure
	    childb->dirty = 1; // just to make sure
	} else {
	    assert(splitk_kvpair);
	    // If we didn't merge the nodes, then we need the correct pivot.
	    node->childkeys[childnuma] = splitk_kvpair;
	    node->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[childnuma]);
	    node->dirty = 1;
	}
    }
    //
    // now we possibly flush the children
    //
    if (did_merge) {
	BLOCKNUM bn = childb->thisnodename;
	int rrb = toku_cachetable_unpin_and_remove(h->cf, bn, TRUE);
	assert(rrb==0);
	toku_free_blocknum(h->blocktable, &bn, h);
        // unlock cachetable
        toku_cachetable_unlock(h->cf);
        // for test
        call_flusher_thread_callback(ft_flush_after_merge);

        // unlock the parent
        assert(node->dirty);
        toku_unpin_brtnode_off_client_thread(h, node);
    } 
    else {
        // unlock cachetable
        toku_cachetable_unlock(h->cf);
        // for test
        call_flusher_thread_callback(ft_flush_after_rebalance);

        // unlock the parent
        assert(node->dirty);
        toku_unpin_brtnode_off_client_thread(h, node);
        toku_unpin_brtnode_off_client_thread(h, childb);
    }
    if (childa->height > 0 && nonleaf_node_is_gorged(childa)) {
        flush_some_child(h, childa, NULL, 0, started_at_root);
    }
    else {
        toku_unpin_brtnode_off_client_thread(h, childa);
    }
}

static void
brt_handle_maybe_reactive_root (BRT brt, CACHEKEY *rootp, BRTNODE *nodep) {
    BRTNODE node = *nodep;
    toku_assert_entire_node_in_memory(node);
    enum reactivity re = get_node_reactivity(node);
    switch (re) {
    case RE_STABLE:
	return;
    case RE_FISSIBLE:
	// The root node should split, so make a new root.
	{
	    BRTNODE nodea,nodeb;
	    DBT splitk;
	    assert(brt->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
            //
            // This happens on the client thread with the ydb lock, so it is safe to
            // not pass in dependent nodes. Although if we wanted to, we could pass
            // in just node. That would be correct.
            //
	    if (node->height==0) {
		brtleaf_split(brt->h, node, &nodea, &nodeb, &splitk, TRUE, 0, NULL);
	    } else {
		brt_nonleaf_split(brt->h, node, &nodea, &nodeb, &splitk, 0, NULL);
	    }
	    brt_init_new_root(brt, nodea, nodeb, splitk, rootp, nodep);
	    return;
	}
    case RE_FUSIBLE:
	return; // Cannot merge anything at the root, so return happy.
    }
    abort(); // cannot happen
}

//
// This returns true if the node MAY be reactive,
// false is we are absolutely sure that it is NOT reactive
// The reason for inaccuracy is that the node may be
// a leaf node that is not entirely in memory. If so, then
// we cannot be sure if the node is reactive
//
static bool may_node_be_reactive(BRTNODE node) {
    if (node->height == 0) return true;
    else {
        return (get_nonleaf_reactivity(node) != RE_STABLE);
    }
}

static void find_heaviest_child (BRTNODE node, int *childnum) {
    int max_child = 0;
    int max_weight = toku_bnc_nbytesinbuf(BNC(node, 0)) + BP_WORKDONE(node, 0);
    int i;

    if (0) printf("%s:%d weights: %d", __FILE__, __LINE__, max_weight);
    assert(node->n_children>0);
    for (i=1; i<node->n_children; i++) {
        if (BP_WORKDONE(node,i)) {
            assert(toku_bnc_nbytesinbuf(BNC(node,i)) > 0);
        }
        int this_weight = toku_bnc_nbytesinbuf(BNC(node,i)) + BP_WORKDONE(node,i);;
        if (0) printf(" %d", this_weight);
        if (max_weight < this_weight) {
            max_child = i;
            max_weight = this_weight;
        }
    }
    *childnum = max_child;
    if (0) printf("\n");
}

int
toku_bnc_flush_to_child(
    brt_compare_func compare_fun, 
    brt_update_func update_fun, 
    DESCRIPTOR desc, 
    CACHEFILE cf,
    NONLEAF_CHILDINFO bnc, 
    BRTNODE child
    )
{
    assert(toku_fifo_n_entries(bnc->buffer)>0);
    OMT snapshot_txnids, live_list_reverse;
    TOKULOGGER logger = toku_cachefile_logger(cf);
    if (child->height == 0 && logger) {
        toku_pthread_mutex_lock(&logger->txn_list_lock);
        int r = toku_omt_clone_noptr(&snapshot_txnids, logger->snapshot_txnids);
        assert_zero(r);
        r = toku_omt_clone_pool(&live_list_reverse, logger->live_list_reverse, sizeof(XID_PAIR_S));
        assert_zero(r);
	size_t buffsize = bnc->n_bytes_in_buffer; 
	brt_status.msg_bytes_out += buffsize;   // take advantage of surrounding mutex
	brt_status.msg_bytes_curr -= buffsize;  // may be misleading if there's a broadcast message in there
	toku_pthread_mutex_unlock(&logger->txn_list_lock);
    } else {
        snapshot_txnids = NULL;
        live_list_reverse = NULL;
    }

    FIFO_ITERATE(
        bnc->buffer, key, keylen, val, vallen, type, msn, xids, is_fresh,
        ({
            DBT hk,hv;
            BRT_MSG_S brtcmd = { (enum brt_msg_type)type, msn, xids, .u.id= {toku_fill_dbt(&hk, key, keylen),
                                                                             toku_fill_dbt(&hv, val, vallen)} };
            brtnode_put_cmd(
                compare_fun,
                update_fun,
                desc,
                child, 
                &brtcmd, 
                is_fresh, 
                snapshot_txnids, 
                live_list_reverse
                );
        }));

    if (snapshot_txnids) {
        toku_omt_destroy(&snapshot_txnids);
    }
    if (live_list_reverse) {
        OMTVALUE v;
        int r = toku_omt_fetch(live_list_reverse, 0, &v);
        if (r == 0) {
            toku_free(v);
        }
        toku_omt_destroy(&live_list_reverse);
    }
    return 0;
}

static void bring_node_fully_into_memory(BRTNODE node, struct brt_header* h)
{
    struct brtnode_fetch_extra bfe;
    PAIR_ATTR attr;
    int fd = toku_cachefile_fd(h->cf);
    fill_bfe_for_full_read(&bfe, h);
    toku_brtnode_pf_callback(node, &bfe, fd, &attr);
}

static void
maybe_destroy_child_blbs(BRTNODE node, BRTNODE child)
{
    if (child->height == 0 && !child->dirty) {
        for (int i = 0; i < child->n_children; ++i) {
            if (BP_STATE(child, i) == PT_AVAIL &&
                node->max_msn_applied_to_node_on_disk.msn < BLB_MAX_MSN_APPLIED(child, i).msn) {
                BASEMENTNODE bn = BLB(child, i);
		struct mempool * mp = &bn->buffer_mempool;
		toku_mempool_destroy(mp);
                destroy_basement_node(bn);
                set_BNULL(child,i);
                BP_STATE(child,i) = PT_ON_DISK;
            }
        }
    }
}

static void
update_flush_status(BRTNODE UU(parent), BRTNODE child, int cascades)
{
    brt_status.flush_total++;
    if (cascades > 0) {
        brt_status.flush_cascades++;
        switch (cascades) {
        case 1:
            brt_status.flush_cascades_1++; break;
        case 2:
            brt_status.flush_cascades_2++; break;
        case 3:
            brt_status.flush_cascades_3++; break;
        case 4:
            brt_status.flush_cascades_4++; break;
        case 5:
            brt_status.flush_cascades_5++; break;
        default:
            brt_status.flush_cascades_gt_5++; break;
        }
    }
    bool flush_needs_io = false;
    for (int i = 0; !flush_needs_io && i < child->n_children; ++i) {
        if (BP_STATE(child, i) == PT_ON_DISK) {
            flush_needs_io = true;
        }
    }
    if (flush_needs_io) {
        brt_status.flush_needed_io++;
    } else {
        brt_status.flush_in_memory++;
    }
}

static void
flush_some_child (struct brt_header* h, BRTNODE parent, int *n_dirtied, int cascades, bool started_at_root)
// Effect: This function does the following:
//   - Pick a child of parent (the heaviest child), 
//   - flush from parent to child, 
//   - possibly split/merge child.
//   - if child is gorged, recursively proceed with child
//  Note that parent is already locked
//  Upon exit of this function, parent is unlocked and no new
//  new nodes (such as a child) remain locked
{
    bool parent_unpinned = false;
    assert(parent->height>0);
    toku_assert_entire_node_in_memory(parent);
    if (n_dirtied && !parent->dirty) {
        (*n_dirtied)++;
    }

    // pick the child we want to flush to
    int childnum;
    find_heaviest_child(parent, &childnum);
    assert(toku_bnc_n_entries(BNC(parent, childnum))>0);

    // for test
    call_flusher_thread_callback(ft_flush_before_child_pin);

    // get the child into memory
    int r;
    BLOCKNUM targetchild = BP_BLOCKNUM(parent, childnum);
    toku_verify_blocknum_allocated(h->blocktable, targetchild);
    u_int32_t childfullhash = compute_child_fullhash(h->cf, parent, childnum);
    BRTNODE child;
    struct brtnode_fetch_extra bfe;
    // Note that we don't read the entire node into memory yet.
    // The idea is let's try to do the minimum work before releasing the parent lock
    fill_bfe_for_min_read(&bfe, h);
    toku_pin_brtnode_off_client_thread(h, targetchild, childfullhash, &bfe, 1, &parent, &child);

    if (n_dirtied && !child->dirty) {
        (*n_dirtied)++;
    }
    update_flush_status(parent, child, cascades);

    // for test
    call_flusher_thread_callback(ft_flush_after_child_pin);

    if (!started_at_root) {
        maybe_destroy_child_blbs(parent, child);
    }

    //Note that at this point, we don't have the entire child in.
    // Let's do a quick check to see if the child may be reactive
    // If the child cannot be reactive, then we can safely unlock
    // the parent before finishing reading in the entire child node.
    bool may_child_be_reactive = may_node_be_reactive(child);

    assert(child->thisnodename.b!=0);
    //VERIFY_NODE(brt, child);

    parent->dirty = 1;

    // detach buffer
    BP_WORKDONE(parent, childnum) = 0;  // this buffer is drained, no work has been done by its contents
    NONLEAF_CHILDINFO bnc = BNC(parent, childnum);
    set_BNC(parent, childnum, toku_create_empty_nl());

    //
    // at this point, the buffer has been detached from the parent
    // and a new empty buffer has been placed in its stead
    // so, if we are absolutely sure that the child is not
    // reactive, we can unpin the parent
    //
    if (!may_child_be_reactive) {
        toku_unpin_brtnode_off_client_thread(h, parent);
        parent_unpinned = true;
    }

    //
    // now, if necessary, read/decompress the rest of child into memory,
    // so that we can proceed and apply the flush
    //
    bring_node_fully_into_memory(child, h);
    child->dirty = 1;

    // It is possible after reading in the entire child,
    // that we now know that the child is not reactive
    // if so, we can unpin parent right now
    // we wont be splitting/merging child
    // and we have already replaced the bnc
    // for the root with a fresh one
    enum reactivity child_re = get_node_reactivity(child);
    if (!parent_unpinned && child_re == RE_STABLE) {
        toku_unpin_brtnode_off_client_thread(h, parent);
        parent_unpinned = true;
    }

    // now we have a bnc to flush to the child
    r = toku_bnc_flush_to_child(
        h->compare_fun, 
        h->update_fun, 
        &h->descriptor, 
        h->cf, 
        bnc, 
        child
        ); 
    assert_zero(r);
    destroy_nonleaf_childinfo(bnc);

    // let's get the reactivity of the child again,
    // it is possible that the flush got rid of some values
    // and now the parent is no longer reactive
    child_re = get_node_reactivity(child);
    if (!started_at_root && child->height == 0 && child_re == RE_FUSIBLE) {
        // prevent merging leaf nodes, sometimes (when the cleaner thread
        // called us)
        child_re = RE_STABLE;
    }
    // if the parent has been unpinned above, then
    // this is our only option, even if the child is not stable
    // if the child is not stable, we'll handle it the next 
    // time we need to flush to the child
    if (parent_unpinned || child_re == RE_STABLE) {
        if (!parent_unpinned) {
            toku_unpin_brtnode_off_client_thread(h, parent);
        }
        //
        // it is the responsibility of flush_some_child to unpin parent
        //
        if (child->height > 0 && nonleaf_node_is_gorged(child)) {
            flush_some_child(h, child, n_dirtied, cascades+1, started_at_root);
        }
        else {
            toku_unpin_brtnode_off_client_thread(h, child);
        }
    }
    else if (child_re == RE_FISSIBLE) {
        //
        // it is responsibility of brt_split_child to unlock nodes
        // of parent and child as it sees fit
        //
        brt_split_child(h, parent, childnum, child, started_at_root);
    }
    else if (child_re == RE_FUSIBLE) {
        BOOL did_react;
        //
        // There is probably a way to pass BRTNODE child
        // into brt_merge_child, but for simplicity for now,
        // we are just going to unpin child and
        // let brt_merge_child pin it again
        //
        toku_unpin_brtnode_off_client_thread(h, child);
        //
        //
        // it is responsibility of brt_merge_child to unlock parent
        //
        brt_merge_child(h, parent, childnum, &did_react, started_at_root);
    }
    else {
        assert(FALSE);
    }
}

static void
flush_this_child (struct brt_header* h, BRTNODE node, BRTNODE child, int childnum, bool started_at_root)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
{
    update_flush_status(node, child, 0);
    int r;
    toku_assert_entire_node_in_memory(node);
    if (!started_at_root) {
        maybe_destroy_child_blbs(node, child);
    }
    bring_node_fully_into_memory(child, h);
    toku_assert_entire_node_in_memory(child);
    assert(node->height>0);
    assert(child->thisnodename.b!=0);
    // VERIFY_NODE does not work off client thread as of now
    //VERIFY_NODE(t, child);
    node->dirty = TRUE;
    child->dirty = TRUE;

    BP_WORKDONE(node, childnum) = 0;  // this buffer is drained, no work has been done by its contents
    NONLEAF_CHILDINFO bnc = BNC(node, childnum);
    set_BNC(node, childnum, toku_create_empty_nl());

    // now we have a bnc to flush to the child
    r = toku_bnc_flush_to_child(h->compare_fun, h->update_fun, &h->descriptor, h->cf, bnc, child); assert_zero(r);
    destroy_nonleaf_childinfo(bnc);

}

static void
brtnode_put_cmd (
    brt_compare_func compare_fun,
    brt_update_func update_fun,
    DESCRIPTOR desc,
    BRTNODE node, 
    BRT_MSG cmd, 
    bool is_fresh, 
    OMT snapshot_txnids, 
    OMT live_list_reverse
    )
// Effect: Push CMD into the subtree rooted at NODE.
//   If NODE is a leaf, then
//	put CMD into leaf, applying it to the leafentries
//   If NODE is a nonleaf, then push the cmd into the FIFO(s) of the relevent child(ren).
//   The node may become overfull.  That's not our problem.
{
    toku_assert_entire_node_in_memory(node);
    if (node->height==0) {
	// we need to make sure that after doing all the put_cmd operations 
	// that the tree above is completely flushed out, 
	// otherwise may have an inconsistency (part of the data is there, and part isn't)
	bool made_change = false;
        uint64_t workdone = 0;
        toku_apply_cmd_to_leaf(
            compare_fun,
            update_fun,
            desc,
            node, 
            cmd, 
            &made_change, 
            &workdone, 
            snapshot_txnids, 
            live_list_reverse
            );
    } else {
	brt_nonleaf_put_cmd(compare_fun, desc, node, cmd, is_fresh);
    }
}

static const struct pivot_bounds infinite_bounds = {.lower_bound_exclusive=NULL,
						    .upper_bound_inclusive=NULL};


// Effect: applies the cmd to the leaf if the appropriate basement node is in memory.
//           If the appropriate basement node is not in memory, then nothing gets applied
//           If the appropriate basement node must be in memory, it is the caller's responsibility to ensure
//             that it is
void toku_apply_cmd_to_leaf(
    brt_compare_func compare_fun, 
    brt_update_func update_fun, 
    DESCRIPTOR desc, 
    BRTNODE node, 
    BRT_MSG cmd, 
    bool *made_change, 
    uint64_t *workdone, 
    OMT snapshot_txnids, 
    OMT live_list_reverse
    ) 
{
    VERIFY_NODE(t, node);
    // ignore messages that have already been applied to this leaf

    if (brt_msg_applies_once(cmd)) {
        unsigned int childnum = toku_brtnode_which_child(node, cmd->u.id.key, desc, compare_fun);
        // only apply the message if we have an available basement node that is up to date
        // we know it is up to date if partition_requires_msg_application returns FALSE
        if (BP_STATE(node,childnum) == PT_AVAIL) {
            if (cmd->msn.msn > BLB(node, childnum)->max_msn_applied.msn) {
                BLB(node, childnum)->max_msn_applied = cmd->msn;
                brt_leaf_put_cmd(compare_fun,
                                 update_fun,
                                 desc,
                                 BLB(node, childnum),
                                 cmd,
                                 made_change,
                                 workdone,
                                 snapshot_txnids,
                                 live_list_reverse);
            } else {
                brt_status.msn_discards++;
            }
        }
    }
    else if (brt_msg_applies_all(cmd)) {
        bool bn_made_change = false;
        for (int childnum=0; childnum<node->n_children; childnum++) {
            // only apply the message if we have an available basement node that is up to date
            // we know it is up to date if partition_requires_msg_application returns FALSE
            if (BP_STATE(node,childnum) == PT_AVAIL) {
                if (cmd->msn.msn > BLB(node, childnum)->max_msn_applied.msn) {
                    BLB(node, childnum)->max_msn_applied = cmd->msn;
                    brt_leaf_put_cmd(compare_fun,
                                     update_fun,
                                     desc,
                                     BLB(node, childnum),
                                     cmd,
                                     &bn_made_change,
                                     workdone,
                                     snapshot_txnids,
                                     live_list_reverse);
                    if (bn_made_change) *made_change = 1;
                } else {
                    brt_status.msn_discards++;
                }
            }
        }
    }
    else if (!brt_msg_does_nothing(cmd)) {
        assert(FALSE);
    }
    VERIFY_NODE(t, node);
}


static void push_something_at_root (BRT brt, BRTNODE *nodep, BRT_MSG cmd)
// Effect:  Put CMD into brt by descending into the tree as deeply as we can
//   without performing I/O (but we must fetch the root),
//   bypassing only empty FIFOs
//   If the cmd is a broadcast message, we copy the message as needed as we descend the tree so that each relevant subtree receives the message.
//   At the end of the descent, we are either at a leaf, or we hit a nonempty FIFO.
//     If it's a leaf, and the leaf is gorged or hungry, then we split the leaf or merge it with the neighbor.
//	Note: for split operations, no disk I/O is required.  For merges, I/O may be required, so for a broadcast delete, quite a bit
//	 of I/O could be induced in the worst case.
//     If it's a nonleaf, and the node is gorged or hungry, then we flush everything in the heaviest fifo to the child.
//	 During flushing, we allow the child to become gorged.
//	   (And for broadcast messages, we simply place the messages into all the relevant fifos of the child, rather than trying to descend.)
//	 After flushing to a child, if the child is gorged (underful), then
//	     if the child is leaf, we split (merge) it
//	     if the child is a nonleaf, we flush the heaviest child recursively.
//	 Note: After flushing, a node could still be gorged (or possibly hungry.)  We let it remain so.
//	 Note: During the initial descent, we may gorged many nonleaf nodes.  We wish to flush only one nonleaf node at each level.
{
    BRTNODE node = *nodep;
    toku_assert_entire_node_in_memory(node);
    if (node->height==0) {
	// Must special case height 0, since brtnode_put_cmd() doesn't modify leaves.
	// Part of the problem is: if the node is in memory, then it was updated as part of the in-memory operation.
	// If the root node is not in memory, then we must apply it.
	bool made_dirty = 0;
	uint64_t workdone_ignore = 0;  // ignore workdone for root-leaf node
	// not up to date, which means the get_and_pin actually fetched it
	// into memory.
        TOKULOGGER logger = toku_cachefile_logger(brt->cf);
        OMT snapshot_txnids = logger ? logger->snapshot_txnids : NULL;
        OMT live_list_reverse = logger ? logger->live_list_reverse : NULL;
        // passing down snapshot_txnids and live_list_reverse directly
        // since we're holding the ydb lock.  TODO: verify this is correct
        toku_apply_cmd_to_leaf(
            brt->compare_fun,
            brt->update_fun,
            &brt->h->descriptor,
            node, 
            cmd, 
            &made_dirty, 
            &workdone_ignore, 
            snapshot_txnids, 
            live_list_reverse
            );
        MSN cmd_msn = cmd->msn;
        invariant(cmd_msn.msn > node->max_msn_applied_to_node_on_disk.msn);
	// max_msn_applied_to_node_on_disk is normally set only when leaf is serialized, 
	// but needs to be done here (for root leaf) so msn can be set in new commands.
        node->max_msn_applied_to_node_on_disk = cmd_msn;
	node->dirty = 1;
   } else {
	uint64_t msgsize = brt_msg_size(cmd);
	brt_status.msg_bytes_in += msgsize;
	brt_status.msg_bytes_curr += msgsize;
	if (brt_status.msg_bytes_curr > brt_status.msg_bytes_max)
	    brt_status.msg_bytes_max = brt_status.msg_bytes_curr;
	brt_status.msg_num++;
	if (brt_msg_applies_all(cmd))
	    brt_status.msg_num_broadcast++;	
        brt_nonleaf_put_cmd(brt->compare_fun, &brt->h->descriptor, node, cmd, true);
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

struct flusher_extra {
    struct brt_header* h;
    BRTNODE node;
    NONLEAF_CHILDINFO bnc;
};

//
// This is the function that gets called by a
// background thread. Its purpose is to complete
// a flush, and possibly do a split/merge.
//
static void flush_node_fun(void *fe_v)
{
    int r;
    struct flusher_extra* fe = fe_v;
    // The node that has been placed on the background
    // thread may not be fully in memory. Some message
    // buffers may be compressed. Before performing
    // any operations, we must first make sure
    // the node is fully in memory
    //
    // If we have a bnc, that means fe->node is a child, and we've already
    // destroyed its basement nodes if necessary, so we now need to either
    // read them back in, or just do the regular partial fetch.  If we
    // don't, that means fe->node is a parent, so we need to do this anyway.
    bring_node_fully_into_memory(fe->node,fe->h);
    fe->node->dirty = 1;

    if (fe->bnc) {
        // In this case, we have a bnc to flush to a node

        // for test purposes
        call_flusher_thread_callback(ft_flush_before_applying_inbox);

        r = toku_bnc_flush_to_child(
            fe->h->compare_fun,
            fe->h->update_fun,
            &fe->h->descriptor,
            fe->h->cf,
            fe->bnc, 
            fe->node
            ); 
        assert_zero(r);
        destroy_nonleaf_childinfo(fe->bnc);

        // after the flush has completed, now check to see if the node needs flushing
        // If so, call flush_some_child on the node, and it is the responsibility
        // of flush_some_child to unlock the node
        // otherwise, we unlock the node here.
        if (fe->node->height > 0 && nonleaf_node_is_gorged(fe->node)) {
            flush_some_child(fe->h, fe->node, NULL, 0, true);
        }
        else {
            toku_unpin_brtnode_off_client_thread(fe->h,fe->node);
        }
    }
    else {
        // In this case, we were just passed a node with no
        // bnc, which means we are tasked with flushing some
        // buffer in the node.
        // It is the responsibility of flush_some_child to unlock the node
        flush_some_child(fe->h, fe->node, NULL, 0, true);
    }
    remove_background_job(fe->h->cf, false);
    toku_free(fe);
}

static void
place_node_and_bnc_on_background_thread(
    BRT brt,
    BRTNODE node,
    NONLEAF_CHILDINFO bnc
    )
{
    struct flusher_extra* fe = NULL;
    fe = toku_xmalloc(sizeof(struct flusher_extra));
    assert(fe);
    fe->h = brt->h;
    fe->node = node;
    fe->bnc = bnc;
    cachefile_kibbutz_enq(brt->cf, flush_node_fun, fe);
}

//
// This takes as input a gorged, locked,  non-leaf node named parent
// and sets up a flush to be done in the background.
// The flush is setup like this:
//  - We call maybe_get_and_pin_clean on the child we want to flush to in order to try to lock the child
//  - if we successfully pin the child, and the child does not need to be split or merged
//     then we detach the buffer, place the child and buffer onto a background thread, and 
//     have the flush complete in the background, and unlock the parent. The child will be
//     unlocked on the background thread
//  - if any of the above does not happen (child cannot be locked, 
//     child needs to be split/merged), then we place the parent on the background thread.
//     The parent will be unlocked on the background thread
//
static void
flush_node_on_background_thread(BRT brt, BRTNODE parent)
{
    //
    // first let's see if we can detach buffer on client thread
    // and pick the child we want to flush to
    //
    int childnum;
    find_heaviest_child(parent, &childnum);
    assert(toku_bnc_n_entries(BNC(parent, childnum))>0);
    //
    // see if we can pin the child
    //
    void *node_v;
    BRTNODE child;
    u_int32_t childfullhash = compute_child_fullhash(brt->cf, parent, childnum);
    int r = toku_cachetable_maybe_get_and_pin_clean (
        brt->cf,
        BP_BLOCKNUM(parent,childnum),
        childfullhash,
        &node_v
        );
    if (r != 0) {
        // In this case, we could not lock the child, so just place the parent on the background thread
        // In the callback, we will use flush_some_child, which checks to
        // see if we should blow away the old basement nodes.
        place_node_and_bnc_on_background_thread(brt, parent, NULL);
    }
    else {
        //
        // successfully locked child
        //
        child = node_v;
        bool may_child_be_reactive = may_node_be_reactive(child);
        if (!may_child_be_reactive) {
            // We're going to unpin the parent, so before we do, we must
            // check to see if we need to blow away the basement nodes to
            // keep the MSN invariants intact.
            maybe_destroy_child_blbs(parent, child);

            //
            // can detach buffer and unpin root here
            //
            parent->dirty = 1;
            BP_WORKDONE(parent, childnum) = 0;  // this buffer is drained, no work has been done by its contents
            NONLEAF_CHILDINFO bnc = BNC(parent, childnum);
            set_BNC(parent, childnum, toku_create_empty_nl());

            //
            // at this point, the buffer has been detached from the parent
            // and a new empty buffer has been placed in its stead
            // so, because we know for sure the child is not
            // reactive, we can unpin the parent
            //
            place_node_and_bnc_on_background_thread(brt, child, bnc);
            toku_unpin_brtnode(brt, parent);
        }
        else {
            // because the child may be reactive, we need to
            // put parent on background thread.
            // As a result, we unlock the child here.
            toku_unpin_brtnode(brt, child);
            // Again, we'll have the parent on the background thread, so
            // we don't need to destroy the basement nodes yet.
            place_node_and_bnc_on_background_thread(brt, parent, NULL);
        }
    }
}

int 
toku_brt_root_put_cmd (BRT brt, BRT_MSG_S * cmd)
// Effect:
//  - assign msn to cmd	 
//  - push the cmd into the brt
//  - cmd will set new msn in tree
{
    BRTNODE node;
    CACHEKEY *rootp;
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    assert(brt->h);
    u_int32_t fullhash;
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    // get the root node
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->h);
    toku_pin_brtnode_holding_lock(brt, *rootp, fullhash,(ANCESTORS)NULL, &infinite_bounds, &bfe, TRUE, &node);

    toku_assert_entire_node_in_memory(node);
    cmd->msn.msn = node->max_msn_applied_to_node_on_disk.msn + 1;
    // Note, the lower level function that filters messages based on msn, 
    // (brt_leaf_put_cmd() or brt_nonleaf_put_cmd()) will capture the msn and
    // store it in the relevant node, including the root node.	This is how the 
    // new msn is set in the root.

    VERIFY_NODE(brt, node);
    assert(node->fullhash==fullhash);
    brt_verify_flags(brt, node);

    // first handle a reactive root, then put in the message
    brt_handle_maybe_reactive_root(brt, rootp, &node);

    push_something_at_root(brt, &node, cmd);
    // verify that msn of latest message was captured in root node (push_something_at_root() did not release ydb lock)
    invariant(cmd->msn.msn == node->max_msn_applied_to_node_on_disk.msn);

    // if we call flush_some_child, then that function unpins the root
    // otherwise, we unpin ourselves
    if (node->height > 0 && nonleaf_node_is_gorged(node)) {
        flush_node_on_background_thread(brt, node);
    }
    else {
        toku_unpin_brtnode(brt, node);  // unpin root
    }

    return 0;
}

// Effect: Insert the key-val pair into brt.
int toku_brt_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    return toku_brt_maybe_insert(brt, key, val, txn, FALSE, ZERO_LSN, TRUE, BRT_INSERT);
}

int
toku_brt_load_recovery(TOKUTXN txn, char const * old_iname, char const * new_iname, int do_fsync, int do_log, LSN *load_lsn) {
    int r = 0;
    assert(txn);
    toku_txn_force_fsync_on_commit(txn);  //If the txn commits, the commit MUST be in the log
					  //before the (old) file is actually unlinked
    TOKULOGGER logger = toku_txn_logger(txn);

    BYTESTRING old_iname_bs = {.len=strlen(old_iname), .data=(char*)old_iname};
    BYTESTRING new_iname_bs = {.len=strlen(new_iname), .data=(char*)new_iname};
    r = toku_logger_save_rollback_load(txn, &old_iname_bs, &new_iname_bs);
    if (r==0 && do_log && logger) {
	TXNID xid = toku_txn_get_txnid(txn);
	r = toku_log_load(logger, load_lsn, do_fsync, xid, old_iname_bs, new_iname_bs);
    }
    return r;
}

// 2954
// this function handles the tasks needed to be recoverable
//  - write to rollback log
//  - write to recovery log
int
toku_brt_hot_index_recovery(TOKUTXN txn, FILENUMS filenums, int do_fsync, int do_log, LSN *hot_index_lsn)
{
    int r = 0;
    assert(txn);
    TOKULOGGER logger = toku_txn_logger(txn);

    // write to the rollback log
    r = toku_logger_save_rollback_hot_index(txn, &filenums);
    if ( r==0 && do_log && logger) {
	TXNID xid = toku_txn_get_txnid(txn);
	// write to the recovery log
	r = toku_log_hot_index(logger, hot_index_lsn, do_fsync, xid, filenums);
    }
    return r;
}

static int brt_optimize (BRT brt, BOOL upgrade);

// Effect: Optimize the brt.
int
toku_brt_optimize (BRT brt) {
    int r = brt_optimize(brt, FALSE);
    return r;
}

int
toku_brt_optimize_for_upgrade (BRT brt) {
    int r = brt_optimize(brt, TRUE);
    return r;
}

static int
brt_optimize (BRT brt, BOOL upgrade) {
    int r = 0;

    TXNID oldest = TXNID_NONE_LIVING;
    if (!upgrade) {
	TOKULOGGER logger = toku_cachefile_logger(brt->cf);
	oldest = toku_logger_get_oldest_living_xid(logger, NULL);
    }

    XIDS root_xids = xids_get_root_xids();
    XIDS message_xids;
    if (oldest == TXNID_NONE_LIVING) {
	message_xids = root_xids;
    }
    else {
	r = xids_create_child(root_xids, &message_xids, oldest);
	invariant(r==0);
    }

    DBT key;
    DBT val;
    toku_init_dbt(&key);
    toku_init_dbt(&val);
    if (upgrade) {
	// maybe there's a better place than the val dbt to put the version, but it seems harmless and is convenient
	toku_fill_dbt(&val, &this_version, sizeof(this_version));  
	BRT_MSG_S brtcmd = { BRT_OPTIMIZE_FOR_UPGRADE, ZERO_MSN, message_xids, .u.id={&key,&val}};
	r = toku_brt_root_put_cmd(brt, &brtcmd);
    }
    else {
	BRT_MSG_S brtcmd = { BRT_OPTIMIZE, ZERO_MSN, message_xids, .u.id={&key,&val}};
	r = toku_brt_root_put_cmd(brt, &brtcmd);
    }
    xids_destroy(&message_xids);
    return r;
}


int
toku_brt_load(BRT brt, TOKUTXN txn, char const * new_iname, int do_fsync, LSN *load_lsn) {
    int r = 0;
    char const * old_iname = toku_cachefile_fname_in_env(brt->cf);
    int do_log = 1;
    r = toku_brt_load_recovery(txn, old_iname, new_iname, do_fsync, do_log, load_lsn);
    return r;
}

// 2954
// brt actions for logging hot index filenums
int 
toku_brt_hot_index(BRT brt __attribute__ ((unused)), TOKUTXN txn, FILENUMS filenums, int do_fsync, LSN *lsn) {
    int r = 0;
    int do_log = 1;
    r = toku_brt_hot_index_recovery(txn, filenums, do_fsync, do_log, lsn);
    return r;
}

int 
toku_brt_log_put (TOKUTXN txn, BRT brt, const DBT *key, const DBT *val) {
    int r = 0;
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger && brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING keybs = {.len=key->size, .data=key->data};
	BYTESTRING valbs = {.len=val->size, .data=val->data};
	TXNID xid = toku_txn_get_txnid(txn);
	// if (type == BRT_INSERT)
	    r = toku_log_enq_insert(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs, valbs);
	// else
	    // r = toku_log_enq_insert_no_overwrite(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs, valbs);
    }
    return r;
}

int
toku_brt_log_put_multiple (TOKUTXN txn, BRT src_brt, BRT *brts, int num_brts, const DBT *key, const DBT *val) {
    int r = 0;
    assert(txn);
    assert(num_brts > 0);
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
	FILENUM	 fnums[num_brts];
	int i;
	int num_unsuppressed_brts = 0;
	for (i = 0; i < num_brts; i++) {
	    if (brts[i]->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
		//Logging not suppressed for this brt.
		fnums[num_unsuppressed_brts++] = toku_cachefile_filenum(brts[i]->cf);
	    }
	}
	if (num_unsuppressed_brts) {
	    FILENUMS filenums = {.num = num_unsuppressed_brts, .filenums = fnums};
	    BYTESTRING keybs = {.len=key->size, .data=key->data};
	    BYTESTRING valbs = {.len=val->size, .data=val->data};
	    TXNID xid = toku_txn_get_txnid(txn);
	    FILENUM src_filenum = src_brt ? toku_cachefile_filenum(src_brt->cf) : FILENUM_NONE;
	    r = toku_log_enq_insert_multiple(logger, (LSN*)0, 0, src_filenum, filenums, xid, keybs, valbs);
	}
    }
    return r;
}

int 
toku_brt_maybe_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging, enum brt_msg_type type) {
    assert(type==BRT_INSERT || type==BRT_INSERT_NO_OVERWRITE);
    int r = 0;
    XIDS message_xids = xids_get_root_xids(); //By default use committed messages
    TXNID xid = toku_txn_get_txnid(txn);
    if (txn) {
	if (brt->h->txnid_that_created_or_locked_when_empty != xid) {
	    BYTESTRING keybs  = {key->size, key->data};
	    r = toku_logger_save_rollback_cmdinsert(txn, toku_cachefile_filenum(brt->cf), &keybs);
	    if (r!=0) return r;
	    r = toku_txn_note_brt(txn, brt);
	    if (r!=0) return r;
	    //We have transactions, and this is not 2440.  We must send the full root-to-leaf-path
	    message_xids = toku_txn_get_xids(txn);
	}
	else if (txn->ancestor_txnid64 != brt->h->root_xid_that_created) {
	    //We have transactions, and this is 2440, however the txn doing 2440 did not create the dictionary.	 We must send the full root-to-leaf-path
	    message_xids = toku_txn_get_xids(txn);
	}
    }
    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger &&
	brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING keybs = {.len=key->size, .data=key->data};
	BYTESTRING valbs = {.len=val->size, .data=val->data};
	if (type == BRT_INSERT) {
	    r = toku_log_enq_insert(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs, valbs);
	}
	else {
	    r = toku_log_enq_insert_no_overwrite(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs, valbs);
	}
	if (r!=0) return r;
    }

    LSN treelsn;
    if (oplsn_valid && oplsn.lsn <= (treelsn = toku_brt_checkpoint_lsn(brt)).lsn) {
	r = 0;
    } else {
	r = toku_brt_send_insert(brt, key, val, message_xids, type);
    }
    return r;
}

static int
brt_send_update_msg(BRT brt, BRT_MSG_S *msg, TOKUTXN txn) {
    msg->xids = (txn
		 ? toku_txn_get_xids(txn)
		 : xids_get_root_xids());
    int r = toku_brt_root_put_cmd(brt, msg);
    return r;
}

int
toku_brt_maybe_update(BRT brt, const DBT *key, const DBT *update_function_extra,
		      TOKUTXN txn, BOOL oplsn_valid, LSN oplsn,
		      BOOL do_logging) {
    int r = 0;

    TXNID xid = toku_txn_get_txnid(txn);
    if (txn) {
	BYTESTRING keybs = { key->size, key->data };
	r = toku_logger_save_rollback_cmdupdate(
	    txn, toku_cachefile_filenum(brt->cf), &keybs);
	if (r != 0) { goto cleanup; }
	r = toku_txn_note_brt(txn, brt);
	if (r != 0) { goto cleanup; }
    }

    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger &&
	brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING keybs = {.len=key->size, .data=key->data};
	BYTESTRING extrabs = {.len=update_function_extra->size,
			      .data=update_function_extra->data};
	r = toku_log_enq_update(logger, NULL, 0,
				toku_cachefile_filenum(brt->cf),
				xid, keybs, extrabs);
	if (r != 0) { goto cleanup; }
    }

    LSN treelsn;
    if (oplsn_valid &&
	oplsn.lsn <= (treelsn = toku_brt_checkpoint_lsn(brt)).lsn) {
	r = 0;
    } else {
	BRT_MSG_S msg = { BRT_UPDATE, ZERO_MSN, NULL,
			  .u.id = { key, update_function_extra }};
	r = brt_send_update_msg(brt, &msg, txn);
    }

cleanup:
    return r;
}

int
toku_brt_maybe_update_broadcast(BRT brt, const DBT *update_function_extra,
				TOKUTXN txn, BOOL oplsn_valid, LSN oplsn,
				BOOL do_logging, BOOL is_resetting_op) {
    int r = 0;

    TXNID xid = toku_txn_get_txnid(txn);
    u_int8_t  resetting = is_resetting_op ? 1 : 0;
    if (txn) {
	r = toku_logger_save_rollback_cmdupdatebroadcast(txn, toku_cachefile_filenum(brt->cf), resetting);
	if (r != 0) { goto cleanup; }
	r = toku_txn_note_brt(txn, brt);
	if (r != 0) { goto cleanup; }
    }

    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger &&
	brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING extrabs = {.len=update_function_extra->size,
			      .data=update_function_extra->data};
	r = toku_log_enq_updatebroadcast(logger, NULL, 0,
					 toku_cachefile_filenum(brt->cf),
					 xid, extrabs, resetting);
	if (r != 0) { goto cleanup; }
    }

    LSN treelsn;
    if (oplsn_valid &&
	oplsn.lsn <= (treelsn = toku_brt_checkpoint_lsn(brt)).lsn) {
	r = 0;
    } else {
	DBT nullkey;
	const DBT *nullkeyp = toku_init_dbt(&nullkey);
	BRT_MSG_S msg = { BRT_UPDATE_BROADCAST_ALL, ZERO_MSN, NULL,
			  .u.id = { nullkeyp, update_function_extra }};
	r = brt_send_update_msg(brt, &msg, txn);
    }

cleanup:
    return r;
}

int
toku_brt_send_insert(BRT brt, DBT *key, DBT *val, XIDS xids, enum brt_msg_type type) {
    BRT_MSG_S brtcmd = { type, ZERO_MSN, xids, .u.id = { key, val }};
    int r = toku_brt_root_put_cmd(brt, &brtcmd);
    return r;
}

int
toku_brt_send_commit_any(BRT brt, DBT *key, XIDS xids) {
    DBT val; 
    BRT_MSG_S brtcmd = { BRT_COMMIT_ANY, ZERO_MSN, xids, .u.id = { key, toku_init_dbt(&val) }};
    int r = toku_brt_root_put_cmd(brt, &brtcmd);
    return r;
}

int toku_brt_delete(BRT brt, DBT *key, TOKUTXN txn) {
    return toku_brt_maybe_delete(brt, key, txn, FALSE, ZERO_LSN, TRUE);
}

int
toku_brt_log_del(TOKUTXN txn, BRT brt, const DBT *key) {
    int r = 0;
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger && brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING keybs = {.len=key->size, .data=key->data};
	TXNID xid = toku_txn_get_txnid(txn);
	r = toku_log_enq_delete_any(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs);
    }
    return r;
}

int
toku_brt_log_del_multiple (TOKUTXN txn, BRT src_brt, BRT *brts, int num_brts, const DBT *key, const DBT *val) {
    int r = 0;
    assert(txn);
    assert(num_brts > 0);
    TOKULOGGER logger = toku_txn_logger(txn);
    if (logger) {
	FILENUM	 fnums[num_brts];
	int i;
	int num_unsuppressed_brts = 0;
	for (i = 0; i < num_brts; i++) {
	    if (brts[i]->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
		//Logging not suppressed for this brt.
		fnums[num_unsuppressed_brts++] = toku_cachefile_filenum(brts[i]->cf);
	    }
	}
	if (num_unsuppressed_brts) {
	    FILENUMS filenums = {.num = num_unsuppressed_brts, .filenums = fnums};
	    BYTESTRING keybs = {.len=key->size, .data=key->data};
	    BYTESTRING valbs = {.len=val->size, .data=val->data};
	    TXNID xid = toku_txn_get_txnid(txn);
	    FILENUM src_filenum = src_brt ? toku_cachefile_filenum(src_brt->cf) : FILENUM_NONE;
	    r = toku_log_enq_delete_multiple(logger, (LSN*)0, 0, src_filenum, filenums, xid, keybs, valbs);
	}
    }
    return r;
}

int 
toku_brt_maybe_delete(BRT brt, DBT *key, TOKUTXN txn, BOOL oplsn_valid, LSN oplsn, BOOL do_logging) {
    int r;
    XIDS message_xids = xids_get_root_xids(); //By default use committed messages
    TXNID xid = toku_txn_get_txnid(txn);
    if (txn) {
	if (brt->h->txnid_that_created_or_locked_when_empty != xid) {
	    BYTESTRING keybs  = {key->size, key->data};
	    r = toku_logger_save_rollback_cmddelete(txn, toku_cachefile_filenum(brt->cf), &keybs);
	    if (r!=0) return r;
	    r = toku_txn_note_brt(txn, brt);
	    if (r!=0) return r;
	    //We have transactions, and this is not 2440.  We must send the full root-to-leaf-path
	    message_xids = toku_txn_get_xids(txn);
	}
	else if (txn->ancestor_txnid64 != brt->h->root_xid_that_created) {
	    //We have transactions, and this is 2440, however the txn doing 2440 did not create the dictionary.	 We must send the full root-to-leaf-path
	    message_xids = toku_txn_get_xids(txn);
	}
    }
    TOKULOGGER logger = toku_txn_logger(txn);
    if (do_logging && logger &&
	brt->h->txnid_that_suppressed_recovery_logs == TXNID_NONE) {
	BYTESTRING keybs = {.len=key->size, .data=key->data};
	r = toku_log_enq_delete_any(logger, (LSN*)0, 0, toku_cachefile_filenum(brt->cf), xid, keybs);
	if (r!=0) return r;
    }

    LSN treelsn;
    if (oplsn_valid && oplsn.lsn <= (treelsn = toku_brt_checkpoint_lsn(brt)).lsn) {
	r = 0;
    } else {
	r = toku_brt_send_delete(brt, key, message_xids);
    }
    return r;
}

int
toku_brt_send_delete(BRT brt, DBT *key, XIDS xids) {
    DBT val; toku_init_dbt(&val);
    BRT_MSG_S brtcmd = { BRT_DELETE_ANY, ZERO_MSN, xids, .u.id = { key, &val }};
    int result = toku_brt_root_put_cmd(brt, &brtcmd);
    return result;
}

/* mempool support */

struct omt_compressor_state {
    struct mempool *new_kvspace;
    OMT omt;
};

static int move_it (OMTVALUE lev, u_int32_t idx, void *v) {
    LEAFENTRY le=lev;
    struct omt_compressor_state *oc = v;
    u_int32_t size = leafentry_memsize(le);
    LEAFENTRY newdata = toku_mempool_malloc(oc->new_kvspace, size, 1);
    lazy_assert(newdata); // we do this on a fresh mempool, so nothing bad shouldhapepn
    memcpy(newdata, le, size);
    toku_omt_set_at(oc->omt, newdata, idx);
    return 0;
}

// Compress things, and grow the mempool if needed.
// TODO 4092 should copy data to new memory, then call toku_mempool_destory() followed by toku_mempool_init()
static int omt_compress_kvspace (OMT omt, struct mempool *memp, size_t added_size, void **maybe_free) {
    u_int32_t total_size_needed = memp->free_offset-memp->frag_size + added_size;
    if (total_size_needed+total_size_needed/4 >= memp->size) {
        memp->size = total_size_needed+total_size_needed/4;
    }
    void *newmem = toku_xmalloc(memp->size);
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
            lazy_assert(v);
        }
    }
    return v;
}

/* ******************** open,close and create  ********************** */

// Test only function (not used in running system). This one has no env
int toku_open_brt (const char *fname, int is_create, BRT *newbrt, int nodesize, int basementnodesize, CACHETABLE cachetable, TOKUTXN txn,
		   int (*compare_fun)(DB *, const DBT*,const DBT*), DB *db) {
    BRT brt;
    int r;
    const int only_create = 0;

    r = toku_brt_create(&brt);
    if (r != 0)
	return r;
    r = toku_brt_set_nodesize(brt, nodesize); assert_zero(r);
    r = toku_brt_set_basementnodesize(brt, basementnodesize); assert_zero(r);
    r = toku_brt_set_bt_compare(brt, compare_fun); assert_zero(r);

    r = toku_brt_open(brt, fname, is_create, only_create, cachetable, txn, db);
    if (r != 0) {
	return r;
    }

    *newbrt = brt;
    return r;
}

static int setup_initial_brt_root_node (BRT t, BLOCKNUM blocknum) {
    BRTNODE XMALLOC(node);
    toku_initialize_empty_brtnode(node, blocknum, 0, 1, t->h->layout_version, t->h->nodesize, t->flags);
    BP_STATE(node,0) = PT_AVAIL;

    u_int32_t fullhash = toku_cachetable_hash(t->cf, blocknum);
    node->fullhash = fullhash;
    int r = toku_cachetable_put(t->cf, blocknum, fullhash,
                                node, make_brtnode_pair_attr(node),
                                toku_brtnode_flush_callback, toku_brtnode_pe_est_callback, toku_brtnode_pe_callback, toku_brtnode_cleaner_callback, t->h);
    if (r != 0)
	toku_free(node);
    else
        toku_unpin_brtnode(t, node);
    return r;
}

// open a file for use by the brt
// Requires:  File does not exist.
static int brt_create_file(BRT brt, const char *fname, int *fdp) {
    brt = brt;
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r;
    int fd;
    fd = open(fname, O_RDWR | O_BINARY, mode);
    assert(fd==-1);
    if (errno != ENOENT) {
	r = errno;
	return r;
    }
    fd = open(fname, O_RDWR | O_CREAT | O_BINARY, mode);
    if (fd==-1) {
	r = errno;
	return r;
    }

    r = toku_fsync_directory(fname);
    resource_assert_zero(r);

    *fdp = fd;
    return 0;
}

// open a file for use by the brt.  if the file does not exist, error
static int brt_open_file(const char *fname, int *fdp) {
    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
    int r;
    int fd;
    fd = open(fname, O_RDWR | O_BINARY, mode);
    if (fd==-1) {
	r = errno;
	assert(r!=0);
	return r;
    }
    *fdp = fd;
    return 0;
}

static int
brtheader_log_fassociate_during_checkpoint (CACHEFILE cf, void *header_v) {
    struct brt_header *h = header_v;
    char* fname_in_env = toku_cachefile_fname_in_env(cf);
    BYTESTRING bs = { strlen(fname_in_env), // don't include the NUL
		      fname_in_env };
    TOKULOGGER logger = toku_cachefile_logger(cf);
    FILENUM filenum = toku_cachefile_filenum (cf);
    int r = toku_log_fassociate(logger, NULL, 0, filenum, h->flags, bs);
    return r;
}

static int
brtheader_log_suppress_rollback_during_checkpoint (CACHEFILE cf, void *header_v) {
    int r = 0;
    struct brt_header *h = header_v;
    TXNID xid = h->txnid_that_created_or_locked_when_empty;
    if (xid != TXNID_NONE) {
	//Only log if useful.
	TOKULOGGER logger = toku_cachefile_logger(cf);
	FILENUM filenum = toku_cachefile_filenum (cf);
	r = toku_log_suppress_rollback(logger, NULL, 0, filenum, xid);
    }
    return r;
}


static int brtheader_note_pin_by_checkpoint (CACHEFILE cachefile, void *header_v);
static int brtheader_note_unpin_by_checkpoint (CACHEFILE cachefile, void *header_v);

static int
brt_init_header_partial (BRT t, TOKUTXN txn) {
    int r;
    t->h->flags = t->flags;
    if (t->h->cf!=NULL) assert(t->h->cf == t->cf);
    t->h->cf = t->cf;
    t->h->nodesize=t->nodesize;
    t->h->basementnodesize=t->basementnodesize;
    t->h->num_blocks_to_upgrade_13 = 0;
    t->h->num_blocks_to_upgrade_14 = 0;
    t->h->root_xid_that_created = txn ? txn->ancestor_txnid64 : TXNID_NONE;
    t->h->compare_fun = t->compare_fun;
    t->h->update_fun = t->update_fun;

    compute_and_fill_remembered_hash(t);

    BLOCKNUM root = t->h->root;
    if ((r=setup_initial_brt_root_node(t, root))!=0) { return r; }
    //printf("%s:%d putting %p (%d)\n", __FILE__, __LINE__, t->h, 0);
    toku_cachefile_set_userdata(t->cf,
				t->h,
				brtheader_log_fassociate_during_checkpoint,
				brtheader_log_suppress_rollback_during_checkpoint,
				toku_brtheader_close,
				toku_brtheader_checkpoint,
				toku_brtheader_begin_checkpoint,
				toku_brtheader_end_checkpoint,
				brtheader_note_pin_by_checkpoint,
				brtheader_note_unpin_by_checkpoint);

    return r;
}

static int
brt_init_header (BRT t, TOKUTXN txn) {
    t->h->type = BRTHEADER_CURRENT;
    t->h->checkpoint_header = NULL;
    toku_blocktable_create_new(&t->h->blocktable);
    BLOCKNUM root;
    //Assign blocknum for root block, also dirty the header
    toku_allocate_blocknum(t->h->blocktable, &root, t->h);
    t->h->root = root;

    toku_list_init(&t->h->live_brts);
    toku_list_init(&t->h->zombie_brts);
    toku_list_init(&t->h->checkpoint_before_commit_link);
    int r = brt_init_header_partial(t, txn);
    if (r==0) toku_block_verify_no_free_blocknums(t->h->blocktable);
    return r;
}


// allocate and initialize a brt header.
// t->cf is not set to anything.
static int 
brt_alloc_init_header(BRT t, TOKUTXN txn) {
    int r;

    r = brtheader_alloc(&t->h);
    if (r != 0) {
	if (0) { died2: toku_free(t->h); }
	t->h=0;
	return r;
    }

    t->h->layout_version = BRT_LAYOUT_VERSION;
    t->h->layout_version_original = BRT_LAYOUT_VERSION;
    t->h->layout_version_read_from_disk = BRT_LAYOUT_VERSION;	     // fake, prevent unnecessary upgrade logic

    t->h->build_id = BUILD_ID;
    t->h->build_id_original = BUILD_ID;

    uint64_t now = (uint64_t) time(NULL);
    t->h->time_of_creation = now;
    t->h->time_of_last_modification = now;
    t->h->time_of_last_verification = 0;

    memset(&t->h->descriptor, 0, sizeof(t->h->descriptor));

    r = brt_init_header(t, txn);
    if (r != 0) goto died2;
    return r;
}

int toku_read_brt_header_and_store_in_cachefile (BRT brt, CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open)
// If the cachefile already has the header, then just get it.
// If the cachefile has not been initialized, then don't modify anything.
// max_acceptable_lsn is the latest acceptable checkpointed version of the file.
{
    {
	struct brt_header *h;
	if ((h=toku_cachefile_get_userdata(cf))!=0) {
	    *header = h;
	    *was_open = TRUE;
            assert(brt->update_fun == h->update_fun);
            assert(brt->compare_fun == h->compare_fun);
	    return 0;
	}
    }
    *was_open = FALSE;
    struct brt_header *h;
    int r;
    {
	int fd = toku_cachefile_get_and_pin_fd (cf);
	r = toku_deserialize_brtheader_from(fd, max_acceptable_lsn, &h);
	toku_cachefile_unpin_fd(cf);
    }
    if (r!=0) return r;
    h->cf = cf;
    h->compare_fun = brt->compare_fun;
    h->update_fun = brt->update_fun;
    toku_cachefile_set_userdata(cf,
				(void*)h,
				brtheader_log_fassociate_during_checkpoint,
				brtheader_log_suppress_rollback_during_checkpoint,
				toku_brtheader_close,
				toku_brtheader_checkpoint,
				toku_brtheader_begin_checkpoint,
				toku_brtheader_end_checkpoint,
				brtheader_note_pin_by_checkpoint,
				brtheader_note_unpin_by_checkpoint);
    *header = h;
    return 0;
}

static void
brtheader_note_brt_close(BRT t) {
    struct brt_header *h = t->h;
    if (h) { //Might not yet have been opened.
	toku_brtheader_lock(h);
	toku_list_remove(&t->live_brt_link);
	toku_list_remove(&t->zombie_brt_link);
	toku_brtheader_unlock(h);
    }
}

static int
brtheader_note_brt_open(BRT live) {
    struct brt_header *h = live->h;
    int retval = 0;
    toku_brtheader_lock(h);
    while (!toku_list_empty(&h->zombie_brts)) {
	//Remove dead brt from list
	BRT zombie = toku_list_struct(toku_list_pop(&h->zombie_brts), struct brt, zombie_brt_link);
	toku_brtheader_unlock(h); //Cannot be holding lock when swapping brts.
	retval = toku_txn_note_swap_brt(live, zombie); //Steal responsibility, close
	toku_brtheader_lock(h);
	if (retval) break;
    }
    if (retval==0) {
	toku_list_push(&h->live_brts, &live->live_brt_link);
	h->dictionary_opened = TRUE;
    }

    toku_brtheader_unlock(h);
    return retval;
}

static int
verify_builtin_comparisons_consistent(BRT t, u_int32_t flags) {
    if ((flags & TOKU_DB_KEYCMP_BUILTIN) && (t->compare_fun != toku_builtin_compare_fun))
	return EINVAL;
    return 0;
}

int
toku_update_descriptor(struct brt_header * h, DESCRIPTOR d, int fd) {
    int r = 0;
    DISKOFF offset;
    //4 for checksum
    toku_realloc_descriptor_on_disk(h->blocktable, toku_serialize_descriptor_size(d)+4, &offset, h);
    r = toku_serialize_descriptor_contents_to_fd(fd, d, offset);
    if (r) {
	goto cleanup;
    }
    if (h->descriptor.dbt.data) {
	toku_free(h->descriptor.dbt.data);
    }
    h->descriptor.dbt.size = d->dbt.size;
    h->descriptor.dbt.data = toku_memdup(d->dbt.data, d->dbt.size);

    r = 0;
cleanup:
    return r;
}

int
toku_brt_change_descriptor(
    BRT t, 
    const DBT* old_descriptor, 
    const DBT* new_descriptor, 
    BOOL do_log, 
    TOKUTXN txn
    ) 
{
    int r = 0;
    int fd;
    DESCRIPTOR_S new_d;
    BYTESTRING old_desc_bs = { old_descriptor->size, old_descriptor->data };
    BYTESTRING new_desc_bs = { new_descriptor->size, new_descriptor->data };
    if (!txn) {
	r = EINVAL;
	goto cleanup;
    }
    // put information into rollback file
    r = toku_logger_save_rollback_change_fdescriptor(
	txn, 
	toku_cachefile_filenum(t->cf), 
	&old_desc_bs
	);
    if (r != 0) { goto cleanup; }
    r = toku_txn_note_brt(txn, t);
    if (r != 0) { goto cleanup; }

    if (do_log) {
	TOKULOGGER logger = toku_txn_logger(txn);
	TXNID xid = toku_txn_get_txnid(txn);
	r = toku_log_change_fdescriptor(
	    logger, NULL, 0, 
	    toku_cachefile_filenum(t->cf),
	    xid,
	    old_desc_bs,
	    new_desc_bs
	    );
	if (r != 0) { goto cleanup; }
    }

    // write new_descriptor to header
    new_d.dbt = *new_descriptor;
    fd = toku_cachefile_get_and_pin_fd (t->cf);
    r = toku_update_descriptor(t->h, &new_d, fd);
    if (r == 0)	 // very infrequent operation, worth precise threadsafe count
	brt_status.descriptor_set++;
    toku_cachefile_unpin_fd(t->cf);
    if (r!=0) goto cleanup;

cleanup:
    return r;
}

// This is the actual open, used for various purposes, such as normal use, recovery, and redirect.  
// fname_in_env is the iname, relative to the env_dir  (data_dir is already in iname as prefix).
// The checkpointed version (checkpoint_lsn) of the dictionary must be no later than max_acceptable_lsn .
static int
brt_open(BRT t, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, DB *db, FILENUM use_filenum, DICTIONARY_ID use_dictionary_id, LSN max_acceptable_lsn) {
    int r;
    BOOL txn_created = FALSE;

    if (t->did_set_flags) {
	r = verify_builtin_comparisons_consistent(t, t->flags);
	if (r!=0) return r;
    }

    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE: %s:%d toku_brt_open(%s, \"%s\", %d, %p, %d, %p)\n",
			  __FILE__, __LINE__, fname_in_env, dbname, is_create, newbrt, nodesize, cachetable));
    char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(cachetable, fname_in_env);
    if (0) { died0:  if (fname_in_cwd) toku_free(fname_in_cwd); assert(r); return r; }

    assert(is_create || !only_create);
    t->db = db;
    BOOL did_create = FALSE;
    FILENUM reserved_filenum = use_filenum;
    {
	int fd = -1;
	r = brt_open_file(fname_in_cwd, &fd);
	int use_reserved_filenum = reserved_filenum.fileid != FILENUM_NONE.fileid;
	if (r==ENOENT && is_create) {
	    toku_cachetable_reserve_filenum(cachetable, &reserved_filenum, use_reserved_filenum, reserved_filenum);
	    if (0) {
                died1:
		if (did_create)
		    toku_cachetable_unreserve_filenum(cachetable, reserved_filenum);
		goto died0;
	    }
	    if (use_reserved_filenum) assert(reserved_filenum.fileid == use_filenum.fileid);
	    did_create = TRUE;
	    mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
	    if (txn) {
		BYTESTRING bs = { .len=strlen(fname_in_env), .data = (char*)fname_in_env };
		r = toku_logger_save_rollback_fcreate(txn, reserved_filenum, &bs); // bs is a copy of the fname relative to the environment
		if (r != 0) goto died1;
	    }
	    txn_created = (BOOL)(txn!=NULL);
	    r = toku_logger_log_fcreate(txn, fname_in_env, reserved_filenum, mode, t->flags, t->nodesize, t->basementnodesize);
	    if (r!=0) goto died1;
	    r = brt_create_file(t, fname_in_cwd, &fd);
	}
	toku_free(fname_in_cwd);
	fname_in_cwd = NULL;
	if (r != 0) goto died1;
	// TODO: #2090
	r=toku_cachetable_openfd_with_filenum(&t->cf, cachetable, fd,
					      fname_in_env,
					      use_reserved_filenum||did_create, reserved_filenum, did_create);
	if (r != 0) goto died1;
    }
    if (r!=0) {
	died_after_open:
	toku_cachefile_close(&t->cf, 0, FALSE, ZERO_LSN);
	goto died1;
    }
    assert(t->nodesize>0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); toku_print_malloced_items();
    if (0) {
    died_after_read_and_pin:
	goto died_after_open;
    }
    BOOL was_already_open;
    if (is_create) {
	r = toku_read_brt_header_and_store_in_cachefile(t, t->cf, max_acceptable_lsn, &t->h, &was_already_open);
	if (r==TOKUDB_DICTIONARY_NO_HEADER) {
	    r = brt_alloc_init_header(t, txn);
	    if (r != 0) goto died_after_read_and_pin;
	}
	else if (r!=0) {
	    goto died_after_read_and_pin;
	}
	else if (only_create) {
	    assert_zero(r);
	    r = EEXIST;
	    goto died_after_read_and_pin;
	}
	else goto found_it;
    } else {
	if ((r = toku_read_brt_header_and_store_in_cachefile(t, t->cf, max_acceptable_lsn, &t->h, &was_already_open))!=0) goto died_after_open;
	found_it:
	t->nodesize = t->h->nodesize;		      /* inherit the pagesize from the file */
        t->basementnodesize = t->h->basementnodesize;
	if (!t->did_set_flags) {
	    r = verify_builtin_comparisons_consistent(t, t->flags);
	    if (r!=0) goto died_after_read_and_pin;
	    t->flags = t->h->flags;
	    t->did_set_flags = TRUE;
	} else {
	    if (t->flags != t->h->flags) {		  /* if flags have been set then flags must match */
		r = EINVAL; goto died_after_read_and_pin;
	    }
	}
    }

    if (!was_already_open) {
	if (!did_create) { //Only log the fopen that OPENs the file.  If it was already open, don't log.
	    r = toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(t->cf), t->flags);
	    if (r!=0) goto died_after_read_and_pin;
	}
    }
    int use_reserved_dict_id = use_dictionary_id.dictid != DICTIONARY_ID_NONE.dictid;
    if (!was_already_open) {
	DICTIONARY_ID dict_id;
	if (use_reserved_dict_id)
	    dict_id = use_dictionary_id;
	else
	    dict_id = next_dict_id();
	t->h->dict_id = dict_id;
    }
    else {
	// dict_id is already in header
	if (use_reserved_dict_id)
	    assert(t->h->dict_id.dictid == use_dictionary_id.dictid);
    }
    assert(t->h);
    assert(t->h->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    assert(t->h->dict_id.dictid < dict_id_serial);

    r = toku_maybe_upgrade_brt(t);	  // possibly do some work to complete the version upgrade of brt
    if (r!=0) goto died_after_read_and_pin;

    // brtheader_note_brt_open must be after all functions that can fail.
    r = brtheader_note_brt_open(t);
    if (r!=0) goto died_after_read_and_pin;
    if (t->db) t->db->descriptor = &t->h->descriptor;
    if (txn_created) {
	assert(txn);
	toku_brt_header_suppress_rollbacks(t->h, txn);
	r = toku_txn_note_brt(txn, t);
	assert_zero(r);
    }

    //Opening a brt may restore to previous checkpoint.	 Truncate if necessary.
    {
	int fd = toku_cachefile_get_and_pin_fd (t->h->cf);
	toku_maybe_truncate_cachefile_on_open(t->h->blocktable, fd, t->h);
	toku_cachefile_unpin_fd(t->h->cf);
    }
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE -> %p\n", t));
    return 0;
}

// Open a brt for the purpose of recovery, which requires that the brt be open to a pre-determined FILENUM
// and may require a specific checkpointed version of the file.	 
// (dict_id is assigned by the brt_open() function.)
int
toku_brt_open_recovery(BRT t, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, 
		       DB *db, FILENUM use_filenum, LSN max_acceptable_lsn) {
    int r;
    assert(use_filenum.fileid != FILENUM_NONE.fileid);
    r = brt_open(t, fname_in_env, is_create, only_create, cachetable,
		 txn, db, use_filenum, DICTIONARY_ID_NONE, max_acceptable_lsn);
    return r;
}

// Open a brt in normal use.  The FILENUM and dict_id are assigned by the brt_open() function.
int
toku_brt_open(BRT t, const char *fname_in_env, int is_create, int only_create, CACHETABLE cachetable, TOKUTXN txn, DB *db) {
    int r;
    r = brt_open(t, fname_in_env, is_create, only_create, cachetable, txn, db, FILENUM_NONE, DICTIONARY_ID_NONE, MAX_LSN);
    return r;
}

// Open a brt for use by redirect.  The new brt must have the same dict_id as the old_brt passed in.  (FILENUM is assigned by the brt_open() function.)
static int
brt_open_for_redirect(BRT *new_brtp, const char *fname_in_env, TOKUTXN txn, BRT old_brt) {
    int r;
    BRT t;
    struct brt_header *old_h = old_brt->h;
    assert(old_h->dict_id.dictid != DICTIONARY_ID_NONE.dictid);
    r = toku_brt_create(&t);
    assert_zero(r);
    r = toku_brt_set_bt_compare(t, old_brt->compare_fun);
    assert_zero(r);
    r = toku_brt_set_update(t, old_brt->update_fun);
    assert_zero(r);
    r = toku_brt_set_nodesize(t, old_brt->nodesize);
    assert_zero(r);
    r = toku_brt_set_basementnodesize(t, old_brt->basementnodesize);
    assert_zero(r);
    CACHETABLE ct = toku_cachefile_get_cachetable(old_brt->cf);
    r = brt_open(t, fname_in_env, 0, 0, ct, txn, old_brt->db, FILENUM_NONE, old_h->dict_id, MAX_LSN);
    assert_zero(r);
    assert(t->h->dict_id.dictid == old_h->dict_id.dictid);
    assert(t->db == old_brt->db);

    *new_brtp = t;
    return r;
}



//Callback to ydb layer to set db->i->brt = brt
//Used for redirect.
static void (*callback_db_set_brt)(DB *db, BRT brt)  = NULL;

static void
brt_redirect_cursors (BRT brt_to, BRT brt_from) {
    assert(brt_to->db == brt_from->db);
    while (!toku_list_empty(&brt_from->cursors)) {
	struct toku_list * c_list = toku_list_head(&brt_from->cursors);
	BRT_CURSOR c = toku_list_struct(c_list, struct brt_cursor, cursors_link);

	toku_list_remove(&c->cursors_link);

	toku_list_push(&brt_to->cursors, &c->cursors_link);

	c->brt = brt_to;
    }
}

static void
brt_redirect_db (BRT brt_to, BRT brt_from) {
    assert(brt_to->db == brt_from->db);
    callback_db_set_brt(brt_from->db, brt_to);
}

static int
fake_db_brt_close_delayed(DB *db, u_int32_t UU(flags)) {
    BRT brt_to_close = db->api_internal;
    char *error_string = NULL;
    int r = toku_close_brt(brt_to_close, &error_string);
    assert_zero(r);
    assert(error_string == NULL);
    toku_free(db);
    return 0;
}

static int
toku_brt_header_close_redirected_brts(struct brt_header * h) {
//Requires:
//  toku_brt_db_delay_closed has NOT been called on any brts referring to h.
//For each brt referring to h, close it.
    struct toku_list *list;
    int num_brts = 0;
    for (list = h->live_brts.next; list != &h->live_brts; list = list->next) {
	num_brts++;
    }
    assert(num_brts>0);
    BRT brts[num_brts];
    DB *dbs[num_brts];
    int which = 0;
    for (list = h->live_brts.next; list != &h->live_brts; list = list->next) {
	XCALLOC(dbs[which]);
	brts[which] = toku_list_struct(list, struct brt, live_brt_link);
	assert(!brts[which]->was_closed);
	dbs[which]->api_internal = brts[which];
	brts[which]->db = dbs[which];
	which++;
    }
    assert(which == num_brts);
    for (which = 0; which < num_brts; which++) {
	int r;
	r = toku_brt_db_delay_closed(brts[which], dbs[which], fake_db_brt_close_delayed, 0);
	assert_zero(r);
    }
    return 0;
}



// This function performs most of the work to redirect a dictionary to different file.
// It is called for redirect and to abort a redirect.  (This function is almost its own inverse.)
static int
dictionary_redirect_internal(const char *dst_fname_in_env, struct brt_header *src_h, TOKUTXN txn, struct brt_header **dst_hp) {
    int r;

    assert(toku_list_empty(&src_h->zombie_brts));
    assert(!toku_list_empty(&src_h->live_brts));

    FILENUM src_filenum = toku_cachefile_filenum(src_h->cf);
    FILENUM dst_filenum = FILENUM_NONE;

    struct brt_header *dst_h = NULL;
    struct toku_list *list;
    for (list = src_h->live_brts.next; list != &src_h->live_brts; list = list->next) {
	BRT src_brt;
	src_brt = toku_list_struct(list, struct brt, live_brt_link);
	assert(!src_brt->was_closed);

	BRT dst_brt;
	r = brt_open_for_redirect(&dst_brt, dst_fname_in_env, txn, src_brt);
	assert_zero(r);
	if (dst_filenum.fileid==FILENUM_NONE.fileid) {	// if first time through loop
	    dst_filenum = toku_cachefile_filenum(dst_brt->cf);
	    assert(dst_filenum.fileid!=FILENUM_NONE.fileid);
	    assert(dst_filenum.fileid!=src_filenum.fileid); //Cannot be same file.
	}
	else { // All dst_brts must have same filenum
	    assert(dst_filenum.fileid == toku_cachefile_filenum(dst_brt->cf).fileid);
	}
	if (!dst_h) dst_h = dst_brt->h;
	else assert(dst_h == dst_brt->h);

	//Do not need to swap descriptors pointers.
	//Done by brt_open_for_redirect
	assert(dst_brt->db->descriptor == &dst_brt->h->descriptor);

	//Set db->i->brt to new brt
	brt_redirect_db(dst_brt, src_brt);
	
	//Move cursors.
	brt_redirect_cursors (dst_brt, src_brt);
    }
    assert(dst_h);

    r = toku_brt_header_close_redirected_brts(src_h);
    assert_zero(r);
    *dst_hp = dst_h;

    return r;

}


//This is the 'abort redirect' function.  The redirect of old_h to new_h was done
//and now must be undone, so here we redirect new_h back to old_h.
int
toku_dictionary_redirect_abort(struct brt_header *old_h, struct brt_header *new_h, TOKUTXN txn) {
    char *old_fname_in_env = toku_cachefile_fname_in_env(old_h->cf);

    int r;
    {
	FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
	FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
	assert(old_filenum.fileid!=new_filenum.fileid); //Cannot be same file.

	//No living brts in old header.
	assert(toku_list_empty(&old_h->live_brts));
	//Must have a zombie in old header.
	assert(!toku_list_empty(&old_h->zombie_brts));
    }

    // If application did not close all DBs using the new file, then there should 
    // be no zombies and we need to redirect the DBs back to the original file.
    if (!toku_list_empty(&new_h->live_brts)) {
	assert(toku_list_empty(&new_h->zombie_brts));
	struct brt_header *dst_h;
	// redirect back from new_h to old_h
	r = dictionary_redirect_internal(old_fname_in_env, new_h, txn, &dst_h);
	assert_zero(r);
	assert(dst_h == old_h);
    }
    else {
	//No live brts.	 Zombies on both sides will die on their own eventually.
	//No need to redirect back.
	assert(!toku_list_empty(&new_h->zombie_brts));
	r = 0;
    }
    return r;
}


/****
 * on redirect or abort:
 *  if redirect txn_note_doing_work(txn)
 *  if redirect connect src brt to txn (txn modified this brt)
 *  for each src brt
 *    open brt to dst file (create new brt struct)
 *    if redirect connect dst brt to txn 
 *    redirect db to new brt
 *    redirect cursors to new brt
 *  close all src brts
 *  if redirect make rollback log entry
 * 
 * on commit:
 *   nothing to do
 *
 *****/

int 
toku_dictionary_redirect (const char *dst_fname_in_env, BRT old_brt, TOKUTXN txn) {
// Input args:
//   new file name for dictionary (relative to env)
//   old_brt is a live brt of open handle ({DB, BRT} pair) that currently refers to old dictionary file.
//   (old_brt may be one of many handles to the dictionary.)
//   txn that created the loader
// Requires: 
//   ydb_lock is held.
//   The brt is open.  (which implies there can be no zombies.)
//   The new file must be a valid dictionary.
//   The block size and flags in the new file must match the existing BRT.
//   The new file must already have its descriptor in it (and it must match the existing descriptor).
// Effect:   
//   Open new BRTs (and related header and cachefile) to the new dictionary file with a new FILENUM.
//   Redirect all DBs that point to brts that point to the old file to point to brts that point to the new file.
//   Copy the dictionary id (dict_id) from the header of the original file to the header of the new file.
//   Create a rollback log entry.
//   The original BRT, header, cachefile and file remain unchanged.  They will be cleaned up on commmit.
//   If the txn aborts, then this operation will be undone
    int r;

    struct brt_header * old_h = old_brt->h;

    // dst file should not be open.  (implies that dst and src are different because src must be open.)
    {
	CACHETABLE ct = toku_cachefile_get_cachetable(old_h->cf);
	CACHEFILE cf;
	r = toku_cachefile_of_iname_in_env(ct, dst_fname_in_env, &cf);
	if (r==0) {
	    r = EINVAL;
	    goto cleanup;
	}
	assert(r==ENOENT);
	r = 0;	      
    }

    if (txn) {
	r = toku_txn_note_brt(txn, old_brt);  // mark old brt as touched by this txn
	assert_zero(r);
    }

    struct brt_header *new_h;
    r = dictionary_redirect_internal(dst_fname_in_env, old_h, txn, &new_h);
    assert_zero(r);

    // make rollback log entry
    if (txn) {
	assert(toku_list_empty(&new_h->zombie_brts));
	assert(!toku_list_empty(&new_h->live_brts));
	struct toku_list *list;
	for (list = new_h->live_brts.next; list != &new_h->live_brts; list = list->next) {
	    BRT new_brt;
	    new_brt = toku_list_struct(list, struct brt, live_brt_link);
	    r = toku_txn_note_brt(txn, new_brt);	  // mark new brt as touched by this txn
	    assert_zero(r);
	}
	FILENUM old_filenum = toku_cachefile_filenum(old_h->cf);
	FILENUM new_filenum = toku_cachefile_filenum(new_h->cf);
	r = toku_logger_save_rollback_dictionary_redirect(txn, old_filenum, new_filenum);
	assert_zero(r);

	TXNID xid = toku_txn_get_txnid(txn);
	toku_brt_header_suppress_rollbacks(new_h, txn);
	r = toku_log_suppress_rollback(txn->logger, NULL, 0, new_filenum, xid);
	assert_zero(r);
    }
    
cleanup:
    return r;
}


DICTIONARY_ID
toku_brt_get_dictionary_id(BRT brt) {
    struct brt_header *h = brt->h;
    DICTIONARY_ID dict_id = h->dict_id;
    return dict_id;
}

int toku_brt_set_flags(BRT brt, unsigned int flags) {
    assert(flags==(flags&TOKU_DB_KEYCMP_BUILTIN)); // make sure there are no extraneous flags
    brt->did_set_flags = TRUE;
    brt->flags = flags;
    return 0;
}

int toku_brt_get_flags(BRT brt, unsigned int *flags) {
    *flags = brt->flags;
    assert(brt->flags==(brt->flags&TOKU_DB_KEYCMP_BUILTIN)); // make sure there are no extraneous flags
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

int toku_brt_set_basementnodesize(BRT brt, unsigned int basementnodesize) {
    brt->basementnodesize = basementnodesize;
    return 0;
}

int toku_brt_get_basementnodesize(BRT brt, unsigned int *basementnodesize) {
    *basementnodesize = brt->basementnodesize;
    return 0;
}

int toku_brt_set_bt_compare(BRT brt, int (*bt_compare)(DB*, const DBT*, const DBT*)) {
    brt->compare_fun = bt_compare;
    return 0;
}

int toku_brt_set_update(BRT brt, brt_update_func update_fun) {
    brt->update_fun = update_fun;
    return 0;
}

brt_compare_func toku_brt_get_bt_compare (BRT brt) {
    return brt->compare_fun;
}


int toku_brt_create_cachetable(CACHETABLE *ct, long cachesize, LSN initial_lsn, TOKULOGGER logger) {
    if (cachesize == 0)
	cachesize = 128*1024*1024;
    return toku_create_cachetable(ct, cachesize, initial_lsn, logger);
}

// Create checkpoint-in-progress versions of header and translation (btt) (and fifo for now...).
//Has access to fd (it is protected).
int
toku_brtheader_begin_checkpoint (CACHEFILE UU(cachefile), int UU(fd), LSN checkpoint_lsn, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
	// hold lock around copying and clearing of dirty bit
	toku_brtheader_lock (h);
	assert(h->type == BRTHEADER_CURRENT);
	assert(h->checkpoint_header == NULL);
	brtheader_copy_for_checkpoint(h, checkpoint_lsn);
	h->dirty = 0;	     // this is only place this bit is cleared	(in currentheader)
	toku_block_translation_note_start_checkpoint_unlocked(h->blocktable);
	toku_brtheader_unlock (h);
    }
    return r;
}

int
toku_brt_zombie_needed(BRT zombie) {
    return toku_omt_size(zombie->txns) != 0 || zombie->pinned_by_checkpoint;
}

//Must be protected by ydb lock.
//Is only called by checkpoint begin, which holds it
static int
brtheader_note_pin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    //Set arbitrary brt (for given header) as pinned by checkpoint.
    //Only one can be pinned (only one checkpoint at a time), but not worth verifying.
    struct brt_header *h = header_v;
    BRT brt_to_pin;
    toku_brtheader_lock(h);
    if (!toku_list_empty(&h->live_brts)) {
	brt_to_pin = toku_list_struct(toku_list_head(&h->live_brts), struct brt, live_brt_link);
    }
    else {
	//Header exists, so at least one brt must.  No live means at least one zombie.
	assert(!toku_list_empty(&h->zombie_brts));
	brt_to_pin = toku_list_struct(toku_list_head(&h->zombie_brts), struct brt, zombie_brt_link);
    }
    toku_brtheader_unlock(h);
    assert(!brt_to_pin->pinned_by_checkpoint);
    brt_to_pin->pinned_by_checkpoint = 1;

    return 0;
}

//Must be protected by ydb lock.
//Called by end_checkpoint, which grabs ydb lock around note_unpin
static int
brtheader_note_unpin_by_checkpoint (CACHEFILE UU(cachefile), void *header_v)
{
    //Must find which brt for this header is pinned, and unpin it.
    //Once found, we might have to close it if it was user closed and no txns touch it.
    //
    //HOW do you loop through a 'list'????
    struct brt_header *h = header_v;
    BRT brt_to_unpin = NULL;

    toku_brtheader_lock(h);
    if (!toku_list_empty(&h->live_brts)) {
	struct toku_list *list;
	for (list = h->live_brts.next; list != &h->live_brts; list = list->next) {
	    BRT candidate;
	    candidate = toku_list_struct(list, struct brt, live_brt_link);
	    if (candidate->pinned_by_checkpoint) {
		brt_to_unpin = candidate;
		break;
	    }
	}
    }
    if (!brt_to_unpin) {
	//Header exists, something is pinned, so exactly one zombie must be pinned
	assert(!toku_list_empty(&h->zombie_brts));
	struct toku_list *list;
	for (list = h->zombie_brts.next; list != &h->zombie_brts; list = list->next) {
	    BRT candidate;
	    candidate = toku_list_struct(list, struct brt, zombie_brt_link);
	    if (candidate->pinned_by_checkpoint) {
		brt_to_unpin = candidate;
		break;
	    }
	}
    }
    toku_brtheader_unlock(h);
    assert(brt_to_unpin);
    assert(brt_to_unpin->pinned_by_checkpoint);
    brt_to_unpin->pinned_by_checkpoint = 0; //Unpin
    int r = 0;
    //Close if necessary
    if (brt_to_unpin->was_closed && !toku_brt_zombie_needed(brt_to_unpin)) {
	//Close immediately.
	assert(brt_to_unpin->close_db);
	r = brt_to_unpin->close_db(brt_to_unpin->db, brt_to_unpin->close_flags);
    }
    return r;

}

// Write checkpoint-in-progress versions of header and translation to disk (really to OS internal buffer).
// Must have access to fd (protected)
int
toku_brtheader_checkpoint (CACHEFILE cf, int fd, void *header_v)
{
    struct brt_header *h = header_v;
    struct brt_header *ch = h->checkpoint_header;
    int r = 0;
    if (h->panic!=0) goto handle_error;
    //printf("%s:%d allocated_limit=%lu writing queue to %lu\n", __FILE__, __LINE__,
    //	     block_allocator_allocated_limit(h->block_allocator), h->unused_blocks.b*h->nodesize);
    assert(ch);
    if (ch->panic!=0) goto handle_error;
    assert(ch->type == BRTHEADER_CHECKPOINT_INPROGRESS);
    if (ch->dirty) {	    // this is only place this bit is tested (in checkpoint_header)
	TOKULOGGER logger = toku_cachefile_logger(cf);
	if (logger) {
	    r = toku_logger_fsync_if_lsn_not_fsynced(logger, ch->checkpoint_lsn);
	    if (r!=0) goto handle_error;
	}
        uint64_t now = (uint64_t) time(NULL); // 4018;
        h->time_of_last_modification = now;
        ch->time_of_last_modification = now;
        ch->checkpoint_count++;
        // write translation and header to disk (or at least to OS internal buffer)
        r = toku_serialize_brt_header_to(fd, ch);
        if (r!=0) goto handle_error;
	ch->dirty = 0;		      // this is only place this bit is cleared (in checkpoint_header)
    } else 
        toku_block_translation_note_skipped_checkpoint(ch->blocktable);
    if (0) {
handle_error:
	if (h->panic) r = h->panic;
	else if (ch->panic) {
	    r = ch->panic;
	    //Steal panic string.  Cannot afford to malloc.
	    h->panic	     = ch->panic;
	    h->panic_string  = ch->panic_string;
	}
	else toku_block_translation_note_failed_checkpoint(ch->blocktable);
    }
    return r;

}

// Really write everything to disk (fsync dictionary), then free unused disk space 
// (i.e. tell BlockAllocator to liberate blocks used by previous checkpoint).
// Must have access to fd (protected)
int
toku_brtheader_end_checkpoint (CACHEFILE cachefile, int fd, void *header_v) {
    struct brt_header *h = header_v;
    int r = h->panic;
    if (r==0) {
	assert(h->type == BRTHEADER_CURRENT);
	struct brt_header *ch = h->checkpoint_header;
	BOOL checkpoint_success_so_far = (BOOL)(ch->checkpoint_count==h->checkpoint_count+1 && ch->dirty==0);
	if (checkpoint_success_so_far) {
	    r = toku_cachefile_fsync(cachefile);
	    if (r!=0) 
		toku_block_translation_note_failed_checkpoint(h->blocktable);
	    else {
		h->checkpoint_count++;	      // checkpoint succeeded, next checkpoint will save to alternate header location
		h->checkpoint_lsn = ch->checkpoint_lsn;	 //Header updated.
	    }
	}
	toku_block_translation_note_end_checkpoint(h->blocktable, fd, h);
    }
    if (h->checkpoint_header) {	 // could be NULL only if panic was true at begin_checkpoint
	brtheader_free(h->checkpoint_header);
	h->checkpoint_header = NULL;
    }
    return r;
}

//Has access to fd (it is protected).
int
toku_brtheader_close (CACHEFILE cachefile, int fd, void *header_v, char **malloced_error_string, BOOL oplsn_valid, LSN oplsn) {
    struct brt_header *h = header_v;
    assert(h->type == BRTHEADER_CURRENT);
    toku_brtheader_lock(h);
    assert(toku_list_empty(&h->live_brts));
    assert(toku_list_empty(&h->zombie_brts));
    toku_brtheader_unlock(h);
    int r = 0;
    if (h->panic) {
	r = h->panic;
    } else if (h->dictionary_opened) { //Otherwise header has never fully been created.
	assert(h->cf == cachefile);
	TOKULOGGER logger = toku_cachefile_logger(cachefile);
	LSN lsn = ZERO_LSN;
	//Get LSN
	if (oplsn_valid) {
	    //Use recovery-specified lsn
	    lsn = oplsn;
	    //Recovery cannot reduce lsn of a header.
	    if (lsn.lsn < h->checkpoint_lsn.lsn)
		lsn = h->checkpoint_lsn;
	}
	else {
	    //Get LSN from logger
	    lsn = ZERO_LSN; // if there is no logger, we use zero for the lsn
	    if (logger) {
		char* fname_in_env = toku_cachefile_fname_in_env(cachefile);
		assert(fname_in_env);
		BYTESTRING bs = {.len=strlen(fname_in_env), .data=fname_in_env};
		r = toku_log_fclose(logger, &lsn, h->dirty, bs, toku_cachefile_filenum(cachefile)); // flush the log on close (if new header is being written), otherwise it might not make it out.
		if (r!=0) return r;
	    }
	}
	if (h->dirty) {	       // this is the only place this bit is tested (in currentheader)
	    if (logger) { //Rollback cachefile MUST NOT BE CLOSED DIRTY
			  //It can be checkpointed only via 'checkpoint'
		assert(logger->rollback_cachefile != cachefile);
	    }
	    int r2;
	    //assert(lsn.lsn!=0);
	    r2 = toku_brtheader_begin_checkpoint(cachefile, fd, lsn, header_v);
	    if (r==0) r = r2;
	    r2 = toku_brtheader_checkpoint(cachefile, fd, h);
	    if (r==0) r = r2;
	    r2 = toku_brtheader_end_checkpoint(cachefile, fd, header_v);
	    if (r==0) r = r2;
	    if (!h->panic) assert(!h->dirty);	     // dirty bit should be cleared by begin_checkpoint and never set again (because we're closing the dictionary)
	}
    }
    if (malloced_error_string) *malloced_error_string = h->panic_string;
    if (r == 0) {
	r = h->panic;
    }
    toku_brtheader_free(h);
    return r;
}

int
toku_brt_db_delay_closed (BRT zombie, DB* db, int (*close_db)(DB*, u_int32_t), u_int32_t close_flags) {
//Requires: close_db needs to call toku_close_brt to delete the final reference.
    int r;
    struct brt_header *h = zombie->h;
    if (zombie->was_closed) r = EINVAL;
    else if (zombie->db && zombie->db!=db) r = EINVAL;
    else {
	assert(zombie->close_db==NULL);
	zombie->close_db    = close_db;
	zombie->close_flags = close_flags;
	zombie->was_closed  = 1;
	if (!zombie->db) zombie->db = db;
	if (!toku_brt_zombie_needed(zombie)) {
	    //Close immediately.
	    r = zombie->close_db(zombie->db, zombie->close_flags);
	}
	else {
	    //Try to pass responsibility off.
	    toku_brtheader_lock(zombie->h);
	    toku_list_remove(&zombie->live_brt_link); //Remove from live.
	    BRT replacement = NULL;
	    if (!toku_list_empty(&h->live_brts)) {
		replacement = toku_list_struct(toku_list_head(&h->live_brts), struct brt, live_brt_link);
	    }
	    else if (!toku_list_empty(&h->zombie_brts)) {
		replacement = toku_list_struct(toku_list_head(&h->zombie_brts), struct brt, zombie_brt_link);
	    }
	    toku_list_push(&h->zombie_brts, &zombie->zombie_brt_link); //Add to dead list.
	    toku_brtheader_unlock(zombie->h);
	    if (replacement == NULL) r = 0;  //Just delay close
	    else {
		//Pass responsibility off and close zombie.
		//Skip adding to dead list
		r = toku_txn_note_swap_brt(replacement, zombie);
	    }
	}
    }
    return r;
}

// Close brt.  If opsln_valid, use given oplsn as lsn in brt header instead of logging 
// the close and using the lsn provided by logging the close.  (Subject to constraint 
// that if a newer lsn is already in the dictionary, don't overwrite the dictionary.)
int toku_close_brt_lsn (BRT brt, char **error_string, BOOL oplsn_valid, LSN oplsn) {
    assert(!toku_brt_zombie_needed(brt));
    assert(!brt->pinned_by_checkpoint);
    if (brt->cf) toku_cachefile_wait_for_background_work_to_quiesce(brt->cf);
    int r;
    while (!toku_list_empty(&brt->cursors)) {
	BRT_CURSOR c = toku_list_struct(toku_list_pop(&brt->cursors), struct brt_cursor, cursors_link);
	r=toku_brt_cursor_close(c);
	if (r!=0) return r;
    }

    // Must do this work before closing the cf
    r=toku_txn_note_close_brt(brt);
    assert_zero(r);
    toku_omt_destroy(&brt->txns);
    brtheader_note_brt_close(brt);

    if (brt->cf) {
	//printf("%s:%d closing cachetable\n", __FILE__, __LINE__);
	// printf("%s:%d brt=%p ,brt->h=%p\n", __FILE__, __LINE__, brt, brt->h);
	if (error_string) assert(*error_string == 0);
	r = toku_cachefile_close(&brt->cf, error_string, oplsn_valid, oplsn);
	if (r==0 && error_string) assert(*error_string == 0);
    }
    toku_free(brt);
    return r;
}

int toku_close_brt (BRT brt, char **error_string) {
    return toku_close_brt_lsn(brt, error_string, FALSE, ZERO_LSN);
}

int toku_brt_create(BRT *brt_ptr) {
    BRT MALLOC(brt);
    if (brt == 0)
	return ENOMEM;
    memset(brt, 0, sizeof *brt);
    toku_list_init(&brt->live_brt_link);
    toku_list_init(&brt->zombie_brt_link);
    toku_list_init(&brt->cursors);
    brt->flags = 0;
    brt->did_set_flags = FALSE;
    brt->nodesize = BRT_DEFAULT_NODE_SIZE;
    brt->basementnodesize = BRT_DEFAULT_BASEMENT_NODE_SIZE;
    brt->compare_fun = toku_builtin_compare_fun;
    brt->update_fun = NULL;
    int r = toku_omt_create(&brt->txns);
    if (r!=0) { toku_free(brt); return r; }
    *brt_ptr = brt;
    return 0;
}


int toku_brt_flush (BRT brt) {
    return toku_cachefile_flush(brt->cf);
}

/* ************* CURSORS ********************* */

static inline void
brt_cursor_cleanup_dbts(BRT_CURSOR c) {
    if (c->key.data) toku_free(c->key.data);
    if (c->val.data) toku_free(c->val.data);
    memset(&c->key, 0, sizeof(c->key));
    memset(&c->val, 0, sizeof(c->val));
}

//
// This function is used by the leafentry iterators.
// returns TOKUDB_ACCEPT if live transaction context is allowed to read a value
// that is written by transaction with LSN of id
// live transaction context may read value if either id is the root ancestor of context, or if
// id was committed before context's snapshot was taken.
// For id to be committed before context's snapshot was taken, the following must be true:
//  - id < context->snapshot_txnid64 AND id is not in context's live root transaction list
// For the above to NOT be true:
//  - id > context->snapshot_txnid64 OR id is in context's live root transaction list
//
static
int does_txn_read_entry(TXNID id, TOKUTXN context) {
    int rval;
    TXNID oldest_live_in_snapshot = toku_get_oldest_in_live_root_txn_list(context);
    if (id < oldest_live_in_snapshot || id == context->ancestor_txnid64) {
	rval = TOKUDB_ACCEPT;
    }
    else if (id > context->snapshot_txnid64 || toku_is_txn_in_live_root_txn_list(context, id)) {
	rval = 0;
    }
    else {
	rval = TOKUDB_ACCEPT;
    }
    return rval;
}

static inline void brt_cursor_extract_key_and_val(
		   LEAFENTRY le,
		   BRT_CURSOR cursor,
		   u_int32_t *keylen,
		   void	    **key,
		   u_int32_t *vallen,
		   void	    **val) {
    if (toku_brt_cursor_is_leaf_mode(cursor)) {
	*key = le_key_and_len(le, keylen);
	*val = le;
	*vallen = leafentry_memsize(le);
    } else if (cursor->is_snapshot_read) {
	le_iterate_val(
	    le, 
	    does_txn_read_entry, 
	    val, 
	    vallen, 
	    cursor->ttxn
	    );
	*key = le_key_and_len(le, keylen);
    } else {
	*key = le_key_and_len(le, keylen);
	*val = le_latest_val_and_len(le, vallen);
    }
}

int toku_brt_cursor (
    BRT brt, 
    BRT_CURSOR *cursorptr, 
    TOKUTXN ttxn, 
    BOOL is_snapshot_read
    ) 
{
    if (is_snapshot_read) {
	invariant(ttxn != NULL);
	int accepted = does_txn_read_entry(brt->h->root_xid_that_created, ttxn);
	if (accepted!=TOKUDB_ACCEPT) {
	    invariant(accepted==0);
	    return TOKUDB_MVCC_DICTIONARY_TOO_NEW;
	}
    }
    BRT_CURSOR cursor = toku_malloc(sizeof *cursor);
    // if this cursor is to do read_committed fetches, then the txn objects must be valid.
    if (cursor == 0)
	return ENOMEM;
    memset(cursor, 0, sizeof(*cursor));
    cursor->brt = brt;
    cursor->prefetching = FALSE;
    toku_init_dbt(&cursor->range_lock_left_key);
    toku_init_dbt(&cursor->range_lock_right_key);
    cursor->left_is_neg_infty = FALSE;
    cursor->right_is_pos_infty = FALSE;
    cursor->is_snapshot_read = is_snapshot_read;
    cursor->is_leaf_mode = FALSE;
    cursor->ttxn = ttxn;
    toku_list_push(&brt->cursors, &cursor->cursors_link);
    *cursorptr = cursor;
    return 0;
}

void
toku_brt_cursor_set_leaf_mode(BRT_CURSOR brtcursor) {
    brtcursor->is_leaf_mode = TRUE;
}

int
toku_brt_cursor_is_leaf_mode(BRT_CURSOR brtcursor) {
    return brtcursor->is_leaf_mode;
}

void
toku_brt_cursor_set_range_lock(BRT_CURSOR cursor, const DBT *left, const DBT *right,
                               BOOL left_is_neg_infty, BOOL right_is_pos_infty)
{
    if (cursor->range_lock_left_key.data) {
        toku_free(cursor->range_lock_left_key.data);
        toku_init_dbt(&cursor->range_lock_left_key);
    }
    if (cursor->range_lock_right_key.data) {
        toku_free(cursor->range_lock_right_key.data);
        toku_init_dbt(&cursor->range_lock_right_key);
    }

    if (left_is_neg_infty) {
        cursor->left_is_neg_infty = TRUE;
    } else {
        toku_fill_dbt(&cursor->range_lock_left_key,
                      toku_xmemdup(left->data, left->size), left->size);
    }
    if (right_is_pos_infty) {
        cursor->right_is_pos_infty = TRUE;
    } else {
        toku_fill_dbt(&cursor->range_lock_right_key,
                      toku_xmemdup(right->data, right->size), right->size);
    }
}

//TODO: #1378 When we split the ydb lock, touching cursor->cursors_link
//is not thread safe.
int toku_brt_cursor_close(BRT_CURSOR cursor) {
    brt_cursor_cleanup_dbts(cursor);
    if (cursor->range_lock_left_key.data) {
        toku_free(cursor->range_lock_left_key.data);
        toku_destroy_dbt(&cursor->range_lock_left_key);
    }
    if (cursor->range_lock_right_key.data) {
        toku_free(cursor->range_lock_right_key.data);
        toku_destroy_dbt(&cursor->range_lock_right_key);
    }
    toku_list_remove(&cursor->cursors_link);
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
    return (BOOL)(cursor->key.data == NULL);
}

static int
pair_leafval_heaviside_le (u_int32_t klen, void *kval,
			   brt_search_t *search) {
    DBT x;
    int cmp = search->compare(search,
			      search->k ? toku_fill_dbt(&x, kval, klen) : 0);
    // The search->compare function returns only 0 or 1
    switch (search->direction) {
    case BRT_SEARCH_LEFT:   return cmp==0 ? -1 : +1;
    case BRT_SEARCH_RIGHT:  return cmp==0 ? +1 : -1; // Because the comparison runs backwards for right searches.
    }
    abort(); return 0;
}


static int
heaviside_from_search_t (OMTVALUE lev, void *extra) {
    LEAFENTRY le=lev;
    brt_search_t *search = extra;
    u_int32_t keylen;
    void* key = le_key_and_len(le, &keylen);

    return pair_leafval_heaviside_le (keylen, key,
				      search);
}


//
// Returns true if the value that is to be read is empty.
//
static inline int 
is_le_val_del(LEAFENTRY le, BRT_CURSOR brtcursor) {
    int rval;
    if (brtcursor->is_snapshot_read) {
	BOOL is_del;
	le_iterate_is_del(
	    le, 
	    does_txn_read_entry, 
	    &is_del, 
	    brtcursor->ttxn
	    );
	rval = is_del;
    }
    else {
	rval = le_latest_is_del(le);
    }
    return rval;
}

static const DBT zero_dbt = {0,0,0,0};

static void search_save_bound (brt_search_t *search, DBT *pivot) {
    if (search->have_pivot_bound) {
	toku_free(search->pivot_bound.data);
    }
    search->pivot_bound = zero_dbt;
    search->pivot_bound.data = toku_malloc(pivot->size);
    search->pivot_bound.size = pivot->size;
    memcpy(search->pivot_bound.data, pivot->data, pivot->size);
    search->have_pivot_bound = TRUE;
}

static BOOL search_pivot_is_bounded (brt_search_t *search, DESCRIPTOR desc, brt_compare_func cmp, DBT *pivot)
// Effect:  Return TRUE iff the pivot has already been searched (for fixing #3522.)
//  If searching from left to right, if we have already searched all the values less than pivot, we don't want to search again.
//  If searching from right to left, if we have already searched all the vlaues greater than pivot, we don't want to search again.
{
    if (!search->have_pivot_bound) return TRUE; // isn't bounded.
    FAKE_DB(db, desc);
    int comp = cmp(&db, pivot, &search->pivot_bound);
    if (search->direction == BRT_SEARCH_LEFT) {
	// searching from left to right.  If the comparison function says the pivot is <= something we already compared, don't do it again.
	return comp>0;
    } else {
	return comp<0;
    }
}

static int
move_to_stale(OMTVALUE v, u_int32_t UU(idx), BRT brt, NONLEAF_CHILDINFO bnc)
{
    // we actually only copy to stale, and then delete messages out of
    // fresh later on, because we call this during an iteration over fresh
    const long offset = (long) v;
    struct fifo_entry *entry = (struct fifo_entry *) toku_fifo_get_entry(bnc->buffer, offset);
    entry->is_fresh = false;
    DBT keydbt;
    DBT *key = fill_dbt_for_fifo_entry(&keydbt, entry);
    struct toku_fifo_entry_key_msn_heaviside_extra heaviside_extra = { .desc = &brt->h->descriptor, .cmp = brt->compare_fun, .fifo = bnc->buffer, .key = key->data, .keylen = key->size, .msn = entry->msn };
    int r = toku_omt_insert(bnc->stale_message_tree, (OMTVALUE) offset, toku_fifo_entry_key_msn_heaviside, &heaviside_extra, NULL); assert_zero(r);
    return r;
}

struct store_fifo_offset_extra {
    long *offsets;
    int i;
};

static int
store_fifo_offset(OMTVALUE v, u_int32_t UU(idx), void *extrap)
{
    struct store_fifo_offset_extra *extra = extrap;
    const long offset = (long) v;
    extra->offsets[extra->i] = offset;
    extra->i++;
    return 0;
}

struct store_fifo_offset_and_move_to_stale_extra {
    BRT brt;
    struct store_fifo_offset_extra *sfo_extra;
    NONLEAF_CHILDINFO bnc;
};

static int
store_fifo_offset_and_move_to_stale(OMTVALUE v, u_int32_t idx, void *extrap)
{
    struct store_fifo_offset_and_move_to_stale_extra *extra = extrap;
    int r = store_fifo_offset(v, idx, extra->sfo_extra); assert_zero(r);
    r = move_to_stale(v, idx, extra->brt, extra->bnc); assert_zero(r);
    return r;
}

static int
fifo_offset_msn_cmp(void *extrap, const void *va, const void *vb)
{
    FIFO fifo = extrap;
    const long *ao = va;
    const long *bo = vb;
    const struct fifo_entry *a = toku_fifo_get_entry(fifo, *ao);
    const struct fifo_entry *b = toku_fifo_get_entry(fifo, *bo);
    return (a->msn.msn > b->msn.msn) - (a->msn.msn < b->msn.msn);
}

static void
do_brt_leaf_put_cmd(BRT t, BASEMENTNODE bn, BRTNODE ancestor, int childnum, OMT snapshot_txnids, OMT live_list_reverse, MSN *max_msn_applied, const struct fifo_entry *entry)
{
    ITEMLEN keylen = entry->keylen;
    ITEMLEN vallen = entry->vallen;
    enum brt_msg_type type = (enum brt_msg_type)entry->type;
    MSN msn = entry->msn;
    const XIDS xids = (XIDS) &entry->xids_s;
    bytevec key = xids_get_end_of_array(xids);
    bytevec val = (u_int8_t*)key + entry->keylen;

    DBT hk;
    toku_fill_dbt(&hk, key, keylen);
    DBT hv;
    BRT_MSG_S brtcmd = { type, msn, xids, .u.id = { &hk, toku_fill_dbt(&hv, val, vallen) } };
    bool made_change;
    // the messages are in (key,msn) order so all the messages for one key
    // in one buffer are in ascending msn order, so it's ok that we don't
    // update the basement node's msn until the end
    if (brtcmd.msn.msn > bn->max_msn_applied.msn) {
        if (brtcmd.msn.msn > max_msn_applied->msn) {
            *max_msn_applied = brtcmd.msn;
        }
        brt_leaf_put_cmd(t->compare_fun, t->update_fun, &t->h->descriptor, bn, &brtcmd, &made_change, &BP_WORKDONE(ancestor, childnum), snapshot_txnids, live_list_reverse);
    } else {
        brt_status.msn_discards++;
    }
}

struct iterate_do_brt_leaf_put_cmd_extra {
    BRT t;
    BASEMENTNODE bn;
    BRTNODE ancestor;
    int childnum;
    OMT snapshot_txnids;
    OMT live_list_reverse;
    MSN *max_msn_applied;
};

static int
iterate_do_brt_leaf_put_cmd(OMTVALUE v, u_int32_t UU(idx), void *extrap)
{
    struct iterate_do_brt_leaf_put_cmd_extra *e = extrap;
    const long offset = (long) v;
    NONLEAF_CHILDINFO bnc = BNC(e->ancestor, e->childnum);
    const struct fifo_entry *entry = toku_fifo_get_entry(bnc->buffer, offset);
    do_brt_leaf_put_cmd(e->t, e->bn, e->ancestor, e->childnum, e->snapshot_txnids, e->live_list_reverse, e->max_msn_applied, entry);
    return 0;
}

struct iterate_do_brt_leaf_put_cmd_and_move_to_stale_extra {
    BRT brt;
    struct iterate_do_brt_leaf_put_cmd_extra *iter_extra;
    NONLEAF_CHILDINFO bnc;
};

static int
iterate_do_brt_leaf_put_cmd_and_move_to_stale(OMTVALUE v, u_int32_t idx, void *extrap)
{
    struct iterate_do_brt_leaf_put_cmd_and_move_to_stale_extra *e = extrap;
    int r = iterate_do_brt_leaf_put_cmd(v, idx, e->iter_extra); assert_zero(r);
    r = move_to_stale(v, idx, e->brt, e->bnc); assert_zero(r);
    return r;
}

static void
bnc_find_iterate_bounds(
    DESCRIPTOR desc,
    brt_compare_func cmp,
    OMT message_tree,
    FIFO buffer,
    struct pivot_bounds const * const bounds,
    u_int32_t *lbi,
    u_int32_t *ube
    )
{
    int r = 0;

    // The bounds given to us are of the form (lbe,ubi] but the omt is
    // going to iterate over [left,right) (see toku_omt_iterate_on_range),
    // so we're going to convert it to [lbe,ubi) through an application of
    // heaviside functions.
    if (bounds->lower_bound_exclusive) {
        struct toku_fifo_entry_key_msn_heaviside_extra lbi_extra = {
            .desc = desc, .cmp = cmp,
            .fifo = buffer,
            .key = kv_pair_key((struct kv_pair *) bounds->lower_bound_exclusive),
            .keylen = kv_pair_keylen((struct kv_pair *) bounds->lower_bound_exclusive),
            .msn = MAX_MSN };
        // TODO: get this value and compare it with ube to see if we even
        // need to continue
        OMTVALUE found_lb;
        r = toku_omt_find(message_tree, toku_fifo_entry_key_msn_heaviside,
                          &lbi_extra, +1, &found_lb, lbi);
        if (r == DB_NOTFOUND) {
            // no relevant data, we're done
            *lbi = 0;
            *ube = 0;
            return;
        }
        if (bounds->upper_bound_inclusive) {
            DBT ubidbt_tmp = kv_pair_key_to_dbt((struct kv_pair *) bounds->upper_bound_inclusive);
            const long offset = (long) found_lb;
            DBT found_lbidbt;
            fill_dbt_for_fifo_entry(&found_lbidbt, toku_fifo_get_entry(buffer, offset));
	    FAKE_DB(db, desc);
            int c = cmp(&db, &found_lbidbt, &ubidbt_tmp);
            // These DBTs really are both inclusive so we need strict inequality.
            if (c > 0) {
                // no relevant data, we're done
                *lbi = 0;
                *ube = 0;
                return;
            }
        }
    } else {
        *lbi = 0;
    }
    if (bounds->upper_bound_inclusive) {
        struct toku_fifo_entry_key_msn_heaviside_extra ube_extra = {
            .desc = desc, .cmp = cmp,
            .fifo = buffer,
            .key = kv_pair_key((struct kv_pair *) bounds->upper_bound_inclusive),
            .keylen = kv_pair_keylen((struct kv_pair *) bounds->upper_bound_inclusive),
            .msn = MAX_MSN };
        r = toku_omt_find(message_tree, toku_fifo_entry_key_msn_heaviside,
                          &ube_extra, +1, NULL, ube);
        if (r == DB_NOTFOUND) {
            *ube = toku_omt_size(message_tree);
        }
    } else {
        *ube = toku_omt_size(message_tree);
    }
}

static int
bnc_apply_messages_to_basement_node(
    BRT t,
    BASEMENTNODE bn,
    BRTNODE ancestor,
    int childnum,
    struct pivot_bounds const * const bounds
    )
// Effect: For each messages in ANCESTOR that is between lower_bound_exclusive (exclusive) and upper_bound_inclusive (inclusive), apply the message to the node.
//  In ANCESTOR, the relevant messages are all in the buffer for child number CHILDNUM.
//  Treat the bounds as minus or plus infinity respectively if they are NULL.
//   Do not mark the node as dirty (preserve previous state of 'dirty' bit).
{
    int r;
    NONLEAF_CHILDINFO bnc = BNC(ancestor, childnum);
    u_int32_t stale_lbi, stale_ube;
    if (!bn->stale_ancestor_messages_applied) {
        bnc_find_iterate_bounds(&t->h->descriptor, t->compare_fun, bnc->stale_message_tree, bnc->buffer, bounds, &stale_lbi, &stale_ube);
    } else {
        stale_lbi = 0;
        stale_ube = 0;
    }
    u_int32_t fresh_lbi, fresh_ube;
    bnc_find_iterate_bounds(&t->h->descriptor, t->compare_fun, bnc->fresh_message_tree, bnc->buffer, bounds, &fresh_lbi, &fresh_ube);

    TOKULOGGER logger = toku_cachefile_logger(t->cf);
    OMT snapshot_txnids = logger ? logger->snapshot_txnids : NULL;
    OMT live_list_reverse = logger ? logger->live_list_reverse : NULL;
    MSN max_msn_applied = MIN_MSN;
    if (toku_omt_size(bnc->broadcast_list) > 0) {
        const int buffer_size = (stale_ube - stale_lbi) + (fresh_ube - fresh_lbi) + toku_omt_size(bnc->broadcast_list);
        long *XMALLOC_N(buffer_size, offsets);

        struct store_fifo_offset_extra sfo_extra = { .offsets = offsets, .i = 0 };
        if (!bn->stale_ancestor_messages_applied) {
            r = toku_omt_iterate_on_range(bnc->stale_message_tree, stale_lbi, stale_ube, store_fifo_offset, &sfo_extra); assert_zero(r);
        }
        struct store_fifo_offset_and_move_to_stale_extra sfoamts_extra = { .brt = t, .sfo_extra = &sfo_extra, .bnc = bnc };
        r = toku_omt_iterate_on_range(bnc->fresh_message_tree, fresh_lbi, fresh_ube, store_fifo_offset_and_move_to_stale, &sfoamts_extra); assert_zero(r);
        r = toku_omt_iterate(bnc->broadcast_list, store_fifo_offset, &sfo_extra); assert_zero(r);
        invariant(sfo_extra.i == buffer_size);
        r = mergesort_r(offsets, buffer_size, sizeof offsets[0], bnc->buffer, fifo_offset_msn_cmp); assert_zero(r);
        for (int i = 0; i < buffer_size; ++i) {
            const struct fifo_entry *entry = toku_fifo_get_entry(bnc->buffer, offsets[i]);
            do_brt_leaf_put_cmd(t, bn, ancestor, childnum, snapshot_txnids, live_list_reverse, &max_msn_applied, entry);
        }

        toku_free(offsets);
    } else if (stale_lbi == stale_ube) {
        struct iterate_do_brt_leaf_put_cmd_extra iter_extra = { .t = t, .bn = bn, .ancestor = ancestor, .childnum = childnum, .snapshot_txnids = snapshot_txnids, .live_list_reverse = live_list_reverse, .max_msn_applied = &max_msn_applied };
        struct iterate_do_brt_leaf_put_cmd_and_move_to_stale_extra iter_amts_extra = { .brt = t, .iter_extra = &iter_extra, .bnc = bnc };
        r = toku_omt_iterate_on_range(bnc->fresh_message_tree, fresh_lbi, fresh_ube, iterate_do_brt_leaf_put_cmd_and_move_to_stale, &iter_amts_extra); assert_zero(r);
    } else if (fresh_lbi == fresh_ube) {
        struct iterate_do_brt_leaf_put_cmd_extra iter_extra = { .t = t, .bn = bn, .ancestor = ancestor, .childnum = childnum, .snapshot_txnids = snapshot_txnids, .live_list_reverse = live_list_reverse, .max_msn_applied = &max_msn_applied };
        r = toku_omt_iterate_on_range(bnc->stale_message_tree, stale_lbi, stale_ube, iterate_do_brt_leaf_put_cmd, &iter_extra); assert_zero(r);
    } else {
        long *XMALLOC_N(fresh_ube - fresh_lbi, fresh_offsets_to_move);
        u_int32_t stale_i = stale_lbi, fresh_i = fresh_lbi;
        OMTVALUE stale_v, fresh_v;
        r = toku_omt_fetch(bnc->stale_message_tree, stale_i, &stale_v); assert_zero(r);
        r = toku_omt_fetch(bnc->fresh_message_tree, fresh_i, &fresh_v); assert_zero(r);
        struct toku_fifo_entry_key_msn_cmp_extra extra = { .desc= &t->h->descriptor, .cmp = t->compare_fun, .fifo = bnc->buffer };
        while (stale_i < stale_ube && fresh_i < fresh_ube) {
            const long stale_offset = (long) stale_v;
            const long fresh_offset = (long) fresh_v;
            int c = toku_fifo_entry_key_msn_cmp(&extra, &stale_offset, &fresh_offset);
            if (c < 0) {
                const struct fifo_entry *stale_entry = toku_fifo_get_entry(bnc->buffer, stale_offset);
                do_brt_leaf_put_cmd(t, bn, ancestor, childnum, snapshot_txnids, live_list_reverse, &max_msn_applied, stale_entry);
                stale_i++;
                if (stale_i != stale_ube) {
                    r = toku_omt_fetch(bnc->stale_message_tree, stale_i, &stale_v); assert_zero(r);
                }
            } else if (c > 0) {
                fresh_offsets_to_move[fresh_i - fresh_lbi] = fresh_offset;
                const struct fifo_entry *fresh_entry = toku_fifo_get_entry(bnc->buffer, fresh_offset);
                do_brt_leaf_put_cmd(t, bn, ancestor, childnum, snapshot_txnids, live_list_reverse, &max_msn_applied, fresh_entry);
                fresh_i++;
                if (fresh_i != fresh_ube) {
                    r = toku_omt_fetch(bnc->fresh_message_tree, fresh_i, &fresh_v); assert_zero(r);
                }
            } else {
                // there is a message in both trees
                assert(false);
            }
        }
        while (stale_i < stale_ube) {
            const long stale_offset = (long) stale_v;
            const struct fifo_entry *stale_entry = toku_fifo_get_entry(bnc->buffer, stale_offset);
            do_brt_leaf_put_cmd(t, bn, ancestor, childnum, snapshot_txnids, live_list_reverse, &max_msn_applied, stale_entry);
            stale_i++;
            if (stale_i != stale_ube) {
                r = toku_omt_fetch(bnc->stale_message_tree, stale_i, &stale_v); assert_zero(r);
            }
        }
        while (fresh_i < fresh_ube) {
            const long fresh_offset = (long) fresh_v;
            fresh_offsets_to_move[fresh_i - fresh_lbi] = fresh_offset;
            const struct fifo_entry *fresh_entry = toku_fifo_get_entry(bnc->buffer, fresh_offset);
            do_brt_leaf_put_cmd(t, bn, ancestor, childnum, snapshot_txnids, live_list_reverse, &max_msn_applied, fresh_entry);
            fresh_i++;
            if (fresh_i != fresh_ube) {
                r = toku_omt_fetch(bnc->fresh_message_tree, fresh_i, &fresh_v); assert_zero(r);
            }
        }
        for (u_int32_t i = 0; i < fresh_ube - fresh_lbi; ++i) {
            r = move_to_stale((OMTVALUE) fresh_offsets_to_move[i], i + fresh_lbi, t, bnc); assert_zero(r);
        }
        toku_free(fresh_offsets_to_move);
    }
    // we can't delete things inside move_to_stale because that happens
    // inside an iteration, instead we have to delete from fresh after
    for (u_int32_t ube = fresh_ube; fresh_lbi < ube; --ube) {
        r = toku_omt_delete_at(bnc->fresh_message_tree, fresh_lbi); assert_zero(r);
    }
    if (ancestor->max_msn_applied_to_node_on_disk.msn > bn->max_msn_applied.msn) {
        bn->max_msn_applied = ancestor->max_msn_applied_to_node_on_disk;
    }
    return r;
}

void
maybe_apply_ancestors_messages_to_node (BRT t, BRTNODE node, ANCESTORS ancestors, struct pivot_bounds const * const bounds)
// Effect:
//   Bring a leaf node up-to-date according to all the messages in the ancestors.   
//   If the leaf node is already up-to-date then do nothing.
//   If the leaf node is not already up-to-date, then record the work done for that leaf in each ancestor.
//   If workdone for any nonleaf nodes exceeds threshold then flush them, but don't do any merges or splits.
{
    VERIFY_NODE(t, node);
    BOOL update_stats = FALSE;
    if (node->height > 0) { goto exit; }
    // know we are a leaf node
    // need to apply messages to each basement node
    // TODO: (Zardosht) cilkify this
    for (int i = 0; i < node->n_children; i++) {
        int height = 0;
        if (BP_STATE(node, i) != PT_AVAIL) { continue; }
        BASEMENTNODE curr_bn = BLB(node, i);
        struct pivot_bounds curr_bounds = next_pivot_keys(node, i, bounds);
        for (ANCESTORS curr_ancestors = ancestors; curr_ancestors; curr_ancestors = curr_ancestors->next) {
            height++;
            if (curr_ancestors->node->max_msn_applied_to_node_on_disk.msn > curr_bn->max_msn_applied.msn) {
                assert(BP_STATE(curr_ancestors->node, curr_ancestors->childnum) == PT_AVAIL);
                bnc_apply_messages_to_basement_node(
                    t,
                    curr_bn,
                    curr_ancestors->node,
                    curr_ancestors->childnum,
                    &curr_bounds
                    );
                // we don't want to check this node again if the next time
                // we query it, the msn hasn't changed.
                curr_bn->max_msn_applied = curr_ancestors->node->max_msn_applied_to_node_on_disk;
                update_stats = TRUE;
            }
        }
        curr_bn->stale_ancestor_messages_applied = true;
    }
    // Must update the leaf estimates.	Might as well use the estimates from the soft copy (even if they make it out to disk), since they are
    // the best estimates we have.
    if (update_stats) {
        ANCESTORS curr_ancestors = ancestors;
        BRTNODE prev_node = node;
        while (curr_ancestors) {
            BRTNODE next_node = curr_ancestors->node;
            prev_node = next_node;
            curr_ancestors = curr_ancestors->next;
        }
    }
exit:
    VERIFY_NODE(t, node);
}


static int
brt_cursor_shortcut (
    BRT_CURSOR cursor, 
    int direction, 
    BRT_GET_CALLBACK_FUNCTION getf, 
    void *getf_v,
    u_int32_t *keylen,
    void **key,
    u_int32_t *vallen,
    void **val
    );


// This is a bottom layer of the search functions.
static int
brt_search_basement_node(
    BASEMENTNODE bn,
    brt_search_t *search,
    BRT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    BOOL *doprefetch,
    BRT_CURSOR brtcursor,
    BOOL can_bulk_fetch
    )
{
    // Now we have to convert from brt_search_t to the heaviside function with a direction.  What a pain...

    int direction;
    switch (search->direction) {
    case BRT_SEARCH_LEFT:   direction = +1; goto ok;
    case BRT_SEARCH_RIGHT:  direction = -1; goto ok;
    }
    return EINVAL;  // This return and the goto are a hack to get both compile-time and run-time checking on enum
 ok: ;
    OMTVALUE datav;
    u_int32_t idx = 0;
    int r = toku_omt_find(bn->buffer,
                          heaviside_from_search_t,
                          search,
                          direction,
                          &datav, &idx);
    if (r!=0) return r;

    LEAFENTRY le = datav;
    if (toku_brt_cursor_is_leaf_mode(brtcursor))
        goto got_a_good_value;	// leaf mode cursors see all leaf entries
    if (is_le_val_del(le,brtcursor)) {
        // Provisionally deleted stuff is gone.
        // So we need to scan in the direction to see if we can find something
        while (1) {
            switch (search->direction) {
            case BRT_SEARCH_LEFT:
                idx++;
                if (idx>=toku_omt_size(bn->buffer)) return DB_NOTFOUND;
                break;
            case BRT_SEARCH_RIGHT:
                if (idx==0) return DB_NOTFOUND;
                idx--;
                break;
            default:
                assert(FALSE);
            }
            r = toku_omt_fetch(bn->buffer, idx, &datav);
            assert_zero(r); // we just validated the index
            le = datav;
            if (!is_le_val_del(le,brtcursor)) goto got_a_good_value;
        }
    }
got_a_good_value:
    {
        u_int32_t keylen;
        void *key;
        u_int32_t vallen;
        void *val;

        brt_cursor_extract_key_and_val(le,
                                       brtcursor,
                                       &keylen,
                                       &key,
                                       &vallen,
                                       &val
                                       );

        r = getf(keylen, key, vallen, val, getf_v);
        if (r==0 || r == TOKUDB_CURSOR_CONTINUE) {
            brtcursor->leaf_info.to_be.omt   = bn->buffer;
            brtcursor->leaf_info.to_be.index = idx;
            
            if (r == TOKUDB_CURSOR_CONTINUE && can_bulk_fetch) {
                r = brt_cursor_shortcut(
                    brtcursor,
                    direction,
                    getf,
                    getf_v,
                    &keylen,
                    &key,
                    &vallen,
                    &val
                    );
            }

            brt_cursor_cleanup_dbts(brtcursor);
            brtcursor->key.data = toku_memdup(key, keylen);
            brtcursor->val.data = toku_memdup(val, vallen);
            brtcursor->key.size = keylen;
            brtcursor->val.size = vallen;
            //The search was successful.  Prefetching can continue.
            *doprefetch = TRUE;
        }
    }
    if (r == TOKUDB_CURSOR_CONTINUE) r = 0;
    return r;
}

static int
brt_search_node (
    BRT brt,
    BRTNODE node,
    brt_search_t *search,
    int child_to_search,
    BRT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    BOOL *doprefetch,
    BRT_CURSOR brtcursor,
    UNLOCKERS unlockers,
    ANCESTORS,
    struct pivot_bounds const * const bounds,
    BOOL can_bulk_fetch
    );

// the number of nodes to prefetch
#define TOKU_DO_PREFETCH 1
#if TOKU_DO_PREFETCH

static int
brtnode_fetch_callback_and_free_bfe(CACHEFILE cf, int fd, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, PAIR_ATTR *sizep, int *dirtyp, void *extraargs)
{
    int r = toku_brtnode_fetch_callback(cf, fd, nodename, fullhash, brtnode_pv, sizep, dirtyp, extraargs);
    destroy_bfe_for_prefetch(extraargs);
    toku_free(extraargs);
    return r;
}

static int
brtnode_pf_callback_and_free_bfe(void *brtnode_pv, void *read_extraargs, int fd, PAIR_ATTR *sizep)
{
    int r = toku_brtnode_pf_callback(brtnode_pv, read_extraargs, fd, sizep);
    destroy_bfe_for_prefetch(read_extraargs);
    toku_free(read_extraargs);
    return r;
}

static void
brt_node_maybe_prefetch(BRT brt, BRTNODE node, int childnum, BRT_CURSOR brtcursor, BOOL *doprefetch) {

    // if we want to prefetch in the tree
    // then prefetch the next children if there are any
    if (*doprefetch && brt_cursor_prefetching(brtcursor)) {
        int rc = brt_cursor_rightmost_child_wanted(brtcursor, brt, node);
        for (int i = childnum + 1; (i <= childnum + TOKU_DO_PREFETCH) && (i <= rc); i++) {
            BLOCKNUM nextchildblocknum = BP_BLOCKNUM(node, i);
            u_int32_t nextfullhash = compute_child_fullhash(brt->cf, node, i);
            struct brtnode_fetch_extra *MALLOC(bfe);
            fill_bfe_for_prefetch(bfe, brt->h, brtcursor);
            BOOL doing_prefetch = FALSE;
            toku_cachefile_prefetch(
                brt->cf,
                nextchildblocknum,
                nextfullhash,
                toku_brtnode_flush_callback,
                brtnode_fetch_callback_and_free_bfe,
                toku_brtnode_pe_est_callback,
                toku_brtnode_pe_callback,
                toku_brtnode_pf_req_callback,
                brtnode_pf_callback_and_free_bfe,
                toku_brtnode_cleaner_callback,
                bfe,
                brt->h,
                &doing_prefetch
                );
            if (!doing_prefetch) {
                destroy_bfe_for_prefetch(bfe);
                toku_free(bfe);
            }
            *doprefetch = FALSE;
        }
    }
}

#endif

struct unlock_brtnode_extra {
    BRT brt;
    BRTNODE node;
};
// When this is called, the cachetable lock is held
static void
unlock_brtnode_fun (void *v) {
    struct unlock_brtnode_extra *x = v;
    BRT brt = x->brt;
    BRTNODE node = x->node;
    // CT lock is held
    int r = toku_cachetable_unpin_ct_prelocked_no_flush(brt->cf, node->thisnodename, node->fullhash, (enum cachetable_dirty) node->dirty, make_brtnode_pair_attr(node));
    assert(r==0);
}

/* search in a node's child */
static int
brt_search_child(BRT brt, BRTNODE node, int childnum, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, BOOL *doprefetch, BRT_CURSOR brtcursor, UNLOCKERS unlockers,
                 ANCESTORS ancestors, struct pivot_bounds const * const bounds, BOOL can_bulk_fetch)
// Effect: Search in a node's child.  Searches are read-only now (at least as far as the hardcopy is concerned).
{
    struct ancestors next_ancestors = {node, childnum, ancestors};

    BLOCKNUM childblocknum = BP_BLOCKNUM(node,childnum);
    u_int32_t fullhash = compute_child_fullhash(brt->cf, node, childnum);
    BRTNODE childnode;

    struct brtnode_fetch_extra bfe;
    fill_bfe_for_subset_read(
        &bfe,
        brt->h,
        search,
        &brtcursor->range_lock_left_key,
        &brtcursor->range_lock_right_key,
        brtcursor->left_is_neg_infty,
        brtcursor->right_is_pos_infty
        );
    {
        int rr = toku_pin_brtnode(brt, childblocknum, fullhash,
                                  unlockers,
                                  &next_ancestors, bounds,
                                  &bfe,
                                  TRUE,
                                  &childnode);
        if (rr==TOKUDB_TRY_AGAIN) return rr;
        assert(rr==0);
    }

    struct unlock_brtnode_extra unlock_extra   = {brt,childnode};
    struct unlockers next_unlockers = {TRUE, unlock_brtnode_fun, (void*)&unlock_extra, unlockers};

    int r = brt_search_node(brt, childnode, search, bfe.child_to_read, getf, getf_v, doprefetch, brtcursor, &next_unlockers, &next_ancestors, bounds, can_bulk_fetch);
    if (r!=TOKUDB_TRY_AGAIN) {
        // Even if r is reactive, we want to handle the maybe reactive child.

#if TOKU_DO_PREFETCH
        // maybe prefetch the next child
        if (r == 0 && node->height == 1) {
            brt_node_maybe_prefetch(brt, node, childnum, brtcursor, doprefetch);
        }
#endif

        assert(next_unlockers.locked);
        toku_unpin_brtnode(brt, childnode); // unpin the childnode before handling the reactive child (because that may make the childnode disappear.)
    } else {
        // try again.

        // there are two cases where we get TOKUDB_TRY_AGAIN
        //  case 1 is when some later call to toku_pin_brtnode returned
        //  that value and unpinned all the nodes anyway. case 2
        //  is when brt_search_node had to stop its search because
        //  some piece of a node that it needed was not in memory. In this case,
        //  the node was not unpinned, so we unpin it here
        if (next_unlockers.locked) {
            toku_unpin_brtnode(brt, childnode);
        }
    }

    return r;
}

int
toku_brt_search_which_child(
    DESCRIPTOR desc,
    brt_compare_func cmp,
    BRTNODE node,
    brt_search_t *search
    )
{
    int c;
    DBT pivotkey;
    toku_init_dbt(&pivotkey);

    /* binary search is overkill for a small array */
    int child[node->n_children];

    /* scan left to right or right to left depending on the search direction */
    for (c = 0; c < node->n_children; c++) {
        child[c] = (search->direction == BRT_SEARCH_LEFT) ? c : node->n_children - 1 - c;
    }
    for (c = 0; c < node->n_children-1; c++) {
        int p = (search->direction == BRT_SEARCH_LEFT) ? child[c] : child[c] - 1;
        struct kv_pair *pivot = node->childkeys[p];
        toku_fill_dbt(&pivotkey, kv_pair_key(pivot), kv_pair_keylen(pivot));
        if (search_pivot_is_bounded(search, desc, cmp, &pivotkey) && search->compare(search, &pivotkey)) {
            return child[c];
        }
    }
    /* check the first (left) or last (right) node if nothing has been found */
    return child[c];
}

static void
maybe_search_save_bound(
    BRTNODE node,
    int child_searched,
    brt_search_t *search
    )
{
    DBT pivotkey;
    toku_init_dbt(&pivotkey);

    int p = (search->direction == BRT_SEARCH_LEFT) ? child_searched : child_searched - 1;
    if (p >=0 && p < node->n_children-1) {
        struct kv_pair *pivot = node->childkeys[p];
        toku_fill_dbt(&pivotkey, kv_pair_key(pivot), kv_pair_keylen(pivot));
        search_save_bound(search, &pivotkey);
    }
}

static int
brt_search_node(
    BRT brt,
    BRTNODE node,
    brt_search_t *search,
    int child_to_search,
    BRT_GET_CALLBACK_FUNCTION getf,
    void *getf_v,
    BOOL *doprefetch,
    BRT_CURSOR brtcursor,
    UNLOCKERS unlockers,
    ANCESTORS ancestors,
    struct pivot_bounds const * const bounds,
    BOOL can_bulk_fetch
    )
{   int r = 0;
    // assert that we got a valid child_to_search
    assert(child_to_search >= 0 || child_to_search < node->n_children);
    //
    // At this point, we must have the necessary partition available to continue the search
    //
    assert(BP_STATE(node,child_to_search) == PT_AVAIL);
    while (child_to_search >= 0 && child_to_search < node->n_children) {
        //
        // Normally, the child we want to use is available, as we checked
        // before entering this while loop. However, if we pass through
        // the loop once, getting DB_NOTFOUND for this first value
        // of child_to_search, we enter the while loop again with a
        // child_to_search that may not be in memory. If it is not,
        // we need to return TOKUDB_TRY_AGAIN so the query can
        // read the appropriate partition into memory
        //
        if (BP_STATE(node,child_to_search) != PT_AVAIL) {
            return TOKUDB_TRY_AGAIN;
        }
        const struct pivot_bounds next_bounds = next_pivot_keys(node, child_to_search, bounds);
        if (node->height > 0) {
            r = brt_search_child(
                brt,
                node,
                child_to_search,
                search,
                getf,
                getf_v,
                doprefetch,
                brtcursor,
                unlockers,
                ancestors,
                &next_bounds,
                can_bulk_fetch
                );
        }
        else {
            r = brt_search_basement_node(
                BLB(node, child_to_search),
                search,
                getf,
                getf_v,
                doprefetch,
                brtcursor,
                can_bulk_fetch
                );
        }
        if (r == 0) return r; //Success

        if (r != DB_NOTFOUND) {
            return r; //Error (or message to quit early, such as TOKUDB_FOUND_BUT_REJECTED or TOKUDB_TRY_AGAIN)
        }
        // we have a new pivotkey
        else {
            // If we got a DB_NOTFOUND then we have to search the next record.        Possibly everything present is not visible.
            // This way of doing DB_NOTFOUND is a kludge, and ought to be simplified.  Something like this is needed for DB_NEXT, but
            //        for point queries, it's overkill.  If we got a DB_NOTFOUND on a point query then we should just stop looking.
            // When releasing locks on I/O we must not search the same subtree again, or we won't be guaranteed to make forward progress.
            // If we got a DB_NOTFOUND, then the pivot is too small if searching from left to right (too large if searching from right to left).
            // So save the pivot key in the search object.
            // printf("%*ssave_bound %s\n", 9-node->height, "", (char*)pivotkey.data);
            maybe_search_save_bound(
                node,
                child_to_search,
                search
                );
        }
        // not really necessary, just put this here so that reading the
        // code becomes simpler. The point is at this point in the code,
        // we know that we got DB_NOTFOUND and we have to continue
        assert(r == DB_NOTFOUND);
        if (search->direction == BRT_SEARCH_LEFT) {
            child_to_search++;
        }
        else {
            child_to_search--;
        }
    }
    return r;
}

static int
toku_brt_search (BRT brt, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, BRT_CURSOR brtcursor, BOOL can_bulk_fetch)
// Effect: Perform a search.  Associate cursor with a leaf if possible.
// All searches are performed through this function.
{
    int r;
    uint trycount = 0;     // How many tries did it take to get the result?
    uint root_tries = 0;   // How many times did we fetch the root node from disk?
    uint tree_height;      // How high is the tree?  This is the height of the root node plus one (leaf is at height 0).

try_again:
    
    trycount++;
    assert(brt->h);

    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

    BRTNODE node;

    //
    // Here is how searches work
    // At a high level, we descend down the tree, using the search parameter
    // to guide us towards where to look. But the search parameter is not
    // used here to determine which child of a node to read (regardless
    // of whether that child is another node or a basement node)
    // The search parameter is used while we are pinning the node into
    // memory, because that is when the system needs to ensure that
    // the appropriate partition of the child we are using is in memory.
    // So, here are the steps for a search (and this applies to this function
    // as well as brt_search_child:
    //  - Take the search parameter, and create a brtnode_fetch_extra, that will be used by toku_pin_brtnode(_holding_lock)
    //  - Call toku_pin_brtnode(_holding_lock) with the bfe as the extra for the fetch callback (in case the node is not at all in memory)
    //       and the partial fetch callback (in case the node is perhaps partially in memory) to the fetch the node
    //  - This eventually calls either toku_brtnode_fetch_callback or  toku_brtnode_pf_req_callback depending on whether the node is in 
    //     memory at all or not.
    //  - Within these functions, the "brt_search_t search" parameter is used to evaluate which child the search is interested in.
    //     If the node is not in memory at all, toku_brtnode_fetch_callback will read the node and decompress only the partition for the 
    //     relevant child, be it a message buffer or basement node. If the node is in memory, then toku_brtnode_pf_req_callback
    //     will tell the cachetable that a partial fetch is required if and only if the relevant child is not in memory. If the relevant child
    //     is not in memory, then toku_brtnode_pf_callback is called to fetch the partition.
    //  - These functions set bfe->child_to_read so that the search code does not need to reevaluate it.
    //  - Just to reiterate, all of the last item happens within toku_brtnode_pin(_holding_lock)
    //  - At this point, toku_brtnode_pin_holding_lock has returned, with bfe.child_to_read set,
    //  - brt_search_node is called, assuming that the node and its relevant partition are in memory.
    //
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_subset_read(
        &bfe, 
        brt->h,
        search,
        &brtcursor->range_lock_left_key,
        &brtcursor->range_lock_right_key,
        brtcursor->left_is_neg_infty,
        brtcursor->right_is_pos_infty
        );
    r = toku_pin_brtnode(brt, *rootp, fullhash,(UNLOCKERS)NULL,(ANCESTORS)NULL, &infinite_bounds, &bfe, TRUE, &node);
    assert(r==0 || r== TOKUDB_TRY_AGAIN);
    if (r == TOKUDB_TRY_AGAIN) {
	root_tries++;
        goto try_again;
    }
    tree_height = node->height + 1;  // height of tree (leaf is at height 0)
    
    struct unlock_brtnode_extra unlock_extra   = {brt,node};
    struct unlockers		unlockers      = {TRUE, unlock_brtnode_fun, (void*)&unlock_extra, (UNLOCKERS)NULL};

    {
	BOOL doprefetch = FALSE;
	//static int counter = 0;	 counter++;
	r = brt_search_node(brt, node, search, bfe.child_to_read, getf, getf_v, &doprefetch, brtcursor, &unlockers, (ANCESTORS)NULL, &infinite_bounds, can_bulk_fetch);
	if (r==TOKUDB_TRY_AGAIN) {
            // there are two cases where we get TOKUDB_TRY_AGAIN
            //  case 1 is when some later call to toku_pin_brtnode returned
            //  that value and unpinned all the nodes anyway. case 2
            //  is when brt_search_node had to stop its search because
            //  some piece of a node that it needed was not in memory. 
            //  In this case, the node was not unpinned, so we unpin it here
            if (unlockers.locked) {
                toku_unpin_brtnode(brt, node);
            }
	    goto try_again;
	} else {
	    assert(unlockers.locked);
	}
    }

    assert(unlockers.locked);
    toku_unpin_brtnode(brt, node);


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
	//Therefore, the BRT_GET_CALLBACK_FUNCTION has NOT been called.
	//The contract specifies that the callback function must be called
	//for 'r= (0|DB_NOTFOUND|TOKUDB_FOUND_BUT_REJECTED)'
	//TODO: #1378 This is not the ultimate location of this call to the
	//callback.  It is surely wrong for node-level locking, and probably
	//wrong for the STRADDLE callback for heaviside function(two sets of key/vals)
	int r2 = getf(0,NULL, 0,NULL, getf_v);
	if (r2!=0) r = r2;
    }

    {   // accounting (to detect and measure thrashing)
	uint retrycount = trycount - 1;         // how many retries were needed?
	brt_status.total_searches++;
	brt_status.total_retries += retrycount;
	if (root_tries > 1) {                   // if root was read from disk more than once
	    brt_status.search_root_retries++;   
	    if (root_tries > brt_status.max_search_root_tries)
		brt_status.max_search_root_tries = root_tries; 
	}
	if (retrycount > tree_height) {         // if at least one node was read from disk more than once
	    brt_status.search_tries_gt_height++;
	    uint excess_tries = retrycount - tree_height;  
	    if (excess_tries > brt_status.max_search_excess_retries)
		brt_status.max_search_excess_retries = excess_tries;
	    if (retrycount > (tree_height+3))
		brt_status.search_tries_gt_heightplus3++;
	}
    }

    return r;
}

struct brt_cursor_search_struct {
    BRT_GET_CALLBACK_FUNCTION getf;
    void *getf_v;
    BRT_CURSOR cursor;
    brt_search_t *search;
};

/* search for the first kv pair that matches the search object */
static int
brt_cursor_search(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, BOOL can_bulk_fetch)
{
    int r = toku_brt_search(cursor->brt, search, getf, getf_v, cursor, can_bulk_fetch);
    return r;
}

static inline int compare_k_x(BRT brt, const DBT *k, const DBT *x) {
    FAKE_DB(db, &brt->h->descriptor);
    return brt->compare_fun(&db, k, x);
}

static int
brt_cursor_compare_one(brt_search_t *search __attribute__((__unused__)), DBT *x __attribute__((__unused__)))
{
    return 1;
}

static int brt_cursor_compare_set(brt_search_t *search, DBT *x) {
    BRT brt = search->context;
    return compare_k_x(brt, search->k, x) <= 0; /* return min xy: kv <= xy */
}

static int
brt_cursor_current_getf(ITEMLEN keylen,		 bytevec key,
			ITEMLEN vallen,		 bytevec val,
			void *v) {
    struct brt_cursor_search_struct *bcss = v;
    int r;
    if (key==NULL) {
	r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v);
    } else {
	BRT_CURSOR cursor = bcss->cursor;
	DBT newkey = {.size=keylen, .data=(void*)key}; // initializes other fields to zero
	if (compare_k_x(cursor->brt, &cursor->key, &newkey) != 0) {
	    r = bcss->getf(0, NULL, 0, NULL, bcss->getf_v); // This was once DB_KEYEMPTY
	    if (r==0) r = TOKUDB_FOUND_BUT_REJECTED;
	}
	else
	    r = bcss->getf(keylen, key, vallen, val, bcss->getf_v);
    }
    return r;
}

int
toku_brt_cursor_current(BRT_CURSOR cursor, int op, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    if (brt_cursor_not_set(cursor))
	return EINVAL;
    if (op == DB_CURRENT) {
	struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, 0};
	brt_search_t search; brt_search_init(&search, brt_cursor_compare_set, BRT_SEARCH_LEFT, &cursor->key, cursor->brt);
	int r = toku_brt_search(cursor->brt, &search, brt_cursor_current_getf, &bcss, cursor, FALSE);
	brt_search_finish(&search);
	return r;
    }
    return getf(cursor->key.size, cursor->key.data, cursor->val.size, cursor->val.data, getf_v); // brt_cursor_copyout(cursor, outkey, outval);
}

static int
brt_flatten_getf(ITEMLEN UU(keylen),	  bytevec UU(key),
		 ITEMLEN UU(vallen),	  bytevec UU(val),
		 void *UU(v)) {
    return DB_NOTFOUND;
}

int
toku_brt_flatten(BRT brt, TOKUTXN ttxn)
{
    BRT_CURSOR tmp_cursor;
    int r = toku_brt_cursor(brt, &tmp_cursor, ttxn, FALSE);
    if (r!=0) return r;
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_LEFT, 0, tmp_cursor->brt);
    r = brt_cursor_search(tmp_cursor, &search, brt_flatten_getf, NULL, FALSE);
    brt_search_finish(&search);
    if (r==DB_NOTFOUND) r = 0;
    {
	//Cleanup temporary cursor
	int r2 = toku_brt_cursor_close(tmp_cursor);
	if (r==0) r = r2;
    }
    return r;
}

int
toku_brt_cursor_first(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_LEFT, 0, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, FALSE);
    brt_search_finish(&search);
    return r;
}

int
toku_brt_cursor_last(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_one, BRT_SEARCH_RIGHT, 0, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, FALSE);
    brt_search_finish(&search);
    return r;
}

static int brt_cursor_compare_next(brt_search_t *search, DBT *x) {
    BRT brt = search->context;
    return compare_k_x(brt, search->k, x) < 0; /* return min xy: kv < xy */
}


static int
brt_cursor_shortcut (
    BRT_CURSOR cursor, 
    int direction, 
    BRT_GET_CALLBACK_FUNCTION getf, 
    void *getf_v,
    u_int32_t *keylen,
    void **key,
    u_int32_t *vallen,
    void **val
    ) 
{
    int r = 0;
    u_int32_t index = cursor->leaf_info.to_be.index;
    OMT omt = cursor->leaf_info.to_be.omt;
    // if we are searching towards the end, limit is last element
    // if we are searching towards the beginning, limit is the first element
    u_int32_t limit = (direction > 0) ? (toku_omt_size(omt) - 1) : 0;

    //Starting with the prev, find the first real (non-provdel) leafentry.
    while (index != limit) {
        OMTVALUE le = NULL;
        index += direction;
        r = toku_omt_fetch(omt, index, &le);
        assert_zero(r);
    
        if (toku_brt_cursor_is_leaf_mode(cursor) || !is_le_val_del(le, cursor)) {
            
            brt_cursor_extract_key_and_val(
                le,
                cursor,
                keylen,
                key,
                vallen,
                val
                );
            
            r = getf(*keylen, *key, *vallen, *val, getf_v);
            if (r==0 || r == TOKUDB_CURSOR_CONTINUE) {
                //Update cursor.
                cursor->leaf_info.to_be.index = index;
            }
            if (r== TOKUDB_CURSOR_CONTINUE) {
                continue;
            }
            else {
                break;
            }
        }
    }
    return r;
}

int
toku_brt_cursor_next(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next, BRT_SEARCH_LEFT, &cursor->key, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, TRUE);
    brt_search_finish(&search);
    if (r == 0) brt_cursor_set_prefetching(cursor);
    return r;
}

static int
brt_cursor_search_eq_k_x_getf(ITEMLEN keylen,	       bytevec key,
			      ITEMLEN vallen,	       bytevec val,
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
brt_cursor_search_eq_k_x(BRT_CURSOR cursor, brt_search_t *search, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    struct brt_cursor_search_struct bcss = {getf, getf_v, cursor, search};
    int r = toku_brt_search(cursor->brt, search, brt_cursor_search_eq_k_x_getf, &bcss, cursor, FALSE);
    return r;
}

static int brt_cursor_compare_prev(brt_search_t *search, DBT *x) {
    BRT brt = search->context;
    return compare_k_x(brt, search->k, x) > 0; /* return max xy: kv > xy */
}

int
toku_brt_cursor_prev(BRT_CURSOR cursor, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev, BRT_SEARCH_RIGHT, &cursor->key, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, TRUE);
    brt_search_finish(&search);
    return r;
}

static int brt_cursor_compare_set_range(brt_search_t *search, DBT *x) {
    BRT brt = search->context;
    return compare_k_x(brt, search->k,	x) <= 0; /* return kv <= xy */
}

int
toku_brt_cursor_set(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, cursor->brt);
    int r = brt_cursor_search_eq_k_x(cursor, &search, getf, getf_v);
    brt_search_finish(&search);
    return r;
}

int
toku_brt_cursor_set_range(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range, BRT_SEARCH_LEFT, key, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, FALSE);
    brt_search_finish(&search);
    return r;
}

static int brt_cursor_compare_set_range_reverse(brt_search_t *search, DBT *x) {
    BRT brt = search->context;
    return compare_k_x(brt, search->k, x) >= 0; /* return kv >= xy */
}

int
toku_brt_cursor_set_range_reverse(BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_set_range_reverse, BRT_SEARCH_RIGHT, key, cursor->brt);
    int r = brt_cursor_search(cursor, &search, getf, getf_v, FALSE);
    brt_search_finish(&search);
    return r;
}


//TODO: When tests have been rewritten, get rid of this function.
//Only used by tests.
int
toku_brt_cursor_get (BRT_CURSOR cursor, DBT *key, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v, int get_flags)
{
    int op = get_flags & DB_OPFLAGS_MASK;
    if (get_flags & ~DB_OPFLAGS_MASK)
	return EINVAL;

    switch (op) {
    case DB_CURRENT:
    case DB_CURRENT_BINDING:
	return toku_brt_cursor_current(cursor, op, getf, getf_v);
    case DB_FIRST:
	return toku_brt_cursor_first(cursor, getf, getf_v);
    case DB_LAST:
	return toku_brt_cursor_last(cursor, getf, getf_v);
    case DB_NEXT:
    case DB_NEXT_NODUP:
	if (brt_cursor_not_set(cursor))
	    return toku_brt_cursor_first(cursor, getf, getf_v);
	else
	    return toku_brt_cursor_next(cursor, getf, getf_v);
    case DB_PREV:
    case DB_PREV_NODUP:
	if (brt_cursor_not_set(cursor))
	    return toku_brt_cursor_last(cursor, getf, getf_v);
	else
	    return toku_brt_cursor_prev(cursor, getf, getf_v);
    case DB_SET:
	return toku_brt_cursor_set(cursor, key, getf, getf_v);
    case DB_SET_RANGE:
	return toku_brt_cursor_set_range(cursor, key, getf, getf_v);
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
    *pkey = &cursor->key;
    *pval = &cursor->val;
}

//We pass in toku_dbt_fake to the search functions, since it will not pass the
//key(or val) to the heaviside function if key(or val) is NULL.
//It is not used for anything else,
//the actual 'extra' information for the heaviside function is inside the
//wrapper.
static const DBT __toku_dbt_fake;
static const DBT* const toku_dbt_fake = &__toku_dbt_fake;

BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c) {
    return brt_cursor_not_set(c);
}

int toku_brt_get_cursor_count (BRT brt) {
    int n = 0;
    struct toku_list *list;
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
toku_brt_lookup (BRT brt, DBT *k, BRT_GET_CALLBACK_FUNCTION getf, void *getf_v)
{
    int r, rr;
    BRT_CURSOR cursor;

    rr = toku_brt_cursor(brt, &cursor, NULL, FALSE);
    if (rr != 0) return rr;

    int op = DB_SET;
    r = toku_brt_cursor_get(cursor, k, getf, getf_v, op);

    rr = toku_brt_cursor_close(cursor); assert_zero(rr);

    return r;
}

/* ********************************* delete **************************************/
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
	    r = toku_brt_cursor_current(cursor, DB_CURRENT, getf_nothing, NULL);
	}
	if (r == 0) {
	    r = toku_brt_delete(cursor->brt, &cursor->key, txn);
	}
    }
    return r;
}

/* ********************* keyrange ************************ */


struct keyrange_compare_s {
    BRT brt;
    DBT *key;
};

static int keyrange_compare (OMTVALUE lev, void *extra) {
    LEAFENTRY le = lev;
    u_int32_t keylen;
    void* key = le_key_and_len(le, &keylen);
    DBT   omt_dbt;
    toku_fill_dbt(&omt_dbt, key, keylen);
    struct keyrange_compare_s *s = extra;
    return s->brt->compare_fun(s->brt->db, &omt_dbt, s->key);
}

static void keyrange_in_leaf_partition (BRT brt, BRTNODE node, DBT *key, int child_number, int estimated_row_size,
					u_int64_t *less, u_int64_t *equal, u_int64_t *greater)
// If the partition is in main memory then estimate the number
// If KEY==NULL then use an arbitrary key (leftmost or zero)
{
    assert(node->height==0); // we are in a leaf
    if (BP_STATE(node, child_number) == PT_AVAIL) {
	// If the partition is in main memory then get an exact count.
	struct keyrange_compare_s s = {brt,key};
	BASEMENTNODE bn = BLB(node, child_number);
	OMTVALUE datav;
	u_int32_t idx = 0;
	// if key is NULL then set r==-1 and idx==0.
	int r = key ? toku_omt_find_zero(bn->buffer, keyrange_compare, &s, &datav, &idx) : -1;
	if (r==0) {
	    *less    = idx;
	    *equal   = 1;
	    *greater = toku_omt_size(bn->buffer)-idx-1;
	} else {
	    // If not found, then the idx says where it's between.
	    *less    = idx;
	    *equal   = 0;
	    *greater = toku_omt_size(bn->buffer)-idx;
	}
    } else {
	u_int32_t size = BP_SIZE(node, child_number);
	*less    = (size/2)/estimated_row_size;
	*equal   = 0;
	*greater = *less;
    }
}

static u_int64_t estimate_rows_in_leaf_partition (BRTNODE node, int child_number, int estimated_row_size) {
    assert(node->height==0);
    if (BP_STATE(node, child_number) == PT_AVAIL) {
	return toku_omt_size(BLB(node, child_number)->buffer);
    } else {
	return BP_SIZE(node, child_number)/estimated_row_size;
    }
}

static u_int64_t ipow (u_int64_t v, int exp)
// Compute v^exp
{
    u_int64_t result = 1;
    for (int j=0; j<exp; j++) {
	result*=v;
    }
    return result;
}

static int toku_brt_keyrange_internal (BRT brt, BRTNODE node,
				       DBT *key,
				       int estimated_row_size,
				       struct brtnode_fetch_extra *bfe, // set up to read a minimal read.
				       u_int64_t *less, u_int64_t *equal, u_int64_t *greater,
				       struct unlockers *unlockers, ANCESTORS ancestors, struct pivot_bounds const * const bounds)
// Implementation note: Assign values to less, equal, and greater, and then on the way out (returning up the stack) we add more values in.
{
    int r = 0;
    // if KEY is NULL then use the leftmost key.
    int child_number = key ? toku_brtnode_which_child (node, key, &brt->h->descriptor, brt->compare_fun) : 0;
    if (node->height==0) {
	// we are at the leaf.
	keyrange_in_leaf_partition(brt, node, key, child_number, estimated_row_size, less, equal, greater);
	for (int j=0; j<child_number; j++) {
	    *less    += estimate_rows_in_leaf_partition(node, j, estimated_row_size);
	}
	for (int j=child_number+1; j<node->n_children; j++) {
	    *greater += estimate_rows_in_leaf_partition(node, j, estimated_row_size);
	}
    } else {
	// do the child.
	struct ancestors next_ancestors = {node, child_number, ancestors};
	BLOCKNUM childblocknum = BP_BLOCKNUM(node, child_number);
	u_int32_t fullhash = compute_child_fullhash(brt->cf, node, child_number);
	BRTNODE childnode;
	r = toku_pin_brtnode(brt, childblocknum, fullhash, unlockers, &next_ancestors, bounds, bfe, FALSE, &childnode);
	if (r!=TOKUDB_TRY_AGAIN) {
	    assert(r==0);
	    struct unlock_brtnode_extra unlock_extra   = {brt,childnode};
	    struct unlockers next_unlockers = {TRUE, unlock_brtnode_fun, (void*)&unlock_extra, unlockers};
	    const struct pivot_bounds next_bounds = next_pivot_keys(node, child_number, bounds);

	    r = toku_brt_keyrange_internal(brt, childnode, key, estimated_row_size, bfe,
					       less, equal, greater,
					       &next_unlockers, &next_ancestors, &next_bounds);
	    if (r!=TOKUDB_TRY_AGAIN) {
		assert(r==0);

		// Then update the left and right
		// Estimate the number of leaf nodes below assuming the fanout is BRT_FANOUT*3/4.
		// Estimate the size of each leaf node as if it is 3/4 full.
		u_int64_t subtree_rows_estimate = (ipow(BRT_FANOUT*3/4, node->height)*(brt->nodesize*3/4))/estimated_row_size;
		*less    += subtree_rows_estimate*child_number;
		*greater += subtree_rows_estimate*(node->n_children-child_number-1);

		assert(unlockers->locked);
		toku_unpin_brtnode(brt, childnode);
	    }
	}
    }
    return r;
}

int toku_brt_keyrange (BRT brt, DBT *key, u_int64_t *less_p, u_int64_t *equal_p, u_int64_t *greater_p) 
// Effect: Return an estimate  of the number of keys to the left, the number equal, and the number to the right of the key.
//   The values are an estimate.
//   If you perform a keyrange on two keys that are in the same in-memory leaf entry, you can the keys_right numbers (or the keys_left) numbers
//   to get an exact number keys in the range.
//   If KEY is NULL then the system picks an arbitrary key and returns it.
{
 try_again:
    {
	u_int64_t less = 0, equal = 0, greater = 0;
	assert(brt->h);
	u_int32_t fullhash;
	CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);

	struct brtnode_fetch_extra bfe;
	fill_bfe_for_min_read(&bfe, 
			      brt->h);

	BRTNODE node;
	{
	    int r = toku_pin_brtnode(brt, *rootp, fullhash,(UNLOCKERS)NULL,(ANCESTORS)NULL, &infinite_bounds, &bfe, FALSE, &node);
	    assert(r==0 || r== TOKUDB_TRY_AGAIN);
	    if (r == TOKUDB_TRY_AGAIN) {
		goto try_again;
	    }
	}

	struct unlock_brtnode_extra unlock_extra   = {brt,node};
	struct unlockers		unlockers      = {TRUE, unlock_brtnode_fun, (void*)&unlock_extra, (UNLOCKERS)NULL};

	{
	    int r = toku_brt_keyrange_internal (brt, node, key,
						100, /* for now we are using 100 as the estimate of the row size.  Later we'd like a better estimate. */
						&bfe, &less, &equal, &greater,
						&unlockers, (ANCESTORS)NULL, &infinite_bounds);
	    assert(r==0 || r== TOKUDB_TRY_AGAIN);
	    if (r==TOKUDB_TRY_AGAIN) {
		assert(!unlockers.locked);
		goto try_again;
	    }
	}
	assert(unlockers.locked);
	toku_unpin_brtnode(brt, node);
	*less_p    = less;
	*equal_p   = equal;
	*greater_p = greater;
    }
    return 0;
}


int toku_brt_stat64 (BRT brt, TOKUTXN UU(txn), struct brtstat64_s *s) {
    {
	int64_t file_size;
	int fd = toku_cachefile_get_and_pin_fd(brt->cf);
	int r = toku_os_get_file_size(fd, &file_size);
	toku_cachefile_unpin_fd(brt->cf);
	assert_zero(r);
	s->fsize = file_size + toku_cachefile_size_in_memory(brt->cf);
    }

    assert(brt->h);
#if 0
    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    CACHEKEY root = *rootp;
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, brt->h);
    BRTNODE node;
    toku_pin_brtnode_holding_lock(brt, root, fullhash, (ANCESTORS)NULL, &infinite_bounds, &bfe, FALSE, &node);

    s->nkeys = s->ndata = s->dsize = 0;
    int i;
    for (i=0; i<node->n_children; i++) {
        SUBTREE_EST se = &BP_SUBTREE_EST(node,i);
        s->nkeys += se->nkeys;
        s->ndata += se->ndata;
        s->dsize += se->dsize;
    }
    {
	int r = toku_cachetable_unpin(brt->cf, root, fullhash, CACHETABLE_CLEAN, make_brtnode_pair_attr(node));
	if (r!=0) return r;
    }
#else
    // A hack for now.
    {
	u_int64_t less=0, equal=0, greater=0;
	int r = toku_brt_keyrange(brt, NULL, &less, &equal, &greater);
	assert(r==0);
	s->nkeys = less + equal + greater;
	s->ndata = less + equal + greater;
	s->dsize = s->nkeys * 100; // estimate for now.
    }
#endif
    // 4018
    s->create_time_sec = brt->h->time_of_creation;
    s->modify_time_sec = brt->h->time_of_last_modification;
    s->verify_time_sec = brt->h->time_of_last_verification;
    
    return 0;
}

/* ********************* debugging dump ************************ */
static int
toku_dump_brtnode (FILE *file, BRT brt, BLOCKNUM blocknum, int depth, struct kv_pair *lorange, struct kv_pair *hirange) {
    int result=0;
    BRTNODE node;
    void *node_v;
    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    result=toku_verify_brtnode(brt, ZERO_MSN, ZERO_MSN, blocknum, -1, lorange, hirange, NULL, NULL, 0, 1, 0);
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->h);
    int r = toku_cachetable_get_and_pin(
        brt->cf, 
        blocknum, 
        fullhash,
        &node_v, 
        NULL,
	toku_brtnode_flush_callback, 
	toku_brtnode_fetch_callback, 
	toku_brtnode_pe_est_callback,
	toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        toku_brtnode_cleaner_callback,
	&bfe, 
	brt->h
	);
    assert_zero(r);
    node=node_v;
    assert(node->fullhash==fullhash);
    fprintf(file, "%*sNode=%p\n", depth, "", node);
    
    fprintf(file, "%*sNode %"PRId64" nodesize=%u height=%d n_children=%d  keyrange=%s %s\n",
	depth, "", blocknum.b, node->nodesize, node->height, node->n_children, (char*)(lorange ? kv_pair_key(lorange) : 0), (char*)(hirange ? kv_pair_key(hirange) : 0));
    {
	int i;
	for (i=0; i+1< node->n_children; i++) {
	    fprintf(file, "%*spivotkey %d =", depth+1, "", i);
	    toku_print_BYTESTRING(file, toku_brt_pivot_key_len(node->childkeys[i]), node->childkeys[i]->key);
	    fprintf(file, "\n");
	}
	for (i=0; i< node->n_children; i++) {
	    if (node->height > 0) {
                NONLEAF_CHILDINFO bnc = BNC(node, i);
		fprintf(file, "%*schild %d buffered (%d entries):", depth+1, "", i, toku_bnc_n_entries(bnc));
		FIFO_ITERATE(bnc->buffer, key, keylen, data, datalen, type, msn, xids, UU(is_fresh),
				  {
				      data=data; datalen=datalen; keylen=keylen;
				      fprintf(file, "%*s xid=%"PRIu64" %u (type=%d) msn=0x%"PRIu64"\n", depth+2, "", xids_get_innermost_xid(xids), (unsigned)toku_dtoh32(*(int*)key), type, msn.msn);
				      //assert(strlen((char*)key)+1==keylen);
				      //assert(strlen((char*)data)+1==datalen);
				  });
	    }
	    else {
		int size = toku_omt_size(BLB_BUFFER(node, i));
		if (0)
		for (int j=0; j<size; j++) {
		    OMTVALUE v = 0;
		    r = toku_omt_fetch(BLB_BUFFER(node, i), j, &v);
		    assert_zero(r);
		    fprintf(file, " [%d]=", j);
		    print_leafentry(file, v);
		    fprintf(file, "\n");
		}
		//	       printf(" (%d)%u ", len, *(int*)le_key(data)));
		fprintf(file, "\n");
	    }
	}
	if (node->height > 0) {
	    for (i=0; i<node->n_children; i++) {
		fprintf(file, "%*schild %d\n", depth, "", i);
		if (i>0) {
		    char *key = node->childkeys[i-1]->key;
		    fprintf(file, "%*spivot %d len=%u %u\n", depth+1, "", i-1, node->childkeys[i-1]->keylen, (unsigned)toku_dtoh32(*(int*)key));
		}
		toku_dump_brtnode(file, brt, BP_BLOCKNUM(node, i), depth+4,
				  (i==0) ? lorange : node->childkeys[i-1],
				  (i==node->n_children-1) ? hirange : node->childkeys[i]);
	    }
	}
    }
    r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, make_brtnode_pair_attr(node));
    assert_zero(r);
    return result;
}

int toku_dump_brt (FILE *f, BRT brt) {
    CACHEKEY *rootp = NULL;
    assert(brt->h);
    u_int32_t fullhash = 0;
    toku_dump_translation_table(f, brt->h->blocktable);
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    return toku_dump_brtnode(f, brt, *rootp, 0, 0, 0);
}

int toku_brt_truncate (BRT brt) {
    int r;

    // flush the cached tree blocks and remove all related pairs from the cachetable
    r = toku_brt_flush(brt);

    // TODO log the truncate?

    int fd = toku_cachefile_get_and_pin_fd(brt->cf);
    toku_brtheader_lock(brt->h);
    if (r==0) {
	//Free all data blocknums and associated disk space (if not held on to by checkpoint)
	toku_block_translation_truncate_unlocked(brt->h->blocktable, fd, brt->h);
	//Assign blocknum for root block, also dirty the header
	toku_allocate_blocknum_unlocked(brt->h->blocktable, &brt->h->root, brt->h);
	// reinit the header
	r = brt_init_header_partial(brt, NULL);
    }

    toku_brtheader_unlock(brt->h);
    toku_cachefile_unpin_fd(brt->cf);

    return r;
}

static int
toku_brt_lock_init(void) {
    int r = 0;
    if (r==0)
	r = toku_pwrite_lock_init();
    return r;
}

static int
toku_brt_lock_destroy(void) {
    int r = 0;
    if (r==0) 
	r = toku_pwrite_lock_destroy();
    return r;
}

int toku_brt_init(void (*ydb_lock_callback)(void),
		  void (*ydb_unlock_callback)(void),
		  void (*db_set_brt)(DB*,BRT)) {
    int r = 0;
    //Portability must be initialized first
    if (r==0) 
	r = toku_portability_init();
    if (r==0) 
	r = toku_brt_lock_init();
    if (r==0) 
	r = toku_checkpoint_init(ydb_lock_callback, ydb_unlock_callback);
    if (r == 0)
	r = toku_brt_serialize_init();
    if (r==0)
	callback_db_set_brt = db_set_brt;
    brt_status.cleaner_min_buffer_size = UINT64_MAX;
    brt_status.cleaner_min_buffer_workdone = UINT64_MAX;
    return r;
}

int toku_brt_destroy(void) {
    int r = 0;
    if (r == 0)
	r = toku_brt_serialize_destroy();
    if (r==0) 
	r = toku_brt_lock_destroy();
    if (r==0)
	r = toku_checkpoint_destroy();
    //Portability must be cleaned up last
    if (r==0) 
	r = toku_portability_destroy();
    return r;
}


// Require that dictionary specified by brt is fully written to disk before
// transaction txn is committed.
void
toku_brt_require_local_checkpoint (BRT brt, TOKUTXN txn) {
    toku_brtheader_lock(brt->h);
    toku_list_push(&txn->checkpoint_before_commit,
		   &brt->h->checkpoint_before_commit_link);
    toku_brtheader_unlock(brt->h);
}


//Suppress both rollback and recovery logs.
void
toku_brt_suppress_recovery_logs (BRT brt, TOKUTXN txn) {
    assert(brt->h->txnid_that_created_or_locked_when_empty == toku_txn_get_txnid(txn));
    assert(brt->h->txnid_that_suppressed_recovery_logs	   == TXNID_NONE);
    brt->h->txnid_that_suppressed_recovery_logs		   = toku_txn_get_txnid(txn);
    toku_list_push(&txn->checkpoint_before_commit, &brt->h->checkpoint_before_commit_link);
}

BOOL
toku_brt_is_recovery_logging_suppressed (BRT brt) {
    return brt->h->txnid_that_suppressed_recovery_logs != TXNID_NONE;
}

LSN toku_brt_checkpoint_lsn(BRT brt) {
    return brt->h->checkpoint_lsn;
}

int toku_brt_header_set_panic(struct brt_header *h, int panic, char *panic_string) {
    if (h->panic == 0) {
	h->panic = panic;
	if (h->panic_string) 
	    toku_free(h->panic_string);
	h->panic_string = toku_strdup(panic_string);
    }
    return 0;
}

int toku_brt_set_panic(BRT brt, int panic, char *panic_string) {
    return toku_brt_header_set_panic(brt->h, panic, panic_string);
}

#if 0

int toku_logger_save_rollback_fdelete (TOKUTXN txn, u_int8_t file_was_open, FILENUM filenum, BYTESTRING iname)

int toku_logger_log_fdelete (TOKUTXN txn, const char *fname, FILENUM filenum, u_int8_t was_open)
#endif

// Prepare to remove a dictionary from the database when this transaction is committed:
//  - if cachetable has file open, mark it as in use so that cf remains valid until we're done
//  - mark transaction as NEED fsync on commit
//  - make entry in rollback log
//  - make fdelete entry in recovery log
int toku_brt_remove_on_commit(TOKUTXN txn, DBT* iname_in_env_dbt_p) {
    assert(txn);
    int r;
    const char *iname_in_env = iname_in_env_dbt_p->data;
    CACHEFILE cf = NULL;
    u_int8_t was_open = 0;
    FILENUM filenum   = {0};

    r = toku_cachefile_of_iname_in_env(txn->logger->ct, iname_in_env, &cf);
    if (r == 0) {
	was_open = TRUE;
	filenum = toku_cachefile_filenum(cf);
	struct brt_header *h = toku_cachefile_get_userdata(cf);
	BRT brt;
	//Any arbitrary brt of that header is fine.
	toku_brtheader_lock(h);
	if (!toku_list_empty(&h->live_brts)) {
	    brt = toku_list_struct(toku_list_head(&h->live_brts), struct brt, live_brt_link);
	}
	else {
	    //Header exists, so at least one brt must.	No live means at least one zombie.
	    assert(!toku_list_empty(&h->zombie_brts));
	    brt = toku_list_struct(toku_list_head(&h->zombie_brts), struct brt, zombie_brt_link);
	}
	toku_brtheader_unlock(h);
	r = toku_txn_note_brt(txn, brt);
	if (r!=0) return r;
    }
    else 
	assert(r==ENOENT);

    toku_txn_force_fsync_on_commit(txn);  //If the txn commits, the commit MUST be in the log
				     //before the file is actually unlinked
    {
	BYTESTRING iname_in_env_bs = { .len=strlen(iname_in_env), .data = (char*)iname_in_env };
	// make entry in rollback log
	r = toku_logger_save_rollback_fdelete(txn, was_open, filenum, &iname_in_env_bs);
	assert_zero(r); //On error we would need to remove the CF reference, which is complicated.
    }
    if (r==0)
	// make entry in recovery log
	r = toku_logger_log_fdelete(txn, iname_in_env);
    return r;
}


// Non-transaction version of fdelete
int toku_brt_remove_now(CACHETABLE ct, DBT* iname_in_env_dbt_p) {
    int r;
    const char *iname_in_env = iname_in_env_dbt_p->data;
    CACHEFILE cf;
    r = toku_cachefile_of_iname_in_env(ct, iname_in_env, &cf);
    if (r == 0) {
	r = toku_cachefile_redirect_nullfd(cf);
	assert_zero(r);
    }
    else
	assert(r==ENOENT);
    char *iname_in_cwd = toku_cachetable_get_fname_in_cwd(ct, iname_in_env_dbt_p->data);
    
    r = unlink(iname_in_cwd);  // we need a pathname relative to cwd
    assert_zero(r);
    toku_free(iname_in_cwd);
    return r;
}

int
toku_brt_get_fragmentation(BRT brt, TOKU_DB_FRAGMENTATION report) {
    int r;

    int fd = toku_cachefile_get_and_pin_fd(brt->cf);
    toku_brtheader_lock(brt->h);

    int64_t file_size;
    if (toku_cachefile_is_dev_null_unlocked(brt->cf))
	r = EINVAL;
    else
	r = toku_os_get_file_size(fd, &file_size);
    if (r==0) {
	report->file_size_bytes = file_size;
	toku_block_table_get_fragmentation_unlocked(brt->h->blocktable, report);
    }
    toku_brtheader_unlock(brt->h);
    toku_cachefile_unpin_fd(brt->cf);
    return r;
}

static BOOL is_empty_fast_iter (BRT brt, BRTNODE node) {
    if (node->height > 0) {
	for (int childnum=0; childnum<node->n_children; childnum++) {
            if (toku_bnc_nbytesinbuf(BNC(node, childnum)) != 0) {
                return 0; // it's not empty if there are bytes in buffers
            }
	    BRTNODE childnode;
	    {
		void *node_v;
		BLOCKNUM childblocknum = BP_BLOCKNUM(node,childnum);
		u_int32_t fullhash =  compute_child_fullhash(brt->cf, node, childnum);
                struct brtnode_fetch_extra bfe;
                fill_bfe_for_full_read(&bfe, brt->h);
		int rr = toku_cachetable_get_and_pin(
                    brt->cf, 
                    childblocknum, 
                    fullhash, 
                    &node_v, 
                    NULL, 
                    toku_brtnode_flush_callback, 
                    toku_brtnode_fetch_callback, 
                    toku_brtnode_pe_est_callback,
                    toku_brtnode_pe_callback, 
                    toku_brtnode_pf_req_callback,
                    toku_brtnode_pf_callback,
                    toku_brtnode_cleaner_callback,
                    &bfe, 
                    brt->h
                    );
		assert(rr ==0);
		childnode = node_v;
	    }
	    int child_is_empty = is_empty_fast_iter(brt, childnode);
	    toku_unpin_brtnode(brt, childnode);
	    if (!child_is_empty) return 0;
	}
	return 1;
    } else {
	// leaf:  If the omt is empty, we are happy.
	for (int i = 0; i < node->n_children; i++) {
	    if (toku_omt_size(BLB_BUFFER(node, i))) {
		return FALSE;
	    }
	}
	return TRUE;
    }
}

BOOL toku_brt_is_empty_fast (BRT brt)
// A fast check to see if the tree is empty.  If there are any messages or leafentries, we consider the tree to be nonempty.  It's possible that those
// messages and leafentries would all optimize away and that the tree is empty, but we'll say it is nonempty.
{
    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    BRTNODE node;
    //assert(fullhash == toku_cachetable_hash(brt->cf, *rootp));
    {
	void *node_v;
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, brt->h);
	int rr = toku_cachetable_get_and_pin(
            brt->cf, 
            *rootp, 
            fullhash,
            &node_v, 
            NULL, 
            toku_brtnode_flush_callback, 
            toku_brtnode_fetch_callback, 
            toku_brtnode_pe_est_callback,
            toku_brtnode_pe_callback, 
            toku_brtnode_pf_req_callback,
            toku_brtnode_pf_callback,
            toku_brtnode_cleaner_callback,
            &bfe,
            brt->h
            );
	assert_zero(rr);
	node = node_v;
    }
    BOOL r = is_empty_fast_iter(brt, node);
    toku_unpin_brtnode(brt, node);
    return r;
}

int toku_brt_strerror_r(int error, char *buf, size_t buflen)
{
    if (error>=0) {
	return strerror_r(error, buf, buflen);
    } else {
	switch (error) {
	case DB_KEYEXIST:
	    snprintf(buf, buflen, "Key exists");
	    return 0;
	case TOKUDB_CANCELED:
	    snprintf(buf, buflen, "User canceled operation");
	    return 0;
	default:
	    snprintf(buf, buflen, "Unknown error %d", error);
	    errno = EINVAL;
	    return -1;
	}
    }
}


void 
toku_reset_root_xid_that_created(BRT brt, TXNID new_root_xid_that_created) {
    // Reset the root_xid_that_created field to the given value.  
    // This redefines which xid created the dictionary.

    struct brt_header *h = brt->h;    

    // hold lock around setting and clearing of dirty bit
    // (see cooperative use of dirty bit in toku_brtheader_begin_checkpoint())
    toku_brtheader_lock (h);
    h->root_xid_that_created = new_root_xid_that_created;
    h->dirty = 1;
    toku_brtheader_unlock (h);
}

#include <valgrind/drd.h>
void __attribute__((__constructor__)) toku_brt_drd_ignore(void);
void
toku_brt_drd_ignore(void) {
    DRD_IGNORE_VAR(brt_status);
}
