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

/* Verify a BRT. */
/* Check:
 *   The tree is of uniform depth (and the height is correct at every node)
 *   For each pivot key:  the max of the stuff to the left is <= the pivot key < the min of the stuff to the right.
 *   For each leaf node:  All the keys are in strictly increasing order.
 *   For each nonleaf node:  All the messages have keys that are between the associated pivot keys ( left_pivot_key < message <= right_pivot_key)
 */

#include "ft-cachetable-wrappers.h"
#include "ft-internal.h"
#include "ft.h"

static int 
compare_pairs (FT_HANDLE brt, const DBT *a, const DBT *b) {
    FAKE_DB(db, &brt->ft->cmp_descriptor);
    int cmp = brt->ft->compare_fun(&db, a, b);
    return cmp;
}

static int 
compare_pair_to_key (FT_HANDLE brt, const DBT *a, bytevec key, ITEMLEN keylen) {
    DBT y;
    FAKE_DB(db, &brt->ft->cmp_descriptor);
    int cmp = brt->ft->compare_fun(&db, a, toku_fill_dbt(&y, key, keylen));
    return cmp;
}

static int
verify_msg_in_child_buffer(FT_HANDLE brt, enum ft_msg_type type, MSN msn, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), const DBT *lesser_pivot, const DBT *greatereq_pivot)
    __attribute__((warn_unused_result));

static int
verify_msg_in_child_buffer(FT_HANDLE brt, enum ft_msg_type type, MSN msn, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), const DBT *lesser_pivot, const DBT *greatereq_pivot) {
    int result = 0;
    if (msn.msn == ZERO_MSN.msn)
        result = EINVAL;
    switch (type) {
    default:
        break;
    case FT_INSERT:
    case FT_INSERT_NO_OVERWRITE:
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY:
        // verify key in bounds
        if (lesser_pivot) {
            int compare = compare_pair_to_key(brt, lesser_pivot, key, keylen);
            if (compare >= 0)
                result = EINVAL;
        }
        if (result == 0 && greatereq_pivot) {
            int compare = compare_pair_to_key(brt, greatereq_pivot, key, keylen);
            if (compare < 0)
                result = EINVAL;
        }
        break;
    }
    return result;
}

static DBT
get_ith_key_dbt (BASEMENTNODE bn, int i) {
    DBT kdbt;
    int r = bn->data_buffer.fetch_le_key_and_len(i, &kdbt.size, &kdbt.data);
    invariant_zero(r); // this is a bad failure if it happens.
    return kdbt;
}

#define VERIFY_ASSERTION(predicate, i, string) ({                                                                              \
    if(!(predicate)) {                                                                                                         \
        if (verbose) {                                                                                                         \
            fprintf(stderr, "%s:%d: Looking at child %d of block %" PRId64 ": %s\n", __FILE__, __LINE__, i, blocknum.b, string); \
        }                                                                                                                      \
        result = TOKUDB_NEEDS_REPAIR;                                                                                          \
        if (!keep_going_on_failure) goto done;                                                                                 \
    }})

struct count_msgs_extra {
    int count;
    MSN msn;
    FIFO fifo;
};

// template-only function, but must be extern
int count_msgs(const int32_t &offset, const uint32_t UU(idx), struct count_msgs_extra *const e)
    __attribute__((nonnull(3)));
int count_msgs(const int32_t &offset, const uint32_t UU(idx), struct count_msgs_extra *const e)
{
    const struct fifo_entry *entry = toku_fifo_get_entry(e->fifo, offset);
    if (entry->msn.msn == e->msn.msn) {
        e->count++;
    }
    return 0;
}

struct verify_message_tree_extra {
    FIFO fifo;
    bool broadcast;
    bool is_fresh;
    int i;
    int verbose;
    BLOCKNUM blocknum;
    int keep_going_on_failure;
    bool messages_have_been_moved;
};

