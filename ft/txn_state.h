/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#if !defined(TOKUTXN_STATE_H)
#define TOKUTXN_STATE_H

// this is a separate file so that the hotindexing tests can see the txn states

enum tokutxn_state {
    TOKUTXN_LIVE,         // initial txn state
    TOKUTXN_PREPARING,    // txn is preparing (or prepared)
    TOKUTXN_COMMITTING,   // txn in the process of committing
    TOKUTXN_ABORTING,     // txn in the process of aborting
    TOKUTXN_RETIRED,      // txn no longer exists
};
typedef enum tokutxn_state TOKUTXN_STATE;

#endif
