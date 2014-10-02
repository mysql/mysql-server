/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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
#include "ft/ft-internal.h"
#include "ft/serialize/ft_node-serialize.h"
#include "ft/node.h"
#include "ft/serialize/rbuf.h"
#include "ft/serialize/wbuf.h"
#include "util/scoped_malloc.h"
#include "util/sort.h"

// Effect: Fill in N as an empty ftnode.
// TODO: Rename toku_ftnode_create
void toku_initialize_empty_ftnode(FTNODE n, BLOCKNUM blocknum, int height, int num_children, int layout_version, unsigned int flags) {
    paranoid_invariant(layout_version != 0);
    paranoid_invariant(height >= 0);

    n->max_msn_applied_to_node_on_disk = ZERO_MSN;    // correct value for root node, harmless for others
    n->flags = flags;
    n->blocknum = blocknum;
    n->layout_version               = layout_version;
    n->layout_version_original = layout_version;
    n->layout_version_read_from_disk = layout_version;
    n->height = height;
    n->pivotkeys.create_empty();
    n->bp = 0;
    n->n_children = num_children;
    n->oldest_referenced_xid_known = TXNID_NONE;

    if (num_children > 0) {
        XMALLOC_N(num_children, n->bp);
        for (int i = 0; i < num_children; i++) {
            BP_BLOCKNUM(n,i).b=0;
            BP_STATE(n,i) = PT_INVALID;
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
    n->dirty = 1;  // special case exception, it's okay to mark as dirty because the basements are empty

    toku_ft_status_note_ftnode(height, true);
}

// destroys the internals of the ftnode, but it does not free the values
// that are stored
// this is common functionality for toku_ftnode_free and rebalance_ftnode_leaf
// MUST NOT do anything besides free the structures that have been allocated
void toku_destroy_ftnode_internals(FTNODE node) {
    node->pivotkeys.destroy();
    for (int i = 0; i < node->n_children; i++) {
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
            paranoid_invariant(is_BNULL(node, i));
        }
        set_BNULL(node, i);
    }
    toku_free(node->bp);
    node->bp = NULL;
}

/* Frees a node, including all the stuff in the hash table. */
void toku_ftnode_free(FTNODE *nodep) {
    FTNODE node = *nodep;
    toku_ft_status_note_ftnode(node->height, false);
    toku_destroy_ftnode_internals(node);
    toku_free(node);
    *nodep = nullptr;
}

void toku_ftnode_update_disk_stats(FTNODE ftnode, FT ft, bool for_checkpoint) {
    STAT64INFO_S deltas = ZEROSTATS;
    // capture deltas before rebalancing basements for serialization
    deltas = toku_get_and_clear_basement_stats(ftnode);
    // locking not necessary here with respect to checkpointing
    // in Clayface (because of the pending lock and cachetable lock
    // in toku_cachetable_begin_checkpoint)
    // essentially, if we are dealing with a for_checkpoint 
    // parameter in a function that is called by the flush_callback,
    // then the cachetable needs to ensure that this is called in a safe
    // manner that does not interfere with the beginning
    // of a checkpoint, which it does with the cachetable lock
    // and pending lock
    toku_ft_update_stats(&ft->h->on_disk_stats, deltas);
    if (for_checkpoint) {
        toku_ft_update_stats(&ft->checkpoint_header->on_disk_stats, deltas);
    }
}

void toku_ftnode_clone_partitions(FTNODE node, FTNODE cloned_node) {
    for (int i = 0; i < node->n_children; i++) {
        BP_BLOCKNUM(cloned_node,i) = BP_BLOCKNUM(node,i);
        paranoid_invariant(BP_STATE(node,i) == PT_AVAIL);
        BP_STATE(cloned_node,i) = PT_AVAIL;
        BP_WORKDONE(cloned_node, i) = BP_WORKDONE(node, i);
        if (node->height == 0) {
            set_BLB(cloned_node, i, toku_clone_bn(BLB(node,i)));
        } else {
            set_BNC(cloned_node, i, toku_clone_nl(BNC(node,i)));
        }
    }
}

void toku_evict_bn_from_memory(FTNODE node, int childnum, FT ft) {
    // free the basement node
    assert(!node->dirty);
    BASEMENTNODE bn = BLB(node, childnum);
    toku_ft_decrease_stats(&ft->in_memory_stats, bn->stat64_delta);
    destroy_basement_node(bn);
    set_BNULL(node, childnum);
    BP_STATE(node, childnum) = PT_ON_DISK;
}

BASEMENTNODE toku_detach_bn(FTNODE node, int childnum) {
    assert(BP_STATE(node, childnum) == PT_AVAIL);
    BASEMENTNODE bn = BLB(node, childnum);
    set_BNULL(node, childnum);
    BP_STATE(node, childnum) = PT_ON_DISK;
    return bn;
}

// 
// Orthopush
//

struct store_msg_buffer_offset_extra {
    int32_t *offsets;
    int i;
};

int store_msg_buffer_offset(const int32_t &offset, const uint32_t UU(idx), struct store_msg_buffer_offset_extra *const extra) __attribute__((nonnull(3)));
int store_msg_buffer_offset(const int32_t &offset, const uint32_t UU(idx), struct store_msg_buffer_offset_extra *const extra)
{
    extra->offsets[extra->i] = offset;
    extra->i++;
    return 0;
}

/**
 * Given pointers to offsets within a message buffer where we can find messages,
 * figure out the MSN of each message, and compare those MSNs.  Returns 1,
 * 0, or -1 if a is larger than, equal to, or smaller than b.
 */
int msg_buffer_offset_msn_cmp(message_buffer &msg_buffer, const int32_t &ao, const int32_t &bo);
int msg_buffer_offset_msn_cmp(message_buffer &msg_buffer, const int32_t &ao, const int32_t &bo)
{
    MSN amsn, bmsn;
    msg_buffer.get_message_key_msn(ao, nullptr, &amsn);
    msg_buffer.get_message_key_msn(bo, nullptr, &bmsn);
    if (amsn.msn > bmsn.msn) {
        return +1;
    }
    if (amsn.msn < bmsn.msn) {
        return -1;
    }
    return 0;
}

/**
 * Given a message buffer and and offset, apply the message with toku_ft_bn_apply_msg, or discard it,
 * based on its MSN and the MSN of the basement node.
 */
static void
do_bn_apply_msg(FT_HANDLE ft_handle, BASEMENTNODE bn, message_buffer *msg_buffer, int32_t offset,
                txn_gc_info *gc_info, uint64_t *workdone, STAT64INFO stats_to_update) {
    DBT k, v;
    ft_msg msg = msg_buffer->get_message(offset, &k, &v);

    // The messages are being iterated over in (key,msn) order or just in
    // msn order, so all the messages for one key, from one buffer, are in
    // ascending msn order.  So it's ok that we don't update the basement
    // node's msn until the end.
    if (msg.msn().msn > bn->max_msn_applied.msn) {
        toku_ft_bn_apply_msg(
            ft_handle->ft->cmp,
            ft_handle->ft->update_fun,
            bn,
            msg,
            gc_info,
            workdone,
            stats_to_update
            );
    } else {
        toku_ft_status_note_msn_discard();
    }

    // We must always mark message as stale since it has been marked
    // (using omt::iterate_and_mark_range)
    // It is possible to call do_bn_apply_msg even when it won't apply the message because
    // the node containing it could have been evicted and brought back in.
    msg_buffer->set_freshness(offset, false);
}


struct iterate_do_bn_apply_msg_extra {
    FT_HANDLE t;
    BASEMENTNODE bn;
    NONLEAF_CHILDINFO bnc;
    txn_gc_info *gc_info;
    uint64_t *workdone;
    STAT64INFO stats_to_update;
};

int iterate_do_bn_apply_msg(const int32_t &offset, const uint32_t UU(idx), struct iterate_do_bn_apply_msg_extra *const e) __attribute__((nonnull(3)));
int iterate_do_bn_apply_msg(const int32_t &offset, const uint32_t UU(idx), struct iterate_do_bn_apply_msg_extra *const e)
{
    do_bn_apply_msg(e->t, e->bn, &e->bnc->msg_buffer, offset, e->gc_info, e->workdone, e->stats_to_update);
    return 0;
}

/**
 * Given the bounds of the basement node to which we will apply messages,
 * find the indexes within message_tree which contain the range of
 * relevant messages.
 *
 * The message tree contains offsets into the buffer, where messages are
 * found.  The pivot_bounds are the lower bound exclusive and upper bound
 * inclusive, because they come from pivot keys in the tree.  We want OMT
 * indices, which must have the lower bound be inclusive and the upper
 * bound exclusive.  We will get these by telling omt::find to look
 * for something strictly bigger than each of our pivot bounds.
 *
 * Outputs the OMT indices in lbi (lower bound inclusive) and ube (upper
 * bound exclusive).
 */
template<typename find_bounds_omt_t>
static void
find_bounds_within_message_tree(
    const toku::comparator &cmp,
    const find_bounds_omt_t &message_tree,      /// tree holding message buffer offsets, in which we want to look for indices
    message_buffer *msg_buffer,           /// message buffer in which messages are found
    const pivot_bounds &bounds,  /// key bounds within the basement node we're applying messages to
    uint32_t *lbi,        /// (output) "lower bound inclusive" (index into message_tree)
    uint32_t *ube         /// (output) "upper bound exclusive" (index into message_tree)
    )
{
    int r = 0;

    if (!toku_dbt_is_empty(bounds.lbe())) {
        // By setting msn to MAX_MSN and by using direction of +1, we will
        // get the first message greater than (in (key, msn) order) any
        // message (with any msn) with the key lower_bound_exclusive.
        // This will be a message we want to try applying, so it is the
        // "lower bound inclusive" within the message_tree.
        struct toku_msg_buffer_key_msn_heaviside_extra lbi_extra(cmp, msg_buffer, bounds.lbe(), MAX_MSN);
        int32_t found_lb;
        r = message_tree.template find<struct toku_msg_buffer_key_msn_heaviside_extra, toku_msg_buffer_key_msn_heaviside>(lbi_extra, +1, &found_lb, lbi);
        if (r == DB_NOTFOUND) {
            // There is no relevant data (the lower bound is bigger than
            // any message in this tree), so we have no range and we're
            // done.
            *lbi = 0;
            *ube = 0;
            return;
        }
        if (!toku_dbt_is_empty(bounds.ubi())) {
            // Check if what we found for lbi is greater than the upper
            // bound inclusive that we have.  If so, there are no relevant
            // messages between these bounds.
            const DBT *ubi = bounds.ubi();
            const int32_t offset = found_lb;
            DBT found_lbidbt;
            msg_buffer->get_message_key_msn(offset, &found_lbidbt, nullptr);
            int c = cmp(&found_lbidbt, ubi);
            // These DBTs really are both inclusive bounds, so we need
            // strict inequality in order to determine that there's
            // nothing between them.  If they're equal, then we actually
            // need to apply the message pointed to by lbi, and also
            // anything with the same key but a bigger msn.
            if (c > 0) {
                *lbi = 0;
                *ube = 0;
                return;
            }
        }
    } else {
        // No lower bound given, it's negative infinity, so we start at
        // the first message in the OMT.
        *lbi = 0;
    }
    if (!toku_dbt_is_empty(bounds.ubi())) {
        // Again, we use an msn of MAX_MSN and a direction of +1 to get
        // the first thing bigger than the upper_bound_inclusive key.
        // This is therefore the smallest thing we don't want to apply,
        // and omt::iterate_on_range will not examine it.
        struct toku_msg_buffer_key_msn_heaviside_extra ube_extra(cmp, msg_buffer, bounds.ubi(), MAX_MSN);
        r = message_tree.template find<struct toku_msg_buffer_key_msn_heaviside_extra, toku_msg_buffer_key_msn_heaviside>(ube_extra, +1, nullptr, ube);
        if (r == DB_NOTFOUND) {
            // Couldn't find anything in the buffer bigger than our key,
            // so we need to look at everything up to the end of
            // message_tree.
            *ube = message_tree.size();
        }
    } else {
        // No upper bound given, it's positive infinity, so we need to go
        // through the end of the OMT.
        *ube = message_tree.size();
    }
}

/**
 * For each message in the ancestor's buffer (determined by childnum) that
 * is key-wise between lower_bound_exclusive and upper_bound_inclusive,
 * apply the message to the basement node.  We treat the bounds as minus
 * or plus infinity respectively if they are NULL.  Do not mark the node
 * as dirty (preserve previous state of 'dirty' bit).
 */
static void
bnc_apply_messages_to_basement_node(
    FT_HANDLE t,             // used for comparison function
    BASEMENTNODE bn,   // where to apply messages
    FTNODE ancestor,  // the ancestor node where we can find messages to apply
    int childnum,      // which child buffer of ancestor contains messages we want
    const pivot_bounds &bounds,  // contains pivot key bounds of this basement node
    txn_gc_info *gc_info,
    bool* msgs_applied
    )
{
    int r;
    NONLEAF_CHILDINFO bnc = BNC(ancestor, childnum);

    // Determine the offsets in the message trees between which we need to
    // apply messages from this buffer
    STAT64INFO_S stats_delta = {0,0};
    uint64_t workdone_this_ancestor = 0;

    uint32_t stale_lbi, stale_ube;
    if (!bn->stale_ancestor_messages_applied) {
        find_bounds_within_message_tree(t->ft->cmp, bnc->stale_message_tree, &bnc->msg_buffer, bounds, &stale_lbi, &stale_ube);
    } else {
        stale_lbi = 0;
        stale_ube = 0;
    }
    uint32_t fresh_lbi, fresh_ube;
    find_bounds_within_message_tree(t->ft->cmp, bnc->fresh_message_tree, &bnc->msg_buffer, bounds, &fresh_lbi, &fresh_ube);

    // We now know where all the messages we must apply are, so one of the
    // following 4 cases will do the application, depending on which of
    // the lists contains relevant messages:
    //
    // 1. broadcast messages and anything else, or a mix of fresh and stale
    // 2. only fresh messages
    // 3. only stale messages
    if (bnc->broadcast_list.size() > 0 ||
        (stale_lbi != stale_ube && fresh_lbi != fresh_ube)) {
        // We have messages in multiple trees, so we grab all
        // the relevant messages' offsets and sort them by MSN, then apply
        // them in MSN order.
        const int buffer_size = ((stale_ube - stale_lbi) + (fresh_ube - fresh_lbi) + bnc->broadcast_list.size());
        toku::scoped_malloc offsets_buf(buffer_size * sizeof(int32_t));
        int32_t *offsets = reinterpret_cast<int32_t *>(offsets_buf.get());
        struct store_msg_buffer_offset_extra sfo_extra = { .offsets = offsets, .i = 0 };

        // Populate offsets array with offsets to stale messages
        r = bnc->stale_message_tree.iterate_on_range<struct store_msg_buffer_offset_extra, store_msg_buffer_offset>(stale_lbi, stale_ube, &sfo_extra);
        assert_zero(r);

        // Then store fresh offsets, and mark them to be moved to stale later.
        r = bnc->fresh_message_tree.iterate_and_mark_range<struct store_msg_buffer_offset_extra, store_msg_buffer_offset>(fresh_lbi, fresh_ube, &sfo_extra);
        assert_zero(r);

        // Store offsets of all broadcast messages.
        r = bnc->broadcast_list.iterate<struct store_msg_buffer_offset_extra, store_msg_buffer_offset>(&sfo_extra);
        assert_zero(r);
        invariant(sfo_extra.i == buffer_size);

        // Sort by MSN.
        toku::sort<int32_t, message_buffer, msg_buffer_offset_msn_cmp>::mergesort_r(offsets, buffer_size, bnc->msg_buffer);

        // Apply the messages in MSN order.
        for (int i = 0; i < buffer_size; ++i) {
            *msgs_applied = true;
            do_bn_apply_msg(t, bn, &bnc->msg_buffer, offsets[i], gc_info, &workdone_this_ancestor, &stats_delta);
        }
    } else if (stale_lbi == stale_ube) {
        // No stale messages to apply, we just apply fresh messages, and mark them to be moved to stale later.
        struct iterate_do_bn_apply_msg_extra iter_extra = { .t = t, .bn = bn, .bnc = bnc, .gc_info = gc_info, .workdone = &workdone_this_ancestor, .stats_to_update = &stats_delta };
        if (fresh_ube - fresh_lbi > 0) *msgs_applied = true;
        r = bnc->fresh_message_tree.iterate_and_mark_range<struct iterate_do_bn_apply_msg_extra, iterate_do_bn_apply_msg>(fresh_lbi, fresh_ube, &iter_extra);
        assert_zero(r);
    } else {
        invariant(fresh_lbi == fresh_ube);
        // No fresh messages to apply, we just apply stale messages.

        if (stale_ube - stale_lbi > 0) *msgs_applied = true;
        struct iterate_do_bn_apply_msg_extra iter_extra = { .t = t, .bn = bn, .bnc = bnc, .gc_info = gc_info, .workdone = &workdone_this_ancestor, .stats_to_update = &stats_delta };

        r = bnc->stale_message_tree.iterate_on_range<struct iterate_do_bn_apply_msg_extra, iterate_do_bn_apply_msg>(stale_lbi, stale_ube, &iter_extra);
        assert_zero(r);
    }
    //
    // update stats
    //
    if (workdone_this_ancestor > 0) {
        (void) toku_sync_fetch_and_add(&BP_WORKDONE(ancestor, childnum), workdone_this_ancestor);
    }
    if (stats_delta.numbytes || stats_delta.numrows) {
        toku_ft_update_stats(&t->ft->in_memory_stats, stats_delta);
    }
}

static void
apply_ancestors_messages_to_bn(
    FT_HANDLE t,
    FTNODE node,
    int childnum,
    ANCESTORS ancestors,
    const pivot_bounds &bounds, 
    txn_gc_info *gc_info,
    bool* msgs_applied
    )
{
    BASEMENTNODE curr_bn = BLB(node, childnum);
    const pivot_bounds curr_bounds = bounds.next_bounds(node, childnum);
    for (ANCESTORS curr_ancestors = ancestors; curr_ancestors; curr_ancestors = curr_ancestors->next) {
        if (curr_ancestors->node->max_msn_applied_to_node_on_disk.msn > curr_bn->max_msn_applied.msn) {
            paranoid_invariant(BP_STATE(curr_ancestors->node, curr_ancestors->childnum) == PT_AVAIL);
            bnc_apply_messages_to_basement_node(
                t,
                curr_bn,
                curr_ancestors->node,
                curr_ancestors->childnum,
                curr_bounds,
                gc_info,
                msgs_applied
                );
            // We don't want to check this ancestor node again if the
            // next time we query it, the msn hasn't changed.
            curr_bn->max_msn_applied = curr_ancestors->node->max_msn_applied_to_node_on_disk;
        }
    }
    // At this point, we know all the stale messages above this
    // basement node have been applied, and any new messages will be
    // fresh, so we don't need to look at stale messages for this
    // basement node, unless it gets evicted (and this field becomes
    // false when it's read in again).
    curr_bn->stale_ancestor_messages_applied = true;
}

void
toku_apply_ancestors_messages_to_node (
    FT_HANDLE t, 
    FTNODE node, 
    ANCESTORS ancestors, 
    const pivot_bounds &bounds, 
    bool* msgs_applied, 
    int child_to_read
    )
// Effect:
//   Bring a leaf node up-to-date according to all the messages in the ancestors.
//   If the leaf node is already up-to-date then do nothing.
//   If the leaf node is not already up-to-date, then record the work done
//   for that leaf in each ancestor.
// Requires:
//   This is being called when pinning a leaf node for the query path.
//   The entire root-to-leaf path is pinned and appears in the ancestors list.
{
    VERIFY_NODE(t, node);
    paranoid_invariant(node->height == 0);

    TXN_MANAGER txn_manager = toku_ft_get_txn_manager(t);
    txn_manager_state txn_state_for_gc(txn_manager);

    TXNID oldest_referenced_xid_for_simple_gc = toku_ft_get_oldest_referenced_xid_estimate(t);
    txn_gc_info gc_info(&txn_state_for_gc,
                        oldest_referenced_xid_for_simple_gc,
                        node->oldest_referenced_xid_known,
                        true);
    if (!node->dirty && child_to_read >= 0) {
        paranoid_invariant(BP_STATE(node, child_to_read) == PT_AVAIL);
        apply_ancestors_messages_to_bn(
            t,
            node,
            child_to_read,
            ancestors,
            bounds,
            &gc_info,
            msgs_applied
            );
    }
    else {
        // know we are a leaf node
        // An important invariant:
        // We MUST bring every available basement node for a dirty node up to date.
        // flushing on the cleaner thread depends on this. This invariant
        // allows the cleaner thread to just pick an internal node and flush it
        // as opposed to being forced to start from the root.
        for (int i = 0; i < node->n_children; i++) {
            if (BP_STATE(node, i) != PT_AVAIL) { continue; }
            apply_ancestors_messages_to_bn(
                t,
                node,
                i,
                ancestors,
                bounds,
                &gc_info,
                msgs_applied
                );
        }
    }
    VERIFY_NODE(t, node);
}

static bool bn_needs_ancestors_messages(
    FT ft,
    FTNODE node,
    int childnum,
    const pivot_bounds &bounds,
    ANCESTORS ancestors, 
    MSN* max_msn_applied
    ) 
{
    BASEMENTNODE bn = BLB(node, childnum);
    const pivot_bounds curr_bounds = bounds.next_bounds(node, childnum);
    bool needs_ancestors_messages = false;
    for (ANCESTORS curr_ancestors = ancestors; curr_ancestors; curr_ancestors = curr_ancestors->next) {
        if (curr_ancestors->node->max_msn_applied_to_node_on_disk.msn > bn->max_msn_applied.msn) {
            paranoid_invariant(BP_STATE(curr_ancestors->node, curr_ancestors->childnum) == PT_AVAIL);
            NONLEAF_CHILDINFO bnc = BNC(curr_ancestors->node, curr_ancestors->childnum);
            if (bnc->broadcast_list.size() > 0) {
                needs_ancestors_messages = true;
                goto cleanup;
            }
            if (!bn->stale_ancestor_messages_applied) {
                uint32_t stale_lbi, stale_ube;
                find_bounds_within_message_tree(ft->cmp,
                                                bnc->stale_message_tree,
                                                &bnc->msg_buffer,
                                                curr_bounds,
                                                &stale_lbi,
                                                &stale_ube);
                if (stale_lbi < stale_ube) {
                    needs_ancestors_messages = true;
                    goto cleanup;
                }
            }
            uint32_t fresh_lbi, fresh_ube;
            find_bounds_within_message_tree(ft->cmp,
                                            bnc->fresh_message_tree,
                                            &bnc->msg_buffer,
                                            curr_bounds,
                                            &fresh_lbi,
                                            &fresh_ube);
            if (fresh_lbi < fresh_ube) {
                needs_ancestors_messages = true;
                goto cleanup;
            }
            if (curr_ancestors->node->max_msn_applied_to_node_on_disk.msn > max_msn_applied->msn) {
                max_msn_applied->msn = curr_ancestors->node->max_msn_applied_to_node_on_disk.msn;
            }
        }
    }
cleanup:
    return needs_ancestors_messages;
}

bool toku_ft_leaf_needs_ancestors_messages(
    FT ft, 
    FTNODE node, 
    ANCESTORS ancestors, 
    const pivot_bounds &bounds, 
    MSN *const max_msn_in_path, 
    int child_to_read
    )
// Effect: Determine whether there are messages in a node's ancestors
//  which must be applied to it.  These messages are in the correct
//  keyrange for any available basement nodes, and are in nodes with the
//  correct max_msn_applied_to_node_on_disk.
// Notes:
//  This is an approximate query.
// Output:
//  max_msn_in_path: max of "max_msn_applied_to_node_on_disk" over
//    ancestors.  This is used later to update basement nodes'
//    max_msn_applied values in case we don't do the full algorithm.
// Returns:
//  true if there may be some such messages
//  false only if there are definitely no such messages
// Rationale:
//  When we pin a node with a read lock, we want to quickly determine if
//  we should exchange it for a write lock in preparation for applying
//  messages.  If there are no messages, we don't need the write lock.
{
    paranoid_invariant(node->height == 0);
    bool needs_ancestors_messages = false;
    // child_to_read may be -1 in test cases
    if (!node->dirty && child_to_read >= 0) {
        paranoid_invariant(BP_STATE(node, child_to_read) == PT_AVAIL);
        needs_ancestors_messages = bn_needs_ancestors_messages(
            ft,
            node,
            child_to_read,
            bounds,
            ancestors,
            max_msn_in_path
            );
    }
    else {
        for (int i = 0; i < node->n_children; ++i) {
            if (BP_STATE(node, i) != PT_AVAIL) { continue; }
            needs_ancestors_messages = bn_needs_ancestors_messages(
                ft,
                node,
                i,
                bounds,
                ancestors,
                max_msn_in_path
                );
            if (needs_ancestors_messages) {
                goto cleanup;
            }
        }
    }
cleanup:
    return needs_ancestors_messages;
}

void toku_ft_bn_update_max_msn(FTNODE node, MSN max_msn_applied, int child_to_read) {
    invariant(node->height == 0);
    if (!node->dirty && child_to_read >= 0) {
        paranoid_invariant(BP_STATE(node, child_to_read) == PT_AVAIL);
        BASEMENTNODE bn = BLB(node, child_to_read);
        if (max_msn_applied.msn > bn->max_msn_applied.msn) {
            // see comment below
            (void) toku_sync_val_compare_and_swap(&bn->max_msn_applied.msn, bn->max_msn_applied.msn, max_msn_applied.msn);
        }
    }
    else {
        for (int i = 0; i < node->n_children; ++i) {
            if (BP_STATE(node, i) != PT_AVAIL) { continue; }
            BASEMENTNODE bn = BLB(node, i);
            if (max_msn_applied.msn > bn->max_msn_applied.msn) {
                // This function runs in a shared access context, so to silence tools
                // like DRD, we use a CAS and ignore the result.
                // Any threads trying to update these basement nodes should be
                // updating them to the same thing (since they all have a read lock on
                // the same root-to-leaf path) so this is safe.
                (void) toku_sync_val_compare_and_swap(&bn->max_msn_applied.msn, bn->max_msn_applied.msn, max_msn_applied.msn);
            }
        }
    }
}

struct copy_to_stale_extra {
    FT ft;
    NONLEAF_CHILDINFO bnc;
};

int copy_to_stale(const int32_t &offset, const uint32_t UU(idx), struct copy_to_stale_extra *const extra) __attribute__((nonnull(3)));
int copy_to_stale(const int32_t &offset, const uint32_t UU(idx), struct copy_to_stale_extra *const extra)
{
    MSN msn;
    DBT key;
    extra->bnc->msg_buffer.get_message_key_msn(offset, &key, &msn);
    struct toku_msg_buffer_key_msn_heaviside_extra heaviside_extra(extra->ft->cmp, &extra->bnc->msg_buffer, &key, msn);
    int r = extra->bnc->stale_message_tree.insert<struct toku_msg_buffer_key_msn_heaviside_extra, toku_msg_buffer_key_msn_heaviside>(offset, heaviside_extra, nullptr);
    invariant_zero(r);
    return 0;
}

void toku_ft_bnc_move_messages_to_stale(FT ft, NONLEAF_CHILDINFO bnc) {
    struct copy_to_stale_extra cts_extra = { .ft = ft, .bnc = bnc };
    int r = bnc->fresh_message_tree.iterate_over_marked<struct copy_to_stale_extra, copy_to_stale>(&cts_extra);
    invariant_zero(r);
    bnc->fresh_message_tree.delete_all_marked();
}

void toku_move_ftnode_messages_to_stale(FT ft, FTNODE node) {
    invariant(node->height > 0);
    for (int i = 0; i < node->n_children; ++i) {
        if (BP_STATE(node, i) != PT_AVAIL) {
            continue;
        }
        NONLEAF_CHILDINFO bnc = BNC(node, i);
        // We can't delete things out of the fresh tree inside the above
        // procedures because we're still looking at the fresh tree.  Instead
        // we have to move messages after we're done looking at it.
        toku_ft_bnc_move_messages_to_stale(ft, bnc);
    }
}

// 
// Balance // Availibility // Size

struct rebalance_array_info {
    uint32_t offset;
    LEAFENTRY *le_array;
    uint32_t *key_sizes_array;
    const void **key_ptr_array;
    static int fn(const void* key, const uint32_t keylen, const LEAFENTRY &le,
           const uint32_t idx, struct rebalance_array_info *const ai) {
        ai->le_array[idx+ai->offset] = le;
        ai->key_sizes_array[idx+ai->offset] = keylen;
        ai->key_ptr_array[idx+ai->offset] = key;
        return 0;
    }
};

// There must still be at least one child
// Requires that all messages in buffers above have been applied.
// Because all messages above have been applied, setting msn of all new basements 
// to max msn of existing basements is correct.  (There cannot be any messages in
// buffers above that still need to be applied.)
void toku_ftnode_leaf_rebalance(FTNODE node, unsigned int basementnodesize) {

    assert(node->height == 0);
    assert(node->dirty);

    uint32_t num_orig_basements = node->n_children;
    // Count number of leaf entries in this leaf (num_le).
    uint32_t num_le = 0;
    for (uint32_t i = 0; i < num_orig_basements; i++) {
        num_le += BLB_DATA(node, i)->num_klpairs();
    }

    uint32_t num_alloc = num_le ? num_le : 1;  // simplify logic below by always having at least one entry per array

    // Create an array of OMTVALUE's that store all the pointers to all the data.
    // Each element in leafpointers is a pointer to a leaf.
    toku::scoped_malloc leafpointers_buf(sizeof(LEAFENTRY) * num_alloc);
    LEAFENTRY *leafpointers = reinterpret_cast<LEAFENTRY *>(leafpointers_buf.get());
    leafpointers[0] = NULL;

    toku::scoped_malloc key_pointers_buf(sizeof(void *) * num_alloc);
    const void **key_pointers = reinterpret_cast<const void **>(key_pointers_buf.get());
    key_pointers[0] = NULL;

    toku::scoped_malloc key_sizes_buf(sizeof(uint32_t) * num_alloc);
    uint32_t *key_sizes = reinterpret_cast<uint32_t *>(key_sizes_buf.get());

    // Capture pointers to old mempools' buffers (so they can be destroyed)
    toku::scoped_malloc old_bns_buf(sizeof(BASEMENTNODE) * num_orig_basements);
    BASEMENTNODE *old_bns = reinterpret_cast<BASEMENTNODE *>(old_bns_buf.get());
    old_bns[0] = NULL;

    uint32_t curr_le = 0;
    for (uint32_t i = 0; i < num_orig_basements; i++) {
        bn_data* bd = BLB_DATA(node, i);
        struct rebalance_array_info ai {.offset = curr_le, .le_array = leafpointers, .key_sizes_array = key_sizes, .key_ptr_array = key_pointers };
        bd->iterate<rebalance_array_info, rebalance_array_info::fn>(&ai);
        curr_le += bd->num_klpairs();
    }

    // Create an array that will store indexes of new pivots.
    // Each element in new_pivots is the index of a pivot key.
    // (Allocating num_le of them is overkill, but num_le is an upper bound.)
    toku::scoped_malloc new_pivots_buf(sizeof(uint32_t) * num_alloc);
    uint32_t *new_pivots = reinterpret_cast<uint32_t *>(new_pivots_buf.get());
    new_pivots[0] = 0;

    // Each element in le_sizes is the size of the leafentry pointed to by leafpointers.
    toku::scoped_malloc le_sizes_buf(sizeof(size_t) * num_alloc);
    size_t *le_sizes = reinterpret_cast<size_t *>(le_sizes_buf.get());
    le_sizes[0] = 0;

    // Create an array that will store the size of each basement.
    // This is the sum of the leaf sizes of all the leaves in that basement.
    // We don't know how many basements there will be, so we use num_le as the upper bound.

    // Sum of all le sizes in a single basement
    toku::scoped_calloc bn_le_sizes_buf(sizeof(size_t) * num_alloc);
    size_t *bn_le_sizes = reinterpret_cast<size_t *>(bn_le_sizes_buf.get());

    // Sum of all key sizes in a single basement
    toku::scoped_calloc bn_key_sizes_buf(sizeof(size_t) * num_alloc);
    size_t *bn_key_sizes = reinterpret_cast<size_t *>(bn_key_sizes_buf.get());

    // TODO 4050: All these arrays should be combined into a single array of some bn_info struct (pivot, msize, num_les).
    // Each entry is the number of leafentries in this basement.  (Again, num_le is overkill upper baound.)
    toku::scoped_malloc num_les_this_bn_buf(sizeof(uint32_t) * num_alloc);
    uint32_t *num_les_this_bn = reinterpret_cast<uint32_t *>(num_les_this_bn_buf.get());
    num_les_this_bn[0] = 0;
    
    // Figure out the new pivots.  
    // We need the index of each pivot, and for each basement we need
    // the number of leaves and the sum of the sizes of the leaves (memory requirement for basement).
    uint32_t curr_pivot = 0;
    uint32_t num_le_in_curr_bn = 0;
    uint32_t bn_size_so_far = 0;
    for (uint32_t i = 0; i < num_le; i++) {
        uint32_t curr_le_size = leafentry_disksize((LEAFENTRY) leafpointers[i]); 
        le_sizes[i] = curr_le_size;
        if ((bn_size_so_far + curr_le_size + sizeof(uint32_t) + key_sizes[i] > basementnodesize) && (num_le_in_curr_bn != 0)) {
            // cap off the current basement node to end with the element before i
            new_pivots[curr_pivot] = i-1;
            curr_pivot++;
            num_le_in_curr_bn = 0;
            bn_size_so_far = 0;
        }
        num_le_in_curr_bn++;
        num_les_this_bn[curr_pivot] = num_le_in_curr_bn;
        bn_le_sizes[curr_pivot] += curr_le_size;
        bn_key_sizes[curr_pivot] += sizeof(uint32_t) + key_sizes[i];  // uint32_t le_offset
        bn_size_so_far += curr_le_size + sizeof(uint32_t) + key_sizes[i];
    }
    // curr_pivot is now the total number of pivot keys in the leaf node
    int num_pivots   = curr_pivot;
    int num_children = num_pivots + 1;

    // now we need to fill in the new basement nodes and pivots

    // TODO: (Zardosht) this is an ugly thing right now
    // Need to figure out how to properly deal with seqinsert.
    // I am not happy with how this is being
    // handled with basement nodes
    uint32_t tmp_seqinsert = BLB_SEQINSERT(node, num_orig_basements - 1);

    // choose the max msn applied to any basement as the max msn applied to all new basements
    MSN max_msn = ZERO_MSN;
    for (uint32_t i = 0; i < num_orig_basements; i++) {
        MSN curr_msn = BLB_MAX_MSN_APPLIED(node,i);
        max_msn = (curr_msn.msn > max_msn.msn) ? curr_msn : max_msn;
    }
    // remove the basement node in the node, we've saved a copy
    for (uint32_t i = 0; i < num_orig_basements; i++) {
        // save a reference to the old basement nodes
        // we will need them to ensure that the memory
        // stays intact
        old_bns[i] = toku_detach_bn(node, i);
    }
    // Now destroy the old basements, but do not destroy leaves
    toku_destroy_ftnode_internals(node);

    // now reallocate pieces and start filling them in
    invariant(num_children > 0);

    node->n_children = num_children;
    XCALLOC_N(num_children, node->bp);             // allocate pointers to basements (bp)
    for (int i = 0; i < num_children; i++) {
        set_BLB(node, i, toku_create_empty_bn());  // allocate empty basements and set bp pointers
    }

    // now we start to fill in the data

    // first the pivots
    toku::scoped_malloc pivotkeys_buf(num_pivots * sizeof(DBT));
    DBT *pivotkeys = reinterpret_cast<DBT *>(pivotkeys_buf.get());
    for (int i = 0; i < num_pivots; i++) {
        uint32_t size = key_sizes[new_pivots[i]];
        const void *key = key_pointers[new_pivots[i]];
        toku_fill_dbt(&pivotkeys[i], key, size);
    }
    node->pivotkeys.create_from_dbts(pivotkeys, num_pivots);

    uint32_t baseindex_this_bn = 0;
    // now the basement nodes
    for (int i = 0; i < num_children; i++) {
        // put back seqinsert
        BLB_SEQINSERT(node, i) = tmp_seqinsert;

        // create start (inclusive) and end (exclusive) boundaries for data of basement node
        uint32_t curr_start = (i==0) ? 0 : new_pivots[i-1]+1;               // index of first leaf in basement
        uint32_t curr_end = (i==num_pivots) ? num_le : new_pivots[i]+1;     // index of first leaf in next basement
        uint32_t num_in_bn = curr_end - curr_start;                         // number of leaves in this basement

        // create indexes for new basement
        invariant(baseindex_this_bn == curr_start);
        uint32_t num_les_to_copy = num_les_this_bn[i];
        invariant(num_les_to_copy == num_in_bn); 

        bn_data* bd = BLB_DATA(node, i);
        bd->set_contents_as_clone_of_sorted_array(
            num_les_to_copy,
            &key_pointers[baseindex_this_bn],
            &key_sizes[baseindex_this_bn],
            &leafpointers[baseindex_this_bn],
            &le_sizes[baseindex_this_bn],
            bn_key_sizes[i],  // Total key sizes
            bn_le_sizes[i]  // total le sizes
            );

        BP_STATE(node,i) = PT_AVAIL;
        BP_TOUCH_CLOCK(node,i);
        BLB_MAX_MSN_APPLIED(node,i) = max_msn;
        baseindex_this_bn += num_les_to_copy;  // set to index of next bn
    }
    node->max_msn_applied_to_node_on_disk = max_msn;

    // destroy buffers of old mempools
    for (uint32_t i = 0; i < num_orig_basements; i++) {
        destroy_basement_node(old_bns[i]);
    }
}

bool toku_ftnode_fully_in_memory(FTNODE node) {
    for (int i = 0; i < node->n_children; i++) {
        if (BP_STATE(node,i) != PT_AVAIL) {
            return false;
        }
    }
    return true;
}

void toku_ftnode_assert_fully_in_memory(FTNODE UU(node)) {
    paranoid_invariant(toku_ftnode_fully_in_memory(node));
}

uint32_t toku_ftnode_leaf_num_entries(FTNODE node) {
    toku_ftnode_assert_fully_in_memory(node);
    uint32_t num_entries = 0;
    for (int i = 0; i < node->n_children; i++) {
        num_entries += BLB_DATA(node, i)->num_klpairs();
    }
    return num_entries;
}

enum reactivity toku_ftnode_get_leaf_reactivity(FTNODE node, uint32_t nodesize) {
    enum reactivity re = RE_STABLE;
    toku_ftnode_assert_fully_in_memory(node);
    paranoid_invariant(node->height==0);
    unsigned int size = toku_serialize_ftnode_size(node);
    if (size > nodesize && toku_ftnode_leaf_num_entries(node) > 1) {
        re = RE_FISSIBLE;
    } else if ((size*4) < nodesize && !BLB_SEQINSERT(node, node->n_children-1)) {
        re = RE_FUSIBLE;
    }
    return re;
}

enum reactivity toku_ftnode_get_nonleaf_reactivity(FTNODE node, unsigned int fanout) {
    paranoid_invariant(node->height > 0);
    int n_children = node->n_children;
    if (n_children > (int) fanout) {
        return RE_FISSIBLE;
    }
    if (n_children * 4 < (int) fanout) {
        return RE_FUSIBLE;
    }
    return RE_STABLE;
}

enum reactivity toku_ftnode_get_reactivity(FT ft, FTNODE node) {
    toku_ftnode_assert_fully_in_memory(node);
    if (node->height == 0) {
        return toku_ftnode_get_leaf_reactivity(node, ft->h->nodesize);
    } else {
        return toku_ftnode_get_nonleaf_reactivity(node, ft->h->fanout);
    }
}

unsigned int toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc) {
    return bnc->msg_buffer.buffer_size_in_use();
}

