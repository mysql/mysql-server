/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <brt-internal.h>
#include <brt-flusher.h>
#include <brt-flusher-internal.h>
#include <brt-cachetable-wrappers.h>

/* Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */
static volatile BRT_FLUSHER_STATUS_S brt_flusher_status;

#define STATUS_INIT(k,                                                           t,                                      l) {                                            \
        brt_flusher_status.status[k].keyname = #k;                      \
        brt_flusher_status.status[k].type    = t;                       \
        brt_flusher_status.status[k].legend  = "brt flusher: " l;       \
    }

#define STATUS_VALUE(x) brt_flusher_status.status[x].value.num
void toku_brt_flusher_status_init(void) {
    // Note,                                                                     this function initializes the keyname,  type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(BRT_FLUSHER_CLEANER_TOTAL_NODES,                UINT64, "total nodes potentially flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_H1_NODES,                   UINT64, "height-one nodes flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_HGT1_NODES,                 UINT64, "height-greater-than-one nodes flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_EMPTY_NODES,                UINT64, "nodes cleaned which had empty buffers");
    STATUS_INIT(BRT_FLUSHER_CLEANER_NODES_DIRTIED,              UINT64, "nodes dirtied by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_MAX_BUFFER_SIZE,            UINT64, "max bytes in a buffer flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_MIN_BUFFER_SIZE,            UINT64, "min bytes in a buffer flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_TOTAL_BUFFER_SIZE,          UINT64, "total bytes in buffers flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE,        UINT64, "max workdone in a buffer flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE,        UINT64, "min workdone in a buffer flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_TOTAL_BUFFER_WORKDONE,      UINT64, "total workdone in buffers flushed by cleaner thread");
    STATUS_INIT(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_STARTED,    UINT64, "times cleaner thread tries to merge a leaf");
    STATUS_INIT(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING,    UINT64, "cleaner thread leaf merges in progress");
    STATUS_INIT(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_COMPLETED,  UINT64, "cleaner thread leaf merges successful");
    STATUS_INIT(BRT_FLUSHER_CLEANER_NUM_DIRTIED_FOR_LEAF_MERGE, UINT64, "nodes dirtied by cleaner thread leaf merges");
    STATUS_INIT(BRT_FLUSHER_FLUSH_TOTAL,                        UINT64, "total number of flushes done by flusher threads or cleaner threads");
    STATUS_INIT(BRT_FLUSHER_FLUSH_IN_MEMORY,                    UINT64, "number of in memory flushes");
    STATUS_INIT(BRT_FLUSHER_FLUSH_NEEDED_IO,                    UINT64, "number of flushes that read something off disk");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES,                     UINT64, "number of flushes that triggered another flush in child");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_1,                   UINT64, "number of flushes that triggered 1 cascading flush");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_2,                   UINT64, "number of flushes that triggered 2 cascading flushes");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_3,                   UINT64, "number of flushes that triggered 3 cascading flushes");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_4,                   UINT64, "number of flushes that triggered 4 cascading flushes");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_5,                   UINT64, "number of flushes that triggered 5 cascading flushes");
    STATUS_INIT(BRT_FLUSHER_FLUSH_CASCADES_GT_5,                UINT64, "number of flushes that triggered over 5 cascading flushes");
    STATUS_INIT(BRT_FLUSHER_SPLIT_LEAF,                         UINT64, "leaf node splits");
    STATUS_INIT(BRT_FLUSHER_SPLIT_NONLEAF,                      UINT64, "nonleaf node splits");
    STATUS_INIT(BRT_FLUSHER_MERGE_LEAF,                         UINT64, "leaf node merges");
    STATUS_INIT(BRT_FLUSHER_MERGE_NONLEAF,                      UINT64, "nonleaf node merges");
    STATUS_INIT(BRT_FLUSHER_BALANCE_LEAF,                       UINT64, "leaf node balances");

    STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_SIZE) = UINT64_MAX;
    STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE) = UINT64_MAX;

    brt_flusher_status.initialized = true;
}
#undef STATUS_INIT

void toku_brt_flusher_get_status(BRT_FLUSHER_STATUS status) {
    if (!brt_flusher_status.initialized) {
        toku_brt_flusher_status_init();
    }
    *status = brt_flusher_status;
}

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

static int
find_heaviest_child(BRTNODE node)
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
    if (0) printf("\n");
    return max_child;
}

