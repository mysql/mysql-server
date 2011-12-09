/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <brt-flusher.h>
#include <brt-cachetable-wrappers.h>
#include <brt-internal.h>

#define ft_flush_before_applying_inbox 1
#define ft_flush_before_child_pin 2
#define ft_flush_after_child_pin 3
#define ft_flush_before_split 4
#define ft_flush_during_split 5
#define ft_flush_before_merge 6
#define ft_flush_after_merge 7
#define ft_flush_after_rebalance 8

//
// For test purposes only.
// These callbacks are never used in production code, only as a way
//  to test the system (for example, by causing crashes at predictable times).
//
static void (*flusher_thread_callback)(int, void*) = NULL;
static void *flusher_thread_callback_extra = NULL;

void toku_flusher_thread_set_callback(void (*callback_f)(int, void*),
                                      void* extra) {
    flusher_thread_callback = callback_f;
    flusher_thread_callback_extra = extra;
}

static void call_flusher_thread_callback(int ft_state) {
    if (flusher_thread_callback) {
        flusher_thread_callback(ft_state, flusher_thread_callback_extra);
    }
}

static void
find_heaviest_child(BRTNODE node, int *childnum)
{
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

static void
update_flush_status(BRTNODE UU(parent), BRTNODE child, int cascades, BRT_STATUS brt_status)
{
    lazy_assert(brt_status);
    brt_status->flush_total++;
    if (cascades > 0) {
        brt_status->flush_cascades++;
        switch (cascades) {
        case 1:
            brt_status->flush_cascades_1++; break;
        case 2:
            brt_status->flush_cascades_2++; break;
        case 3:
            brt_status->flush_cascades_3++; break;
        case 4:
            brt_status->flush_cascades_4++; break;
        case 5:
            brt_status->flush_cascades_5++; break;
        default:
            brt_status->flush_cascades_gt_5++; break;
        }
    }
    bool flush_needs_io = false;
    for (int i = 0; !flush_needs_io && i < child->n_children; ++i) {
        if (BP_STATE(child, i) == PT_ON_DISK) {
            flush_needs_io = true;
        }
    }
    if (flush_needs_io) {
        brt_status->flush_needed_io++;
    } else {
        brt_status->flush_in_memory++;
    }
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


//
// This returns true if the node MAY be reactive,
// false is we are absolutely sure that it is NOT reactive.
// The reason for inaccuracy is that the node may be
// a leaf node that is not entirely in memory. If so, then
// we cannot be sure if the node is reactive.
//
static bool may_node_be_reactive(BRTNODE node)
{
    if (node->height == 0) return true;
    else {
        return (get_nonleaf_reactivity(node) != RE_STABLE);
    }
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
handle_split_of_child(
    BRTNODE node,
    int childnum,
    BRTNODE childa,
    BRTNODE childb,
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

static int
verify_in_mempool(OMTVALUE lev, u_int32_t UU(idx), void *vmp)
{
    LEAFENTRY le = lev;
    struct mempool *mp = vmp;
    lazy_assert(toku_mempool_inrange(mp, le, leafentry_memsize(le)));
    return 0;
}

static void
verify_all_in_mempool(BRTNODE node)
{
    if (node->height==0) {
        for (int i = 0; i < node->n_children; i++) {
            invariant(BP_STATE(node,i) == PT_AVAIL);
            BASEMENTNODE bn = BLB(node, i);
            toku_omt_iterate(bn->buffer, verify_in_mempool, &bn->buffer_mempool);
        }
    }
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

static LEAFENTRY
fetch_from_buf(OMT omt, u_int32_t idx)
{
    OMTVALUE v = 0;
    int r = toku_omt_fetch(omt, idx, &v);
    assert_zero(r);
    return (LEAFENTRY)v;
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
    struct mempool *dest_mp = &dest_bn->buffer_mempool;
    struct mempool *src_mp  = &src_bn->buffer_mempool;
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
brtleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    BOOL create_new_node,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes)
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

void
brt_nonleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes)
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
    B->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;

    node->dirty = 1;
    B->dirty = 1;
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(B);
    //VERIFY_NODE(t,node);
    //VERIFY_NODE(t,B);
    *nodea = node;
    *nodeb = B;
}

static void
flush_some_child(
    struct brt_header* h,
    BRTNODE parent,
    int *n_dirtied,
    int cascades,
    bool started_at_root,
    BRT_STATUS brt_status);

//
// responsibility of brt_split_child is to take locked BRTNODEs node and child
// and do the following:
//  - split child,
//  - fix node,
//  - release lock on node
//  - possibly flush either new children created from split, otherwise unlock children
//
static void
brt_split_child(
    struct brt_header* h,
    BRTNODE node,
    int childnum,
    BRTNODE child,
    bool started_at_root,
    BRT_STATUS brt_status)
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
    if (nodea->height > 0 && toku_brt_nonleaf_is_gorged(nodea)) {
        toku_unpin_brtnode_off_client_thread(h, nodeb);
        flush_some_child(h, nodea, NULL, 0, started_at_root, brt_status);
    }
    else if (nodeb->height > 0 && toku_brt_nonleaf_is_gorged(nodeb)) {
        toku_unpin_brtnode_off_client_thread(h, nodea);
        flush_some_child(h, nodeb, NULL, 0, started_at_root, brt_status);
    }
    else {
        toku_unpin_brtnode_off_client_thread(h, nodea);
        toku_unpin_brtnode_off_client_thread(h, nodeb);
    }
}

static void
flush_this_child(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE child,
    int childnum,
    bool started_at_root,
    BRT_STATUS brt_status)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
{
    update_flush_status(node, child, 0, brt_status);
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
merge_leaf_nodes(BRTNODE a, BRTNODE b)
{
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
balance_leaf_nodes(
    BRTNODE a,
    BRTNODE b,
    struct kv_pair **splitk)
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
maybe_merge_pinned_leaf_nodes(
    BRTNODE a,
    BRTNODE b,
    struct kv_pair *parent_splitk,
    BOOL *did_merge,
    BOOL *did_rebalance,
    struct kv_pair **splitk)
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
        toku_free(parent_splitk); // We don't need the parent_splitk any more. If we need a splitk (if we don't merge) we'll malloc a new one.
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
maybe_merge_pinned_nonleaf_nodes(
    struct kv_pair *parent_splitk,
    BRTNODE a,
    BRTNODE b,
    BOOL *did_merge,
    BOOL *did_rebalance,
    struct kv_pair **splitk)
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
    *splitk = NULL;
}

static void
maybe_merge_pinned_nodes(
    BRTNODE parent,
    struct kv_pair *parent_splitk,
    BRTNODE a,
    BRTNODE b,
    BOOL *did_merge,
    BOOL *did_rebalance,
    struct kv_pair **splitk)
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
        // accurate for leaf nodes because all msgs above have been
        // applied, accurate for non-leaf nodes because buffer immediately
        // above each node has been flushed
        a->max_msn_applied_to_node_on_disk = msn_max;
        b->max_msn_applied_to_node_on_disk = msn_max;
    }
}

//
// Takes as input a locked node and a childnum_to_merge
// As output, two of node's children are merged or rebalanced, and node is unlocked
//
static void
brt_merge_child(
    struct brt_header* h,
    BRTNODE node,
    int childnum_to_merge,
    BOOL *did_react,
    bool started_at_root,
    BRT_STATUS brt_status)
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
        flush_this_child(h, node, childa, childnuma, started_at_root, brt_status);
    }
    if (toku_bnc_n_entries(BNC(node,childnumb))>0) {
        flush_this_child(h, node, childb, childnumb, started_at_root, brt_status);
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
    if (childa->height > 0 && toku_brt_nonleaf_is_gorged(childa)) {
        flush_some_child(h, childa, NULL, 0, started_at_root, brt_status);
    }
    else {
        toku_unpin_brtnode_off_client_thread(h, childa);
    }
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
static void
flush_some_child(
    struct brt_header* h,
    BRTNODE parent,
    int *n_dirtied,
    int cascades,
    bool started_at_root,
    BRT_STATUS brt_status)
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
    update_flush_status(parent, child, cascades, brt_status);

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
        if (child->height > 0 && toku_brt_nonleaf_is_gorged(child)) {
            flush_some_child(h, child, n_dirtied, cascades+1, started_at_root, brt_status);
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
        brt_split_child(h, parent, childnum, child, started_at_root, brt_status);
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
        brt_merge_child(h, parent, childnum, &did_react, started_at_root, brt_status);
    }
    else {
        assert(FALSE);
    }
}