int verify_message_tree(const int32_t &offset, const uint32_t UU(idx), struct verify_message_tree_extra *const e) __attribute__((nonnull(3)));
int verify_message_tree(const int32_t &offset, const uint32_t UU(idx), struct verify_message_tree_extra *const e)
{
    int verbose = e->verbose;
    BLOCKNUM blocknum = e->blocknum;
    int keep_going_on_failure = e->keep_going_on_failure;
    int result = 0;
    const struct fifo_entry *entry = toku_fifo_get_entry(e->fifo, offset);
    if (e->broadcast) {
        VERIFY_ASSERTION(ft_msg_type_applies_all((enum ft_msg_type) entry->type) || ft_msg_type_does_nothing((enum ft_msg_type) entry->type),
                         e->i, "message found in broadcast list that is not a broadcast");
    } else {
        VERIFY_ASSERTION(ft_msg_type_applies_once((enum ft_msg_type) entry->type),
                         e->i, "message found in fresh or stale message tree that does not apply once");
        if (e->is_fresh) {
            if (e->messages_have_been_moved) {
                VERIFY_ASSERTION(entry->is_fresh,
                                 e->i, "message found in fresh message tree that is not fresh");
            }
        } else {
            VERIFY_ASSERTION(!entry->is_fresh,
                             e->i, "message found in stale message tree that is fresh");
        }
    }
done:
    return result;
}

int error_on_iter(const int32_t &UU(offset), const uint32_t UU(idx), void *UU(e));
int error_on_iter(const int32_t &UU(offset), const uint32_t UU(idx), void *UU(e)) {
    return TOKUDB_NEEDS_REPAIR;
}

int verify_marked_messages(const int32_t &offset, const uint32_t UU(idx), struct verify_message_tree_extra *const e) __attribute__((nonnull(3)));
int verify_marked_messages(const int32_t &offset, const uint32_t UU(idx), struct verify_message_tree_extra *const e)
{
    int verbose = e->verbose;
    BLOCKNUM blocknum = e->blocknum;
    int keep_going_on_failure = e->keep_going_on_failure;
    int result = 0;
    const struct fifo_entry *entry = toku_fifo_get_entry(e->fifo, offset);
    VERIFY_ASSERTION(!entry->is_fresh, e->i, "marked message found in the fresh message tree that is fresh");
 done:
    return result;
}

template<typename verify_omt_t>
static int
verify_sorted_by_key_msn(FT_HANDLE brt, FIFO fifo, const verify_omt_t &mt) {
    int result = 0;
    size_t last_offset = 0;
    for (uint32_t i = 0; i < mt.size(); i++) {
        int32_t offset;
        int r = mt.fetch(i, &offset);
        assert_zero(r);
        if (i > 0) {
            struct toku_fifo_entry_key_msn_cmp_extra extra;
            ZERO_STRUCT(extra);
            extra.desc = &brt->ft->cmp_descriptor;
            extra.cmp = brt->ft->compare_fun;
            extra.fifo = fifo;
            if (toku_fifo_entry_key_msn_cmp(extra, last_offset, offset) >= 0) {
                result = TOKUDB_NEEDS_REPAIR;
                break;
            }
        }
        last_offset = offset;
    }
    return result;
}

template<typename count_omt_t>
static int
count_eq_key_msn(FT_HANDLE brt, FIFO fifo, const count_omt_t &mt, const DBT *key, MSN msn) {
    struct toku_fifo_entry_key_msn_heaviside_extra extra;
    ZERO_STRUCT(extra);
    extra.desc = &brt->ft->cmp_descriptor;
    extra.cmp = brt->ft->compare_fun;
    extra.fifo = fifo;
    extra.key = key;
    extra.msn = msn;
    int r = mt.template find_zero<struct toku_fifo_entry_key_msn_heaviside_extra, toku_fifo_entry_key_msn_heaviside>(extra, nullptr, nullptr);
    int count;
    if (r == 0) {
        count = 1;
    } else {
        assert(r == DB_NOTFOUND);
        count = 0;
    }
    return count;
}