static void
update_flush_status(BRTNODE child, int cascades) {
    STATUS_VALUE(BRT_FLUSHER_FLUSH_TOTAL)++;
    if (cascades > 0) {
        STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES)++;
        switch (cascades) {
        case 1:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_1)++; break;
        case 2:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_2)++; break;
        case 3:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_3)++; break;
        case 4:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_4)++; break;
        case 5:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_5)++; break;
        default:
            STATUS_VALUE(BRT_FLUSHER_FLUSH_CASCADES_GT_5)++; break;
        }
    }
    bool flush_needs_io = false;
    for (int i = 0; !flush_needs_io && i < child->n_children; ++i) {
        if (BP_STATE(child, i) == PT_ON_DISK) {
            flush_needs_io = true;
        }
    }
    if (flush_needs_io) {
        STATUS_VALUE(BRT_FLUSHER_FLUSH_NEEDED_IO)++;
    } else {
        STATUS_VALUE(BRT_FLUSHER_FLUSH_IN_MEMORY)++;
    }
}

static void
maybe_destroy_child_blbs(BRTNODE node, BRTNODE child)
{
    // If the node is already fully in memory, as in upgrade, we don't
    // need to destroy the basement nodes because they are all equally
    // up to date.
    if (child->n_children > 1 && 
        child->height == 0 && 
        !child->dirty) {
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
brt_merge_child(
    struct brt_header* h,
    BRTNODE node,
    int childnum_to_merge,
    BOOL *did_react,
    struct flusher_advice *fa);

static int
pick_heaviest_child(struct brt_header *UU(h),
                    BRTNODE parent,
                    void* UU(extra))
{
    int childnum = find_heaviest_child(parent);
    assert(toku_bnc_n_entries(BNC(parent, childnum))>0);
    return childnum;
}

bool
dont_destroy_basement_nodes(void* UU(extra))
{
    return false;
}

static bool
do_destroy_basement_nodes(void* UU(extra))
{
    return true;
}

bool
always_recursively_flush(BRTNODE UU(child), void* UU(extra))
{
    return true;
}

static bool
recurse_if_child_is_gorged(BRTNODE child, void* UU(extra))
{
    return toku_brt_nonleaf_is_gorged(child);
}

int
default_pick_child_after_split(struct brt_header* UU(h),
                               BRTNODE UU(parent),
                               int UU(childnuma),
                               int UU(childnumb),
                               void* UU(extra))
{
    return -1;
}

void
default_merge_child(struct flusher_advice *fa,
                    struct brt_header *h,
                    BRTNODE parent,
                    int childnum,
                    BRTNODE child,
                    void* UU(extra))
{
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
    BOOL did_react;
    brt_merge_child(h, parent, childnum, &did_react, fa);
}

void
flusher_advice_init(
    struct flusher_advice *fa,
    FA_PICK_CHILD pick_child,
    FA_SHOULD_DESTROY_BN should_destroy_basement_nodes,
    FA_SHOULD_RECURSIVELY_FLUSH should_recursively_flush,
    FA_MAYBE_MERGE_CHILD maybe_merge_child,
    FA_UPDATE_STATUS update_status,
    FA_PICK_CHILD_AFTER_SPLIT pick_child_after_split,
    void* extra
    )
{
    fa->pick_child = pick_child;
    fa->should_destroy_basement_nodes = should_destroy_basement_nodes;
    fa->should_recursively_flush = should_recursively_flush;
    fa->maybe_merge_child = maybe_merge_child;
    fa->update_status = update_status;
    fa->pick_child_after_split = pick_child_after_split;
    fa->extra = extra;
}

/**
 * Flusher thread ("normal" flushing) implementation.
 */
struct flush_status_update_extra {
    int cascades;
};

static void
ft_update_status(BRTNODE child,
                 int UU(dirtied),
                 void* extra)
{
    struct flush_status_update_extra *fste = extra;
    update_flush_status(child, fste->cascades);
    // If `flush_some_child` decides to recurse after this, we'll need
    // cascades to increase.  If not it doesn't matter.
    fste->cascades++;
}

static void
ft_flusher_advice_init(struct flusher_advice *fa, struct flush_status_update_extra *fste)
{
    fste->cascades = 0;
    flusher_advice_init(fa,
                        pick_heaviest_child,
                        dont_destroy_basement_nodes,
                        recurse_if_child_is_gorged,
                        default_merge_child,
                        ft_update_status,
                        default_pick_child_after_split,
                        fste);
}

struct ctm_extra {
    BOOL is_last_child;
    DBT target_key;
};

static int
ctm_pick_child(struct brt_header *h,
               BRTNODE parent,
               void* extra)
{
    struct ctm_extra* ctme = extra;
    int childnum;
    if (parent->height == 1 && ctme->is_last_child) {
        childnum = parent->n_children - 1;
    }
    else {
        childnum = toku_brtnode_which_child(
            parent,
            &ctme->target_key,
            &h->cmp_descriptor,
            h->compare_fun);
    }
    return childnum;
}

static void
ctm_update_status(
    BRTNODE UU(child),
    int dirtied,
    void* UU(extra)
    )
{
    STATUS_VALUE(BRT_FLUSHER_CLEANER_NUM_DIRTIED_FOR_LEAF_MERGE) += dirtied;
}

static void
ctm_maybe_merge_child(struct flusher_advice *fa,
                      struct brt_header *h,
                      BRTNODE parent,
                      int childnum,
                      BRTNODE child,
                      void *extra)
{
    if (child->height == 0) {
        (void) __sync_fetch_and_add(&STATUS_VALUE(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_COMPLETED), 1);
    }
    default_merge_child(fa, h, parent, childnum, child, extra);
}

static void
ct_maybe_merge_child(struct flusher_advice *fa,
                     struct brt_header *h,
                     BRTNODE parent,
                     int childnum,
                     BRTNODE child,
                     void* extra)
{
    if (child->height > 0) {
        default_merge_child(fa, h, parent, childnum, child, extra);
    }
    else {
        struct ctm_extra ctme;
        assert(parent->n_children > 1);
        int pivot_to_save;
        //
        // we have two cases, one where the childnum
        // is the last child, and therefore the pivot we
        // save is not of the pivot which we wish to descend
        // and another where it is not the last child,
        // so the pivot is sufficient for identifying the leaf
        // to be merged
        //
        if (childnum == (parent->n_children - 1)) {
            ctme.is_last_child = TRUE;
            pivot_to_save = childnum - 1;
        }
        else {
            ctme.is_last_child = FALSE;
            pivot_to_save = childnum;
        }
        struct kv_pair *pivot = parent->childkeys[pivot_to_save];
        size_t pivotlen = kv_pair_keylen(pivot);
        char *buf = toku_xmemdup(kv_pair_key_const(pivot), pivotlen);
        toku_fill_dbt(&ctme.target_key, buf, pivotlen);

        // at this point, ctme is properly setup, now we can do the merge
        struct flusher_advice new_fa;
        flusher_advice_init(
            &new_fa,
            ctm_pick_child,
            dont_destroy_basement_nodes,
            always_recursively_flush,
            ctm_maybe_merge_child,
            ctm_update_status,
            default_pick_child_after_split,
            &ctme);

        toku_unpin_brtnode_off_client_thread(h, parent);
        toku_unpin_brtnode_off_client_thread(h, child);

        BRTNODE root_node = NULL;
        {
            toku_brtheader_grab_treelock(h);

            u_int32_t fullhash;
            CACHEKEY *rootp = toku_calculate_root_offset_pointer(h, &fullhash);
            struct brtnode_fetch_extra bfe;
            fill_bfe_for_full_read(&bfe, h);
            toku_pin_brtnode_off_client_thread(h, *rootp, fullhash, &bfe, TRUE, 0, NULL, &root_node);
            toku_assert_entire_node_in_memory(root_node);

            toku_brtheader_release_treelock(h);
        }

        (void) __sync_fetch_and_add(&STATUS_VALUE(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_STARTED), 1);
        (void) __sync_fetch_and_add(&STATUS_VALUE(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING), 1);

        flush_some_child(h, root_node, &new_fa);

        (void) __sync_fetch_and_add(&STATUS_VALUE(BRT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING), -1);

        toku_free(buf);
    }
}

static void
ct_update_status(BRTNODE child,
                 int dirtied,
                 void* extra)
{
    struct flush_status_update_extra* fste = extra;
    update_flush_status(child, fste->cascades);
    STATUS_VALUE(BRT_FLUSHER_CLEANER_NODES_DIRTIED) += dirtied;
    // Incrementing this in case `flush_some_child` decides to recurse.
    fste->cascades++;
}

static void
ct_flusher_advice_init(struct flusher_advice *fa, struct flush_status_update_extra* fste)
{
    fste->cascades = 0;
    flusher_advice_init(fa,
                        pick_heaviest_child,
                        do_destroy_basement_nodes,
                        recurse_if_child_is_gorged,
                        ct_maybe_merge_child,
                        ct_update_status,
                        default_pick_child_after_split,
                        fste);
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

    toku_mark_node_dirty(node);

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
// Effect: Split a leaf node.
// Argument "node" is node to be split.
// Upon return:
//   nodea and nodeb point to new nodes that result from split of "node"
//   nodea is the left node that results from the split
//   splitk is the right-most key of nodea
{

    invariant(node->height == 0);
    STATUS_VALUE(BRT_FLUSHER_SPLIT_LEAF)++;
    if (node->n_children) {
	// First move all the accumulated stat64info deltas into the first basement.
	// After the split, either both nodes or neither node will be included in the next checkpoint.
	// The accumulated stats in the dictionary will be correct in either case.
	// By moving all the deltas into one (arbitrary) basement, we avoid the need to maintain
	// correct information for a basement that is divided between two leafnodes (i.e. when split is
	// not on a basement boundary).
	STAT64INFO_S delta_for_leafnode = toku_get_and_clear_basement_stats(node);
	BASEMENTNODE bn = BLB(node,0);
	bn->stat64_delta = delta_for_leafnode;
    }


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
                h->flags,
                h);
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
                BP_WORKDONE(B,i) = 0;
                set_BLB(B, i, toku_create_empty_bn());
            }
        }

        // now move all the data

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

    verify_all_in_mempool(node);
    verify_all_in_mempool(B);

    node->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;
    B->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;

    toku_mark_node_dirty(node);
    toku_mark_node_dirty(B);

    *nodea = node;
    *nodeb = B;

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
    STATUS_VALUE(BRT_FLUSHER_SPLIT_NONLEAF)++;
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

    toku_mark_node_dirty(node);
    toku_mark_node_dirty(B);
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(B);
    //VERIFY_NODE(t,node);
    //VERIFY_NODE(t,B);
    *nodea = node;
    *nodeb = B;
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
brt_split_child(
    struct brt_header* h,
    BRTNODE node,
    int childnum,
    BRTNODE child,
    struct flusher_advice *fa)
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
    int picked_child = fa->pick_child_after_split(h, node, childnum, childnum + 1, fa->extra);
    toku_unpin_brtnode_off_client_thread(h, node);
    if (picked_child == childnum ||
        (picked_child < 0 && nodea->height > 0 && fa->should_recursively_flush(nodea, fa->extra))) {
        toku_unpin_brtnode_off_client_thread(h, nodeb);
        flush_some_child(h, nodea, fa);
    }
    else if (picked_child == childnum + 1 ||
             (picked_child < 0 && nodeb->height > 0 && fa->should_recursively_flush(nodeb, fa->extra))) {
        toku_unpin_brtnode_off_client_thread(h, nodea);
        flush_some_child(h, nodeb, fa);
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
    struct flusher_advice *fa)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
{
    update_flush_status(child, 0);
    int r;
    toku_assert_entire_node_in_memory(node);
    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(node, child);
    }
    bring_node_fully_into_memory(child, h);
    toku_assert_entire_node_in_memory(child);
    assert(node->height>0);
    assert(child->thisnodename.b!=0);
    // VERIFY_NODE does not work off client thread as of now
    //VERIFY_NODE(t, child);
    toku_mark_node_dirty(node);
    toku_mark_node_dirty(child);

    BP_WORKDONE(node, childnum) = 0;  // this buffer is drained, no work has been done by its contents
    NONLEAF_CHILDINFO bnc = BNC(node, childnum);
    set_BNC(node, childnum, toku_create_empty_nl());

    // now we have a bnc to flush to the child
    r = toku_bnc_flush_to_child(h->compare_fun, h->update_fun, &h->cmp_descriptor, h->cf, bnc, child); assert_zero(r);
    destroy_nonleaf_childinfo(bnc);
}

