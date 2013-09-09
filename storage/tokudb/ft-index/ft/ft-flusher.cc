/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <ft-internal.h>
#include <ft-flusher.h>
#include <ft-flusher-internal.h>
#include <ft-cachetable-wrappers.h>
#include <ft.h>
#include <toku_assert.h>
#include <portability/toku_atomic.h>
#include <util/status.h>

/* Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */
static FT_FLUSHER_STATUS_S ft_flusher_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(ft_flusher_status, k, c, t, "brt flusher: " l, inc)

#define STATUS_VALUE(x) ft_flusher_status.status[x].value.num
void toku_ft_flusher_status_init(void) {
    // Note,                                                                     this function initializes the keyname,  type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(FT_FLUSHER_CLEANER_TOTAL_NODES,                nullptr, UINT64, "total nodes potentially flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_H1_NODES,                   nullptr, UINT64, "height-one nodes flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_HGT1_NODES,                 nullptr, UINT64, "height-greater-than-one nodes flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_EMPTY_NODES,                nullptr, UINT64, "nodes cleaned which had empty buffers", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_NODES_DIRTIED,              nullptr, UINT64, "nodes dirtied by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_MAX_BUFFER_SIZE,            nullptr, UINT64, "max bytes in a buffer flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_MIN_BUFFER_SIZE,            nullptr, UINT64, "min bytes in a buffer flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_TOTAL_BUFFER_SIZE,          nullptr, UINT64, "total bytes in buffers flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE,        nullptr, UINT64, "max workdone in a buffer flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE,        nullptr, UINT64, "min workdone in a buffer flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_TOTAL_BUFFER_WORKDONE,      nullptr, UINT64, "total workdone in buffers flushed by cleaner thread", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_STARTED,    nullptr, UINT64, "times cleaner thread tries to merge a leaf", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING,    nullptr, UINT64, "cleaner thread leaf merges in progress", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_COMPLETED,  nullptr, UINT64, "cleaner thread leaf merges successful", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_CLEANER_NUM_DIRTIED_FOR_LEAF_MERGE, nullptr, UINT64, "nodes dirtied by cleaner thread leaf merges", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_TOTAL,                        nullptr, UINT64, "total number of flushes done by flusher threads or cleaner threads", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_IN_MEMORY,                    nullptr, UINT64, "number of in memory flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_NEEDED_IO,                    nullptr, UINT64, "number of flushes that read something off disk", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES,                     nullptr, UINT64, "number of flushes that triggered another flush in child", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_1,                   nullptr, UINT64, "number of flushes that triggered 1 cascading flush", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_2,                   nullptr, UINT64, "number of flushes that triggered 2 cascading flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_3,                   nullptr, UINT64, "number of flushes that triggered 3 cascading flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_4,                   nullptr, UINT64, "number of flushes that triggered 4 cascading flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_5,                   nullptr, UINT64, "number of flushes that triggered 5 cascading flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_FLUSH_CASCADES_GT_5,                nullptr, UINT64, "number of flushes that triggered over 5 cascading flushes", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_SPLIT_LEAF,                         nullptr, UINT64, "leaf node splits", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_SPLIT_NONLEAF,                      nullptr, UINT64, "nonleaf node splits", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_MERGE_LEAF,                         nullptr, UINT64, "leaf node merges", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_MERGE_NONLEAF,                      nullptr, UINT64, "nonleaf node merges", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_FLUSHER_BALANCE_LEAF,                       nullptr, UINT64, "leaf node balances", TOKU_ENGINE_STATUS);

    STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_SIZE) = UINT64_MAX;
    STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE) = UINT64_MAX;

    ft_flusher_status.initialized = true;
}
#undef STATUS_INIT

void toku_ft_flusher_get_status(FT_FLUSHER_STATUS status) {
    if (!ft_flusher_status.initialized) {
        toku_ft_flusher_status_init();
    }
    *status = ft_flusher_status;
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

static void call_flusher_thread_callback(int flt_state) {
    if (flusher_thread_callback) {
        flusher_thread_callback(flt_state, flusher_thread_callback_extra);
    }
}

static int
find_heaviest_child(FTNODE node)
{
    int max_child = 0;
    int max_weight = toku_bnc_nbytesinbuf(BNC(node, 0)) + BP_WORKDONE(node, 0);
    int i;

    if (0) printf("%s:%d weights: %d", __FILE__, __LINE__, max_weight);
    paranoid_invariant(node->n_children>0);
    for (i=1; i<node->n_children; i++) {
#ifdef TOKU_DEBUG_PARANOID
        if (BP_WORKDONE(node,i)) {
            assert(toku_bnc_nbytesinbuf(BNC(node,i)) > 0);
        }
#endif
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
update_flush_status(FTNODE child, int cascades) {
    STATUS_VALUE(FT_FLUSHER_FLUSH_TOTAL)++;
    if (cascades > 0) {
        STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES)++;
        switch (cascades) {
        case 1:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_1)++; break;
        case 2:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_2)++; break;
        case 3:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_3)++; break;
        case 4:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_4)++; break;
        case 5:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_5)++; break;
        default:
            STATUS_VALUE(FT_FLUSHER_FLUSH_CASCADES_GT_5)++; break;
        }
    }
    bool flush_needs_io = false;
    for (int i = 0; !flush_needs_io && i < child->n_children; ++i) {
        if (BP_STATE(child, i) == PT_ON_DISK) {
            flush_needs_io = true;
        }
    }
    if (flush_needs_io) {
        STATUS_VALUE(FT_FLUSHER_FLUSH_NEEDED_IO)++;
    } else {
        STATUS_VALUE(FT_FLUSHER_FLUSH_IN_MEMORY)++;
    }
}

static void
maybe_destroy_child_blbs(FTNODE node, FTNODE child, FT h)
{
    // If the node is already fully in memory, as in upgrade, we don't
    // need to destroy the basement nodes because they are all equally
    // up to date.
    if (child->n_children > 1 && 
        child->height == 0 && 
        !child->dirty) {
        for (int i = 0; i < child->n_children; ++i) {
            if (BP_STATE(child, i) == PT_AVAIL &&
                node->max_msn_applied_to_node_on_disk.msn < BLB_MAX_MSN_APPLIED(child, i).msn) 
            {
                toku_evict_bn_from_memory(child, i, h);
            }
        }
    }
}

static void
ft_merge_child(
    FT h,
    FTNODE node,
    int childnum_to_merge,
    bool *did_react,
    struct flusher_advice *fa);

