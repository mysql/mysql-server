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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
  This software is covered by US Patent No. 8,489,638.

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

#include <config.h>

#include "ft/ft.h"
#include "ft/ft-cachetable-wrappers.h"
#include "ft/ft-internal.h"
#include "ft/ft-flusher.h"
#include "ft/ft-flusher-internal.h"
#include "ft/node.h"
#include "ft/serialize/block_table.h"
#include "ft/serialize/ft_node-serialize.h"
#include "portability/toku_assert.h"
#include "portability/toku_atomic.h"
#include "util/status.h"
#include "util/context.h"

/* Status is intended for display to humans to help understand system behavior.
 * It does not need to be perfectly thread-safe.
 */
static FT_FLUSHER_STATUS_S ft_flusher_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUFT_STATUS_INIT(ft_flusher_status, k, c, t, "ft flusher: " l, inc)

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
    uint64_t max_weight = toku_bnc_nbytesinbuf(BNC(node, 0)) + BP_WORKDONE(node, 0);

    invariant(node->n_children > 0);
    for (int i = 1; i < node->n_children; i++) {
        uint64_t bytes_in_buf = toku_bnc_nbytesinbuf(BNC(node, i));
        uint64_t workdone = BP_WORKDONE(node, i);
        if (workdone > 0) {
            invariant(bytes_in_buf > 0);
        }
        uint64_t this_weight = bytes_in_buf + workdone;
        if (max_weight < this_weight) {
            max_child = i;
            max_weight = this_weight;
        }
    }
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
maybe_destroy_child_blbs(FTNODE node, FTNODE child, FT ft)
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
                toku_evict_bn_from_memory(child, i, ft);
            }
        }
    }
}

static void
ft_merge_child(
    FT ft,
    FTNODE node,
    int childnum_to_merge,
    bool *did_react,
    struct flusher_advice *fa);

static int
pick_heaviest_child(FT UU(ft),
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
    return toku_ftnode_nonleaf_is_gorged(child, fste->nodesize);
}

int
default_pick_child_after_split(FT UU(ft),
                               FTNODE UU(parent),
                               int UU(childnuma),
                               int UU(childnumb),
                               void* UU(extra))
{
    return -1;
}

void
default_merge_child(struct flusher_advice *fa,
                    FT ft,
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
    toku_unpin_ftnode(ft, child);
    //
    //
    // it is responsibility of ft_merge_child to unlock parent
    //
    bool did_react;
    ft_merge_child(ft, parent, childnum, &did_react, fa);
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
ctm_pick_child(FT ft,
               FTNODE parent,
               void* extra)
{
    struct ctm_extra* ctme = (struct ctm_extra *) extra;
    int childnum;
    if (parent->height == 1 && ctme->is_last_child) {
        childnum = parent->n_children - 1;
    } else {
        childnum = toku_ftnode_which_child(parent, &ctme->target_key, ft->cmp);
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
                      FT ft,
                      FTNODE parent,
                      int childnum,
                      FTNODE child,
                      void *extra)
{
    if (child->height == 0) {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_COMPLETED), 1);
    }
    default_merge_child(fa, ft, parent, childnum, child, extra);
}

