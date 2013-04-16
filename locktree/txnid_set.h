/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_TXNID_SET_H
#define TOKU_TXNID_SET_H

#include <ft/fttypes.h>

#include <util/omt.h>

namespace toku {

class txnid_set {
public:
    // effect: Creates an empty set. Does not malloc space for
    //         any entries yet. That is done lazily on add().
    void create(void);

    // effect: Destroy the set's internals.
    void destroy(void);

    // returns: True if the given txnid is a member of the set.
    bool contains(TXNID id) const;

    // effect: Adds a given txnid to the set if it did not exist
    void add(TXNID txnid);

    // effect: Deletes a txnid from the set if it exists.
    void remove(TXNID txnid);

    // returns: Size of the set
    size_t size(void) const;

    // returns: The "i'th" id in the set, as if it were sorted.
    TXNID get(size_t i) const;

private:
    toku::omt<TXNID> m_txnids;

    friend class txnid_set_unit_test;
};
ENSURE_POD(txnid_set);

} /* namespace toku */

#endif /* TOKU_TXNID_SET_H */