// Return true if the size of the buffers plus the amount of work done is large enough.
// Return false if there is nothing to be flushed (the buffers empty).
bool toku_ftnode_nonleaf_is_gorged(FTNODE node, uint32_t nodesize) {
    uint64_t size = toku_serialize_ftnode_size(node);

    bool buffers_are_empty = true;
    toku_ftnode_assert_fully_in_memory(node);
    //
    // the nonleaf node is gorged if the following holds true:
    //  - the buffers are non-empty
    //  - the total workdone by the buffers PLUS the size of the buffers
    //     is greater than nodesize (which as of Maxwell should be
    //     4MB)
    //
    paranoid_invariant(node->height > 0);
    for (int child = 0; child < node->n_children; ++child) {
        size += BP_WORKDONE(node, child);
    }
    for (int child = 0; child < node->n_children; ++child) {
        if (toku_bnc_nbytesinbuf(BNC(node, child)) > 0) {
            buffers_are_empty = false;
            break;
        }
    }
    return ((size > nodesize)
            &&
            (!buffers_are_empty));
}

int toku_bnc_n_entries(NONLEAF_CHILDINFO bnc) {
    return bnc->msg_buffer.num_entries();
}

// how much memory does this child buffer consume?
long toku_bnc_memory_size(NONLEAF_CHILDINFO bnc) {
    return (sizeof(*bnc) +
            bnc->msg_buffer.memory_footprint() +
            bnc->fresh_message_tree.memory_size() +
            bnc->stale_message_tree.memory_size() +
            bnc->broadcast_list.memory_size());
}