static int
pick_heaviest_child(FT UU(h),
                    FTNODE parent,
                    void* UU(extra))
{
    int childnum = find_heaviest_child(parent);
    paranoid_invariant(toku_bnc_n_entries(BNC(parent, childnum))>0);
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
always_recursively_flush(FTNODE UU(child), void* UU(extra))
{
    return true;
}

bool
never_recursively_flush(FTNODE UU(child), void* UU(extra))
{
    return false;
}

/**
 * Flusher thread ("normal" flushing) implementation.
 */
struct flush_status_update_extra {
    int cascades;
    uint32_t nodesize;
};

static bool
recurse_if_child_is_gorged(FTNODE child, void* extra)
{
    struct flush_status_update_extra *fste = (flush_status_update_extra *)extra;
    return toku_ft_nonleaf_is_gorged(child, fste->nodesize);
}

int
default_pick_child_after_split(FT UU(h),
                               FTNODE UU(parent),
                               int UU(childnuma),
                               int UU(childnumb),
                               void* UU(extra))
{
    return -1;
}

void
default_merge_child(struct flusher_advice *fa,
                    FT h,
                    FTNODE parent,
                    int childnum,
                    FTNODE child,
                    void* UU(extra))
{
    //
    // There is probably a way to pass FTNODE child
    // into ft_merge_child, but for simplicity for now,
    // we are just going to unpin child and
    // let ft_merge_child pin it again
    //
    toku_unpin_ftnode_off_client_thread(h, child);
    //
    //
    // it is responsibility of ft_merge_child to unlock parent
    //
    bool did_react;
    ft_merge_child(h, parent, childnum, &did_react, fa);
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

static void
flt_update_status(FTNODE child,
                 int UU(dirtied),
                 void* extra)
{
    struct flush_status_update_extra *fste = (struct flush_status_update_extra *) extra;
    update_flush_status(child, fste->cascades);
    // If `toku_ft_flush_some_child` decides to recurse after this, we'll need
    // cascades to increase.  If not it doesn't matter.
    fste->cascades++;
}

static void
flt_flusher_advice_init(struct flusher_advice *fa, struct flush_status_update_extra *fste, uint32_t nodesize)
{
    fste->cascades = 0;
    fste->nodesize = nodesize;
    flusher_advice_init(fa,
                        pick_heaviest_child,
                        dont_destroy_basement_nodes,
                        recurse_if_child_is_gorged,
                        default_merge_child,
                        flt_update_status,
                        default_pick_child_after_split,
                        fste);
}

struct ctm_extra {
    bool is_last_child;
    DBT target_key;
};

static int
ctm_pick_child(FT h,
               FTNODE parent,
               void* extra)
{
    struct ctm_extra* ctme = (struct ctm_extra *) extra;
    int childnum;
    if (parent->height == 1 && ctme->is_last_child) {
        childnum = parent->n_children - 1;
    }
    else {
        childnum = toku_ftnode_which_child(
            parent,
            &ctme->target_key,
            &h->cmp_descriptor,
            h->compare_fun);
    }
    return childnum;
}

static void
ctm_update_status(
    FTNODE UU(child),
    int dirtied,
    void* UU(extra)
    )
{
    STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_DIRTIED_FOR_LEAF_MERGE) += dirtied;
}

static void
ctm_maybe_merge_child(struct flusher_advice *fa,
                      FT h,
                      FTNODE parent,
                      int childnum,
                      FTNODE child,
                      void *extra)
{
    if (child->height == 0) {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_COMPLETED), 1);
    }
    default_merge_child(fa, h, parent, childnum, child, extra);
}

static void
ct_maybe_merge_child(struct flusher_advice *fa,
                     FT h,
                     FTNODE parent,
                     int childnum,
                     FTNODE child,
                     void* extra)
{
    if (child->height > 0) {
        default_merge_child(fa, h, parent, childnum, child, extra);
    }
    else {
        struct ctm_extra ctme;
        paranoid_invariant(parent->n_children > 1);
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
            ctme.is_last_child = true;
            pivot_to_save = childnum - 1;
        }
        else {
            ctme.is_last_child = false;
            pivot_to_save = childnum;
        }
        const DBT *pivot = &parent->childkeys[pivot_to_save];
        toku_clone_dbt(&ctme.target_key, *pivot);

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

        toku_unpin_ftnode_off_client_thread(h, parent);
        toku_unpin_ftnode_off_client_thread(h, child);

        FTNODE root_node = NULL;
        {
            uint32_t fullhash;
            CACHEKEY root;
            toku_calculate_root_offset_pointer(h, &root, &fullhash);
            struct ftnode_fetch_extra bfe;
            fill_bfe_for_full_read(&bfe, h);
            toku_pin_ftnode_off_client_thread(h, root, fullhash, &bfe, PL_WRITE_EXPENSIVE, 0, NULL, &root_node);
            toku_assert_entire_node_in_memory(root_node);
        }

        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_STARTED), 1);
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING), 1);

        toku_ft_flush_some_child(h, root_node, &new_fa);

        (void) toku_sync_fetch_and_sub(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING), 1);

        toku_destroy_dbt(&ctme.target_key);
    }
}

static void
ct_update_status(FTNODE child,
                 int dirtied,
                 void* extra)
{
    struct flush_status_update_extra* fste = (struct flush_status_update_extra *) extra;
    update_flush_status(child, fste->cascades);
    STATUS_VALUE(FT_FLUSHER_CLEANER_NODES_DIRTIED) += dirtied;
    // Incrementing this in case `toku_ft_flush_some_child` decides to recurse.
    fste->cascades++;
}