static void
merge_leaf_nodes(BRTNODE a, BRTNODE b)
{
    STATUS_VALUE(BRT_FLUSHER_MERGE_LEAF)++;
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    assert(a->height == 0);
    assert(b->height == 0);
    assert(a->n_children > 0);
    assert(b->n_children > 0);

    // Mark nodes as dirty before moving basements from b to a.
    // This way, whatever deltas are accumulated in the basements are
    // applied to the in_memory_stats in the header if they have not already
    // been (if nodes are clean).
    toku_mark_node_dirty(a);
    toku_mark_node_dirty(b);

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
    STATUS_VALUE(BRT_FLUSHER_BALANCE_LEAF)++;
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

    toku_mark_node_dirty(a);
    toku_mark_node_dirty(b);

    *did_merge = TRUE;
    *did_rebalance = FALSE;
    *splitk = NULL;

    STATUS_VALUE(BRT_FLUSHER_MERGE_NONLEAF)++;
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
    toku_mark_node_dirty(parent);   // just to make sure
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

static void merge_remove_key_callback(
    BLOCKNUM *bp,
    BOOL for_checkpoint,
    void *extra)
{
    struct brt_header *h = extra;
    toku_free_blocknum(h->blocktable, bp, h, for_checkpoint);
}

//
// Takes as input a locked node and a childnum_to_merge
// As output, two of node's children are merged or rebalanced, and node is unlocked
//
static void
brt_merge_child(
    struct brt_header *h,
    BRTNODE node,
    int childnum_to_merge,
    BOOL *did_react,
    struct flusher_advice *fa)
{
    // this function should not be called
    // if the child is not mergable
    assert(node->n_children > 1);
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
        toku_pin_brtnode_off_client_thread(h, BP_BLOCKNUM(node, childnuma), childfullhash, &bfe, TRUE, 1, &node, &childa);
    }
    // for test
    call_flusher_thread_callback(ft_flush_before_pin_second_node_for_merge);
    {
        BRTNODE dep_nodes[2];
        dep_nodes[0] = node;
        dep_nodes[1] = childa;
        u_int32_t childfullhash = compute_child_fullhash(h->cf, node, childnumb);
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, h);
        toku_pin_brtnode_off_client_thread(h, BP_BLOCKNUM(node, childnumb), childfullhash, &bfe, TRUE, 2, dep_nodes, &childb);
    }

    if (toku_bnc_n_entries(BNC(node,childnuma))>0) {
        flush_this_child(h, node, childa, childnuma, fa);
    }
    if (toku_bnc_n_entries(BNC(node,childnumb))>0) {
        flush_this_child(h, node, childb, childnumb, fa);
    }

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
            toku_mark_node_dirty(childa);  // just to make sure
            toku_mark_node_dirty(childb);  // just to make sure
        } else {
            assert(splitk_kvpair);
            // If we didn't merge the nodes, then we need the correct pivot.
            node->childkeys[childnuma] = splitk_kvpair;
            node->totalchildkeylens += toku_brt_pivot_key_len(node->childkeys[childnuma]);
            toku_mark_node_dirty(node);
        }
    }
    //
    // now we possibly flush the children
    //
    if (did_merge) {
        BLOCKNUM bn = childb->thisnodename;
        // for test
        call_flusher_thread_callback(ft_flush_before_unpin_remove);

        // merge_remove_key_callback will free the blocknum
        int rrb = toku_cachetable_unpin_and_remove(
            h->cf,
            bn,
            merge_remove_key_callback,
            h
            );
        assert(rrb==0);

        // for test
        call_flusher_thread_callback(ft_flush_after_merge);

        // unlock the parent
        assert(node->dirty);
        toku_unpin_brtnode_off_client_thread(h, node);
    }
    else {
        // for test
        call_flusher_thread_callback(ft_flush_after_rebalance);

        // unlock the parent
        assert(node->dirty);
        toku_unpin_brtnode_off_client_thread(h, node);
        toku_unpin_brtnode_off_client_thread(h, childb);
    }
    if (childa->height > 0 && fa->should_recursively_flush(childa, fa->extra)) {
        flush_some_child(h, childa, fa);
    }
    else {
        toku_unpin_brtnode_off_client_thread(h, childa);
    }
}

