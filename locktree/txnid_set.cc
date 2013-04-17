/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db.h>
#include "txnid_set.h"

namespace toku {

int find_by_txnid(const TXNID &txnid_a, const TXNID &txnid_b);
int find_by_txnid(const TXNID &txnid_a, const TXNID &txnid_b) {
    if (txnid_a < txnid_b) {
        return -1;
    } else if (txnid_a == txnid_b) {
        return 0;
    } else {
        return 1;
    }
}

void txnid_set::create(void) {
    // lazily allocate the underlying omt, since it is common
    // to create a txnid set and never put anything in it.
    m_txnids.create_no_array();
}

void txnid_set::destroy(void) {
    m_txnids.destroy();
}

// Return true if the given transaction id is a member of the set.
// Otherwise, return false.
bool txnid_set::contains(TXNID txnid) const {
    TXNID find_txnid;
    int r = m_txnids.find_zero<TXNID, find_by_txnid>(txnid, &find_txnid, nullptr);
    return r == 0 ? true : false;
}

// Add a given txnid to the set
void txnid_set::add(TXNID txnid) {
    int r = m_txnids.insert<TXNID, find_by_txnid>(txnid, txnid, nullptr);
    invariant(r == 0 || r == DB_KEYEXIST);
}

// Delete a given txnid from the set.
void txnid_set::remove(TXNID txnid) {
    uint32_t idx;
    int r = m_txnids.find_zero<TXNID, find_by_txnid>(txnid, nullptr, &idx);
    if (r == 0) {
        r = m_txnids.delete_at(idx);
        invariant_zero(r);
    }
}

// Return the size of the set
size_t txnid_set::size(void) const {
    return m_txnids.size();
}

// Get the ith id in the set, assuming that the set is sorted.
TXNID txnid_set::get(size_t i) const {
    TXNID txnid;
    int r = m_txnids.fetch(i, &txnid);
    invariant_zero(r);
    return txnid;
}

} /* namespace toku */