static void
ct_flusher_advice_init(struct flusher_advice *fa, struct flush_status_update_extra* fste, uint32_t nodesize)
{
    fste->cascades = 0;
    fste->nodesize = nodesize;
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
static bool may_node_be_reactive(FTNODE node)
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
    FTNODE node,
    int childnum,
    FTNODE childa,
    FTNODE childb,
    DBT *splitk /* the data in the childsplitk is alloc'd and is consumed by this call. */
    )
{
    paranoid_invariant(node->height>0);
    paranoid_invariant(0 <= childnum);
    paranoid_invariant(childnum < node->n_children);
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(childa);
    toku_assert_entire_node_in_memory(childb);
    NONLEAF_CHILDINFO old_bnc = BNC(node, childnum);
    paranoid_invariant(toku_bnc_nbytesinbuf(old_bnc)==0);
    int cnum;
    WHEN_NOT_GCOV(
    if (toku_ft_debug_mode) {
        int i;
        printf("%s:%d Child %d splitting on %s\n", __FILE__, __LINE__, childnum, (char*)splitk->data);
        printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
        for(i=0; i<node->n_children-1; i++) printf(" %s", (char *) node->childkeys[i].data);
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

    paranoid_invariant(BP_BLOCKNUM(node, childnum).b==childa->thisnodename.b); // use the same child

    BP_BLOCKNUM(node, childnum+1) = childb->thisnodename;
    BP_WORKDONE(node, childnum+1)  = 0;
    BP_STATE(node,childnum+1) = PT_AVAIL;

    NONLEAF_CHILDINFO new_bnc = toku_create_empty_nl();
    for (unsigned int i = 0; i < (sizeof new_bnc->flow) / (sizeof new_bnc->flow[0]); ++i) {
        // just split the flows in half for now, can't guess much better
        // at the moment
        new_bnc->flow[i] = old_bnc->flow[i] / 2;
        old_bnc->flow[i] = (old_bnc->flow[i] + 1) / 2;
    }
    set_BNC(node, childnum+1, new_bnc);

    // Slide the keys over
    {
        for (cnum=node->n_children-2; cnum>childnum; cnum--) {
            toku_copy_dbt(&node->childkeys[cnum], node->childkeys[cnum-1]);
        }
        //if (logger) assert((t->flags&TOKU_DB_DUPSORT)==0); // the setpivot is wrong for TOKU_DB_DUPSORT, so recovery will be broken.
        toku_copy_dbt(&node->childkeys[childnum], *splitk);
        node->totalchildkeylens += splitk->size;
    }

    WHEN_NOT_GCOV(
    if (toku_ft_debug_mode) {
        int i;
        printf("%s:%d splitkeys:", __FILE__, __LINE__);
        for(i=0; i<node->n_children-2; i++) printf(" %s", (char*)node->childkeys[i].data);
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
UU() verify_in_mempool(OMTVALUE lev, uint32_t UU(idx), void *mpv)
{
    LEAFENTRY CAST_FROM_VOIDP(le, lev);
    struct mempool *CAST_FROM_VOIDP(mp, mpv);
    int r = toku_mempool_inrange(mp, le, leafentry_memsize(le));
    lazy_assert(r);
    return 0;
}

static void
verify_all_in_mempool(FTNODE UU() node)
{
#ifdef TOKU_DEBUG_PARANOID
    if (node->height==0) {
        for (int i = 0; i < node->n_children; i++) {
            invariant(BP_STATE(node,i) == PT_AVAIL);
            BASEMENTNODE bn = BLB(node, i);
            toku_omt_iterate(bn->buffer, verify_in_mempool, &bn->buffer_mempool);
        }
    }
#endif
}

static uint64_t
ftleaf_disk_size(FTNODE node)
// Effect: get the disk size of a leafentry
{
    paranoid_invariant(node->height == 0);
    toku_assert_entire_node_in_memory(node);
    uint64_t retval = 0;
    for (int i = 0; i < node->n_children; i++) {
        OMT curr_buffer = BLB_BUFFER(node, i);
        const uint32_t n_leafentries = toku_omt_size(curr_buffer);
        for (uint32_t j=0; j < n_leafentries; j++) {
            OMTVALUE v;
            int r = toku_omt_fetch(curr_buffer, j, &v);
            assert_zero(r);
            LEAFENTRY CAST_FROM_VOIDP(curr_le, v);
            retval += leafentry_disksize(curr_le);
        }
    }
    return retval;
}

static void
ftleaf_get_split_loc(
    FTNODE node,
    enum split_mode split_mode,
    int *num_left_bns,   // which basement within leaf
    int *num_left_les    // which key within basement
    )
// Effect: Find the location within a leaf node where we want to perform a split
// num_left_bns is how many basement nodes (which OMT) should be split to the left.
// num_left_les is how many leafentries in OMT of the last bn should be on the left side of the split.
{
    switch (split_mode) {
    case SPLIT_LEFT_HEAVY: {
        *num_left_bns = node->n_children;
        *num_left_les = toku_omt_size(BLB_BUFFER(node, *num_left_bns - 1));
        if (*num_left_les == 0) {
            *num_left_bns = node->n_children - 1;
            *num_left_les = toku_omt_size(BLB_BUFFER(node, *num_left_bns - 1));
        }
        goto exit;
    }
    case SPLIT_RIGHT_HEAVY: {
        *num_left_bns = 1;
        *num_left_les = toku_omt_size(BLB_BUFFER(node, 0)) ? 1 : 0;
        goto exit;
    }
    case SPLIT_EVENLY: {
        paranoid_invariant(node->height == 0);
        // TODO: (Zardosht) see if we can/should make this faster, we iterate over the rows twice
        uint64_t sumlesizes = ftleaf_disk_size(node);
        uint32_t size_so_far = 0;
        for (int i = 0; i < node->n_children; i++) {
            OMT curr_buffer = BLB_BUFFER(node, i);
            uint32_t n_leafentries = toku_omt_size(curr_buffer);
            for (uint32_t j=0; j < n_leafentries; j++) {
                OMTVALUE lev;
                int r = toku_omt_fetch(curr_buffer, j, &lev);
                assert_zero(r);
                LEAFENTRY CAST_FROM_VOIDP(curr_le, lev);
                size_so_far += leafentry_disksize(curr_le);
                if (size_so_far >= sumlesizes/2) {
                    *num_left_bns = i + 1;
                    *num_left_les = j + 1;
                    if (*num_left_bns == node->n_children &&
                        (unsigned int) *num_left_les == n_leafentries) {
                        // need to correct for when we're splitting after the
                        // last element, that makes no sense
                        if (*num_left_les > 1) {
                            (*num_left_les)--;
                        } else if (*num_left_bns > 1) {
                            (*num_left_bns)--;
                            *num_left_les = toku_omt_size(BLB_BUFFER(node, *num_left_bns - 1));
                        } else {
                            // we are trying to split a leaf with only one
                            // leafentry in it
                            abort();
                        }
                    }
                    goto exit;
                }
            }
        }
    }
    }
    abort();
exit:
    return;
}

// TODO: (Zardosht) possibly get rid of this function and use toku_omt_split_at in
// ftleaf_split
static void
move_leafentries(
    BASEMENTNODE dest_bn,
    BASEMENTNODE src_bn,
    uint32_t lbi, //lower bound inclusive
    uint32_t ube, //upper bound exclusive
    uint32_t* num_bytes_moved
    )
//Effect: move leafentries in the range [lbi, upe) from src_omt to newly created dest_omt
{
    paranoid_invariant(lbi < ube);
    OMTVALUE *XMALLOC_N(ube-lbi, newleafpointers);    // create new omt

    size_t mpsize = toku_mempool_get_used_space(&src_bn->buffer_mempool);   // overkill, but safe
    struct mempool *dest_mp = &dest_bn->buffer_mempool;
    struct mempool *src_mp  = &src_bn->buffer_mempool;
    toku_mempool_construct(dest_mp, mpsize);

    uint32_t i = 0;
    *num_bytes_moved = 0;
    for (i = lbi; i < ube; i++) {
        OMTVALUE lev;
        int r = toku_omt_fetch(src_bn->buffer, i, &lev);
        assert_zero(r);
        LEAFENTRY CAST_FROM_VOIDP(curr_le, lev);
        size_t le_size = leafentry_memsize(curr_le);
        *num_bytes_moved += leafentry_disksize(curr_le);
        LEAFENTRY CAST_FROM_VOIDP(new_le, toku_mempool_malloc(dest_mp, le_size, 1));
        memcpy(new_le, curr_le, le_size);
        newleafpointers[i-lbi] = new_le;
        toku_mempool_mfree(src_mp, curr_le, le_size);
    }

    toku_omt_create_steal_sorted_array(&dest_bn->buffer, &newleafpointers, ube-lbi, ube-lbi);
    // now remove the elements from src_omt
    for (i=ube-1; i >= lbi; i--) {
        toku_omt_delete_at(src_bn->buffer, i);
    }
}

static void ftnode_finalize_split(FTNODE node, FTNODE B, MSN max_msn_applied_to_node) {
// Effect: Finalizes a split by updating some bits and dirtying both nodes
    toku_assert_entire_node_in_memory(node);
    toku_assert_entire_node_in_memory(B);
    verify_all_in_mempool(node);
    verify_all_in_mempool(B);

    node->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;
    B->max_msn_applied_to_node_on_disk = max_msn_applied_to_node;

    // The new node in the split inherits the oldest known reference xid
    B->oldest_referenced_xid_known = node->oldest_referenced_xid_known;

    node->dirty = 1;
    B->dirty = 1;
}

void
ftleaf_split(
    FT h,
    FTNODE node,
    FTNODE *nodea,
    FTNODE *nodeb,
    DBT *splitk,
    bool create_new_node,
    enum split_mode split_mode,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes)
// Effect: Split a leaf node.
// Argument "node" is node to be split.
// Upon return:
//   nodea and nodeb point to new nodes that result from split of "node"
//   nodea is the left node that results from the split
//   splitk is the right-most key of nodea
{

    paranoid_invariant(node->height == 0);
    STATUS_VALUE(FT_FLUSHER_SPLIT_LEAF)++;
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


    FTNODE B = nullptr;
    uint32_t fullhash;
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
        // GCC 4.8 seems to get confused and think B is maybe uninitialized at link time.
        // TODO(leif): figure out why it thinks this and actually fix it.
        invariant_notnull(B);
    }


    paranoid_invariant(node->height==0);
    toku_assert_entire_node_in_memory(node);
    verify_all_in_mempool(node);
    MSN max_msn_applied_to_node = node->max_msn_applied_to_node_on_disk;

    // variables that say where we will do the split.
    // After the split, there will be num_left_bns basement nodes in the left node,
    // and the last basement node in the left node will have num_left_les leafentries.
    int num_left_bns;
    int num_left_les;
    ftleaf_get_split_loc(node, split_mode, &num_left_bns, &num_left_les);
    {
        // did we split right on the boundary between basement nodes?
        const bool split_on_boundary = (num_left_les == 0) || (num_left_les == (int) toku_omt_size(BLB_BUFFER(node, num_left_bns - 1)));
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
        int num_children_in_node = num_left_bns;
        // In the SPLIT_RIGHT_HEAVY case, we need to add 1 back because
        // while it's not on the boundary, we do need node->n_children
        // children in B.
        int num_children_in_b = node->n_children - num_left_bns + (!split_on_boundary ? 1 : 0);
        if (num_children_in_b == 0) {
            // for uneven split, make sure we have at least 1 bn
            paranoid_invariant(split_mode == SPLIT_LEFT_HEAVY);
            num_children_in_b = 1;
        }
        paranoid_invariant(num_children_in_node > 0);
        if (create_new_node) {
            toku_initialize_empty_ftnode(
                B,
                name,
                0,
                num_children_in_b,
                h->h->layout_version,
                h->h->flags);
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

        int curr_src_bn_index = num_left_bns - 1;
        int curr_dest_bn_index = 0;

        // handle the move of a subset of data in last_bn_on_left from node to B
        if (!split_on_boundary) {
            BP_STATE(B,curr_dest_bn_index) = PT_AVAIL;
            uint32_t diff_size = 0;
            destroy_basement_node(BLB(B, curr_dest_bn_index)); // Destroy B's empty OMT, so I can rebuild it from an array
            set_BNULL(B, curr_dest_bn_index);
            set_BLB(B, curr_dest_bn_index, toku_create_empty_bn_no_buffer());
            move_leafentries(BLB(B, curr_dest_bn_index),
                             BLB(node, curr_src_bn_index),
                             num_left_les,         // first row to be moved to B
                             toku_omt_size(BLB_BUFFER(node, curr_src_bn_index)),    // number of rows in basement to be split
                             &diff_size);
            BLB_MAX_MSN_APPLIED(B, curr_dest_bn_index) = BLB_MAX_MSN_APPLIED(node, curr_src_bn_index);
            BLB_NBYTESINBUF(node, curr_src_bn_index) -= diff_size;
            BLB_NBYTESINBUF(B, curr_dest_bn_index) += diff_size;
            curr_dest_bn_index++;
        }
        curr_src_bn_index++;

        paranoid_invariant(B->n_children >= curr_dest_bn_index);
        paranoid_invariant(node->n_children >= curr_src_bn_index);

        // move the rest of the basement nodes
        for ( ; curr_src_bn_index < node->n_children; curr_src_bn_index++, curr_dest_bn_index++) {
            destroy_basement_node(BLB(B, curr_dest_bn_index));
            set_BNULL(B, curr_dest_bn_index);
            B->bp[curr_dest_bn_index] = node->bp[curr_src_bn_index];
        }
        if (curr_dest_bn_index < B->n_children) {
            // B already has an empty basement node here.
            BP_STATE(B, curr_dest_bn_index) = PT_AVAIL;
        }

        //
        // now handle the pivots
        //

        // the child index in the original node that corresponds to the
        // first node in the right node of the split
        int base_index = num_left_bns - (split_on_boundary ? 0 : 1);
        // make pivots in B
        for (int i=0; i < num_children_in_b-1; i++) {
            toku_copy_dbt(&B->childkeys[i], node->childkeys[i+base_index]);
            B->totalchildkeylens += node->childkeys[i+base_index].size;
            node->totalchildkeylens -= node->childkeys[i+base_index].size;
            toku_init_dbt(&node->childkeys[i+base_index]);
        }
        if (split_on_boundary && num_left_bns < node->n_children) {
            if (splitk) {
                toku_copy_dbt(splitk, node->childkeys[num_left_bns - 1]);
            } else {
                toku_destroy_dbt(&node->childkeys[num_left_bns - 1]);
            }
        } else if (splitk) {
            OMTVALUE lev;
            OMT buffer = BLB_BUFFER(node, num_left_bns - 1);
            int r = toku_omt_fetch(buffer, toku_omt_size(buffer) - 1, &lev);
            assert_zero(r); // that fetch should have worked.
            LEAFENTRY CAST_FROM_VOIDP(le, lev);
            uint32_t keylen;
            void *key = le_key_and_len(le, &keylen);
            toku_memdup_dbt(splitk, key, keylen);
        }

        node->n_children = num_children_in_node;
        REALLOC_N(num_children_in_node, node->bp);
        REALLOC_N(num_children_in_node-1, node->childkeys);
    }

    ftnode_finalize_split(node, B, max_msn_applied_to_node);
    *nodea = node;
    *nodeb = B;
}    // end of ftleaf_split()

void
ft_nonleaf_split(
    FT h,
    FTNODE node,
    FTNODE *nodea,
    FTNODE *nodeb,
    DBT *splitk,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes)
{
    //VERIFY_NODE(t,node);
    STATUS_VALUE(FT_FLUSHER_SPLIT_NONLEAF)++;
    toku_assert_entire_node_in_memory(node);
    int old_n_children = node->n_children;
    int n_children_in_a = old_n_children/2;
    int n_children_in_b = old_n_children-n_children_in_a;
    MSN max_msn_applied_to_node = node->max_msn_applied_to_node_on_disk;
    FTNODE B;
    paranoid_invariant(node->height>0);
    paranoid_invariant(node->n_children>=2); // Otherwise, how do we split?	 We need at least two children to split. */
    create_new_ftnode_with_dep_nodes(h, &B, node->height, n_children_in_b, num_dependent_nodes, dependent_nodes);
    {
        /* The first n_children_in_a go into node a.
         * That means that the first n_children_in_a-1 keys go into node a.
         * The splitter key is key number n_children_in_a */
        int i;

        for (i=n_children_in_a; i<old_n_children; i++) {
            int targchild = i-n_children_in_a;
            // TODO: Figure out better way to handle this
            // the problem is that create_new_ftnode_with_dep_nodes for B creates
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
                paranoid_invariant(i>0);
                if (i>n_children_in_a) {
                    toku_copy_dbt(&B->childkeys[targchild-1], node->childkeys[i-1]);
                    B->totalchildkeylens += node->childkeys[i-1].size;
                    node->totalchildkeylens -= node->childkeys[i-1].size;
                    toku_init_dbt(&node->childkeys[i-1]);
                }
            }
        }

        node->n_children=n_children_in_a;

        toku_copy_dbt(splitk, node->childkeys[n_children_in_a-1]);
        node->totalchildkeylens -= node->childkeys[n_children_in_a-1].size;

        REALLOC_N(n_children_in_a,   node->bp);
        REALLOC_N(n_children_in_a-1, node->childkeys);
    }

    ftnode_finalize_split(node, B, max_msn_applied_to_node);
    *nodea = node;
    *nodeb = B;
}

//
// responsibility of ft_split_child is to take locked FTNODEs node and child
// and do the following:
//  - split child,
//  - fix node,
//  - release lock on node
//  - possibly flush either new children created from split, otherwise unlock children
//
static void
ft_split_child(
    FT h,
    FTNODE node,
    int childnum,
    FTNODE child,
    enum split_mode split_mode,
    struct flusher_advice *fa)
{
    paranoid_invariant(node->height>0);
    paranoid_invariant(toku_bnc_nbytesinbuf(BNC(node, childnum))==0); // require that the buffer for this child is empty
    FTNODE nodea, nodeb;
    DBT splitk;

    // for test
    call_flusher_thread_callback(flt_flush_before_split);

    FTNODE dep_nodes[2];
    dep_nodes[0] = node;
    dep_nodes[1] = child;
    if (child->height==0) {
        ftleaf_split(h, child, &nodea, &nodeb, &splitk, true, split_mode, 2, dep_nodes);
    } else {
        ft_nonleaf_split(h, child, &nodea, &nodeb, &splitk, 2, dep_nodes);
    }
    // printf("%s:%d child did split\n", __FILE__, __LINE__);
    handle_split_of_child (node, childnum, nodea, nodeb, &splitk);

    // for test
    call_flusher_thread_callback(flt_flush_during_split);

    // at this point, the split is complete
    // now we need to unlock node,
    // and possibly continue
    // flushing one of the children
    int picked_child = fa->pick_child_after_split(h, node, childnum, childnum + 1, fa->extra);
    toku_unpin_ftnode_off_client_thread(h, node);
    if (picked_child == childnum ||
        (picked_child < 0 && nodea->height > 0 && fa->should_recursively_flush(nodea, fa->extra))) {
        toku_unpin_ftnode_off_client_thread(h, nodeb);
        toku_ft_flush_some_child(h, nodea, fa);
    }
    else if (picked_child == childnum + 1 ||
             (picked_child < 0 && nodeb->height > 0 && fa->should_recursively_flush(nodeb, fa->extra))) {
        toku_unpin_ftnode_off_client_thread(h, nodea);
        toku_ft_flush_some_child(h, nodeb, fa);
    }
    else {
        toku_unpin_ftnode_off_client_thread(h, nodea);
        toku_unpin_ftnode_off_client_thread(h, nodeb);
    }
}

static void bring_node_fully_into_memory(FTNODE node, FT ft) {
    if (!is_entire_node_in_memory(node)) {
        struct ftnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, ft);
        toku_cachetable_pf_pinned_pair(
            node,
            toku_ftnode_pf_callback,
            &bfe,
            ft->cf,
            node->thisnodename,
            toku_cachetable_hash(ft->cf, node->thisnodename)
            );
    }
}

static void
flush_this_child(
    FT h,
    FTNODE node,
    FTNODE child,
    int childnum,
    struct flusher_advice *fa)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
{
    update_flush_status(child, 0);
    toku_assert_entire_node_in_memory(node);
    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(node, child, h);
    }
    bring_node_fully_into_memory(child, h);
    toku_assert_entire_node_in_memory(child);
    paranoid_invariant(node->height>0);
    paranoid_invariant(child->thisnodename.b!=0);
    // VERIFY_NODE does not work off client thread as of now
    //VERIFY_NODE(t, child);
    node->dirty = 1;
    child->dirty = 1;

    BP_WORKDONE(node, childnum) = 0;  // this buffer is drained, no work has been done by its contents
    NONLEAF_CHILDINFO bnc = BNC(node, childnum);
    set_BNC(node, childnum, toku_create_empty_nl());

    // now we have a bnc to flush to the child. pass down the parent's
    // oldest known referenced xid as we flush down to the child.
    toku_bnc_flush_to_child(h, bnc, child, node->oldest_referenced_xid_known);
    destroy_nonleaf_childinfo(bnc);
}

static void
merge_leaf_nodes(FTNODE a, FTNODE b)
{
    STATUS_VALUE(FT_FLUSHER_MERGE_LEAF)++;
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    paranoid_invariant(a->height == 0);
    paranoid_invariant(b->height == 0);
    paranoid_invariant(a->n_children > 0);
    paranoid_invariant(b->n_children > 0);

    // Mark nodes as dirty before moving basements from b to a.
    // This way, whatever deltas are accumulated in the basements are
    // applied to the in_memory_stats in the header if they have not already
    // been (if nodes are clean).
    // TODO(leif): this is no longer the way in_memory_stats is
    // maintained. verify that it's ok to move this just before the unpin
    // and then do that.
    a->dirty = 1;
    b->dirty = 1;

    OMT a_last_buffer = BLB_BUFFER(a, a->n_children-1);
    // this bool states if the last basement node in a has any items or not
    // If it does, then it stays in the merge. If it does not, the last basement node
    // of a gets eliminated because we do not have a pivot to store for it (because it has no elements)
    const bool a_has_tail = toku_omt_size(a_last_buffer) > 0;

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
        OMTVALUE lev;
        int r = toku_omt_fetch(a_last_buffer, toku_omt_size(a_last_buffer) - 1, &lev);
        assert_zero(r);
        LEAFENTRY CAST_FROM_VOIDP(le, lev);
        uint32_t keylen;
        void *key = le_key_and_len(le, &keylen);
        toku_memdup_dbt(&a->childkeys[a->n_children-1], key, keylen);
        a->totalchildkeylens += keylen;
    }

    uint32_t offset = a_has_tail ? a->n_children : a->n_children - 1;
    for (int i = 0; i < b->n_children; i++) {
        a->bp[i+offset] = b->bp[i];
        memset(&b->bp[i],0,sizeof(b->bp[0]));
        if (i < (b->n_children-1)) {
            toku_copy_dbt(&a->childkeys[i+offset], b->childkeys[i]);
            toku_init_dbt(&b->childkeys[i]);
        }
    }
    a->totalchildkeylens += b->totalchildkeylens;
    a->n_children = num_children;

    // now that all the data has been moved from b to a, we can destroy the data in b
    // b can remain untouched, as it will be destroyed later
    b->totalchildkeylens = 0;
    b->n_children = 0;
}

static void balance_leaf_nodes(
    FTNODE a,
    FTNODE b,
    DBT *splitk)
// Effect:
//  If b is bigger then move stuff from b to a until b is the smaller.
//  If a is bigger then move stuff from a to b until a is the smaller.
{
    STATUS_VALUE(FT_FLUSHER_BALANCE_LEAF)++;
    // first merge all the data into a
    merge_leaf_nodes(a,b);
    // now split them
    // because we are not creating a new node, we can pass in no dependent nodes
    ftleaf_split(NULL, a, &a, &b, splitk, false, SPLIT_EVENLY, 0, NULL);
}

static void
maybe_merge_pinned_leaf_nodes(
    FTNODE a,
    FTNODE b,
    DBT *parent_splitk,
    bool *did_merge,
    bool *did_rebalance,
    DBT *splitk,
    uint32_t nodesize
    )
// Effect: Either merge a and b into one one node (merge them into a) and set *did_merge = true.
//	   (We do this if the resulting node is not fissible)
//	   or distribute the leafentries evenly between a and b, and set *did_rebalance = true.
//	   (If a and be are already evenly distributed, we may do nothing.)
{
    unsigned int sizea = toku_serialize_ftnode_size(a);
    unsigned int sizeb = toku_serialize_ftnode_size(b);
    uint32_t num_leafentries = get_leaf_num_entries(a) + get_leaf_num_entries(b);
    if (num_leafentries > 1 && (sizea + sizeb)*4 > (nodesize*3)) {
        // the combined size is more than 3/4 of a node, so don't merge them.
        *did_merge = false;
        if (sizea*4 > nodesize && sizeb*4 > nodesize) {
            // no need to do anything if both are more than 1/4 of a node.
            *did_rebalance = false;
            toku_clone_dbt(splitk, *parent_splitk);
            return;
        }
        // one is less than 1/4 of a node, and together they are more than 3/4 of a node.
        toku_destroy_dbt(parent_splitk); // We don't need the parent_splitk any more. If we need a splitk (if we don't merge) we'll malloc a new one.
        *did_rebalance = true;
        balance_leaf_nodes(a, b, splitk);
    } else {
        // we are merging them.
        *did_merge = true;
        *did_rebalance = false;
        toku_init_dbt(splitk);
        toku_destroy_dbt(parent_splitk); // if we are merging, the splitk gets freed.
        merge_leaf_nodes(a, b);
    }
}

static void
maybe_merge_pinned_nonleaf_nodes(
    const DBT *parent_splitk,
    FTNODE a,
    FTNODE b,
    bool *did_merge,
    bool *did_rebalance,
    DBT *splitk)
{
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    paranoid_invariant(parent_splitk->data);
    int old_n_children = a->n_children;
    int new_n_children = old_n_children + b->n_children;
    XREALLOC_N(new_n_children, a->bp);
    memcpy(a->bp + old_n_children,
           b->bp,
           b->n_children*sizeof(b->bp[0]));
    memset(b->bp,0,b->n_children*sizeof(b->bp[0]));

    XREALLOC_N(new_n_children-1, a->childkeys);
    toku_copy_dbt(&a->childkeys[old_n_children-1], *parent_splitk);
    a->totalchildkeylens += parent_splitk->size;
    for (int i = 0; i < b->n_children - 1; ++i) {
        toku_copy_dbt(&a->childkeys[old_n_children + i], b->childkeys[i]);
        a->totalchildkeylens += b->childkeys[i].size;
        toku_init_dbt(&b->childkeys[i]);
    }
    a->n_children = new_n_children;

    b->totalchildkeylens = 0;
    b->n_children = 0;

    a->dirty = 1;
    b->dirty = 1;

    *did_merge = true;
    *did_rebalance = false;
    toku_init_dbt(splitk);

    STATUS_VALUE(FT_FLUSHER_MERGE_NONLEAF)++;
}

static void
maybe_merge_pinned_nodes(
    FTNODE parent,
    DBT *parent_splitk,
    FTNODE a,
    FTNODE b,
    bool *did_merge,
    bool *did_rebalance,
    DBT *splitk,
    uint32_t nodesize
    )
// Effect: either merge a and b into one node (merge them into a) and set *did_merge = true.
//	   (We do this if the resulting node is not fissible)
//	   or distribute a and b evenly and set *did_merge = false and *did_rebalance = true
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
    paranoid_invariant(a->height == b->height);
    toku_assert_entire_node_in_memory(parent);
    toku_assert_entire_node_in_memory(a);
    toku_assert_entire_node_in_memory(b);
    parent->dirty = 1;   // just to make sure
    {
        MSN msna = a->max_msn_applied_to_node_on_disk;
        MSN msnb = b->max_msn_applied_to_node_on_disk;
        msn_max = (msna.msn > msnb.msn) ? msna : msnb;
    }
    if (a->height == 0) {
        maybe_merge_pinned_leaf_nodes(a, b, parent_splitk, did_merge, did_rebalance, splitk, nodesize);
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
    bool for_checkpoint,
    void *extra)
{
    FT h = (FT) extra;
    toku_free_blocknum(h->blocktable, bp, h, for_checkpoint);
}

//
// Takes as input a locked node and a childnum_to_merge
// As output, two of node's children are merged or rebalanced, and node is unlocked
//
static void
ft_merge_child(
    FT h,
    FTNODE node,
    int childnum_to_merge,
    bool *did_react,
    struct flusher_advice *fa)
{
    // this function should not be called
    // if the child is not mergable
    paranoid_invariant(node->n_children > 1);
    toku_assert_entire_node_in_memory(node);

    int childnuma,childnumb;
    if (childnum_to_merge > 0) {
        childnuma = childnum_to_merge-1;
        childnumb = childnum_to_merge;
    } else {
        childnuma = childnum_to_merge;
        childnumb = childnum_to_merge+1;
    }
    paranoid_invariant(0 <= childnuma);
    paranoid_invariant(childnuma+1 == childnumb);
    paranoid_invariant(childnumb < node->n_children);

    paranoid_invariant(node->height>0);

    // We suspect that at least one of the children is fusible, but they might not be.
    // for test
    call_flusher_thread_callback(flt_flush_before_merge);

    FTNODE childa, childb;
    {
        uint32_t childfullhash = compute_child_fullhash(h->cf, node, childnuma);
        struct ftnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, h);
        toku_pin_ftnode_off_client_thread(h, BP_BLOCKNUM(node, childnuma), childfullhash, &bfe, PL_WRITE_EXPENSIVE, 1, &node, &childa);
    }
    // for test
    call_flusher_thread_callback(flt_flush_before_pin_second_node_for_merge);
    {
        FTNODE dep_nodes[2];
        dep_nodes[0] = node;
        dep_nodes[1] = childa;
        uint32_t childfullhash = compute_child_fullhash(h->cf, node, childnumb);
        struct ftnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, h);
        toku_pin_ftnode_off_client_thread(h, BP_BLOCKNUM(node, childnumb), childfullhash, &bfe, PL_WRITE_EXPENSIVE, 2, dep_nodes, &childb);
    }

    if (toku_bnc_n_entries(BNC(node,childnuma))>0) {
        flush_this_child(h, node, childa, childnuma, fa);
    }
    if (toku_bnc_n_entries(BNC(node,childnumb))>0) {
        flush_this_child(h, node, childb, childnumb, fa);
    }

    // now we have both children pinned in main memory, and cachetable locked,
    // so no checkpoints will occur.

    bool did_merge, did_rebalance;
    {
        DBT splitk;
        toku_init_dbt(&splitk);
        DBT *old_split_key = &node->childkeys[childnuma];
        unsigned int deleted_size = old_split_key->size;
        maybe_merge_pinned_nodes(node, &node->childkeys[childnuma], childa, childb, &did_merge, &did_rebalance, &splitk, h->h->nodesize);
        if (childa->height>0) {
            for (int i=0; i+1<childa->n_children; i++) {
                paranoid_invariant(childa->childkeys[i].data);
            }
        }
        //toku_verify_estimates(t,childa);
        // the tree did react if a merge (did_merge) or rebalance (new spkit key) occurred
        *did_react = (bool)(did_merge || did_rebalance);
        if (did_merge) {
            paranoid_invariant(!splitk.data);
        } else {
            paranoid_invariant(splitk.data);
        }

        node->totalchildkeylens -= deleted_size; // The key was free()'d inside the maybe_merge_pinned_nodes.

        if (did_merge) {
            NONLEAF_CHILDINFO remaining_bnc = BNC(node, childnuma);
            NONLEAF_CHILDINFO merged_bnc = BNC(node, childnumb);
            for (unsigned int i = 0; i < (sizeof remaining_bnc->flow) / (sizeof remaining_bnc->flow[0]); ++i) {
                remaining_bnc->flow[i] += merged_bnc->flow[i];
            }
            destroy_nonleaf_childinfo(merged_bnc);
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
            paranoid_invariant(BP_BLOCKNUM(node, childnuma).b == childa->thisnodename.b);
            childa->dirty = 1;  // just to make sure
            childb->dirty = 1;  // just to make sure
        } else {
            // flow will be inaccurate for a while, oh well.  the children
            // are leaves in this case so it's not a huge deal (we're
            // pretty far down the tree)

            // If we didn't merge the nodes, then we need the correct pivot.
            toku_copy_dbt(&node->childkeys[childnuma], splitk);
            node->totalchildkeylens += node->childkeys[childnuma].size;
            node->dirty = 1;
        }
    }
    //
    // now we possibly flush the children
    //
    if (did_merge) {
        // for test
        call_flusher_thread_callback(flt_flush_before_unpin_remove);

        // merge_remove_key_callback will free the blocknum
        int rrb = toku_cachetable_unpin_and_remove(
            h->cf,
            childb->ct_pair,
            merge_remove_key_callback,
            h
            );
        assert_zero(rrb);

        // for test
        call_flusher_thread_callback(ft_flush_aflter_merge);

        // unlock the parent
        paranoid_invariant(node->dirty);
        toku_unpin_ftnode_off_client_thread(h, node);
    }
    else {
        // for test
        call_flusher_thread_callback(ft_flush_aflter_rebalance);

        // unlock the parent
        paranoid_invariant(node->dirty);
        toku_unpin_ftnode_off_client_thread(h, node);
        toku_unpin_ftnode_off_client_thread(h, childb);
    }
    if (childa->height > 0 && fa->should_recursively_flush(childa, fa->extra)) {
        toku_ft_flush_some_child(h, childa, fa);
    }
    else {
        toku_unpin_ftnode_off_client_thread(h, childa);
    }
}