void
toku_get_node_for_verify(
    BLOCKNUM blocknum,
    FT_HANDLE brt,
    FTNODE* nodep
    )
{
    uint32_t fullhash = toku_cachetable_hash(brt->ft->cf, blocknum);
    struct ftnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->ft);
    toku_pin_ftnode_off_client_thread_and_maybe_move_messages(
        brt->ft,
        blocknum,
        fullhash,
        &bfe,
        PL_WRITE_EXPENSIVE, // may_modify_node
        0,
        NULL,
        nodep,
        false
        );
}

static int
toku_verify_ftnode_internal(FT_HANDLE brt,
                            MSN rootmsn, MSN parentmsn, bool messages_exist_above,
                            FTNODE node, int height,
                            const DBT *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                            const DBT *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                            int verbose, int keep_going_on_failure, bool messages_have_been_moved)
{
    int result=0;
    MSN   this_msn;
    BLOCKNUM blocknum = node->thisnodename;

    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    toku_assert_entire_node_in_memory(node);
    this_msn = node->max_msn_applied_to_node_on_disk;

    if (height >= 0) {
        invariant(height == node->height);   // this is a bad failure if wrong
    }
    if (node->height > 0 && messages_exist_above) {
        VERIFY_ASSERTION((parentmsn.msn >= this_msn.msn), 0, "node msn must be descending down tree, newest messages at top");
    }
    // Verify that all the pivot keys are in order.
    for (int i = 0; i < node->n_children-2; i++) {
        int compare = compare_pairs(brt, &node->childkeys[i], &node->childkeys[i+1]);
        VERIFY_ASSERTION(compare < 0, i, "Value is >= the next value");
    }
    // Verify that all the pivot keys are lesser_pivot < pivot <= greatereq_pivot
    for (int i = 0; i < node->n_children-1; i++) {
        if (lesser_pivot) {
            int compare = compare_pairs(brt, lesser_pivot, &node->childkeys[i]);
            VERIFY_ASSERTION(compare < 0, i, "Pivot is >= the lower-bound pivot");
        }
        if (greatereq_pivot) {
            int compare = compare_pairs(brt, greatereq_pivot, &node->childkeys[i]);
            VERIFY_ASSERTION(compare >= 0, i, "Pivot is < the upper-bound pivot");
        }
    }

    for (int i = 0; i < node->n_children; i++) {
        const DBT *curr_less_pivot = (i==0) ? lesser_pivot : &node->childkeys[i-1];
        const DBT *curr_geq_pivot = (i==node->n_children-1) ? greatereq_pivot : &node->childkeys[i];
        if (node->height > 0) {
            MSN last_msn = ZERO_MSN;
            // Verify that messages in the buffers are in the right place.
            NONLEAF_CHILDINFO bnc = BNC(node, i);
            VERIFY_ASSERTION(verify_sorted_by_key_msn(brt, bnc->buffer, bnc->fresh_message_tree) == 0, i, "fresh_message_tree");
            VERIFY_ASSERTION(verify_sorted_by_key_msn(brt, bnc->buffer, bnc->stale_message_tree) == 0, i, "stale_message_tree");
            FIFO_ITERATE(bnc->buffer, key, keylen, data, datalen, itype, msn, xid, is_fresh,
                         ({
                             enum ft_msg_type type = (enum ft_msg_type) itype;
                             int r = verify_msg_in_child_buffer(brt, type, msn, key, keylen, data, datalen, xid,
                                                                curr_less_pivot,
                                                                curr_geq_pivot);
                             VERIFY_ASSERTION(r==0, i, "A message in the buffer is out of place");
                             VERIFY_ASSERTION((msn.msn > last_msn.msn), i, "msn per msg must be monotonically increasing toward newer messages in buffer");
                             VERIFY_ASSERTION((msn.msn <= this_msn.msn), i, "all messages must have msn within limit of this node's max_msn_applied_to_node_in_memory");
                             if (ft_msg_type_applies_once(type)) {
                                 int count;
                                 DBT keydbt;
                                 toku_fill_dbt(&keydbt, key, keylen);
                                 int total_count = 0;
                                 count = count_eq_key_msn(brt, bnc->buffer, bnc->fresh_message_tree, toku_fill_dbt(&keydbt, key, keylen), msn);
                                 total_count += count;
                                 if (is_fresh) {
                                     VERIFY_ASSERTION(count == 1, i, "a fresh message was not found in the fresh message tree");
                                 } else if (messages_have_been_moved) {
                                     VERIFY_ASSERTION(count == 0, i, "a stale message was found in the fresh message tree");
                                 }
                                 VERIFY_ASSERTION(count <= 1, i, "a message was found multiple times in the fresh message tree");
                                 count = count_eq_key_msn(brt, bnc->buffer, bnc->stale_message_tree, &keydbt, msn);

                                 total_count += count;
                                 if (is_fresh) {
                                     VERIFY_ASSERTION(count == 0, i, "a fresh message was found in the stale message tree");
                                 } else if (messages_have_been_moved) {
                                     VERIFY_ASSERTION(count == 1, i, "a stale message was not found in the stale message tree");
                                 }
                                 VERIFY_ASSERTION(count <= 1, i, "a message was found multiple times in the stale message tree");

                                 VERIFY_ASSERTION(total_count <= 1, i, "a message was found in both message trees (or more than once in a single tree)");
                                 VERIFY_ASSERTION(total_count >= 1, i, "a message was not found in either message tree");
                             } else {
                                 VERIFY_ASSERTION(ft_msg_type_applies_all(type) || ft_msg_type_does_nothing(type), i, "a message was found that does not apply either to all or to only one key");
                                 struct count_msgs_extra extra = { .count = 0, .msn = msn, .fifo = bnc->buffer };
                                 bnc->broadcast_list.iterate<struct count_msgs_extra, count_msgs>(&extra);
                                 VERIFY_ASSERTION(extra.count == 1, i, "a broadcast message was not found in the broadcast list");
                             }
                             last_msn = msn;
                         }));
            struct verify_message_tree_extra extra = { .fifo = bnc->buffer, .broadcast = false, .is_fresh = true, .i = i, .verbose = verbose, .blocknum = node->thisnodename, .keep_going_on_failure = keep_going_on_failure, .messages_have_been_moved = messages_have_been_moved };
            int r = bnc->fresh_message_tree.iterate<struct verify_message_tree_extra, verify_message_tree>(&extra);
            if (r != 0) { result = r; goto done; }
            extra.is_fresh = false;
            r = bnc->stale_message_tree.iterate<struct verify_message_tree_extra, verify_message_tree>(&extra);
            if (r != 0) { result = r; goto done; }

            bnc->fresh_message_tree.verify_marks_consistent();
            if (messages_have_been_moved) {
                VERIFY_ASSERTION(!bnc->fresh_message_tree.has_marks(), i, "fresh message tree still has marks after moving messages");
                r = bnc->fresh_message_tree.iterate_over_marked<void, error_on_iter>(nullptr);
                if (r != 0) { result = r; goto done; }
            }
            else {
                r = bnc->fresh_message_tree.iterate_over_marked<struct verify_message_tree_extra, verify_marked_messages>(&extra);
                if (r != 0) { result = r; goto done; }
            }

            extra.broadcast = true;
            r = bnc->broadcast_list.iterate<struct verify_message_tree_extra, verify_message_tree>(&extra);
            if (r != 0) { result = r; goto done; }
        }
        else {
            BASEMENTNODE bn = BLB(node, i);
            for (uint32_t j = 0; j < bn->data_buffer.omt_size(); j++) {
                VERIFY_ASSERTION((rootmsn.msn >= this_msn.msn), 0, "leaf may have latest msn, but cannot be greater than root msn");
                DBT kdbt = get_ith_key_dbt(bn, j);
                if (curr_less_pivot) {
                    int compare = compare_pairs(brt, curr_less_pivot, &kdbt);
                    VERIFY_ASSERTION(compare < 0, j, "The leafentry is >= the lower-bound pivot");
                }
                if (curr_geq_pivot) {
                    int compare = compare_pairs(brt, curr_geq_pivot, &kdbt);
                    VERIFY_ASSERTION(compare >= 0, j, "The leafentry is < the upper-bound pivot");
                }
                if (0 < j) {
                    DBT prev_key_dbt = get_ith_key_dbt(bn, j-1);
                    int compare = compare_pairs(brt, &prev_key_dbt, &kdbt);
                    VERIFY_ASSERTION(compare < 0, j, "Adjacent leafentries are out of order");
                }
            }
        }
    }

done:
    return result;
}