void
flush_some_child(
    struct brt_header *h,
    BRTNODE parent,
    struct flusher_advice *fa)
// Effect: This function does the following:
//   - Pick a child of parent (the heaviest child),
//   - flush from parent to child,
//   - possibly split/merge child.
//   - if child is gorged, recursively proceed with child
//  Note that parent is already locked
//  Upon exit of this function, parent is unlocked and no new
//  new nodes (such as a child) remain locked
{
    int dirtied = 0;
    NONLEAF_CHILDINFO bnc = NULL;
    assert(parent->height>0);
    toku_assert_entire_node_in_memory(parent);

    // pick the child we want to flush to
    int childnum = fa->pick_child(h, parent, fa->extra);

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
    toku_pin_brtnode_off_client_thread(h, targetchild, childfullhash, &bfe, TRUE, 1, &parent, &child);

    // for test
    call_flusher_thread_callback(ft_flush_after_child_pin);

    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(parent, child);
    }

    //Note that at this point, we don't have the entire child in.
    // Let's do a quick check to see if the child may be reactive
    // If the child cannot be reactive, then we can safely unlock
    // the parent before finishing reading in the entire child node.
    bool may_child_be_reactive = may_node_be_reactive(child);

    assert(child->thisnodename.b!=0);
    //VERIFY_NODE(brt, child);

    // only do the following work if there is a flush to perform
    if (toku_bnc_n_entries(BNC(parent, childnum)) > 0) {
        if (!parent->dirty) {
            dirtied++;
            toku_mark_node_dirty(parent);
        }
        // detach buffer
        BP_WORKDONE(parent, childnum) = 0;  // this buffer is drained, no work has been done by its contents
        bnc = BNC(parent, childnum);
        set_BNC(parent, childnum, toku_create_empty_nl());
    }

    //
    // at this point, the buffer has been detached from the parent
    // and a new empty buffer has been placed in its stead
    // so, if we are absolutely sure that the child is not
    // reactive, we can unpin the parent
    //
    if (!may_child_be_reactive) {
        toku_unpin_brtnode_off_client_thread(h, parent);
        parent = NULL;
    }

    //
    // now, if necessary, read/decompress the rest of child into memory,
    // so that we can proceed and apply the flush
    //
    bring_node_fully_into_memory(child, h);

    // It is possible after reading in the entire child,
    // that we now know that the child is not reactive
    // if so, we can unpin parent right now
    // we wont be splitting/merging child
    // and we have already replaced the bnc
    // for the root with a fresh one
    enum reactivity child_re = get_node_reactivity(child);
    if (parent && child_re == RE_STABLE) {
        toku_unpin_brtnode_off_client_thread(h, parent);
        parent = NULL;
    }

    // from above, we know at this point that either the bnc
    // is detached from the parent (which may be unpinned),
    // and we have to apply the flush, or there was no data
    // in the buffer to flush, and as a result, flushing is not necessary
    // and bnc is NULL
    if (bnc != NULL) {
        if (!child->dirty) {
            dirtied++;
            toku_mark_node_dirty(child);
        }
        // do the actual flush
        r = toku_bnc_flush_to_child(
            h->compare_fun,
            h->update_fun,
            &h->cmp_descriptor,
            h->cf,
            bnc,
            child
            );
        assert_zero(r);
        destroy_nonleaf_childinfo(bnc);
    }

    fa->update_status(child, dirtied, fa->extra);
    // let's get the reactivity of the child again,
    // it is possible that the flush got rid of some values
    // and now the parent is no longer reactive
    child_re = get_node_reactivity(child);
    // if the parent has been unpinned above, then
    // this is our only option, even if the child is not stable
    // if the child is not stable, we'll handle it the next
    // time we need to flush to the child
    if (!parent ||
        child_re == RE_STABLE ||
        (child_re == RE_FUSIBLE && parent->n_children == 1)
        )
    {
        if (parent) {
            toku_unpin_brtnode_off_client_thread(h, parent);
            parent = NULL;
        }
        //
        // it is the responsibility of flush_some_child to unpin child
        //
        if (child->height > 0 && fa->should_recursively_flush(child, fa->extra)) {
            flush_some_child(h, child, fa);
        }
        else {
            toku_unpin_brtnode_off_client_thread(h, child);
        }
    }
    else if (child_re == RE_FISSIBLE) {
        //
        // it is responsibility of `brt_split_child` to unlock nodes of
        // parent and child as it sees fit
        //
        assert(parent); // just make sure we have not accidentally unpinned parent
        brt_split_child(h, parent, childnum, child, fa);
    }
    else if (child_re == RE_FUSIBLE) {
        //
        // it is responsibility of `maybe_merge_child to unlock nodes of
        // parent and child as it sees fit
        //
        assert(parent); // just make sure we have not accidentally unpinned parent
        fa->maybe_merge_child(fa, h, parent, childnum, child, fa->extra);
    }
    else {
        assert(FALSE);
    }
}