// how much memory in this child buffer holds useful data?
// originally created solely for use by test program(s).
long toku_bnc_memory_used(NONLEAF_CHILDINFO bnc) {
    return (sizeof(*bnc) +
            bnc->msg_buffer.memory_size_in_use() +
            bnc->fresh_message_tree.memory_size() +
            bnc->stale_message_tree.memory_size() +
            bnc->broadcast_list.memory_size());
}

//
// Garbage collection
// Message injection
// Message application
//

// Used only by test programs: append a child node to a parent node
void toku_ft_nonleaf_append_child(FTNODE node, FTNODE child, const DBT *pivotkey) {
    int childnum = node->n_children;
    node->n_children++;
    REALLOC_N(node->n_children, node->bp);
    BP_BLOCKNUM(node,childnum) = child->blocknum;
    BP_STATE(node,childnum) = PT_AVAIL;
    BP_WORKDONE(node, childnum)   = 0;
    set_BNC(node, childnum, toku_create_empty_nl());
    if (pivotkey) {
        invariant(childnum > 0);
        node->pivotkeys.insert_at(pivotkey, childnum - 1);
    }
    node->dirty = 1;
}

void
toku_ft_bn_apply_msg_once (
    BASEMENTNODE bn,
    const ft_msg &msg,
    uint32_t idx,
    uint32_t le_keylen,
    LEAFENTRY le,
    txn_gc_info *gc_info,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    )