// input is a pinned node, on exit, node is unpinned
int
toku_verify_ftnode (FT_HANDLE brt,
                    MSN rootmsn, MSN parentmsn, bool messages_exist_above,
                     FTNODE node, int height,
                     const DBT *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     const DBT *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     int (*progress_callback)(void *extra, float progress), void *progress_extra,
                     int recurse, int verbose, int keep_going_on_failure)
{
    MSN   this_msn;

    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    toku_assert_entire_node_in_memory(node);
    this_msn = node->max_msn_applied_to_node_on_disk;

    int result = 0;
    int result2 = 0;
    if (node->height > 0) {
        // Otherwise we'll just do the next call

        result = toku_verify_ftnode_internal(
                brt, rootmsn, parentmsn, messages_exist_above, node, height, lesser_pivot, greatereq_pivot,
                verbose, keep_going_on_failure, false);
        if (result != 0 && (!keep_going_on_failure || result != TOKUDB_NEEDS_REPAIR)) goto done;
    }
    if (node->height > 0) {
        toku_move_ftnode_messages_to_stale(brt->ft, node);
    }
    result2 = toku_verify_ftnode_internal(
            brt, rootmsn, parentmsn, messages_exist_above, node, height, lesser_pivot, greatereq_pivot,
            verbose, keep_going_on_failure, true);
    if (result == 0) {
        result = result2;
        if (result != 0 && (!keep_going_on_failure || result != TOKUDB_NEEDS_REPAIR)) goto done;
    }
    
    // Verify that the subtrees have the right properties.
    if (recurse && node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            FTNODE child_node;
            toku_get_node_for_verify(BP_BLOCKNUM(node, i), brt, &child_node);
            int r = toku_verify_ftnode(brt, rootmsn, this_msn, messages_exist_above || toku_bnc_n_entries(BNC(node, i)) > 0,
                                        child_node, node->height-1,
                                        (i==0)                  ? lesser_pivot        : &node->childkeys[i-1],
                                        (i==node->n_children-1) ? greatereq_pivot     : &node->childkeys[i],
                                        progress_callback, progress_extra,
                                        recurse, verbose, keep_going_on_failure);
            if (r) {
                result = r;
                if (!keep_going_on_failure || result != TOKUDB_NEEDS_REPAIR) goto done;
            }
        }
    }