static void
ct_maybe_merge_child(struct flusher_advice *fa,
                     FT ft,
                     FTNODE parent,
                     int childnum,
                     FTNODE child,
                     void* extra)
{
    if (child->height > 0) {
        default_merge_child(fa, ft, parent, childnum, child, extra);
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
        toku_clone_dbt(&ctme.target_key, parent->pivotkeys.get_pivot(pivot_to_save));

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

        toku_unpin_ftnode(ft, parent);
        toku_unpin_ftnode(ft, child);

        FTNODE root_node = NULL;
        {
            uint32_t fullhash;
            CACHEKEY root;
            toku_calculate_root_offset_pointer(ft, &root, &fullhash);
            ftnode_fetch_extra bfe;
            bfe.create_for_full_read(ft);
            toku_pin_ftnode(ft, root, fullhash, &bfe, PL_WRITE_EXPENSIVE, &root_node, true);
            toku_ftnode_assert_fully_in_memory(root_node);
        }

        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_STARTED), 1);
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_FLUSHER_CLEANER_NUM_LEAF_MERGES_RUNNING), 1);

        toku_ft_flush_some_child(ft, root_node, &new_fa);

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
static bool ft_ftnode_may_be_reactive(FT ft, FTNODE node)
{
    if (node->height == 0) {
        return true;
    } else {
        return toku_ftnode_get_nonleaf_reactivity(node, ft->h->fanout) != RE_STABLE;
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
    FT ft,
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
    toku_ftnode_assert_fully_in_memory(node);
    toku_ftnode_assert_fully_in_memory(childa);
    toku_ftnode_assert_fully_in_memory(childb);
    NONLEAF_CHILDINFO old_bnc = BNC(node, childnum);
    paranoid_invariant(toku_bnc_nbytesinbuf(old_bnc)==0);
    WHEN_NOT_GCOV(
        if (toku_ft_debug_mode) {
            printf("%s:%d Child %d splitting on %s\n", __FILE__, __LINE__, childnum, (char*)splitk->data);
            printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
            for(int i = 0; i < node->n_children - 1; i++) printf(" %s", (char *) node->pivotkeys.get_pivot(i).data);
            printf("\n");
        }
    )

    node->dirty = 1;

    XREALLOC_N(node->n_children+1, node->bp);
    // Slide the children over.
    // suppose n_children is 10 and childnum is 5, meaning node->childnum[5] just got split
    // this moves node->bp[6] through node->bp[9] over to
    // node->bp[7] through node->bp[10]
    for (int cnum=node->n_children; cnum>childnum+1; cnum--) {
        node->bp[cnum] = node->bp[cnum-1];
    }
    memset(&node->bp[childnum+1],0,sizeof(node->bp[0]));
    node->n_children++;

    paranoid_invariant(BP_BLOCKNUM(node, childnum).b==childa->blocknum.b); // use the same child

    // We never set the rightmost blocknum to be the root.
    // Instead, we wait for the root to split and let promotion initialize the rightmost
    // blocknum to be the first non-root leaf node on the right extreme to recieve an insert.
    BLOCKNUM rightmost_blocknum = toku_drd_unsafe_fetch(&ft->rightmost_blocknum);
    invariant(ft->h->root_blocknum.b != rightmost_blocknum.b);
    if (childa->blocknum.b == rightmost_blocknum.b) {
        // The rightmost leaf (a) split into (a) and (b). We want (b) to swap pair values
        // with (a), now that it is the new rightmost leaf. This keeps the rightmost blocknum
        // constant, the same the way we keep the root blocknum constant.
        toku_ftnode_swap_pair_values(childa, childb);
        BP_BLOCKNUM(node, childnum) = childa->blocknum;
    }

    BP_BLOCKNUM(node, childnum+1) = childb->blocknum;
    BP_WORKDONE(node, childnum+1) = 0;
    BP_STATE(node,childnum+1) = PT_AVAIL;

    NONLEAF_CHILDINFO new_bnc = toku_create_empty_nl();
    for (unsigned int i = 0; i < (sizeof new_bnc->flow) / (sizeof new_bnc->flow[0]); ++i) {
        // just split the flows in half for now, can't guess much better
        // at the moment
        new_bnc->flow[i] = old_bnc->flow[i] / 2;
        old_bnc->flow[i] = (old_bnc->flow[i] + 1) / 2;
    }
    set_BNC(node, childnum+1, new_bnc);

    // Insert the new split key , sliding the other keys over
    node->pivotkeys.insert_at(splitk, childnum);

    WHEN_NOT_GCOV(
        if (toku_ft_debug_mode) {
            printf("%s:%d splitkeys:", __FILE__, __LINE__);
            for (int i = 0; i < node->n_children - 2; i++) printf(" %s", (char *) node->pivotkeys.get_pivot(i).data);
            printf("\n");
        }
    )

    /* Keep pushing to the children, but not if the children would require a pushdown */
    toku_ftnode_assert_fully_in_memory(node);
    toku_ftnode_assert_fully_in_memory(childa);
    toku_ftnode_assert_fully_in_memory(childb);

    VERIFY_NODE(t, node);
    VERIFY_NODE(t, childa);
    VERIFY_NODE(t, childb);
}

static void
verify_all_in_mempool(FTNODE UU() node)
{
#ifdef TOKU_DEBUG_PARANOID
    if (node->height==0) {
        for (int i = 0; i < node->n_children; i++) {
            invariant(BP_STATE(node,i) == PT_AVAIL);
            BLB_DATA(node, i)->verify_mempool();
        }
    }
#endif
}

static uint64_t
ftleaf_disk_size(FTNODE node)
// Effect: get the disk size of a leafentry
{
    paranoid_invariant(node->height == 0);
    toku_ftnode_assert_fully_in_memory(node);
    uint64_t retval = 0;
    for (int i = 0; i < node->n_children; i++) {
        retval += BLB_DATA(node, i)->get_disk_size();
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
        *num_left_les = BLB_DATA(node, *num_left_bns - 1)->num_klpairs();
        if (*num_left_les == 0) {
            *num_left_bns = node->n_children - 1;
            *num_left_les = BLB_DATA(node, *num_left_bns - 1)->num_klpairs();
        }
        goto exit;
    }
    case SPLIT_RIGHT_HEAVY: {
        *num_left_bns = 1;
        *num_left_les = BLB_DATA(node, 0)->num_klpairs() ? 1 : 0;
        goto exit;
    }
    case SPLIT_EVENLY: {
        paranoid_invariant(node->height == 0);
        // TODO: (Zardosht) see if we can/should make this faster, we iterate over the rows twice
        uint64_t sumlesizes = ftleaf_disk_size(node);
        uint32_t size_so_far = 0;
        for (int i = 0; i < node->n_children; i++) {
            bn_data* bd = BLB_DATA(node, i);
            uint32_t n_leafentries = bd->num_klpairs();
            for (uint32_t j=0; j < n_leafentries; j++) {
                size_t size_this_le;
                int rr = bd->fetch_klpair_disksize(j, &size_this_le);
                invariant_zero(rr);
                size_so_far += size_this_le;
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
                            *num_left_les = BLB_DATA(node, *num_left_bns - 1)->num_klpairs();
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

static void
move_leafentries(
    BASEMENTNODE dest_bn,
    BASEMENTNODE src_bn,
    uint32_t lbi, //lower bound inclusive
    uint32_t ube //upper bound exclusive
    )
//Effect: move leafentries in the range [lbi, upe) from src_omt to newly created dest_omt
{
    invariant(ube == src_bn->data_buffer.num_klpairs());
    src_bn->data_buffer.split_klpairs(&dest_bn->data_buffer, lbi);
}

static void ftnode_finalize_split(FTNODE node, FTNODE B, MSN max_msn_applied_to_node) {
// Effect: Finalizes a split by updating some bits and dirtying both nodes
    toku_ftnode_assert_fully_in_memory(node);
    toku_ftnode_assert_fully_in_memory(B);
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
    FT ft,
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
            ft,
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
    toku_ftnode_assert_fully_in_memory(node);
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
        const bool split_on_boundary = (num_left_les == 0) || (num_left_les == (int) BLB_DATA(node, num_left_bns - 1)->num_klpairs());
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
                ft->h->layout_version,
                ft->h->flags);
            B->fullhash = fullhash;
        }
        else {
            B = *nodeb;
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
            destroy_basement_node(BLB(B, curr_dest_bn_index)); // Destroy B's empty OMT, so I can rebuild it from an array
            set_BNULL(B, curr_dest_bn_index);
            set_BLB(B, curr_dest_bn_index, toku_create_empty_bn_no_buffer());
            move_leafentries(BLB(B, curr_dest_bn_index),
                             BLB(node, curr_src_bn_index),
                             num_left_les,         // first row to be moved to B
                             BLB_DATA(node, curr_src_bn_index)->num_klpairs()  // number of rows in basement to be split
                             );
            BLB_MAX_MSN_APPLIED(B, curr_dest_bn_index) = BLB_MAX_MSN_APPLIED(node, curr_src_bn_index);
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
        int split_idx = num_left_bns - (split_on_boundary ? 0 : 1);
        node->pivotkeys.split_at(split_idx, &B->pivotkeys);
        if (split_on_boundary && num_left_bns < node->n_children && splitk) {
            toku_copyref_dbt(splitk, node->pivotkeys.get_pivot(num_left_bns - 1));
        } else if (splitk) {
            bn_data* bd = BLB_DATA(node, num_left_bns - 1);
            uint32_t keylen;
            void *key;
            int rr = bd->fetch_key_and_len(bd->num_klpairs() - 1, &keylen, &key);
            invariant_zero(rr);
            toku_memdup_dbt(splitk, key, keylen);
        }

        node->n_children = num_children_in_node;
        REALLOC_N(num_children_in_node, node->bp);
    }

    ftnode_finalize_split(node, B, max_msn_applied_to_node);
    *nodea = node;
    *nodeb = B;
}    // end of ftleaf_split()

void
ft_nonleaf_split(
    FT ft,
    FTNODE node,
    FTNODE *nodea,
    FTNODE *nodeb,
    DBT *splitk,
    uint32_t num_dependent_nodes,
    FTNODE* dependent_nodes)
{
    //VERIFY_NODE(t,node);
    STATUS_VALUE(FT_FLUSHER_SPLIT_NONLEAF)++;
    toku_ftnode_assert_fully_in_memory(node);
    int old_n_children = node->n_children;
    int n_children_in_a = old_n_children/2;
    int n_children_in_b = old_n_children-n_children_in_a;
    MSN max_msn_applied_to_node = node->max_msn_applied_to_node_on_disk;
    FTNODE B;
    paranoid_invariant(node->height>0);
    paranoid_invariant(node->n_children>=2); // Otherwise, how do we split?	 We need at least two children to split. */
    create_new_ftnode_with_dep_nodes(ft, &B, node->height, n_children_in_b, num_dependent_nodes, dependent_nodes);
    {
        /* The first n_children_in_a go into node a.
         * That means that the first n_children_in_a-1 keys go into node a.
         * The splitter key is key number n_children_in_a */
        for (int i = n_children_in_a; i<old_n_children; i++) {
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
        }

        // the split key for our parent is the rightmost pivot key in node
        node->pivotkeys.split_at(n_children_in_a, &B->pivotkeys);
        toku_clone_dbt(splitk, node->pivotkeys.get_pivot(n_children_in_a - 1));
        node->pivotkeys.delete_at(n_children_in_a - 1);

        node->n_children = n_children_in_a;
        REALLOC_N(node->n_children, node->bp);
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
    FT ft,
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
        ftleaf_split(ft, child, &nodea, &nodeb, &splitk, true, split_mode, 2, dep_nodes);
    } else {
        ft_nonleaf_split(ft, child, &nodea, &nodeb, &splitk, 2, dep_nodes);
    }
    // printf("%s:%d child did split\n", __FILE__, __LINE__);
    handle_split_of_child (ft, node, childnum, nodea, nodeb, &splitk);

    // for test
    call_flusher_thread_callback(flt_flush_during_split);

    // at this point, the split is complete
    // now we need to unlock node,
    // and possibly continue
    // flushing one of the children
    int picked_child = fa->pick_child_after_split(ft, node, childnum, childnum + 1, fa->extra);
    toku_unpin_ftnode(ft, node);
    if (picked_child == childnum ||
        (picked_child < 0 && nodea->height > 0 && fa->should_recursively_flush(nodea, fa->extra))) {
        toku_unpin_ftnode(ft, nodeb);
        toku_ft_flush_some_child(ft, nodea, fa);
    }
    else if (picked_child == childnum + 1 ||
             (picked_child < 0 && nodeb->height > 0 && fa->should_recursively_flush(nodeb, fa->extra))) {
        toku_unpin_ftnode(ft, nodea);
        toku_ft_flush_some_child(ft, nodeb, fa);
    }
    else {
        toku_unpin_ftnode(ft, nodea);
        toku_unpin_ftnode(ft, nodeb);
    }

    toku_destroy_dbt(&splitk);
}

static void bring_node_fully_into_memory(FTNODE node, FT ft) {
    if (!toku_ftnode_fully_in_memory(node)) {
        ftnode_fetch_extra bfe;
        bfe.create_for_full_read(ft);
        toku_cachetable_pf_pinned_pair(
            node,
            toku_ftnode_pf_callback,
            &bfe,
            ft->cf,
            node->blocknum,
            toku_cachetable_hash(ft->cf, node->blocknum)
            );
    }
}

static void
flush_this_child(
    FT ft,
    FTNODE node,
    FTNODE child,
    int childnum,
    struct flusher_advice *fa)
// Effect: Push everything in the CHILDNUMth buffer of node down into the child.
{
    update_flush_status(child, 0);
    toku_ftnode_assert_fully_in_memory(node);
    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(node, child, ft);
    }
    bring_node_fully_into_memory(child, ft);
    toku_ftnode_assert_fully_in_memory(child);
    paranoid_invariant(node->height>0);
    paranoid_invariant(child->blocknum.b!=0);
    // VERIFY_NODE does not work off client thread as of now
    //VERIFY_NODE(t, child);
    node->dirty = 1;
    child->dirty = 1;

    BP_WORKDONE(node, childnum) = 0;  // this buffer is drained, no work has been done by its contents
    NONLEAF_CHILDINFO bnc = BNC(node, childnum);
    set_BNC(node, childnum, toku_create_empty_nl());

    // now we have a bnc to flush to the child. pass down the parent's
    // oldest known referenced xid as we flush down to the child.
    toku_bnc_flush_to_child(ft, bnc, child, node->oldest_referenced_xid_known);
    destroy_nonleaf_childinfo(bnc);
}

static void
merge_leaf_nodes(FTNODE a, FTNODE b)
{
    STATUS_VALUE(FT_FLUSHER_MERGE_LEAF)++;
    toku_ftnode_assert_fully_in_memory(a);
    toku_ftnode_assert_fully_in_memory(b);
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

    bn_data* a_last_bd = BLB_DATA(a, a->n_children-1);
    // this bool states if the last basement node in a has any items or not
    // If it does, then it stays in the merge. If it does not, the last basement node
    // of a gets eliminated because we do not have a pivot to store for it (because it has no elements)
    const bool a_has_tail = a_last_bd->num_klpairs() > 0;

    int num_children = a->n_children + b->n_children;
    if (!a_has_tail) {
        int lastchild = a->n_children - 1;
        BASEMENTNODE bn = BLB(a, lastchild);

        // verify that last basement in a is empty, then destroy mempool
        size_t used_space = a_last_bd->get_disk_size();
        invariant_zero(used_space);
        destroy_basement_node(bn);
        set_BNULL(a, lastchild);
        num_children--;
        if (lastchild < a->pivotkeys.num_pivots()) {
            a->pivotkeys.delete_at(lastchild);
        }
    } else {
        // fill in pivot for what used to be max of node 'a', if it is needed
        uint32_t keylen;
        void *key;
        int r = a_last_bd->fetch_key_and_len(a_last_bd->num_klpairs() - 1, &keylen, &key);
        invariant_zero(r);
        DBT pivotkey;
        toku_fill_dbt(&pivotkey, key, keylen);
        a->pivotkeys.replace_at(&pivotkey, a->n_children - 1);
    }

    // realloc basement nodes in `a'
    REALLOC_N(num_children, a->bp);

    // move each basement node from b to a
    uint32_t offset = a_has_tail ? a->n_children : a->n_children - 1;
    for (int i = 0; i < b->n_children; i++) {
        a->bp[i + offset] = b->bp[i];
        memset(&b->bp[i], 0, sizeof(b->bp[0]));
    }

    // append b's pivots to a's pivots
    a->pivotkeys.append(b->pivotkeys);

    // now that all the data has been moved from b to a, we can destroy the data in b
    a->n_children = num_children;
    b->pivotkeys.destroy();
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
    const DBT *parent_splitk,
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
    uint32_t num_leafentries = toku_ftnode_leaf_num_entries(a) + toku_ftnode_leaf_num_entries(b);
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
        *did_rebalance = true;
        balance_leaf_nodes(a, b, splitk);
    } else {
        // we are merging them.
        *did_merge = true;
        *did_rebalance = false;
        toku_init_dbt(splitk);
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
    toku_ftnode_assert_fully_in_memory(a);
    toku_ftnode_assert_fully_in_memory(b);
    invariant_notnull(parent_splitk->data);

    int old_n_children = a->n_children;
    int new_n_children = old_n_children + b->n_children;

    XREALLOC_N(new_n_children, a->bp);
    memcpy(a->bp + old_n_children, b->bp, b->n_children * sizeof(b->bp[0]));
    memset(b->bp, 0, b->n_children * sizeof(b->bp[0]));

    a->pivotkeys.insert_at(parent_splitk, old_n_children - 1);
    a->pivotkeys.append(b->pivotkeys);
    a->n_children = new_n_children;
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
    const DBT *parent_splitk,
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
//  t			The FT.
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
    toku_ftnode_assert_fully_in_memory(parent);
    toku_ftnode_assert_fully_in_memory(a);
    toku_ftnode_assert_fully_in_memory(b);
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

static void merge_remove_key_callback(BLOCKNUM *bp, bool for_checkpoint, void *extra) {
    FT ft = (FT) extra;
    ft->blocktable.free_blocknum(bp, ft, for_checkpoint);
}

//
// Takes as input a locked node and a childnum_to_merge
// As output, two of node's children are merged or rebalanced, and node is unlocked
//
static void
ft_merge_child(
    FT ft,
    FTNODE node,
    int childnum_to_merge,
    bool *did_react,
    struct flusher_advice *fa)
{
    // this function should not be called
    // if the child is not mergable
    paranoid_invariant(node->n_children > 1);
    toku_ftnode_assert_fully_in_memory(node);

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
        uint32_t childfullhash = compute_child_fullhash(ft->cf, node, childnuma);
        ftnode_fetch_extra bfe;
        bfe.create_for_full_read(ft);
        toku_pin_ftnode_with_dep_nodes(ft, BP_BLOCKNUM(node, childnuma), childfullhash, &bfe, PL_WRITE_EXPENSIVE, 1, &node, &childa, true);
    }
    // for test
    call_flusher_thread_callback(flt_flush_before_pin_second_node_for_merge);
    {
        FTNODE dep_nodes[2];
        dep_nodes[0] = node;
        dep_nodes[1] = childa;
        uint32_t childfullhash = compute_child_fullhash(ft->cf, node, childnumb);
        ftnode_fetch_extra bfe;
        bfe.create_for_full_read(ft);
        toku_pin_ftnode_with_dep_nodes(ft, BP_BLOCKNUM(node, childnumb), childfullhash, &bfe, PL_WRITE_EXPENSIVE, 2, dep_nodes, &childb, true);
    }

    if (toku_bnc_n_entries(BNC(node,childnuma))>0) {
        flush_this_child(ft, node, childa, childnuma, fa);
    }
    if (toku_bnc_n_entries(BNC(node,childnumb))>0) {
        flush_this_child(ft, node, childb, childnumb, fa);
    }

    // now we have both children pinned in main memory, and cachetable locked,
    // so no checkpoints will occur.

    bool did_merge, did_rebalance;
    {
        DBT splitk;
        toku_init_dbt(&splitk);
        const DBT old_split_key = node->pivotkeys.get_pivot(childnuma);
        maybe_merge_pinned_nodes(node, &old_split_key, childa, childb, &did_merge, &did_rebalance, &splitk, ft->h->nodesize);
        //toku_verify_estimates(t,childa);
        // the tree did react if a merge (did_merge) or rebalance (new spkit key) occurred
        *did_react = (bool)(did_merge || did_rebalance);

        if (did_merge) {
            invariant_null(splitk.data);
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
            node->pivotkeys.delete_at(childnuma);

            // Handle a merge of the rightmost leaf node.
            BLOCKNUM rightmost_blocknum = toku_drd_unsafe_fetch(&ft->rightmost_blocknum); 
            if (did_merge && childb->blocknum.b == rightmost_blocknum.b) {
                invariant(childb->blocknum.b != ft->h->root_blocknum.b);
                toku_ftnode_swap_pair_values(childa, childb);
                BP_BLOCKNUM(node, childnuma) = childa->blocknum;
            }

            paranoid_invariant(BP_BLOCKNUM(node, childnuma).b == childa->blocknum.b);
            childa->dirty = 1;  // just to make sure
            childb->dirty = 1;  // just to make sure
        } else {
            // flow will be inaccurate for a while, oh well.  the children
            // are leaves in this case so it's not a huge deal (we're
            // pretty far down the tree)

            // If we didn't merge the nodes, then we need the correct pivot.
            invariant_notnull(splitk.data);
            node->pivotkeys.replace_at(&splitk, childnuma);
            node->dirty = 1;
        }
        toku_destroy_dbt(&splitk);
    }
    //
    // now we possibly flush the children
    //
    if (did_merge) {
        // for test
        call_flusher_thread_callback(flt_flush_before_unpin_remove);

        // merge_remove_key_callback will free the blocknum
        int rrb = toku_cachetable_unpin_and_remove(
            ft->cf,
            childb->ct_pair,
            merge_remove_key_callback,
            ft
            );
        assert_zero(rrb);

        // for test
        call_flusher_thread_callback(ft_flush_aflter_merge);

        // unlock the parent
        paranoid_invariant(node->dirty);
        toku_unpin_ftnode(ft, node);
    }
    else {
        // for test
        call_flusher_thread_callback(ft_flush_aflter_rebalance);

        // unlock the parent
        paranoid_invariant(node->dirty);
        toku_unpin_ftnode(ft, node);
        toku_unpin_ftnode(ft, childb);
    }
    if (childa->height > 0 && fa->should_recursively_flush(childa, fa->extra)) {
        toku_ft_flush_some_child(ft, childa, fa);
    }
    else {
        toku_unpin_ftnode(ft, childa);
    }
}

void toku_ft_flush_some_child(FT ft, FTNODE parent, struct flusher_advice *fa)
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
    toku_ftnode_assert_fully_in_memory(parent);
    TXNID parent_oldest_referenced_xid_known = parent->oldest_referenced_xid_known;

    // pick the child we want to flush to
    int childnum = fa->pick_child(ft, parent, fa->extra);

    // for test
    call_flusher_thread_callback(flt_flush_before_child_pin);

    // get the child into memory
    BLOCKNUM targetchild = BP_BLOCKNUM(parent, childnum);
    ft->blocktable.verify_blocknum_allocated(targetchild);
    uint32_t childfullhash = compute_child_fullhash(ft->cf, parent, childnum);
    FTNODE child;
    ftnode_fetch_extra bfe;
    // Note that we don't read the entire node into memory yet.
    // The idea is let's try to do the minimum work before releasing the parent lock
    bfe.create_for_min_read(ft);
    toku_pin_ftnode_with_dep_nodes(ft, targetchild, childfullhash, &bfe, PL_WRITE_EXPENSIVE, 1, &parent, &child, true);

    // for test
    call_flusher_thread_callback(ft_flush_aflter_child_pin);

    if (fa->should_destroy_basement_nodes(fa)) {
        maybe_destroy_child_blbs(parent, child, ft);
    }

    //Note that at this point, we don't have the entire child in.
    // Let's do a quick check to see if the child may be reactive
    // If the child cannot be reactive, then we can safely unlock
    // the parent before finishing reading in the entire child node.
    bool may_child_be_reactive = ft_ftnode_may_be_reactive(ft, child);

    paranoid_invariant(child->blocknum.b!=0);

    // only do the following work if there is a flush to perform
    if (toku_bnc_n_entries(BNC(parent, childnum)) > 0 || parent->height == 1) {
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
        toku_unpin_ftnode(ft, parent);
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
    enum reactivity child_re = toku_ftnode_get_reactivity(ft, child);
    if (parent && child_re == RE_STABLE) {
        toku_unpin_ftnode(ft, parent);
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
            parent_oldest_referenced_xid_known
            );
        destroy_nonleaf_childinfo(bnc);
    }

    fa->update_status(child, dirtied, fa->extra);
    // let's get the reactivity of the child again,
    // it is possible that the flush got rid of some values
    // and now the parent is no longer reactive
    child_re = toku_ftnode_get_reactivity(ft, child);
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
            toku_unpin_ftnode(ft, parent);
            parent = NULL;
        }
        //
        // it is the responsibility of toku_ft_flush_some_child to unpin child
        //
        if (child->height > 0 && fa->should_recursively_flush(child, fa->extra)) {
            toku_ft_flush_some_child(ft, child, fa);
        }
        else {
            toku_unpin_ftnode(ft, child);
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

void toku_bnc_flush_to_child(FT ft, NONLEAF_CHILDINFO bnc, FTNODE child, TXNID parent_oldest_referenced_xid_known) {
    paranoid_invariant(bnc);

    TOKULOGGER logger = toku_cachefile_logger(ft->cf);
    TXN_MANAGER txn_manager = logger != nullptr ? toku_logger_get_txn_manager(logger) : nullptr;
    TXNID oldest_referenced_xid_for_simple_gc = TXNID_NONE;

    txn_manager_state txn_state_for_gc(txn_manager);
    bool do_garbage_collection = child->height == 0 && txn_manager != nullptr;
    if (do_garbage_collection) {
        txn_state_for_gc.init();
        oldest_referenced_xid_for_simple_gc = toku_txn_manager_get_oldest_referenced_xid_estimate(txn_manager);
    }
    txn_gc_info gc_info(&txn_state_for_gc,
                        oldest_referenced_xid_for_simple_gc,                    
                        child->oldest_referenced_xid_known,
                        true);
    struct flush_msg_fn {
        FT ft;
        FTNODE child;
        NONLEAF_CHILDINFO bnc;
        txn_gc_info *gc_info;

        STAT64INFO_S stats_delta;
        size_t remaining_memsize = bnc->msg_buffer.buffer_size_in_use();

        flush_msg_fn(FT t, FTNODE n, NONLEAF_CHILDINFO nl, txn_gc_info *g) :
            ft(t), child(n), bnc(nl), gc_info(g), remaining_memsize(bnc->msg_buffer.buffer_size_in_use()) {
            stats_delta = { 0, 0 };
        }
        int operator()(const ft_msg &msg, bool is_fresh) {
            size_t flow_deltas[] = { 0, 0 };
            size_t memsize_in_buffer = message_buffer::msg_memsize_in_buffer(msg);
            if (remaining_memsize <= bnc->flow[0]) {
                // this message is in the current checkpoint's worth of
                // the end of the message buffer
                flow_deltas[0] = memsize_in_buffer;
            } else if (remaining_memsize <= bnc->flow[0] + bnc->flow[1]) {
                // this message is in the last checkpoint's worth of the
                // end of the message buffer
                flow_deltas[1] = memsize_in_buffer;
            }
            toku_ftnode_put_msg(
                ft->cmp,
                ft->update_fun,
                child,
                -1,
                msg,
                is_fresh,
                gc_info,
                flow_deltas,
                &stats_delta
                );
            remaining_memsize -= memsize_in_buffer;
            return 0;
        }
    } flush_fn(ft, child, bnc, &gc_info);
    bnc->msg_buffer.iterate(flush_fn);

    child->oldest_referenced_xid_known = parent_oldest_referenced_xid_known;

    invariant(flush_fn.remaining_memsize == 0);
    if (flush_fn.stats_delta.numbytes || flush_fn.stats_delta.numrows) {
        toku_ft_update_stats(&ft->in_memory_stats, flush_fn.stats_delta);
    }
    if (do_garbage_collection) {
        size_t buffsize = bnc->msg_buffer.buffer_size_in_use();
        // may be misleading if there's a broadcast message in there
        toku_ft_status_note_msg_bytes_out(buffsize);
    }
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
    invariant(node->blocknum.b == blocknum.b);
    invariant(node->fullhash == fullhash);
    invariant(node->height > 0);   // we should never pick a leaf node (for now at least)
    FT ft = (FT) extraargs;
    bring_node_fully_into_memory(node, ft);
    int childnum = find_heaviest_child(node);
    update_cleaner_status(node, childnum);

    // Either toku_ft_flush_some_child will unlock the node, or we do it here.
    if (toku_bnc_nbytesinbuf(BNC(node, childnum)) > 0) {
        struct flusher_advice fa;
        struct flush_status_update_extra fste;
        ct_flusher_advice_init(&fa, &fste, ft->h->nodesize);
        toku_ft_flush_some_child(ft, node, &fa);
    } else {
        toku_unpin_ftnode(ft, node);
    }
    return 0;
}

struct flusher_extra {
    FT ft;
    FTNODE node;
    NONLEAF_CHILDINFO bnc;
    TXNID parent_oldest_referenced_xid_known;
};

//
// This is the function that gets called by a
// background thread. Its purpose is to complete
// a flush, and possibly do a split/merge.
//
static void flush_node_fun(void *fe_v)
{
    toku::context flush_ctx(CTX_FLUSH);
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
    bring_node_fully_into_memory(fe->node,fe->ft);
    fe->node->dirty = 1;

    struct flusher_advice fa;
    struct flush_status_update_extra fste;
    flt_flusher_advice_init(&fa, &fste, fe->ft->h->nodesize);

    if (fe->bnc) {
        // In this case, we have a bnc to flush to a node

        // for test purposes
        call_flusher_thread_callback(flt_flush_before_applying_inbox);

        toku_bnc_flush_to_child(
            fe->ft,
            fe->bnc,
            fe->node,
            fe->parent_oldest_referenced_xid_known
            );
        destroy_nonleaf_childinfo(fe->bnc);

        // after the flush has completed, now check to see if the node needs flushing
        // If so, call toku_ft_flush_some_child on the node (because this flush intends to
        // pass a meaningful oldest referenced xid for simple garbage collection), and it is the
        // responsibility of the flush to unlock the node. otherwise, we unlock it here.
        if (fe->node->height > 0 && toku_ftnode_nonleaf_is_gorged(fe->node, fe->ft->h->nodesize)) {
            toku_ft_flush_some_child(fe->ft, fe->node, &fa);
        }
        else {
            toku_unpin_ftnode(fe->ft,fe->node);
        }
    }
    else {
        // In this case, we were just passed a node with no
        // bnc, which means we are tasked with flushing some
        // buffer in the node.
        // It is the responsibility of flush some child to unlock the node
        toku_ft_flush_some_child(fe->ft, fe->node, &fa);
    }
    remove_background_job_from_cf(fe->ft->cf);
    toku_free(fe);
}

static void
place_node_and_bnc_on_background_thread(
    FT ft,
    FTNODE node,
    NONLEAF_CHILDINFO bnc,
    TXNID parent_oldest_referenced_xid_known)
{
    struct flusher_extra *XMALLOC(fe);
    fe->ft = ft;
    fe->node = node;
    fe->bnc = bnc;
    fe->parent_oldest_referenced_xid_known = parent_oldest_referenced_xid_known;
    cachefile_kibbutz_enq(ft->cf, flush_node_fun, fe);
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
void toku_ft_flush_node_on_background_thread(FT ft, FTNODE parent)
{
    toku::context flush_ctx(CTX_FLUSH);
    TXNID parent_oldest_referenced_xid_known = parent->oldest_referenced_xid_known;
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
    uint32_t childfullhash = compute_child_fullhash(ft->cf, parent, childnum);
    int r = toku_maybe_pin_ftnode_clean(ft, BP_BLOCKNUM(parent, childnum), childfullhash, PL_WRITE_EXPENSIVE, &child);
    if (r != 0) {
        // In this case, we could not lock the child, so just place the parent on the background thread
        // In the callback, we will use toku_ft_flush_some_child, which checks to
        // see if we should blow away the old basement nodes.
        place_node_and_bnc_on_background_thread(ft, parent, NULL, parent_oldest_referenced_xid_known);
    }
    else {
        //
        // successfully locked child
        //
        bool may_child_be_reactive = ft_ftnode_may_be_reactive(ft, child);
        if (!may_child_be_reactive) {
            // We're going to unpin the parent, so before we do, we must
            // check to see if we need to blow away the basement nodes to
            // keep the MSN invariants intact.
            maybe_destroy_child_blbs(parent, child, ft);

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
            place_node_and_bnc_on_background_thread(ft, child, bnc, parent_oldest_referenced_xid_known);
            toku_unpin_ftnode(ft, parent);
        }
        else {
            // because the child may be reactive, we need to
            // put parent on background thread.
            // As a result, we unlock the child here.
            toku_unpin_ftnode(ft, child);
            // Again, we'll have the parent on the background thread, so
            // we don't need to destroy the basement nodes yet.
            place_node_and_bnc_on_background_thread(ft, parent, NULL, parent_oldest_referenced_xid_known);
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
