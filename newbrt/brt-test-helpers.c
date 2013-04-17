#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"
#include "ule.h"


// dummymsn needed to simulate msn because messages are injected at a lower level than toku_brt_root_put_cmd()
#define MIN_DUMMYMSN ((MSN) {(uint64_t)1<<48})
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
int toku_testsetup_leaf(BRT brt, BLOCKNUM *blocknum) {
    BRTNODE node;
    assert(testsetup_initialized);
    int r = toku_read_brt_header_and_store_in_cachefile(brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
    if (r!=0) return r;
    toku_create_new_brtnode(brt, &node, 0, 1);
    BP_STATE(node,0) = PT_AVAIL;

    *blocknum = node->thisnodename;
    toku_unpin_brtnode(brt, node);
    return 0;
}

// Don't bother to clean up carefully if something goes wrong.  (E.g., it's OK to have malloced stuff that hasn't been freed.)
int toku_testsetup_nonleaf (BRT brt, int height, BLOCKNUM *blocknum, int n_children, BLOCKNUM *children, char **keys, int *keylens) {
    BRTNODE node;
    assert(testsetup_initialized);
    assert(n_children<=BRT_FANOUT);
    int r = toku_read_brt_header_and_store_in_cachefile(brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
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
    int r = toku_read_brt_header_and_store_in_cachefile(brt->cf, MAX_LSN, &brt->h, &ignore_if_was_already_open);
    if (r!=0) return r;
    brt->h->root = blocknum;
    brt->h->root_hash.valid = FALSE;
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
        toku_brtnode_flush_callback, 
        toku_brtnode_fetch_callback, 
        toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
        &bfe, 
        brt->h
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
	toku_brtnode_flush_callback, 
	toku_brtnode_fetch_callback, 
	toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
	&bfe, 
	brt->h
	);
    if (r!=0) return r;
    BRTNODE node=node_v;
    toku_verify_or_set_counts(node);
    assert(node->height==0);

    size_t lesize, disksize;
    LEAFENTRY leafentry;
    OMTVALUE storeddatav;
    u_int32_t idx;
    DBT keydbt,valdbt;
    MSN msn = next_dummymsn();
    BRT_MSG_S cmd = {BRT_INSERT, msn, xids_get_root_xids(),
                     .u.id={toku_fill_dbt(&keydbt, key, keylen),
                            toku_fill_dbt(&valdbt, val, vallen)}};
    //Generate a leafentry (committed insert key,val)
    r = apply_msg_to_leafentry(&cmd, NULL, //No old leafentry
                               &lesize, &disksize, &leafentry, 
                               NULL, NULL);
    assert(r==0);


    struct cmd_leafval_heaviside_extra be = {brt, &keydbt};
    r = toku_omt_find_zero(BLB_BUFFER(node, 0), toku_cmd_leafval_heaviside, &be, &storeddatav, &idx);


    if (r==0) {
	LEAFENTRY storeddata=storeddatav;
	// It's already there.  So now we have to remove it and put the new one back in.
	BLB_NBYTESINBUF(node, 0) -= OMT_ITEM_OVERHEAD + leafentry_disksize(storeddata);
	toku_free(storeddata);
	// Now put the new kv in.
	toku_omt_set_at(BLB_BUFFER(node, 0), leafentry, idx);
    } else {
	r = toku_omt_insert(BLB_BUFFER(node, 0), leafentry, toku_cmd_leafval_heaviside, &be, 0);
	assert(r==0);
    }
    // hack to get tests passing. These tests should not be directly inserting into buffers
    BLB(node, 0)->max_msn_applied = msn;

    BLB_NBYTESINBUF(node, 0) += OMT_ITEM_OVERHEAD + disksize;

    node->dirty=1;

    toku_verify_or_set_counts(node);

    toku_unpin_brtnode(brt, node_v);
    return 0;
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
	toku_brtnode_flush_callback, 
	toku_brtnode_fetch_callback, 
	toku_brtnode_pe_callback, 
        toku_brtnode_pf_req_callback,
        toku_brtnode_pf_callback,
	&bfe, 
	brt->h
	);
    if (r!=0) return r;
    BRTNODE node=node_v;
    assert(node->height>0);

    DBT k;
    int childnum = toku_brtnode_which_child(node,
				       toku_fill_dbt(&k, key, keylen),
				       brt);

    XIDS xids_0 = xids_get_root_xids();
    MSN msn = next_dummymsn();
    r = toku_fifo_enq(BNC_BUFFER(node, childnum), key, keylen, val, vallen, cmdtype, msn, xids_0);
    assert(r==0);
    // Hack to get the test working. The problem is that this test
    // is directly queueing something in a FIFO instead of 
    // using brt APIs.
    node->max_msn_applied_to_node_on_disk = msn;
    int sizediff = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD + xids_get_serialize_size(xids_0);
    BNC_NBYTESINBUF(node, childnum) += sizediff;
    node->dirty = 1;

    toku_unpin_brtnode(brt, node_v);
    return 0;
}
