/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;
static char fname[] = __FILE__ ".brt";

static int dummy_cmp(DB *e __attribute__((unused)),
                     const DBT *a, const DBT *b) {
    if (a->size > b->size) {
        return memcmp(a->data, b->data, b->size) || +1;
    } else if (a->size < b->size) {
        return memcmp(a->data, b->data, a->size) || -1;
    } else {
        return memcmp(a->data, b->data, a->size);
    }
}

static void
rand_bytes(void *dest, int size)
{
    long *l;
    for (l = dest; (unsigned int) size >= (sizeof *l); ++l, size -= (sizeof *l)) {
        *l = random();
    }
    for (char *c = (char *) l; size > 0; ++c, --size) {
        *c = random() & 0xff;
    }
}

#if 0
static void
rand_bytes_limited(void *dest, int size)
{
    long *l;
    for (l = dest; (unsigned int) size >= (sizeof *l); ++l, size -= (sizeof *l)) {
        char c = random() & 0xff;
        int i = 0;
        for (char *p = (char *) l; i < (sizeof *l); ++i) {
            *p = c;
        }
    }
    char c = random() & 0xff;
    for (char *p = (char *) l; size > 0; ++c, --size) {
        *p = c;
    }
}
#endif

static void
insert_random_message(NONLEAF_CHILDINFO bnc, BRT_MSG_S **save, bool *is_fresh_out, XIDS xids, int pfx)
{
    int keylen = (random() % 1024) + 16;
    int vallen = (random() % 1024) + 16;
    void *key = toku_xmalloc(keylen + (sizeof pfx));
    void *val = toku_xmalloc(vallen);
    *(int *) key = pfx;
    rand_bytes((char *) key + (sizeof pfx), keylen);
    rand_bytes(val, vallen);
    MSN msn = next_dummymsn();
    bool is_fresh = (random() & 0x100) == 0;

    DBT *keydbt, *valdbt;
    keydbt = toku_xmalloc(sizeof *keydbt);
    valdbt = toku_xmalloc(sizeof *valdbt);
    toku_fill_dbt(keydbt, key, keylen + (sizeof pfx));
    toku_fill_dbt(valdbt, val, vallen);
    BRT_MSG_S *result = toku_xmalloc(sizeof *result);
    result->type = BRT_INSERT;
    result->msn = msn;
    result->xids = xids;
    result->u.id.key = keydbt;
    result->u.id.val = valdbt;
    *save = result;
    *is_fresh_out = is_fresh;

    int r = toku_bnc_insert_msg(bnc, key, keylen + (sizeof pfx), val, vallen,
                                BRT_INSERT, msn, xids, is_fresh,
                                NULL, dummy_cmp);
    assert_zero(r);
}

#if 0
static void
insert_random_message_to_leaf(BRT t, BASEMENTNODE blb, LEAFENTRY *save, XIDS xids, int pfx)
{
    int keylen = (random() % 16) + 16;
    int vallen = (random() % 1024) + 16;
    void *key = toku_xmalloc(keylen + (sizeof pfx));
    void *val = toku_xmalloc(vallen);
    *(int *) key = pfx;
    rand_bytes_limited((char *) key + (sizeof pfx), keylen);
    rand_bytes(val, vallen);
    MSN msn = next_dummymsn();
    bool is_fresh = (random() & 0x100) == 0;

    DBT *keydbt, *valdbt;
    keydbt = toku_xmalloc(sizeof *keydbt);
    valdbt = toku_xmalloc(sizeof *valdbt);
    toku_fill_dbt(keydbt, key, keylen + (sizeof pfx));
    toku_fill_dbt(valdbt, val, vallen);
    BRT_MSG_S *result = toku_xmalloc(sizeof *result);
    result->type = BRT_INSERT;
    result->msn = msn;
    result->xids = xids;
    result->u.id.key = keydbt;
    result->u.id.val = valdbt;
    size_t memsize, disksize;
    int r = apply_msg_to_leafentry(result, NULL, &memsize, &disksize, save, NULL, NULL);
    assert_zero(r);
    struct subtree_estimates subtree_est = zero_estimates;
    bool made_change;
    uint64_t workdone;
    brt_leaf_put_cmd(t, blb, &subtree_est, result, &made_change, &workdone, NULL, NULL);
}

struct orthopush_flush_update_fun_extra {
    ...;
};

static int
orthopush_flush_update_fun(DB *db, const DBT *key, const DBT *old_val, const DBT *extra,
                           void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    ...;
    return 0;
}

