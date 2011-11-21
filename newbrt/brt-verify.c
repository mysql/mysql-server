/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/* Verify a BRT. */
/* Check:
 *   The tree is of uniform depth (and the height is correct at every node)
 *   For each pivot key:  the max of the stuff to the left is <= the pivot key < the min of the stuff to the right.
 *   For each leaf node:  All the keys are in strictly increasing order.
 *   For each nonleaf node:  All the messages have keys that are between the associated pivot keys ( left_pivot_key < message <= right_pivot_key)
 */

#include "includes.h"

static int 
compare_pairs (BRT brt, struct kv_pair *a, struct kv_pair *b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
                               toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
                               toku_fill_dbt(&y, kv_pair_key(b), kv_pair_keylen(b)));
    return cmp;
}

static int 
compare_leafentries (BRT brt, LEAFENTRY a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
                               toku_fill_dbt(&x, le_key(a), le_keylen(a)),
                               toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}

static int 
compare_pair_to_leafentry (BRT brt, struct kv_pair *a, LEAFENTRY b) {
    DBT x,y;
    int cmp = brt->compare_fun(brt->db,
                               toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
                               toku_fill_dbt(&y, le_key(b), le_keylen(b)));
    return cmp;
}

static int 
compare_pair_to_key (BRT brt, struct kv_pair *a, bytevec key, ITEMLEN keylen) {
    DBT x, y;
    int cmp = brt->compare_fun(brt->db,
                               toku_fill_dbt(&x, kv_pair_key(a), kv_pair_keylen(a)),
                               toku_fill_dbt(&y, key, keylen));
    return cmp;
}

static int
verify_msg_in_child_buffer(BRT brt, int type, MSN msn, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot)
    __attribute__((warn_unused_result));