static void ft_flush_some_child(
    FT ft,
    FTNODE parent,
    struct flusher_advice *fa
    )
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
    paranoid_invariant(parent->height>0);
    toku_assert_entire_node_in_memory(parent);
    TXNID oldest_referenced_xid = parent->oldest_referenced_xid_known;

    // pick the child we want to flush to
    int childnum = fa->pick_child(ft, parent, fa->extra);

    // for test
    call_flusher_thread_callback(flt_flush_before_child_pin);

    // get the child into memory
    BLOCKNUM targetchild = BP_BLOCKNUM(parent, childnum);
    toku_verify_blocknum_allocated(ft->blocktable, targetchild);
    uint32_t childfullhash = compute_child_fullhash(ft->cf, parent, childnum);
    FTNODE child;
    struct ftnode_fetch_extra bfe;
    // Note that we don't read the entire node into memory yet.
    // The idea is let's try to do the minimum work before releasing the parent lock
    fill_bfe_for_min_read(&bfe, ft);
    toku_pin_ftnode_off_client_thread(ft, targetchild, childfullhash, &bfe, PL_WRITE_EXPENSIVE, 1, &parent, &child);

    // for test
    call_flusher_thread_callback(ft_flush_aflter_child_pin);

    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(parent, child, ft);
    }

    //Note that at this point, we don't have the entire child in.
    // Let's do a quick check to see if the child may be reactive
    // If the child cannot be reactive, then we can safely unlock
    // the parent before finishing reading in the entire child node.
    bool may_child_be_reactive = may_node_be_reactive(child);

    paranoid_invariant(child->thisnodename.b!=0);
    //VERIFY_NODE(brt, child);

    // only do the following work if there is a flush to perform
    if (toku_bnc_n_entries(BNC(parent, childnum)) > 0) {
        if (!parent->dirty) {
            dirtied++;
            parent->dirty = 1;
        }
        // detach buffer
        BP_WORKDONE(parent, childnum) = 0;  // this buffer is drained, no work has been done by its contents
        bnc = BNC(parent, childnum);
        NONLEAF_CHILDINFO new_bnc = toku_create_empty_nl();
        memcpy(new_bnc->flow, bnc->flow, sizeof bnc->flow);
        set_BNC(parent, childnum, new_bnc);
    }

    //
    // at this point, the buffer has been detached from the parent
    // and a new empty buffer has been placed in its stead
    // so, if we are absolutely sure that the child is not
    // reactive, we can unpin the parent
    //
    if (!may_child_be_reactive) {
        toku_unpin_ftnode_off_client_thread(ft, parent);
        parent = NULL;
    }

    //
    // now, if necessary, read/decompress the rest of child into memory,
    // so that we can proceed and apply the flush
    //
    bring_node_fully_into_memory(child, ft);

    // It is possible after reading in the entire child,
    // that we now know that the child is not reactive
    // if so, we can unpin parent right now
    // we wont be splitting/merging child
    // and we have already replaced the bnc
    // for the root with a fresh one
    enum reactivity child_re = get_node_reactivity(child, ft->h->nodesize);
    if (parent && child_re == RE_STABLE) {
        toku_unpin_ftnode_off_client_thread(ft, parent);
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
            child->dirty = 1;
        }
        // do the actual flush
        toku_bnc_flush_to_child(
            ft,
            bnc,
            child,
            oldest_referenced_xid
            );
        destroy_nonleaf_childinfo(bnc);
    }

    fa->update_status(child, dirtied, fa->extra);
    // let's get the reactivity of the child again,
    // it is possible that the flush got rid of some values
    // and now the parent is no longer reactive
    child_re = get_node_reactivity(child, ft->h->nodesize);
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
            toku_unpin_ftnode_off_client_thread(ft, parent);
            parent = NULL;
        }
        //
        // it is the responsibility of ft_flush_some_child to unpin child
        //
        if (child->height > 0 && fa->should_recursively_flush(child, fa->extra)) {
            ft_flush_some_child(ft, child, fa);
        }
        else {
            toku_unpin_ftnode_off_client_thread(ft, child);
        }
    }
    else if (child_re == RE_FISSIBLE) {
        //
        // it is responsibility of `ft_split_child` to unlock nodes of
        // parent and child as it sees fit
        //
        paranoid_invariant(parent); // just make sure we have not accidentally unpinned parent
        ft_split_child(ft, parent, childnum, child, SPLIT_EVENLY, fa);
    }
    else if (child_re == RE_FUSIBLE) {
        //
        // it is responsibility of `maybe_merge_child to unlock nodes of
        // parent and child as it sees fit
        //
        paranoid_invariant(parent); // just make sure we have not accidentally unpinned parent
        fa->maybe_merge_child(fa, ft, parent, childnum, child, fa->extra);
    }
    else {
        abort();
    }
}

