#include <stdbool.h>
#include <stdio.h>
#include "brttypes.h"
#include "memory.h"
#include "txnid_set.h"
#include "toku_assert.h"

void 
txnid_set_init(txnid_set *txnids) {
    int r = toku_omt_create(&txnids->ids); 
    assert_zero(r);
}

void 
txnid_set_destroy(txnid_set *txnids) {
    toku_omt_destroy(&txnids->ids);
}

static int 
txnid_compare(OMTVALUE a, void *b) {
    TXNID a_txnid = (TXNID) a;
    TXNID b_txnid = * (TXNID *) b;
    int r;
    if (a_txnid < b_txnid) 
        r = -1;
    else if (a_txnid > b_txnid) 
        r = +1;
    else
        r = 0;
    return r;
}

void 
txnid_set_add(txnid_set *txnids, TXNID id) {
    OMTVALUE v = (OMTVALUE) id;
    int r = toku_omt_insert(txnids->ids, v, txnid_compare, &id, NULL);
    assert(r == 0 || r == DB_KEYEXIST);
}

void 
txnid_set_delete(txnid_set *txnids, TXNID id) {
    int r;
    OMTVALUE v;
    uint32_t idx;
    r = toku_omt_find_zero(txnids->ids, txnid_compare, &id, &v, &idx);
    if (r == 0) {
        r = toku_omt_delete_at(txnids->ids, idx); 
        assert_zero(r);
    }
}

bool 
txnid_set_is_member(txnid_set *txnids, TXNID id) {
    int r;
    OMTVALUE v;
    uint32_t idx;
    r = toku_omt_find_zero(txnids->ids, txnid_compare, &id, &v, &idx);
    return r == 0;
}

size_t 
txnid_set_size(txnid_set *txnids) {
    return toku_omt_size(txnids->ids);
}

TXNID 
txnid_set_get(txnid_set *txnids, size_t ith) {
    OMTVALUE v;
    int r = toku_omt_fetch(txnids->ids, ith, &v); assert_zero(r);
    return (TXNID) v;
}