// Effect: Apply msg to leafentry (msn is ignored)
//         Calculate work done by message on leafentry and add it to caller's workdone counter.
//   idx is the location where it goes
//   le is old leafentry
{
    size_t newsize=0, oldsize=0, workdone_this_le=0;
    LEAFENTRY new_le=0;
    int64_t numbytes_delta = 0;  // how many bytes of user data (not including overhead) were added or deleted from this row
    int64_t numrows_delta = 0;   // will be +1 or -1 or 0 (if row was added or deleted or not)
    uint32_t key_storage_size = msg.kdbt()->size + sizeof(uint32_t);
    if (le) {
        oldsize = leafentry_memsize(le) + key_storage_size;
    }

    // toku_le_apply_msg() may call bn_data::mempool_malloc_and_update_dmt() to allocate more space.
    // That means le is guaranteed to not cause a sigsegv but it may point to a mempool that is
    // no longer in use.  We'll have to release the old mempool later.
    toku_le_apply_msg(
        msg, 
        le,
        &bn->data_buffer,
        idx,
        le_keylen,
        gc_info, 
        &new_le, 
        &numbytes_delta
        );
    // at this point, we cannot trust cmd->u.id.key to be valid.
    // The dmt may have realloced its mempool and freed the one containing key.

    newsize = new_le ? (leafentry_memsize(new_le) +  + key_storage_size) : 0;
    if (le && new_le) {
        workdone_this_le = (oldsize > newsize ? oldsize : newsize);  // work done is max of le size before and after message application

    } else {           // we did not just replace a row, so ...
        if (le) {
            //            ... we just deleted a row ...
            workdone_this_le = oldsize;
            numrows_delta = -1;
        }
        if (new_le) {
            //            ... or we just added a row
            workdone_this_le = newsize;
            numrows_delta = 1;
        }
    }
    if (workdone) {  // test programs may call with NULL
        *workdone += workdone_this_le;
    }

    // now update stat64 statistics
    bn->stat64_delta.numrows  += numrows_delta;
    bn->stat64_delta.numbytes += numbytes_delta;
    // the only reason stats_to_update may be null is for tests
    if (stats_to_update) {
        stats_to_update->numrows += numrows_delta;
        stats_to_update->numbytes += numbytes_delta;
    }

}