void toku_ft_flush_some_child(FT ft, FTNODE parent, struct flusher_advice *fa) {
    // Vanilla flush_some_child flushes from parent to child without
    // providing a meaningful oldest_referenced_xid. No simple garbage
    // collection is performed.
    return ft_flush_some_child(ft, parent, fa);
}

static void
update_cleaner_status(
    FTNODE node,
    int childnum)
{
    STATUS_VALUE(FT_FLUSHER_CLEANER_TOTAL_NODES)++;
    if (node->height == 1) {
        STATUS_VALUE(FT_FLUSHER_CLEANER_H1_NODES)++;
    } else {
        STATUS_VALUE(FT_FLUSHER_CLEANER_HGT1_NODES)++;
    }

    unsigned int nbytesinbuf = toku_bnc_nbytesinbuf(BNC(node, childnum));
    if (nbytesinbuf == 0) {
        STATUS_VALUE(FT_FLUSHER_CLEANER_EMPTY_NODES)++;
    } else {
        if (nbytesinbuf > STATUS_VALUE(FT_FLUSHER_CLEANER_MAX_BUFFER_SIZE)) {
            STATUS_VALUE(FT_FLUSHER_CLEANER_MAX_BUFFER_SIZE) = nbytesinbuf;
        }
        if (nbytesinbuf < STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_SIZE)) {
            STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_SIZE) = nbytesinbuf;
        }
        STATUS_VALUE(FT_FLUSHER_CLEANER_TOTAL_BUFFER_SIZE) += nbytesinbuf;

        uint64_t workdone = BP_WORKDONE(node, childnum);
        if (workdone > STATUS_VALUE(FT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE)) {
            STATUS_VALUE(FT_FLUSHER_CLEANER_MAX_BUFFER_WORKDONE) = workdone;
        }
        if (workdone < STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE)) {
            STATUS_VALUE(FT_FLUSHER_CLEANER_MIN_BUFFER_WORKDONE) = workdone;
        }
        STATUS_VALUE(FT_FLUSHER_CLEANER_TOTAL_BUFFER_WORKDONE) += workdone;
    }
}

