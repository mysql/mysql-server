#ifndef _TDB_INTERNAL_H
#define _TDB_INTERNAL_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// Included by db.h, defines some internal structures.  These structures are inlined in some versions of db.h
// the types DB_TXN and so forth have been defined.

struct simple_dbt {
    u_int32_t len;
    void     *data;
};

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    struct tokutxn *tokutxn;
    struct __toku_lth *lth;
    u_int32_t flags;
    DB_TXN *child, *next, *prev;
};

struct __toku_dbc_internal {
    struct brt_cursor *c;
    DB_TXN *txn;
    struct simple_dbt skey_s,sval_s;
    struct simple_dbt *skey,*sval;
};

// end of _TDB_INTERNAL_H:
#endif