static const uint32_t setval_tag = 0xee0ccb99; // this was gotten by doing "cat /dev/random|head -c4|od -x" to get a random number.  We want to make sure that the user actually passes us the setval_extra_s that we passed in.
struct setval_extra_s {
    uint32_t  tag;
    bool did_set_val;
    int         setval_r;    // any error code that setval_fun wants to return goes here.
    // need arguments for toku_ft_bn_apply_msg_once
    BASEMENTNODE bn;
    MSN msn;              // captured from original message, not currently used
    XIDS xids;
    const DBT *key;
    uint32_t idx;
    uint32_t le_keylen;
    LEAFENTRY le;
    txn_gc_info *gc_info;
    uint64_t * workdone;  // set by toku_ft_bn_apply_msg_once()
    STAT64INFO stats_to_update;
};

/*
 * If new_val == NULL, we send a delete message instead of an insert.
 * This happens here instead of in do_delete() for consistency.
 * setval_fun() is called from handlerton, passing in svextra_v
 * from setval_extra_s input arg to ft->update_fun().
 */
static void setval_fun (const DBT *new_val, void *svextra_v) {
    struct setval_extra_s *CAST_FROM_VOIDP(svextra, svextra_v);
    paranoid_invariant(svextra->tag==setval_tag);
    paranoid_invariant(!svextra->did_set_val);
    svextra->did_set_val = true;

    {
        // can't leave scope until toku_ft_bn_apply_msg_once if
        // this is a delete
        DBT val;
        ft_msg msg(svextra->key,
                   new_val ? new_val : toku_init_dbt(&val),
                   new_val ? FT_INSERT : FT_DELETE_ANY,
                   svextra->msn, svextra->xids);
        toku_ft_bn_apply_msg_once(svextra->bn, msg,
                                  svextra->idx, svextra->le_keylen, svextra->le,
                                  svextra->gc_info,
                                  svextra->workdone, svextra->stats_to_update);
        svextra->setval_r = 0;
    }
}