static void
dummy_update_status(
    FTNODE UU(child),
    int UU(dirtied),
    void* UU(extra)
    ) 
{
}

static int
dummy_pick_heaviest_child(FT UU(h),
                    FTNODE UU(parent),
                    void* UU(extra))
{
    abort();
    return -1;
}

void toku_ft_split_child(
    FT ft,
    FTNODE node,
    int childnum,
    FTNODE child,
    enum split_mode split_mode
    )
{
    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        dummy_pick_heaviest_child,
        dont_destroy_basement_nodes,
        never_recursively_flush,
        default_merge_child,
        dummy_update_status,
        default_pick_child_after_split,
        NULL
        );
    ft_split_child(
        ft,
        node,
        childnum, // childnum to split
        child,
        split_mode,
        &fa
        );
}

void toku_ft_merge_child(
    FT ft,
    FTNODE node,
    int childnum
    )
{
    struct flusher_advice fa;
    flusher_advice_init(
        &fa,
        dummy_pick_heaviest_child,
        dont_destroy_basement_nodes,
        never_recursively_flush,
        default_merge_child,
        dummy_update_status,
        default_pick_child_after_split,
        NULL
        );
    bool did_react;
    ft_merge_child(
        ft,
        node,
        childnum, // childnum to merge
        &did_react,
        &fa
        );
}