static void
insert_random_update_message(NONLEAF_CHILDINFO bnc, BRT_MSG_S **save, bool *is_fresh_out, XIDS xids, int pfx)
{
    int keylen = (random() % 16) + 16;
    int vallen = sizeof(struct orthopush_flush_update_fun_extra);
    void *key = toku_xmalloc(keylen + (sizeof pfx));
    void *val = toku_xmalloc(vallen);
    *(int *) key = pfx;
    rand_bytes_limited((char *) key + (sizeof pfx), keylen);
    struct orthopush_flush_update_fun_extra *update_extra = val;
    ...;
    MSN msn = next_dummymsn();
    bool is_fresh = (random() & 0x100) == 0;

    DBT *keydbt, *valdbt;
    keydbt = toku_xmalloc(sizeof *keydbt);
    valdbt = toku_xmalloc(sizeof *valdbt);
    toku_fill_dbt(keydbt, key, keylen + (sizeof pfx));
    toku_fill_dbt(valdbt, val, vallen);
    BRT_MSG_S *result = toku_xmalloc(sizeof *result);
    result->type = BRT_UPDATE;
    result->msn = msn;
    result->xids = xids;
    result->u.id.key = keydbt;
    result->u.id.val = valdbt;
    *save = result;
    *is_fresh_out = is_fresh;

    int r = toku_bnc_insert_msg(bnc, key, keylen + (sizeof pfx), val, vallen,
                                BRT_INSERT, msn, xids, is_fresh,
                                NULL, dummy_cmp);
    assert_zero(r);
}
#endif

const int M = 1024 * 1024;

static void
flush_to_internal(BRT t) {
    int r;

    BRT_MSG_S **MALLOC_N(128*1024,parent_messages);  // 4m / 32 = 128k
    BRT_MSG_S **MALLOC_N(128*1024,child_messages);
    bool *MALLOC_N(128*1024,parent_messages_is_fresh);
    bool *MALLOC_N(128*1024,child_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, sizeof parent_messages_is_fresh);
    memset(child_messages_is_fresh, 0, sizeof child_messages_is_fresh);

    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    NONLEAF_CHILDINFO child_bnc = toku_create_empty_nl();
    int i;
    for (i = 0; toku_bnc_memory_size(child_bnc) < 4*M; ++i) {
        insert_random_message(child_bnc, &child_messages[i], &child_messages_is_fresh[i], xids_123, 0);
    }
    int num_child_messages = i;

    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    for (i = 0; toku_bnc_memory_size(parent_bnc) < 4*M; ++i) {
        insert_random_message(parent_bnc, &parent_messages[i], &parent_messages_is_fresh[i], xids_234, 0);
    }
    int num_parent_messages = i;

    BRTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_brtnode(child, blocknum, 1, 1, BRT_LAYOUT_VERSION, 4*M, 0);
    destroy_nonleaf_childinfo(BNC(child, 0));
    set_BNC(child, 0, child_bnc);
    BP_STATE(child, 0) = PT_AVAIL;

    toku_bnc_flush_to_child(t, parent_bnc, child);

    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);

    FIFO_ITERATE(child_bnc->buffer, key, keylen, val, vallen, type, msn, xids, is_fresh,
                 {
                     DBT keydbt;
                     DBT valdbt;
                     toku_fill_dbt(&keydbt, key, keylen);
                     toku_fill_dbt(&valdbt, val, vallen);
                     int found = 0;
                     for (i = 0; i < num_parent_messages; ++i) {
                         if (dummy_cmp(NULL, &keydbt, parent_messages[i]->u.id.key) == 0 &&
                             msn.msn == parent_messages[i]->msn.msn) {
                             assert(parent_messages_present[i] == 0);
                             assert(found == 0);
                             assert(dummy_cmp(NULL, &valdbt, parent_messages[i]->u.id.val) == 0);
                             assert(type == parent_messages[i]->type);
                             assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(parent_messages[i]->xids));
                             assert(parent_messages_is_fresh[i] == is_fresh);
                             parent_messages_present[i]++;
                             found++;
                         }
                     }
                     for (i = 0; i < num_child_messages; ++i) {
                         if (dummy_cmp(NULL, &keydbt, child_messages[i]->u.id.key) == 0 &&
                             msn.msn == child_messages[i]->msn.msn) {
                             assert(child_messages_present[i] == 0);
                             assert(found == 0);
                             assert(dummy_cmp(NULL, &valdbt, child_messages[i]->u.id.val) == 0);
                             assert(type == child_messages[i]->type);
                             assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(child_messages[i]->xids));
                             assert(child_messages_is_fresh[i] == is_fresh);
                             child_messages_present[i]++;
                             found++;
                         }
                     }
                     assert(found == 1);
                 });

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->u.id.key->data);
        toku_free((DBT *) parent_messages[i]->u.id.key);
        toku_free(parent_messages[i]->u.id.val->data);
        toku_free((DBT *) parent_messages[i]->u.id.val);
        toku_free(parent_messages[i]);
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]->u.id.key->data);
        toku_free((DBT *) child_messages[i]->u.id.key);
        toku_free(child_messages[i]->u.id.val->data);
        toku_free((DBT *) child_messages[i]->u.id.val);
        toku_free(child_messages[i]);
    }
    destroy_nonleaf_childinfo(parent_bnc);
    toku_brtnode_free(&child);
    toku_free(parent_messages);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(child_messages_is_fresh);
}