// We are already past the msn filter (in toku_ft_bn_apply_msg(), which calls do_update()),
// so capturing the msn in the setval_extra_s is not strictly required.         The alternative
// would be to put a dummy msn in the messages created by setval_fun(), but preserving
// the original msn seems cleaner and it preserves accountability at a lower layer.
static int do_update(ft_update_func update_fun, const DESCRIPTOR_S *desc, BASEMENTNODE bn, const ft_msg &msg, uint32_t idx,
                     LEAFENTRY le,
                     void* keydata,
                     uint32_t keylen,
                     txn_gc_info *gc_info,
                     uint64_t * workdone,
                     STAT64INFO stats_to_update) {
    LEAFENTRY le_for_update;
    DBT key;
    const DBT *keyp;
    const DBT *update_function_extra;
    DBT vdbt;
    const DBT *vdbtp;

    // the location of data depends whether this is a regular or
    // broadcast update
    if (msg.type() == FT_UPDATE) {
        // key is passed in with command (should be same as from le)
        // update function extra is passed in with command
        keyp = msg.kdbt();
        update_function_extra = msg.vdbt();
    } else {
        invariant(msg.type() == FT_UPDATE_BROADCAST_ALL);
        // key is not passed in with broadcast, it comes from le
        // update function extra is passed in with command
        paranoid_invariant(le);  // for broadcast updates, we just hit all leafentries
                     // so this cannot be null
        paranoid_invariant(keydata);
        paranoid_invariant(keylen);
        paranoid_invariant(msg.kdbt()->size == 0);
        keyp = toku_fill_dbt(&key, keydata, keylen);
        update_function_extra = msg.vdbt();
    }
    toku_ft_status_note_update(msg.type() == FT_UPDATE_BROADCAST_ALL);

    if (le && !le_latest_is_del(le)) {
        // if the latest val exists, use it, and we'll use the leafentry later
        uint32_t vallen;
        void *valp = le_latest_val_and_len(le, &vallen);
        vdbtp = toku_fill_dbt(&vdbt, valp, vallen);
    } else {
        // otherwise, the val and leafentry are both going to be null
        vdbtp = NULL;
    }
    le_for_update = le;

    struct setval_extra_s setval_extra = {setval_tag, false, 0, bn, msg.msn(), msg.xids(),
                                          keyp, idx, keylen, le_for_update, gc_info,
                                          workdone, stats_to_update};
    // call handlerton's ft->update_fun(), which passes setval_extra to setval_fun()
    FAKE_DB(db, desc);
    int r = update_fun(
        &db,
        keyp,
        vdbtp,
        update_function_extra,
        setval_fun, &setval_extra
        );

    if (r == 0) { r = setval_extra.setval_r; }
    return r;
}

// Should be renamed as something like "apply_msg_to_basement()."
void
toku_ft_bn_apply_msg (
    const toku::comparator &cmp,
    ft_update_func update_fun,
    BASEMENTNODE bn,
    const ft_msg &msg,
    txn_gc_info *gc_info, 
    uint64_t *workdone,
    STAT64INFO stats_to_update
    )
// Effect:
//   Put a msg into a leaf.
//   Calculate work done by message on leafnode and add it to caller's workdone counter.
// The leaf could end up "too big" or "too small".  The caller must fix that up.
{
    LEAFENTRY storeddata;
    void* key = NULL;
    uint32_t keylen = 0;

    uint32_t num_klpairs;
    int r;
    struct toku_msg_leafval_heaviside_extra be(cmp, msg.kdbt());

    unsigned int doing_seqinsert = bn->seqinsert;
    bn->seqinsert = 0;

    switch (msg.type()) {
    case FT_INSERT_NO_OVERWRITE:
    case FT_INSERT: {
        uint32_t idx;
        if (doing_seqinsert) {
            idx = bn->data_buffer.num_klpairs();
            DBT kdbt;
            r = bn->data_buffer.fetch_key_and_len(idx-1, &kdbt.size, &kdbt.data);
            if (r != 0) goto fz;
            int c = toku_msg_leafval_heaviside(kdbt, be);
            if (c >= 0) goto fz;
            r = DB_NOTFOUND;
        } else {
        fz:
            r = bn->data_buffer.find_zero<decltype(be), toku_msg_leafval_heaviside>(
                be,
                &storeddata,
                &key,
                &keylen,
                &idx
                );
        }
        if (r==DB_NOTFOUND) {
            storeddata = 0;
        } else {
            assert_zero(r);
        }
        toku_ft_bn_apply_msg_once(bn, msg, idx, keylen, storeddata, gc_info, workdone, stats_to_update);

        // if the insertion point is within a window of the right edge of
        // the leaf then it is sequential
        // window = min(32, number of leaf entries/16)
        {
            uint32_t s = bn->data_buffer.num_klpairs();
            uint32_t w = s / 16;
            if (w == 0) w = 1;
            if (w > 32) w = 32;

            // within the window?
            if (s - idx <= w)
                bn->seqinsert = doing_seqinsert + 1;
        }
        break;
    }
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY: {
        uint32_t idx;
        // Apply to all the matches

        r = bn->data_buffer.find_zero<decltype(be), toku_msg_leafval_heaviside>(
            be,
            &storeddata,
            &key,
            &keylen,
            &idx
            );
        if (r == DB_NOTFOUND) break;
        assert_zero(r);
        toku_ft_bn_apply_msg_once(bn, msg, idx, keylen, storeddata, gc_info, workdone, stats_to_update);

        break;
    }
    case FT_OPTIMIZE_FOR_UPGRADE:
        // fall through so that optimize_for_upgrade performs rest of the optimize logic
    case FT_COMMIT_BROADCAST_ALL:
    case FT_OPTIMIZE:
        // Apply to all leafentries
        num_klpairs = bn->data_buffer.num_klpairs();
        for (uint32_t idx = 0; idx < num_klpairs; ) {
            void* curr_keyp = NULL;
            uint32_t curr_keylen = 0;
            r = bn->data_buffer.fetch_klpair(idx, &storeddata, &curr_keylen, &curr_keyp);
            assert_zero(r);
            int deleted = 0;
            if (!le_is_clean(storeddata)) { //If already clean, nothing to do.
                // message application code needs a key in order to determine how much
                // work was done by this message. since this is a broadcast message,
                // we have to create a new message whose key is the current le's key.
                DBT curr_keydbt;
                ft_msg curr_msg(toku_fill_dbt(&curr_keydbt, curr_keyp, curr_keylen),
                                msg.vdbt(), msg.type(), msg.msn(), msg.xids());
                toku_ft_bn_apply_msg_once(bn, curr_msg, idx, curr_keylen, storeddata, gc_info, workdone, stats_to_update);
                // at this point, we cannot trust msg.kdbt to be valid.
                uint32_t new_dmt_size = bn->data_buffer.num_klpairs();
                if (new_dmt_size != num_klpairs) {
                    paranoid_invariant(new_dmt_size + 1 == num_klpairs);
                    //Item was deleted.
                    deleted = 1;
                }
            }
            if (deleted)
                num_klpairs--;
            else
                idx++;
        }
        paranoid_invariant(bn->data_buffer.num_klpairs() == num_klpairs);

        break;
    case FT_COMMIT_BROADCAST_TXN:
    case FT_ABORT_BROADCAST_TXN:
        // Apply to all leafentries if txn is represented
        num_klpairs = bn->data_buffer.num_klpairs();
        for (uint32_t idx = 0; idx < num_klpairs; ) {
            void* curr_keyp = NULL;
            uint32_t curr_keylen = 0;
            r = bn->data_buffer.fetch_klpair(idx, &storeddata, &curr_keylen, &curr_keyp);
            assert_zero(r);
            int deleted = 0;
            if (le_has_xids(storeddata, msg.xids())) {
                // message application code needs a key in order to determine how much
                // work was done by this message. since this is a broadcast message,
                // we have to create a new message whose key is the current le's key.
                DBT curr_keydbt;
                ft_msg curr_msg(toku_fill_dbt(&curr_keydbt, curr_keyp, curr_keylen),
                                msg.vdbt(), msg.type(), msg.msn(), msg.xids());
                toku_ft_bn_apply_msg_once(bn, curr_msg, idx, curr_keylen, storeddata, gc_info, workdone, stats_to_update);
                uint32_t new_dmt_size = bn->data_buffer.num_klpairs();
                if (new_dmt_size != num_klpairs) {
                    paranoid_invariant(new_dmt_size + 1 == num_klpairs);
                    //Item was deleted.
                    deleted = 1;
                }
            }
            if (deleted)
                num_klpairs--;
            else
                idx++;
        }
        paranoid_invariant(bn->data_buffer.num_klpairs() == num_klpairs);

        break;
    case FT_UPDATE: {
        uint32_t idx;
        r = bn->data_buffer.find_zero<decltype(be), toku_msg_leafval_heaviside>(
            be,
            &storeddata,
            &key,
            &keylen,
            &idx
            );
        if (r==DB_NOTFOUND) {
            {
                //Point to msg's copy of the key so we don't worry about le being freed
                //TODO: 46 MAYBE Get rid of this when le_apply message memory is better handled
                key = msg.kdbt()->data;
                keylen = msg.kdbt()->size;
            }
            r = do_update(update_fun, cmp.get_descriptor(), bn, msg, idx, NULL, NULL, 0, gc_info, workdone, stats_to_update);
        } else if (r==0) {
            r = do_update(update_fun, cmp.get_descriptor(), bn, msg, idx, storeddata, key, keylen, gc_info, workdone, stats_to_update);
        } // otherwise, a worse error, just return it
        break;
    }
    case FT_UPDATE_BROADCAST_ALL: {
        // apply to all leafentries.
        uint32_t idx = 0;
        uint32_t num_leafentries_before;
        while (idx < (num_leafentries_before = bn->data_buffer.num_klpairs())) {
            void* curr_key = nullptr;
            uint32_t curr_keylen = 0;
            r = bn->data_buffer.fetch_klpair(idx, &storeddata, &curr_keylen, &curr_key);
            assert_zero(r);

            //TODO: 46 replace this with something better than cloning key
            // TODO: (Zardosht) This may be unnecessary now, due to how the key
            // is handled in the bndata. Investigate and determine
            char clone_mem[curr_keylen];  // only lasts one loop, alloca would overflow (end of function)
            memcpy((void*)clone_mem, curr_key, curr_keylen);
            curr_key = (void*)clone_mem;

            // This is broken below. Have a compilation error checked
            // in as a reminder
            r = do_update(update_fun, cmp.get_descriptor(), bn, msg, idx, storeddata, curr_key, curr_keylen, gc_info, workdone, stats_to_update);
            assert_zero(r);

            if (num_leafentries_before == bn->data_buffer.num_klpairs()) {
                // we didn't delete something, so increment the index.
                idx++;
            }
        }
        break;
    }
    case FT_NONE: break; // don't do anything
    }

    return;
}