static int
verify_msg_in_child_buffer(BRT brt, int type, MSN msn, bytevec key, ITEMLEN keylen, bytevec UU(data), ITEMLEN UU(datalen), XIDS UU(xids), struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot) {
    int result = 0;
    if (msn.msn == ZERO_MSN.msn)
        result = EINVAL;
    switch (type) {
    case BRT_INSERT:
    case BRT_INSERT_NO_OVERWRITE:
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
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

static LEAFENTRY 
get_ith_leafentry (BASEMENTNODE bn, int i) {
    OMTVALUE le_v;
    int r = toku_omt_fetch(bn->buffer, i, &le_v);
    invariant(r == 0); // this is a bad failure if it happens.
    return (LEAFENTRY)le_v;
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
    DBT *key;
    MSN msn;
    FIFO fifo;
    DB *cmp_extra;
    brt_compare_func cmp;
};

static int
count_msgs(OMTVALUE v, u_int32_t UU(idx), void *ve)
{
    struct count_msgs_extra *e = ve;
    long offset = (long) v;
    const struct fifo_entry *entry = toku_fifo_get_entry(e->fifo, offset);
    DBT dbt;
    const DBT *buffer_key = fill_dbt_for_fifo_entry(&dbt, entry);
    if (entry->msn.msn == e->msn.msn && e->cmp(e->cmp_extra, e->key, buffer_key) == 0) {
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
};

static int
verify_message_tree(OMTVALUE v, u_int32_t UU(idx), void *ve)
{
    struct verify_message_tree_extra *e = ve;
    int verbose = e->verbose;
    BLOCKNUM blocknum = e->blocknum;
    int keep_going_on_failure = e->keep_going_on_failure;
    int result = 0;
    long offset = (long) v;
    const struct fifo_entry *entry = toku_fifo_get_entry(e->fifo, offset);
    if (e->broadcast) {
        VERIFY_ASSERTION(brt_msg_type_applies_all((enum brt_msg_type) entry->type) || brt_msg_type_does_nothing((enum brt_msg_type) entry->type),
                         e->i, "message found in broadcast list that is not a broadcast");
    } else {
        VERIFY_ASSERTION(brt_msg_type_applies_once((enum brt_msg_type) entry->type),
                         e->i, "message found in fresh or stale message tree that does not apply once");
        if (e->is_fresh) {
            VERIFY_ASSERTION(entry->is_fresh,
                             e->i, "message found in fresh message tree that is not fresh");
        } else {
            VERIFY_ASSERTION(!entry->is_fresh,
                             e->i, "message found in stale message tree that is fresh");
        }
    }
done:
    return result;
}

static int
verify_sorted_by_key_msn(BRT brt, FIFO fifo, OMT mt) {
    int result = 0;
    size_t last_offset = 0;
    for (u_int32_t i = 0; i < toku_omt_size(mt); i++) {
        OMTVALUE v;
        int r = toku_omt_fetch(mt, i, &v); 
        assert_zero(r);
        size_t offset = (size_t) v;
        if (i > 0) {
            struct toku_fifo_entry_key_msn_cmp_extra extra = { .desc = &brt->h->descriptor, .cmp = brt->compare_fun, .fifo = fifo };
            if (toku_fifo_entry_key_msn_cmp(&extra, &last_offset, &offset) >= 0) {
                result = TOKUDB_NEEDS_REPAIR;
                break;
            }
        }
        last_offset = offset;
    }
    return result;
}

static int
count_eq_key_msn(BRT brt, FIFO fifo, OMT mt, const void *key, size_t keylen, MSN msn) {
    struct toku_fifo_entry_key_msn_heaviside_extra extra = { 
        .desc = &brt->h->descriptor, .cmp = brt->compare_fun, .fifo = fifo, .key = key, .keylen = keylen, .msn = msn 
    };
    OMTVALUE v; u_int32_t idx;
    int r = toku_omt_find_zero(mt, toku_fifo_entry_key_msn_heaviside, &extra, &v, &idx);
    int count;
    if (r == 0) {
        count = 1;
    } else {
        assert(r == DB_NOTFOUND);
        count = 0;
    }
    return count;
}

int
toku_verify_brtnode (BRT brt,
                     MSN rootmsn, MSN parentmsn,
                     BLOCKNUM blocknum, int height,
                     struct kv_pair *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     struct kv_pair *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     int (*progress_callback)(void *extra, float progress), void *progress_extra,
                     int recurse, int verbose, int keep_going_on_failure)
{
    int result=0;
    BRTNODE node;
    void *node_v;
    MSN   this_msn;

    u_int32_t fullhash = toku_cachetable_hash(brt->cf, blocknum);
    {
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
        assert_zero(r); // this is a bad failure if it happens.
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node = node_v;
    toku_assert_entire_node_in_memory(node);
    this_msn = node->max_msn_applied_to_node_on_disk;
    if (rootmsn.msn == ZERO_MSN.msn) {
        assert(parentmsn.msn == ZERO_MSN.msn);
        rootmsn = this_msn;
        parentmsn = this_msn;
    }

    invariant(node->fullhash == fullhash);   // this is a bad failure if wrong
    if (height >= 0) {
        invariant(height == node->height);   // this is a bad failure if wrong
    }
    if (node->height > 0) {
        VERIFY_ASSERTION((parentmsn.msn >= this_msn.msn), 0, "node msn must be descending down tree, newest messages at top");
    }
    // Verify that all the pivot keys are in order.
    for (int i = 0; i < node->n_children-2; i++) {
        int compare = compare_pairs(brt, node->childkeys[i], node->childkeys[i+1]);
        VERIFY_ASSERTION(compare < 0, i, "Value is >= the next value");
    }
    // Verify that all the pivot keys are lesser_pivot < pivot <= greatereq_pivot
    for (int i = 0; i < node->n_children-1; i++) {
        if (lesser_pivot) {
            int compare = compare_pairs(brt, lesser_pivot, node->childkeys[i]);
            VERIFY_ASSERTION(compare < 0, i, "Pivot is >= the lower-bound pivot");
        }
        if (greatereq_pivot) {
            int compare = compare_pairs(brt, greatereq_pivot, node->childkeys[i]);
            VERIFY_ASSERTION(compare >= 0, i, "Pivot is < the upper-bound pivot");
        }
    }

    for (int i = 0; i < node->n_children; i++) {
        struct kv_pair *curr_less_pivot = (i==0) ? lesser_pivot : node->childkeys[i-1];
        struct kv_pair *curr_geq_pivot = (i==node->n_children-1) ? greatereq_pivot : node->childkeys[i];
        if (node->height > 0) {
            MSN last_msn = ZERO_MSN;
            // Verify that messages in the buffers are in the right place.
            NONLEAF_CHILDINFO bnc = BNC(node, i);
            VERIFY_ASSERTION(verify_sorted_by_key_msn(brt, bnc->buffer, bnc->fresh_message_tree) == 0, i, "fresh_message_tree");
            VERIFY_ASSERTION(verify_sorted_by_key_msn(brt, bnc->buffer, bnc->stale_message_tree) == 0, i, "stale_message_tree");
            FIFO_ITERATE(bnc->buffer, key, keylen, data, datalen, itype, msn, xid, is_fresh,
                         ({
                             enum brt_msg_type type = (enum brt_msg_type) itype;
                             int r = verify_msg_in_child_buffer(brt, type, msn, key, keylen, data, datalen, xid,
                                                                curr_less_pivot,
                                                                curr_geq_pivot);
                             VERIFY_ASSERTION(r==0, i, "A message in the buffer is out of place");
                             VERIFY_ASSERTION((msn.msn > last_msn.msn), i, "msn per msg must be monotonically increasing toward newer messages in buffer");
                             VERIFY_ASSERTION((msn.msn <= this_msn.msn), i, "all messages must have msn within limit of this node's max_msn_applied_to_node_in_memory");
                             int count;
                             count = count_eq_key_msn(brt, bnc->buffer, bnc->fresh_message_tree, key, keylen, msn);
                             if (brt_msg_type_applies_all(type) || brt_msg_type_does_nothing(type)) {
                                 VERIFY_ASSERTION(count == 0, i, "a broadcast message was found in the fresh message tree");
                             } else {
                                 VERIFY_ASSERTION(brt_msg_type_applies_once(type), i, "a message was found that does not apply either to all or to only one key");
                                 if (is_fresh) {
                                     VERIFY_ASSERTION(count == 1, i, "a fresh message was not found in the fresh message tree");
                                 } else {
                                     VERIFY_ASSERTION(count == 0, i, "a stale message was found in the fresh message tree");
                                 }
                             }
                             count = count_eq_key_msn(brt, bnc->buffer, bnc->stale_message_tree, key, keylen, msn);
                             if (brt_msg_type_applies_all(type) || brt_msg_type_does_nothing(type)) {
                                 VERIFY_ASSERTION(count == 0, i, "a broadcast message was found in the stale message tree");
                             } else {
                                 VERIFY_ASSERTION(brt_msg_type_applies_once(type), i, "a message was found that does not apply either to all or to only one key");
                                 if (is_fresh) {
                                     VERIFY_ASSERTION(count == 0, i, "a fresh message was found in the stale message tree");
                                 } else {
                                     VERIFY_ASSERTION(count == 1, i, "a stale message was not found in the stale message tree");
                                 }
                             }
                             DBT keydbt;
                             struct count_msgs_extra extra = { .count = 0, .key = toku_fill_dbt(&keydbt, key, keylen),
                                                               .msn = msn, .fifo = bnc->buffer,
                                                               .cmp_extra = brt->db, .cmp = brt->compare_fun };
                             extra.count = 0;
                             toku_omt_iterate(bnc->broadcast_list, count_msgs, &extra);
                             if (brt_msg_type_applies_all(type) || brt_msg_type_does_nothing(type)) {
                                 VERIFY_ASSERTION(extra.count == 1, i, "a broadcast message was not found in the broadcast list");
                             } else {
                                 VERIFY_ASSERTION(brt_msg_type_applies_once(type), i, "a message was found that does not apply either to all or to only one key");
                                 if (is_fresh) {
                                     VERIFY_ASSERTION(extra.count == 0, i, "a broadcast message was found in the fresh message tree");
                                 } else {
                                     VERIFY_ASSERTION(extra.count == 0, i, "a broadcast message was found in the fresh message tree");
                                 }
                             }
                             last_msn = msn;
                         }));
            struct verify_message_tree_extra extra = { .fifo = bnc->buffer, .broadcast = false, .is_fresh = true, .i = i, .verbose = verbose, .blocknum = blocknum, .keep_going_on_failure = keep_going_on_failure };
            int r = toku_omt_iterate(bnc->fresh_message_tree, verify_message_tree, &extra);
            if (r != 0) { result = r; goto done; }
            extra.is_fresh = false;
            r = toku_omt_iterate(bnc->stale_message_tree, verify_message_tree, &extra);
            if (r != 0) { result = r; goto done; }
            extra.broadcast = true;
            r = toku_omt_iterate(bnc->broadcast_list, verify_message_tree, &extra);
            if (r != 0) { result = r; goto done; }
        }
        else {
            BASEMENTNODE bn = BLB(node, i);
            for (u_int32_t j = 0; j < toku_omt_size(bn->buffer); j++) {
                VERIFY_ASSERTION((rootmsn.msn >= this_msn.msn), 0, "leaf may have latest msn, but cannot be greater than root msn");
                LEAFENTRY le = get_ith_leafentry(bn, j);
                if (curr_less_pivot) {
                    int compare = compare_pair_to_leafentry(brt, curr_less_pivot, le);
                    VERIFY_ASSERTION(compare < 0, j, "The leafentry is >= the lower-bound pivot");
                }
                if (curr_geq_pivot) {
                    int compare = compare_pair_to_leafentry(brt, curr_geq_pivot, le);
                    VERIFY_ASSERTION(compare >= 0, j, "The leafentry is < the upper-bound pivot");
                }
                if (0 < j) {
                    LEAFENTRY prev_le = get_ith_leafentry(bn, j-1);
                    int compare = compare_leafentries(brt, prev_le, le);
                    VERIFY_ASSERTION(compare < 0, j, "Adjacent leafentries are out of order");
                }
            }
        }
    } 
    
    // Verify that the subtrees have the right properties.
    if (recurse && node->height > 0) {
        for (int i = 0; i < node->n_children; i++) {
            int r = toku_verify_brtnode(brt, rootmsn, this_msn,
                                        BP_BLOCKNUM(node, i), node->height-1,
                                        (i==0)                  ? lesser_pivot        : node->childkeys[i-1],
                                        (i==node->n_children-1) ? greatereq_pivot     : node->childkeys[i],
                                        progress_callback, progress_extra,
                                        recurse, verbose, keep_going_on_failure);
            if (r) {
                result = r;
                if (!keep_going_on_failure || result != TOKUDB_NEEDS_REPAIR) goto done;
            }
        }
    }
done:
    {
    int r = toku_cachetable_unpin(brt->cf, blocknum, fullhash, CACHETABLE_CLEAN, make_brtnode_pair_attr(node));
    assert_zero(r); // this is a bad failure if it happens.
    }
    
    if (result == 0 && progress_callback) 
    result = progress_callback(progress_extra, 0.0);
    
    return result;
}

int 
toku_verify_brt_with_progress (BRT brt, int (*progress_callback)(void *extra, float progress), void *progress_extra, int verbose, int keep_on_going) {
    assert(brt->h);
    u_int32_t root_hash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &root_hash);
    int r = toku_verify_brtnode(brt, ZERO_MSN, ZERO_MSN, *rootp, -1, NULL, NULL, progress_callback, progress_extra, 1, verbose, keep_on_going);
    if (r == 0) {
        toku_brtheader_lock(brt->h);
        brt->h->time_of_last_verification = time(NULL);
        brt->h->dirty = 1;
        toku_brtheader_unlock(brt->h);
    }
    return r;
}

int 
toku_verify_brt (BRT brt) {
    return toku_verify_brt_with_progress(brt, NULL, NULL, 0, 0);
}

