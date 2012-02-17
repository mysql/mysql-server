/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "ule.h"
#include <brt-cachetable-wrappers.h>
#include <brt-flusher.h>


// dummymsn needed to simulate msn because messages are injected at a lower level than toku_brt_root_put_cmd()
#define MIN_DUMMYMSN ((MSN) {(uint64_t)100000000000})
static MSN dummymsn;      
static int testsetup_initialized = 0;


// Must be called before any other test_setup_xxx() functions are called.
void
toku_testsetup_initialize(void) {
    if (testsetup_initialized == 0) {
        testsetup_initialized = 1;
        dummymsn = MIN_DUMMYMSN;
    }
}

static MSN
next_dummymsn(void) {
    ++(dummymsn.msn);
    return dummymsn;
}


BOOL ignore_if_was_already_open;
int toku_testsetup_leaf(BRT brt, BLOCKNUM *blocknum, int n_children, char **keys, int *keylens) {
    BRTNODE node;
    assert(testsetup_initialized);
    int r = toku_read_brt_header_and_store_in_cachefile(brt, brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
    if (r!=0) return r;
    toku_create_new_brtnode(brt, &node, 0, n_children);
    int i;
    for (i=0; i<n_children; i++) {
        BP_STATE(node,i) = PT_AVAIL;
    }

    for (i=0; i+1<n_children; i++) {
        node->childkeys[i] = kv_pair_malloc(keys[i], keylens[i], 0, 0);
        node->totalchildkeylens += keylens[i];
    }

    *blocknum = node->thisnodename;
    toku_unpin_brtnode(brt, node);
    return 0;
}

// Don't bother to clean up carefully if something goes wrong.  (E.g., it's OK to have malloced stuff that hasn't been freed.)
int toku_testsetup_nonleaf (BRT brt, int height, BLOCKNUM *blocknum, int n_children, BLOCKNUM *children, char **keys, int *keylens) {
    BRTNODE node;
    assert(testsetup_initialized);
    assert(n_children<=BRT_FANOUT);
    int r = toku_read_brt_header_and_store_in_cachefile(brt, brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
    if (r!=0) return r;
    toku_create_new_brtnode(brt, &node, height, n_children);
    int i;
    for (i=0; i<n_children; i++) {
        BP_BLOCKNUM(node, i) = children[i];
        BP_STATE(node,i) = PT_AVAIL;
    }
    for (i=0; i+1<n_children; i++) {
        node->childkeys[i] = kv_pair_malloc(keys[i], keylens[i], 0, 0);
        node->totalchildkeylens += keylens[i];
    }
    *blocknum = node->thisnodename;
    toku_unpin_brtnode(brt, node);
    return 0;
}

int toku_testsetup_root(BRT brt, BLOCKNUM blocknum) {
    assert(testsetup_initialized);
    int r = toku_read_brt_header_and_store_in_cachefile(brt, brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
    if (r!=0) return r;
    brt->h->root_blocknum = blocknum;
    return 0;
}

int toku_testsetup_get_sersize(BRT brt, BLOCKNUM diskoff) // Return the size on disk
{
    assert(testsetup_initialized);
    void *node_v;
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->h);
    int r  = toku_cachetable_get_and_pin(
        brt->cf, diskoff,
        toku_cachetable_hash(brt->cf, diskoff),
        &node_v,
        NULL,
        get_write_callbacks_for_node(brt->h),
        toku_brtnode_fetch_callback,
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        &bfe
        );
    assert(r==0);
    int size = toku_serialize_brtnode_size(node_v);
    toku_unpin_brtnode(brt, node_v);
    return size;
}

int toku_testsetup_insert_to_leaf (BRT brt, BLOCKNUM blocknum, char *key, int keylen, char *val, int vallen) {
    void *node_v;
    int r;

    assert(testsetup_initialized);

    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->h);
    r = toku_cachetable_get_and_pin(
        brt->cf,
        blocknum,
        toku_cachetable_hash(brt->cf, blocknum),
        &node_v,
        NULL,
        get_write_callbacks_for_node(brt->h),
	toku_brtnode_fetch_callback,
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
	&bfe
	);
    if (r!=0) return r;
    BRTNODE node=node_v;
    toku_verify_or_set_counts(node);
    assert(node->height==0);

    DBT keydbt,valdbt;
    MSN msn = next_dummymsn();
    BRT_MSG_S cmd = {BRT_INSERT, msn, xids_get_root_xids(),
                     .u.id={toku_fill_dbt(&keydbt, key, keylen),
                            toku_fill_dbt(&valdbt, val, vallen)}};

    brtnode_put_cmd (
        brt->h->compare_fun,
        brt->h->update_fun,
        &brt->h->descriptor,
        node,
        &cmd,
        true
        );    

    toku_verify_or_set_counts(node);

    toku_unpin_brtnode(brt, node_v);
    return 0;
}

static int
testhelper_string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *s = a->data, *t = b->data;
    return strcmp(s, t);
}


void
toku_pin_node_with_min_bfe(BRTNODE* node, BLOCKNUM b, BRT t)
{
    struct brtnode_fetch_extra bfe;
    fill_bfe_for_min_read(&bfe, t->h);
    toku_pin_brtnode_off_client_thread(
        t->h, 
        b,
        toku_cachetable_hash(t->h->cf, b),
        &bfe,
        0,
        NULL,
        node
        );
}

int toku_testsetup_insert_to_nonleaf (BRT brt, BLOCKNUM blocknum, enum brt_msg_type cmdtype, char *key, int keylen, char *val, int vallen) {
    void *node_v;
    int r;

    assert(testsetup_initialized);

    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt->h);
    r = toku_cachetable_get_and_pin(
        brt->cf,
        blocknum,
        toku_cachetable_hash(brt->cf, blocknum),
        &node_v,
        NULL,
        get_write_callbacks_for_node(brt->h),
	toku_brtnode_fetch_callback,
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
	&bfe
        );
    if (r!=0) return r;
    BRTNODE node=node_v;
    assert(node->height>0);

    DBT k;
    int childnum = toku_brtnode_which_child(node,
                                            toku_fill_dbt(&k, key, keylen),
                                            &brt->h->descriptor, brt->compare_fun);

    XIDS xids_0 = xids_get_root_xids();
    MSN msn = next_dummymsn();
    r = toku_bnc_insert_msg(BNC(node, childnum), key, keylen, val, vallen, cmdtype, msn, xids_0, true, NULL, testhelper_string_key_cmp);
    assert(r==0);
    // Hack to get the test working. The problem is that this test
    // is directly queueing something in a FIFO instead of 
    // using brt APIs.
    node->max_msn_applied_to_node_on_disk = msn;
    node->dirty = 1;

    toku_unpin_brtnode(brt, node_v);
    return 0;
}