// TODO 3988 Leif set cleaner_nodes_dirtied
static void
update_cleaner_status(
    BRTNODE node,
    int childnum,
    BRT_STATUS brt_status)
{
    brt_status->cleaner_total_nodes++;
    if (node->height == 1) {
        brt_status->cleaner_h1_nodes++;
    } else {
        brt_status->cleaner_hgt1_nodes++;
    }

    unsigned int nbytesinbuf = toku_bnc_nbytesinbuf(BNC(node, childnum));
    if (nbytesinbuf == 0) {
        brt_status->cleaner_empty_nodes++;
    } else {
        if (nbytesinbuf > brt_status->cleaner_max_buffer_size) {
            brt_status->cleaner_max_buffer_size = nbytesinbuf;
        }
        if (nbytesinbuf < brt_status->cleaner_min_buffer_size) {
            brt_status->cleaner_min_buffer_size = nbytesinbuf;
        }
        brt_status->cleaner_total_buffer_size += nbytesinbuf;

        uint64_t workdone = BP_WORKDONE(node, childnum);
        if (workdone > brt_status->cleaner_max_buffer_workdone) {
            brt_status->cleaner_max_buffer_workdone = workdone;
        }
        if (workdone < brt_status->cleaner_min_buffer_workdone) {
            brt_status->cleaner_min_buffer_workdone = workdone;
        }
        brt_status->cleaner_total_buffer_workdone += workdone;
    }
}