static void
update_cleaner_status(
    BRTNODE node,
    int childnum)
{
    STATUS_VALUE(BRT_FLUSHER_CLEANER_TOTAL_NODES)++;
    if (node->height == 1) {
        STATUS_VALUE(BRT_FLUSHER_CLEANER_H1_NODES)++;
    } else {
        STATUS_VALUE(BRT_FLUSHER_CLEANER_HGT1_NODES)++;
    }

    unsigned int nbytesinbuf = toku_bnc_nbytesinbuf(BNC(node, childnum));
    if (nbytesinbuf == 0) {
        STATUS_VALUE(BRT_FLUSHER_CLEANER_EMPTY_NODES)++;
    } else {
        if (nbytesinbuf > STATUS_VALUE(BRT_FLUSHER_CLEANER_MAX_BUFFER_SIZE)) {
            STATUS_VALUE(BRT_FLUSHER_CLEANER_MAX_BUFFER_SIZE) = nbytesinbuf;
        }
        if (nbytesinbuf < STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_SIZE)) {
            STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_SIZE) = nbytesinbuf;
        }
        STATUS_VALUE(BRT_FLUSHER_CLEANER_TOTAL_BUFFER_SIZE) += nbytesinbuf;

        uint64_t workdone = BP_WORKDONE(node, childnum);
        if (workdone > STATUS_VALUE(BRT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE)) {
            STATUS_VALUE(BRT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE) = workdone;
        }
        if (workdone < STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE)) {
            STATUS_VALUE(BRT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE) = workdone;
        }
        STATUS_VALUE(BRT_FLUSHER_CLEANER_TOTAL_BUFFER_WORKDONE) += workdone;
    }
}