static inline int
key_msn_cmp(const DBT *a, const DBT *b, const MSN amsn, const MSN bmsn, const toku::comparator &cmp) {
    int r = cmp(a, b);
    if (r == 0) {
        if (amsn.msn > bmsn.msn) {
            r = +1;
        } else if (amsn.msn < bmsn.msn) {
            r = -1;
        } else {
            r = 0;
        }
    }
    return r;
}

int toku_msg_buffer_key_msn_heaviside(const int32_t &offset, const struct toku_msg_buffer_key_msn_heaviside_extra &extra) {
    MSN query_msn;
    DBT query_key;
    extra.msg_buffer->get_message_key_msn(offset, &query_key, &query_msn);
    return key_msn_cmp(&query_key, extra.key, query_msn, extra.msn, extra.cmp);
}

int toku_msg_buffer_key_msn_cmp(const struct toku_msg_buffer_key_msn_cmp_extra &extra, const int32_t &ao, const int32_t &bo) {
    MSN amsn, bmsn;
    DBT akey, bkey;
    extra.msg_buffer->get_message_key_msn(ao, &akey, &amsn);
    extra.msg_buffer->get_message_key_msn(bo, &bkey, &bmsn);
    return key_msn_cmp(&akey, &bkey, amsn, bmsn, extra.cmp);
}

// Effect: Enqueue the message represented by the parameters into the
//   bnc's buffer, and put it in either the fresh or stale message tree,
//   or the broadcast list.
static void bnc_insert_msg(NONLEAF_CHILDINFO bnc, const ft_msg &msg, bool is_fresh, const toku::comparator &cmp) {
    int r = 0;
    int32_t offset;
    bnc->msg_buffer.enqueue(msg, is_fresh, &offset);
    enum ft_msg_type type = msg.type();
    if (ft_msg_type_applies_once(type)) {
        DBT key;
        toku_fill_dbt(&key, msg.kdbt()->data, msg.kdbt()->size);
        struct toku_msg_buffer_key_msn_heaviside_extra extra(cmp, &bnc->msg_buffer, &key, msg.msn());
        if (is_fresh) {
            r = bnc->fresh_message_tree.insert<struct toku_msg_buffer_key_msn_heaviside_extra, toku_msg_buffer_key_msn_heaviside>(offset, extra, nullptr);
            assert_zero(r);
        } else {
            r = bnc->stale_message_tree.insert<struct toku_msg_buffer_key_msn_heaviside_extra, toku_msg_buffer_key_msn_heaviside>(offset, extra, nullptr);
            assert_zero(r);
        }
    } else {
        invariant(ft_msg_type_applies_all(type) || ft_msg_type_does_nothing(type));
        const uint32_t idx = bnc->broadcast_list.size();
        r = bnc->broadcast_list.insert_at(offset, idx);
        assert_zero(r);
    }
}

// This is only exported for tests.
void toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, uint32_t keylen, const void *data, uint32_t datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const toku::comparator &cmp)
{
    DBT k, v;
    ft_msg msg(toku_fill_dbt(&k, key, keylen), toku_fill_dbt(&v, data, datalen), type, msn, xids);
    bnc_insert_msg(bnc, msg, is_fresh, cmp);
}

// append a msg to a nonleaf node's child buffer
static void ft_append_msg_to_child_buffer(const toku::comparator &cmp, FTNODE node,
                                          int childnum, const ft_msg &msg, bool is_fresh) {
    paranoid_invariant(BP_STATE(node,childnum) == PT_AVAIL);
    bnc_insert_msg(BNC(node, childnum), msg, is_fresh, cmp);
    node->dirty = 1;
}

// This is only exported for tests.
void toku_ft_append_to_child_buffer(const toku::comparator &cmp, FTNODE node, int childnum, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val) {
    ft_msg msg(key, val, type, msn, xids);
    ft_append_msg_to_child_buffer(cmp, node, childnum, msg, is_fresh);
}

static void ft_nonleaf_msg_once_to_child(const toku::comparator &cmp, FTNODE node, int target_childnum, const ft_msg &msg, bool is_fresh, size_t flow_deltas[])
// Previously we had passive aggressive promotion, but that causes a lot of I/O a the checkpoint.  So now we are just putting it in the buffer here.
// Also we don't worry about the node getting overfull here.  It's the caller's problem.
{
    unsigned int childnum = (target_childnum >= 0
                             ? target_childnum
                             : toku_ftnode_which_child(node, msg.kdbt(), cmp));
    ft_append_msg_to_child_buffer(cmp, node, childnum, msg, is_fresh);
    NONLEAF_CHILDINFO bnc = BNC(node, childnum);
    bnc->flow[0] += flow_deltas[0];
    bnc->flow[1] += flow_deltas[1];
}

// TODO: Remove me, I'm boring.
static int ft_compare_pivot(const toku::comparator &cmp, const DBT *key, const DBT *pivot) {
    return cmp(key, pivot);
}

/* Find the leftmost child that may contain the key.
 * If the key exists it will be in the child whose number
 * is the return value of this function.
 */