int
toku_brtnode_cleaner_callback_internal(
    void *brtnode_pv,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void *extraargs,
    BRT_STATUS brt_status)
{
    BRTNODE node = brtnode_pv;
    invariant(node->thisnodename.b == blocknum.b);
    invariant(node->fullhash == fullhash);
    invariant(node->height > 0);   // we should never pick a leaf node (for now at least)
    struct brt_header *h = extraargs;
    bring_node_fully_into_memory(node, h);
    int childnum;
    find_heaviest_child(node, &childnum);
    update_cleaner_status(node, childnum, brt_status);

    // Either flush_some_child will unlock the node, or we do it here.
    if (toku_bnc_nbytesinbuf(BNC(node, childnum)) > 0) {
        int n_dirtied = 0;
        flush_some_child(h, node, &n_dirtied, 0, false, brt_status);
        brt_status->cleaner_nodes_dirtied += n_dirtied;
    } else {
        toku_unpin_brtnode_off_client_thread(h, node);
    }
    return 0;
}

struct flusher_extra {
    struct brt_header* h;
    BRTNODE node;
    NONLEAF_CHILDINFO bnc;
    BRT_STATUS brt_status;
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
        if (fe->node->height > 0 && toku_brt_nonleaf_is_gorged(fe->node)) {
            flush_some_child(fe->h, fe->node, NULL, 0, true, fe->brt_status);
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
        flush_some_child(fe->h, fe->node, NULL, 0, true, fe->brt_status);
    }
    remove_background_job(fe->h->cf, false);
    toku_free(fe);
}

static void
place_node_and_bnc_on_background_thread(
    BRT brt,
    BRTNODE node,
    NONLEAF_CHILDINFO bnc,
    BRT_STATUS brt_status
    )
{
    struct flusher_extra* fe = NULL;
    fe = toku_xmalloc(sizeof(struct flusher_extra));
    assert(fe);
    fe->h = brt->h;
    fe->node = node;
    fe->bnc = bnc;
    fe->brt_status = brt_status;
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
void
flush_node_on_background_thread(BRT brt, BRTNODE parent, BRT_STATUS brt_status)
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
        place_node_and_bnc_on_background_thread(brt, parent, NULL, brt_status);
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
            place_node_and_bnc_on_background_thread(brt, child, bnc, brt_status);
            toku_unpin_brtnode(brt, parent);
        }
        else {
            // because the child may be reactive, we need to
            // put parent on background thread.
            // As a result, we unlock the child here.
            toku_unpin_brtnode(brt, child);
            // Again, we'll have the parent on the background thread, so
            // we don't need to destroy the basement nodes yet.
            place_node_and_bnc_on_background_thread(brt, parent, NULL, brt_status);
        }
    }
}