static void
flush_to_internal_multiple(BRT t) {
    int r;

    BRT_MSG_S **MALLOC_N(128*1024,parent_messages);  // 4m / 32 = 128k
    BRT_MSG_S **MALLOC_N(128*1024,child_messages);
    bool *MALLOC_N(128*1024,parent_messages_is_fresh);
    bool *MALLOC_N(128*1024,child_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, sizeof parent_messages_is_fresh);
    memset(child_messages_is_fresh, 0, sizeof child_messages_is_fresh);

    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    NONLEAF_CHILDINFO child_bncs[8];
    BRT_MSG childkeys[7];
    int i;
    for (i = 0; i < 8; ++i) {
        child_bncs[i] = toku_create_empty_nl();
        if (i < 7) {
            childkeys[i] = NULL;
        }
    }
    int total_size = 0;
    for (i = 0; total_size < 4*M; ++i) {
        total_size -= toku_bnc_memory_size(child_bncs[i%8]);
        insert_random_message(child_bncs[i%8], &child_messages[i], &child_messages_is_fresh[i], xids_123, i%8);
        total_size += toku_bnc_memory_size(child_bncs[i%8]);
        if (i % 8 < 7) {
            if (childkeys[i%8] == NULL || dummy_cmp(NULL, child_messages[i]->u.id.key, childkeys[i%8]->u.id.key) > 0) {
                childkeys[i%8] = child_messages[i];
            }
        }
    }
    int num_child_messages = i;

    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    for (i = 0; toku_bnc_memory_size(parent_bnc) < 4*M; ++i) {
        insert_random_message(parent_bnc, &parent_messages[i], &parent_messages_is_fresh[i], xids_234, 0);
    }
    int num_parent_messages = i;

    BRTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_brtnode(child, blocknum, 1, 8, BRT_LAYOUT_VERSION, 4*M, 0);
    for (i = 0; i < 8; ++i) {
        destroy_nonleaf_childinfo(BNC(child, i));
        set_BNC(child, i, child_bncs[i]);
        BP_STATE(child, i) = PT_AVAIL;
        if (i < 7) {
            child->childkeys[i] = kv_pair_malloc(childkeys[i]->u.id.key->data, childkeys[i]->u.id.key->size, NULL, 0);
        }
    }

    toku_bnc_flush_to_child(t, parent_bnc, child);

    int total_messages = 0;
    for (i = 0; i < 8; ++i) {
        total_messages += toku_bnc_n_entries(BNC(child, i));
    }
    assert(total_messages == num_parent_messages + num_child_messages);
    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);

    for (int j = 0; j < 8; ++j) {
        FIFO_ITERATE(child_bncs[j]->buffer, key, keylen, val, vallen, type, msn, xids, is_fresh,
                     {
                         DBT keydbt;
                         DBT valdbt;
                         toku_fill_dbt(&keydbt, key, keylen);
                         toku_fill_dbt(&valdbt, val, vallen);
                         int found = 0;
                         for (i = 0; i < num_parent_messages; ++i) {
                             if (dummy_cmp(NULL, &keydbt, parent_messages[i]->u.id.key) == 0 &&
                                 msn.msn == parent_messages[i]->msn.msn) {
                                 assert(parent_messages_present[i] == 0);
                                 assert(found == 0);
                                 assert(dummy_cmp(NULL, &valdbt, parent_messages[i]->u.id.val) == 0);
                                 assert(type == parent_messages[i]->type);
                                 assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(parent_messages[i]->xids));
                                 assert(parent_messages_is_fresh[i] == is_fresh);
                                 parent_messages_present[i]++;
                                 found++;
                             }
                         }
                         for (i = 0; i < num_child_messages; ++i) {
                             if (dummy_cmp(NULL, &keydbt, child_messages[i]->u.id.key) == 0 &&
                                 msn.msn == child_messages[i]->msn.msn) {
                                 assert(child_messages_present[i] == 0);
                                 assert(found == 0);
                                 assert(dummy_cmp(NULL, &valdbt, child_messages[i]->u.id.val) == 0);
                                 assert(type == child_messages[i]->type);
                                 assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(child_messages[i]->xids));
                                 assert(child_messages_is_fresh[i] == is_fresh);
                                 child_messages_present[i]++;
                                 found++;
                             }
                         }
                         assert(found == 1);
                     });
    }

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->u.id.key->data);
        toku_free((DBT *) parent_messages[i]->u.id.key);
        toku_free(parent_messages[i]->u.id.val->data);
        toku_free((DBT *) parent_messages[i]->u.id.val);
        toku_free(parent_messages[i]);
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]->u.id.key->data);
        toku_free((DBT *) child_messages[i]->u.id.key);
        toku_free(child_messages[i]->u.id.val->data);
        toku_free((DBT *) child_messages[i]->u.id.val);
        toku_free(child_messages[i]);
    }
    destroy_nonleaf_childinfo(parent_bnc);
    toku_brtnode_free(&child);
    toku_free(parent_messages);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(child_messages_is_fresh);
}

