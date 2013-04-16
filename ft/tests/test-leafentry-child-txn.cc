/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: test-leafentry-nested.cc 49851 2012-11-12 00:43:22Z esmet $"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "fttypes.h"

#include "ule.h"
#include "ule-internal.h"

static void init_empty_ule(ULE ule, DBT *key) {
    ule->num_cuxrs = 0;
    ule->num_puxrs = 0;
    ule->uxrs = ule->uxrs_static;
    ule->keylen   = key->size;
    ule->keyp     = key->data;
}

static void add_committed_entry(ULE ule, DBT *val, TXNID xid) {
    uint32_t index = ule->num_cuxrs;
    ule->num_cuxrs++;
    ule->uxrs[index].type   = XR_INSERT;
    ule->uxrs[index].vallen = val->size;
    ule->uxrs[index].valp   = val->data;
    ule->uxrs[index].xid    = xid;
}

static FT_MSG_S
msg_init(enum ft_msg_type type, XIDS xids,
         DBT *key, DBT *val) {
    FT_MSG_S msg;
    msg.type = type;
    msg.xids = xids;
    msg.u.id.key = key;
    msg.u.id.val = val;
    return msg;
}

//Test all the different things that can happen to a
//committed leafentry (logical equivalent of a committed insert).
static void
run_test(void) {
    ULE_S ule_initial;
    ULE ule = &ule_initial;
    ule_initial.uxrs = ule_initial.uxrs_static;
    int r;
    DBT key;
    DBT val;
    uint64_t key_data = 1;
    uint64_t val_data_one = 1;
    uint64_t val_data_two = 2;
    uint64_t val_data_three = 3;
    uint32_t keysize = 8;
    uint32_t valsize = 8;

    toku_fill_dbt(&key, &key_data, keysize);
    toku_fill_dbt(&val, &val_data_one, valsize);

    // test case where we apply a message and the innermost child_id
    // is the same as the innermost committed TXNID    
    XIDS root_xids = xids_get_root_xids();
    TXNID root_txnid = 1000;
    TXNID child_id = 10;
    XIDS msg_xids_1;
    XIDS msg_xids_2;
    r = xids_create_child(root_xids, &msg_xids_1, root_txnid);
    assert(r==0);
    r = xids_create_child(msg_xids_1, &msg_xids_2, child_id);
    assert(r==0);

    init_empty_ule(&ule_initial, &key);
    add_committed_entry(&ule_initial, &val, 0);
    val.data = &val_data_two;
    // make the TXNID match the child id of xids
    add_committed_entry(&ule_initial, &val, 10);

    // now do the application of xids to the ule    
    FT_MSG_S msg;
    // do a commit
    msg = msg_init(FT_COMMIT_ANY, msg_xids_2, &key, &val);
    test_msg_modify_ule(&ule_initial, &msg);
    assert(ule->num_cuxrs == 2);
    assert(ule->uxrs[0].xid == TXNID_NONE);
    assert(ule->uxrs[1].xid == 10);
    assert(ule->uxrs[0].valp == &val_data_one);
    assert(ule->uxrs[1].valp == &val_data_two);

    // do an abort
    msg = msg_init(FT_ABORT_ANY, msg_xids_2, &key, &val);
    test_msg_modify_ule(&ule_initial, &msg);
    assert(ule->num_cuxrs == 2);
    assert(ule->uxrs[0].xid == TXNID_NONE);
    assert(ule->uxrs[1].xid == 10);
    assert(ule->uxrs[0].valp == &val_data_one);
    assert(ule->uxrs[1].valp == &val_data_two);

    // do an insert
    val.data = &val_data_three;
    msg = msg_init(FT_INSERT, msg_xids_2, &key, &val);
    test_msg_modify_ule(&ule_initial, &msg);
    // now that message applied, verify that things are good
    assert(ule->num_cuxrs == 2);
    assert(ule->num_puxrs == 2);
    assert(ule->uxrs[0].xid == TXNID_NONE);
    assert(ule->uxrs[1].xid == 10);
    assert(ule->uxrs[2].xid == 1000);
    assert(ule->uxrs[3].xid == 10);
    assert(ule->uxrs[0].valp == &val_data_one);
    assert(ule->uxrs[1].valp == &val_data_two);
    assert(ule->uxrs[2].type == XR_PLACEHOLDER);
    assert(ule->uxrs[3].valp == &val_data_three);
    

    xids_destroy(&msg_xids_2);
    xids_destroy(&msg_xids_1);
    xids_destroy(&root_xids);

}


int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    run_test();
    return 0;
}