done:
    toku_unpin_ftnode(brt->ft, node);
    
    if (result == 0 && progress_callback) 
    result = progress_callback(progress_extra, 0.0);
    
    return result;
}

int 
toku_verify_ft_with_progress (FT_HANDLE brt, int (*progress_callback)(void *extra, float progress), void *progress_extra, int verbose, int keep_on_going) {
    assert(brt->ft);
    FTNODE root_node = NULL;
    {
        uint32_t root_hash;
        CACHEKEY root_key;
        toku_calculate_root_offset_pointer(brt->ft, &root_key, &root_hash);
        toku_get_node_for_verify(root_key, brt, &root_node);
    }
    int r = toku_verify_ftnode(brt, brt->ft->h->max_msn_in_ft, brt->ft->h->max_msn_in_ft, false, root_node, -1, NULL, NULL, progress_callback, progress_extra, 1, verbose, keep_on_going);
    if (r == 0) {
        toku_ft_lock(brt->ft);
        brt->ft->h->time_of_last_verification = time(NULL);
        brt->ft->h->dirty = 1;
        toku_ft_unlock(brt->ft);
    }
    return r;
}

int 
toku_verify_ft (FT_HANDLE brt) {
    return toku_verify_ft_with_progress(brt, NULL, NULL, 0, 0);
}
