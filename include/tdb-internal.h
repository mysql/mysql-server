#ifndef _TDB_INTERNAL_H
#define _TDB_INTERNAL_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "toku_list.h"
// Included by db.h, defines some internal structures.  These structures are inlined in some versions of db.h
// the types DB_TXN and so forth have been defined.

//// This list structure is repeated here (from toku_list.h) so that the db.h file will be standalone.  Any code that depends on this list matching the structure in toku_list.h
//// will get flagged by the compiler if someone changes one but not the other.   See #2276.
//struct toku_list {
//    struct toku_list *next, *prev;
//};

struct simple_dbt {
    u_int32_t len;
    void     *data;
};

struct __toku_db_txn_internal {
    //TXNID txnid64; /* A sixty-four bit txn id. */
    struct tokutxn *tokutxn;
    struct __toku_lth *lth;  //Hash table holding list of dictionaries this txn has touched
    u_int32_t flags;
    DB_TXN *child;
    struct toku_list dbs_that_must_close_before_abort;
};

struct __toku_dbc_internal {
    struct brt_cursor *c;
    DB_TXN *txn;
    struct simple_dbt skey_s,sval_s;
    struct simple_dbt *skey,*sval;
};

// end of _TDB_INTERNAL_H:
#endif