int toku_ftnode_which_child(FTNODE node, const DBT *k, const toku::comparator &cmp) {
    // a funny case of no pivots
    if (node->n_children <= 1) return 0;

    DBT pivot;

    // check the last key to optimize seq insertions
    int n = node->n_children-1;
    int c = ft_compare_pivot(cmp, k, node->pivotkeys.fill_pivot(n - 1, &pivot));
    if (c > 0) return n;

    // binary search the pivots
    int lo = 0;
    int hi = n-1; // skip the last one, we checked it above
    int mi;
    while (lo < hi) {
        mi = (lo + hi) / 2;
        c = ft_compare_pivot(cmp, k, node->pivotkeys.fill_pivot(mi, &pivot));
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
}

// Used for HOT.
int toku_ftnode_hot_next_child(FTNODE node, const DBT *k, const toku::comparator &cmp) {
    DBT pivot;
    int low = 0;
    int hi = node->n_children - 1;
    int mi;
    while (low < hi) {
        mi = (low + hi) / 2;
        int r = ft_compare_pivot(cmp, k, node->pivotkeys.fill_pivot(mi, &pivot));
        if (r > 0) {
            low = mi + 1;
        } else if (r < 0) {
            hi = mi;
        } else {
            // if they were exactly equal, then we want the sub-tree under
            // the next pivot.
            return mi + 1;
        }
    }
    invariant(low == hi);
    return low;
}

void toku_ftnode_save_ct_pair(CACHEKEY UU(key), void *value_data, PAIR p) {
    FTNODE CAST_FROM_VOIDP(node, value_data);
    node->ct_pair = p;
}

static void
ft_nonleaf_msg_all(const toku::comparator &cmp, FTNODE node, const ft_msg &msg, bool is_fresh, size_t flow_deltas[])
// Effect: Put the message into a nonleaf node.  We put it into all children, possibly causing the children to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.         (And there may be several such children.)
{
    for (int i = 0; i < node->n_children; i++) {
        ft_nonleaf_msg_once_to_child(cmp, node, i, msg, is_fresh, flow_deltas);
    }
}

static void
ft_nonleaf_put_msg(const toku::comparator &cmp, FTNODE node, int target_childnum, const ft_msg &msg, bool is_fresh, size_t flow_deltas[])
// Effect: Put the message into a nonleaf node.  We may put it into a child, possibly causing the child to become reactive.
//  We don't do the splitting and merging.  That's up to the caller after doing all the puts it wants to do.
//  The re_array[i] gets set to the reactivity of any modified child i.         (And there may be several such children.)
//
{

    //
    // see comments in toku_ft_leaf_apply_msg
    // to understand why we handle setting
    // node->max_msn_applied_to_node_on_disk here,
    // and don't do it in toku_ftnode_put_msg
    //
    MSN msg_msn = msg.msn();
    invariant(msg_msn.msn > node->max_msn_applied_to_node_on_disk.msn);
    node->max_msn_applied_to_node_on_disk = msg_msn;

    if (ft_msg_type_applies_once(msg.type())) {
        ft_nonleaf_msg_once_to_child(cmp, node, target_childnum, msg, is_fresh, flow_deltas);
    } else if (ft_msg_type_applies_all(msg.type())) {
        ft_nonleaf_msg_all(cmp, node, msg, is_fresh, flow_deltas);
    } else {
        paranoid_invariant(ft_msg_type_does_nothing(msg.type()));
    }
}

// Garbage collect one leaf entry.
static void
ft_basement_node_gc_once(BASEMENTNODE bn,
                          uint32_t index,
                          void* keyp,
                          uint32_t keylen,
                          LEAFENTRY leaf_entry,
                          txn_gc_info *gc_info,
                          STAT64INFO_S * delta)
{
    paranoid_invariant(leaf_entry);

    // Don't run garbage collection on non-mvcc leaf entries.
    if (leaf_entry->type != LE_MVCC) {
        goto exit;
    }

    // Don't run garbage collection if this leafentry decides it's not worth it.
    if (!toku_le_worth_running_garbage_collection(leaf_entry, gc_info)) {
        goto exit;
    }

    LEAFENTRY new_leaf_entry;
    new_leaf_entry = NULL;

    // The mempool doesn't free itself.  When it allocates new memory,
    // this pointer will be set to the older memory that must now be
    // freed.
    void * maybe_free;
    maybe_free = NULL;

    // These will represent the number of bytes and rows changed as
    // part of the garbage collection.
    int64_t numbytes_delta;
    int64_t numrows_delta;
    toku_le_garbage_collect(leaf_entry,
                            &bn->data_buffer,
                            index,
                            keyp,
                            keylen,
                            gc_info,
                            &new_leaf_entry,
                            &numbytes_delta);

    numrows_delta = 0;
    if (new_leaf_entry) {
        numrows_delta = 0;
    } else {
        numrows_delta = -1;
    }

    // If we created a new mempool buffer we must free the
    // old/original buffer.
    if (maybe_free) {
        toku_free(maybe_free);
    }

    // Update stats.
    bn->stat64_delta.numrows += numrows_delta;
    bn->stat64_delta.numbytes += numbytes_delta;
    delta->numrows += numrows_delta;
    delta->numbytes += numbytes_delta;

exit:
    return;
}

// Garbage collect all leaf entries for a given basement node.
static void
basement_node_gc_all_les(BASEMENTNODE bn,
                         txn_gc_info *gc_info,
                         STAT64INFO_S * delta)
{
    int r = 0;
    uint32_t index = 0;
    uint32_t num_leafentries_before;
    while (index < (num_leafentries_before = bn->data_buffer.num_klpairs())) {
        void* keyp = NULL;
        uint32_t keylen = 0;
        LEAFENTRY leaf_entry;
        r = bn->data_buffer.fetch_klpair(index, &leaf_entry, &keylen, &keyp);
        assert_zero(r);
        ft_basement_node_gc_once(
            bn,
            index,
            keyp,
            keylen,
            leaf_entry,
            gc_info,
            delta
            );
        // Check if the leaf entry was deleted or not.
        if (num_leafentries_before == bn->data_buffer.num_klpairs()) {
            ++index;
        }
    }
}

// Garbage collect all leaf entires in all basement nodes.
static void
ft_leaf_gc_all_les(FT ft, FTNODE node, txn_gc_info *gc_info)
{
    toku_ftnode_assert_fully_in_memory(node);
    paranoid_invariant_zero(node->height);
    // Loop through each leaf entry, garbage collecting as we go.
    for (int i = 0; i < node->n_children; ++i) {
        // Perform the garbage collection.
        BASEMENTNODE bn = BLB(node, i);
        STAT64INFO_S delta;
        delta.numrows = 0;
        delta.numbytes = 0;
        basement_node_gc_all_les(bn, gc_info, &delta);
        toku_ft_update_stats(&ft->in_memory_stats, delta);
    }
}

void toku_ftnode_leaf_run_gc(FT ft, FTNODE node) {
    TOKULOGGER logger = toku_cachefile_logger(ft->cf);
    if (logger) {
        TXN_MANAGER txn_manager = toku_logger_get_txn_manager(logger);
        txn_manager_state txn_state_for_gc(txn_manager);
        txn_state_for_gc.init();
        TXNID oldest_referenced_xid_for_simple_gc = toku_txn_manager_get_oldest_referenced_xid_estimate(txn_manager);
        
        // Perform full garbage collection.
        //
        // - txn_state_for_gc
        //     a fresh snapshot of the transaction system.
        // - oldest_referenced_xid_for_simple_gc
        //     the oldest xid in any live list as of right now - suitible for simple gc 
        // - node->oldest_referenced_xid_known
        //     the last known oldest referenced xid for this node and any unapplied messages.
        //     it is a lower bound on the actual oldest referenced xid - but becasue there
        //     may be abort messages above us, we need to be careful to only use this value
        //     for implicit promotion (as opposed to the oldest referenced xid for simple gc)
        //
        // The node has its own oldest referenced xid because it must be careful not to implicitly promote
        // provisional entries for transactions that are no longer live, but may have abort messages
        // somewhere above us in the tree.
        txn_gc_info gc_info(&txn_state_for_gc,
                            oldest_referenced_xid_for_simple_gc,
                            node->oldest_referenced_xid_known,
                            true);
        ft_leaf_gc_all_les(ft, node, &gc_info);
    }
}

void
toku_ftnode_put_msg (
    const toku::comparator &cmp,
    ft_update_func update_fun,
    FTNODE node,
    int target_childnum,
    const ft_msg &msg,
    bool is_fresh,
    txn_gc_info *gc_info,
    size_t flow_deltas[],
    STAT64INFO stats_to_update
    )
// Effect: Push message into the subtree rooted at NODE.
//   If NODE is a leaf, then
//   put message into leaf, applying it to the leafentries
//   If NODE is a nonleaf, then push the message into the message buffer(s) of the relevent child(ren).
//   The node may become overfull.  That's not our problem.
{
    toku_ftnode_assert_fully_in_memory(node);
    //
    // see comments in toku_ft_leaf_apply_msg
    // to understand why we don't handle setting
    // node->max_msn_applied_to_node_on_disk here,
    // and instead defer to these functions
    //
    if (node->height==0) {
        toku_ft_leaf_apply_msg(cmp, update_fun, node, target_childnum, msg, gc_info, nullptr, stats_to_update);
    } else {
        ft_nonleaf_put_msg(cmp, node, target_childnum, msg, is_fresh, flow_deltas);
    }
}

// Effect: applies the message to the leaf if the appropriate basement node is in memory.
//           This function is called during message injection and/or flushing, so the entire
//           node MUST be in memory.
void toku_ft_leaf_apply_msg(
    const toku::comparator &cmp,
    ft_update_func update_fun,
    FTNODE node,
    int target_childnum,  // which child to inject to, or -1 if unknown
    const ft_msg &msg,
    txn_gc_info *gc_info,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    )
{
    VERIFY_NODE(t, node);
    toku_ftnode_assert_fully_in_memory(node);

    //
    // Because toku_ft_leaf_apply_msg is called with the intent of permanently
    // applying a message to a leaf node (meaning the message is permanently applied
    // and will be purged from the system after this call, as opposed to
    // toku_apply_ancestors_messages_to_node, which applies a message
    // for a query, but the message may still reside in the system and
    // be reapplied later), we mark the node as dirty and
    // take the opportunity to update node->max_msn_applied_to_node_on_disk.
    //
    node->dirty = 1;

    //
    // we cannot blindly update node->max_msn_applied_to_node_on_disk,
    // we must check to see if the msn is greater that the one already stored,
    // because the message may have already been applied earlier (via
    // toku_apply_ancestors_messages_to_node) to answer a query
    //
    // This is why we handle node->max_msn_applied_to_node_on_disk both here
    // and in ft_nonleaf_put_msg, as opposed to in one location, toku_ftnode_put_msg.
    //
    MSN msg_msn = msg.msn();
    if (msg_msn.msn > node->max_msn_applied_to_node_on_disk.msn) {
        node->max_msn_applied_to_node_on_disk = msg_msn;
    }

    if (ft_msg_type_applies_once(msg.type())) {
        unsigned int childnum = (target_childnum >= 0
                                 ? target_childnum
                                 : toku_ftnode_which_child(node, msg.kdbt(), cmp));
        BASEMENTNODE bn = BLB(node, childnum);
        if (msg.msn().msn > bn->max_msn_applied.msn) {
            bn->max_msn_applied = msg.msn();
            toku_ft_bn_apply_msg(cmp,
                                 update_fun,
                                 bn,
                                 msg,
                                 gc_info,
                                 workdone,
                                 stats_to_update);
        } else {
            toku_ft_status_note_msn_discard();
        }
    }
    else if (ft_msg_type_applies_all(msg.type())) {
        for (int childnum=0; childnum<node->n_children; childnum++) {
            if (msg.msn().msn > BLB(node, childnum)->max_msn_applied.msn) {
                BLB(node, childnum)->max_msn_applied = msg.msn();
                toku_ft_bn_apply_msg(cmp,
                                     update_fun,
                                     BLB(node, childnum),
                                     msg,
                                     gc_info,
                                     workdone,
                                     stats_to_update);
            } else {
                toku_ft_status_note_msn_discard();
            }
        }
    }
    else if (!ft_msg_type_does_nothing(msg.type())) {
        invariant(ft_msg_type_does_nothing(msg.type()));
    }
    VERIFY_NODE(t, node);
}