int
toku_ftnode_cleaner_callback(
    void *ftnode_pv,
    BLOCKNUM blocknum,
    uint32_t fullhash,
    void *extraargs)
{
    FTNODE node = (FTNODE) ftnode_pv;
    invariant(node->thisnodename.b == blocknum.b);
    invariant(node->fullhash == fullhash);
    invariant(node->height > 0);   // we should never pick a leaf node (for now at least)
    FT h = (FT) extraargs;
    bring_node_fully_into_memory(node, h);
    int childnum = find_heaviest_child(node);
    update_cleaner_status(node, childnum);

    // Either toku_ft_flush_some_child will unlock the node, or we do it here.
    if (toku_bnc_nbytesinbuf(BNC(node, childnum)) > 0) {
        struct flusher_advice fa;
        struct flush_status_update_extra fste;
        ct_flusher_advice_init(&fa, &fste, h->h->nodesize);
        toku_ft_flush_some_child(h, node, &fa);
    } else {
        toku_unpin_ftnode_off_client_thread(h, node);
    }
    return 0;
}

struct flusher_extra {
    FT h;
    FTNODE node;
    NONLEAF_CHILDINFO bnc;
    TXNID oldest_referenced_xid;
};

//
// This is the function that gets called by a
// background thread. Its purpose is to complete
// a flush, and possibly do a split/merge.
//
static void flush_node_fun(void *fe_v)
{
    struct flusher_extra* fe = (struct flusher_extra *) fe_v;
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

    struct flusher_advice fa;
    struct flush_status_update_extra fste;
    flt_flusher_advice_init(&fa, &fste, fe->h->h->nodesize);

    if (fe->bnc) {
        // In this case, we have a bnc to flush to a node

        // for test purposes
        call_flusher_thread_callback(flt_flush_before_applying_inbox);

        toku_bnc_flush_to_child(
            fe->h,
            fe->bnc,
            fe->node,
            fe->oldest_referenced_xid
            );
        destroy_nonleaf_childinfo(fe->bnc);

        // after the flush has completed, now check to see if the node needs flushing
        // If so, call ft_flush_some_child on the node (because this flush intends to
        // pass a meaningful oldest referenced xid for simple garbage collection), and it is the
        // responsibility of the flush to unlock the node. otherwise, we unlock it here.
        if (fe->node->height > 0 && toku_ft_nonleaf_is_gorged(fe->node, fe->h->h->nodesize)) {
            ft_flush_some_child(fe->h, fe->node, &fa);
        }
        else {
            toku_unpin_ftnode_off_client_thread(fe->h,fe->node);
        }
    }
    else {
        // In this case, we were just passed a node with no
        // bnc, which means we are tasked with flushing some
        // buffer in the node.
        // It is the responsibility of flush some child to unlock the node
        ft_flush_some_child(fe->h, fe->node, &fa);
    }
    remove_background_job_from_cf(fe->h->cf);
    toku_free(fe);
}