#if 0
static void
flush_to_leaf(BRT t, bool make_leaf_up_to_date, bool use_flush) {
    int r;

    BRT_MSG_S **MALLOC_N(128*1024,parent_messages);  // 4m / 32 = 128k
    LEAFENTRY *MALLOC_N(128*1024,child_messages);
    bool *MALLOC_N(128*1024,parent_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, sizeof parent_messages_is_fresh);

    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    BASEMENTNODE child_blbs[8];
    DBT childkeys[7];
    int i;
    for (i = 0; i < 8; ++i) {
        child_blbs[i] = toku_create_empty_bn();
        if (i < 7) {
            toku_init_dbt(&childkeys[i]);
        }
    }
    int total_size = 0;
    for (i = 0; total_size < 4*M; ++i) {
        total_size -= child_blbs[i%8]->n_bytes_in_buffer;
        insert_random_message_to_leaf(t, child_blbs[i%8], &child_messages[i], xids_123, i%8);
        total_size += child_blbs[i%8]->n_bytes_in_buffer;
        if (i % 8 < 7) {
            u_int32_t keylen;
            char *key = le_key_and_len(child_messages[i], &keylen);
            DBT keydbt;
            if (dummy_cmp(NULL, toku_fill_dbt(&keydbt, key, keylen), childkeys[i%8]) > 0) {
                toku_fill_dbt(&childkeys[i%8], key, keylen);
            }
        }
    }
    int num_child_messages = i;

    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    for (i = 0; toku_bnc_memory_size(parent_bnc) < 4*M; ++i) {
        insert_random_update_message(parent_bnc, &parent_messages[i], &parent_messages_is_fresh[i], xids_234, i%8);
    }
    int num_parent_messages = i;

    BRTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_brtnode(child, blocknum, 0, 8, BRT_LAYOUT_VERSION, 4*M, 0);
    for (i = 0; i < 8; ++i) {
        destroy_basement_node(BLB(child, i));
        set_BLB(child, i, child_blbs[i]);
        BP_STATE(child, i) = PT_AVAIL;
        if (i < 7) {
            child->childkeys[i] = kv_pair_malloc(childkeys[i].data, childkeys[i].size, NULL, 0);
        }
    }

    if (make_leaf_up_to_date) {
        for (i = 0; i < num_parent_messages; ++i) {
            if (!parent_messages_is_fresh[i]) {
                bool made_change;
                uint64_t workdone;
                toku_apply_cmd_to_leaf(t, child, parent_messages[i], &made_change, &workdone, NULL, NULL);
            }
        }
        child->stale_ancestor_messages_applied = true;
    } else {
        child->stale_ancestor_messages_applied = false;
    }

    if (use_flush) {
        toku_bnc_flush_to_child(t, parent_bnc, child);
    } else {
        BRTNODE XMALLOC(parentnode);
        BLOCKNUM parentblocknum = { 17 };
        toku_initialize_empty_brtnode(parentnode, parentblocknum, 1, 1, BRT_LAYOUT_VERSION, 4*M, 0);
        destroy_nonleaf_childinfo(BNC(parent, 0));
        set_BNC(parent, 0, parent_bnc);
        BP_STATE(parent, 0) = PT_AVAIL;
        struct ancestors ancestors = { .node = parentnode, .childnum = 0, .next = NULL };
        const struct pivot_bounds infinite_bounds = { .lower_bound_exclusive = NULL, .upper_bound_inclusive = NULL };
        maybe_apply_ancestors_messages_to_node(t, child, &ancestors, &infinite_bounds);
    }

    int total_messages = 0;
    for (i = 0; i < 8; ++i) {
        total_messages += toku_bnc_n_entries(BNC(child, i));
    }
    assert(total_messages == num_parent_messages + num_child_messages);
    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);

    for (int j = 0; j < 8; ++j) {
        FIFO_ITERATE(child_bncs[j]->buffer, key, keylen, val, vallen, type, msn, xids, is_fresh,
                     {
                         DBT keydbt;
                         DBT valdbt;
                         toku_fill_dbt(&keydbt, key, keylen);
                         toku_fill_dbt(&valdbt, val, vallen);
                         int found = 0;
                         for (i = 0; i < num_parent_messages; ++i) {
                             if (dummy_cmp(NULL, &keydbt, parent_messages[i]->u.id.key) == 0 &&
                                 msn.msn == parent_messages[i]->msn.msn) {
                                 assert(parent_messages_present[i] == 0);
                                 assert(found == 0);
                                 assert(dummy_cmp(NULL, &valdbt, parent_messages[i]->u.id.val) == 0);
                                 assert(type == parent_messages[i]->type);
                                 assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(parent_messages[i]->xids));
                                 assert(parent_messages_is_fresh[i] == is_fresh);
                                 parent_messages_present[i]++;
                                 found++;
                             }
                         }
                         for (i = 0; i < num_child_messages; ++i) {
                             if (dummy_cmp(NULL, &keydbt, child_messages[i]->u.id.key) == 0 &&
                                 msn.msn == child_messages[i]->msn.msn) {
                                 assert(child_messages_present[i] == 0);
                                 assert(found == 0);
                                 assert(dummy_cmp(NULL, &valdbt, child_messages[i]->u.id.val) == 0);
                                 assert(type == child_messages[i]->type);
                                 assert(xids_get_innermost_xid(xids) == xids_get_innermost_xid(child_messages[i]->xids));
                                 assert(child_messages_is_fresh[i] == is_fresh);
                                 child_messages_present[i]++;
                                 found++;
                             }
                         }
                         assert(found == 1);
                     });
    }

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->u.id.key->data);
        toku_free((DBT *) parent_messages[i]->u.id.key);
        toku_free(parent_messages[i]->u.id.val->data);
        toku_free((DBT *) parent_messages[i]->u.id.val);
        toku_free(parent_messages[i]);
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]->u.id.key->data);
        toku_free((DBT *) child_messages[i]->u.id.key);
        toku_free(child_messages[i]->u.id.val->data);
        toku_free((DBT *) child_messages[i]->u.id.val);
        toku_free(child_messages[i]);
    }
    destroy_nonleaf_childinfo(parent_bnc);
    toku_brtnode_free(&child);
    toku_free(parent_messages);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(child_messages_is_fresh);
}
#endif

int
test_main (int argc, const char *argv[]) {
    toku_memory_check = 1;
    default_parse_args(argc, argv);

    int r;
    CACHETABLE ct;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);
    assert(r==0);
    unlink(fname);
    BRT t;
    r = toku_open_brt(fname, 1, &t, 4*M, 64*1024, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    // just run a bunch, it's a randomized test
    for (int i = 0; i < 10; ++i) {
        flush_to_internal(t);
    }
    for (int i = 0; i < 10; ++i) {
        flush_to_internal_multiple(t);
    }
#if 0
    r = toku_brt_set_update(t, orthopush_flush_update_fun); assert(r==0);
    for (int i = 0; i < 10; ++i) {
        flush_to_leaf(t, false, false);
        flush_to_leaf(t, false, true);
        flush_to_leaf(t, true, false);
        flush_to_leaf(t, true, true);
    }
#endif

    r = toku_close_brt(t, 0);          assert(r==0);
    r = toku_cachetable_close(&ct); assert(r==0);

    return 0;
}