int
toku_brtnode_cleaner_callback(
    void *brtnode_pv,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void *extraargs)
{
    BRTNODE node = brtnode_pv;
    invariant(node->thisnodename.b == blocknum.b);
    invariant(node->fullhash == fullhash);
    invariant(node->height > 0);   // we should never pick a leaf node (for now at least)
    struct brt_header *h = extraargs;
    bring_node_fully_into_memory(node, h);
    int childnum = find_heaviest_child(node);
    update_cleaner_status(node, childnum);

    // Either flush_some_child will unlock the node, or we do it here.
    if (toku_bnc_nbytesinbuf(BNC(node, childnum)) > 0) {
        struct flusher_advice fa;
        struct flush_status_update_extra fste;
        ct_flusher_advice_init(&fa, &fste);
        flush_some_child(h, node, &fa);
    } else {
        toku_unpin_brtnode_off_client_thread(h, node);
    }
    return 0;
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
    toku_mark_node_dirty(fe->node);

    struct flusher_advice fa;
    struct flush_status_update_extra fste;
    ft_flusher_advice_init(&fa, &fste);

    if (fe->bnc) {
        // In this case, we have a bnc to flush to a node

        // for test purposes
        call_flusher_thread_callback(ft_flush_before_applying_inbox);

        r = toku_bnc_flush_to_child(
            fe->h->compare_fun,
            fe->h->update_fun,
            &fe->h->cmp_descriptor,
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
            flush_some_child(fe->h, fe->node, &fa);
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
        flush_some_child(fe->h, fe->node, &fa);
    }
    remove_background_job(fe->h->cf, false);
    toku_free(fe);
}

static void
place_node_and_bnc_on_background_thread(
    BRT brt,
    BRTNODE node,
    NONLEAF_CHILDINFO bnc)
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
void
flush_node_on_background_thread(BRT brt, BRTNODE parent)
{
    //
    // first let's see if we can detach buffer on client thread
    // and pick the child we want to flush to
    //
    int childnum = find_heaviest_child(parent);
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
            toku_mark_node_dirty(parent);
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

#include <valgrind/helgrind.h>
void __attribute__((__constructor__)) toku_brt_flusher_helgrind_ignore(void);
void
toku_brt_flusher_helgrind_ignore(void) {
    VALGRIND_HG_DISABLE_CHECKING(&brt_flusher_status, sizeof brt_flusher_status);
}

#undef STATUS_VALUE