static void
place_node_and_bnc_on_background_thread(
    FT h,
    FTNODE node,
    NONLEAF_CHILDINFO bnc,
    TXNID oldest_referenced_xid)
{
    struct flusher_extra *XMALLOC(fe);
    fe->h = h;
    fe->node = node;
    fe->bnc = bnc;
    fe->oldest_referenced_xid = oldest_referenced_xid;
    cachefile_kibbutz_enq(h->cf, flush_node_fun, fe);
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
void toku_ft_flush_node_on_background_thread(FT h, FTNODE parent)
{
    TXNID oldest_referenced_xid_known = parent->oldest_referenced_xid_known;
    //
    // first let's see if we can detach buffer on client thread
    // and pick the child we want to flush to
    //
    int childnum = find_heaviest_child(parent);
    paranoid_invariant(toku_bnc_n_entries(BNC(parent, childnum))>0);
    //
    // see if we can pin the child
    //
    FTNODE child;
    uint32_t childfullhash = compute_child_fullhash(h->cf, parent, childnum);
    int r = toku_maybe_pin_ftnode_clean(h, BP_BLOCKNUM(parent, childnum), childfullhash, PL_WRITE_EXPENSIVE, &child);
    if (r != 0) {
        // In this case, we could not lock the child, so just place the parent on the background thread
        // In the callback, we will use toku_ft_flush_some_child, which checks to
        // see if we should blow away the old basement nodes.
        place_node_and_bnc_on_background_thread(h, parent, NULL, oldest_referenced_xid_known);
    }
    else {
        //
        // successfully locked child
        //
        bool may_child_be_reactive = may_node_be_reactive(child);
        if (!may_child_be_reactive) {
            // We're going to unpin the parent, so before we do, we must
            // check to see if we need to blow away the basement nodes to
            // keep the MSN invariants intact.
            maybe_destroy_child_blbs(parent, child, h);

            //
            // can detach buffer and unpin root here
            //
            parent->dirty = 1;
            BP_WORKDONE(parent, childnum) = 0;  // this buffer is drained, no work has been done by its contents
            NONLEAF_CHILDINFO bnc = BNC(parent, childnum);
            NONLEAF_CHILDINFO new_bnc = toku_create_empty_nl();
            memcpy(new_bnc->flow, bnc->flow, sizeof bnc->flow);
            set_BNC(parent, childnum, new_bnc);

            //
            // at this point, the buffer has been detached from the parent
            // and a new empty buffer has been placed in its stead
            // so, because we know for sure the child is not
            // reactive, we can unpin the parent
            //
            place_node_and_bnc_on_background_thread(h, child, bnc, oldest_referenced_xid_known);
            toku_unpin_ftnode(h, parent);
        }
        else {
            // because the child may be reactive, we need to
            // put parent on background thread.
            // As a result, we unlock the child here.
            toku_unpin_ftnode(h, child);
            // Again, we'll have the parent on the background thread, so
            // we don't need to destroy the basement nodes yet.
            place_node_and_bnc_on_background_thread(h, parent, NULL, oldest_referenced_xid_known);
        }
    }
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_ft_flusher_helgrind_ignore(void);
void
toku_ft_flusher_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&ft_flusher_status, sizeof ft_flusher_status);
}

#undef STATUS_VALUE
