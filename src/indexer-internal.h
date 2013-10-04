/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#ifndef TOKU_INDEXER_INTERNAL_H
#define TOKU_INDEXER_INTERNAL_H

#include <ft/txn_state.h>
#include <toku_pthread.h>

// the indexer_commit_keys is an ordered set of keys described by a DBT in the keys array.
// the array is a resizeable array with max size "max_keys" and current size "current_keys".
// the ordered set is used by the hotindex undo function to collect the commit keys.
struct indexer_commit_keys {
    int max_keys;        // max number of keys
    int current_keys;    // number of valid keys
    DBT *keys;           // the variable length keys array
};

// a ule and all of its provisional txn info
// used by the undo-do algorithm to gather up ule provisional info in
// a cursor callback that provides exclusive access to the source DB
// with respect to txn commit and abort
struct ule_prov_info {
    // these are pointers to the allocated leafentry and ule needed to calculate
    // provisional info. we only borrow them - whoever created the provisional info
    // is responsible for cleaning up the leafentry and ule when done.
    LEAFENTRY le;
    ULEHANDLE ule;
    void* key;
    uint32_t keylen;
    // provisional txn info for the ule
    uint32_t num_provisional;
    uint32_t num_committed;
    TXNID *prov_ids;
    TOKUTXN *prov_txns;
    TOKUTXN_STATE *prov_states;
};

struct __toku_indexer_internal {
    DB_ENV *env;
    DB_TXN *txn;
    toku_mutex_t indexer_lock;
    toku_mutex_t indexer_estimate_lock;
    DBT position_estimate;
    DB *src_db;
    int N;
    DB **dest_dbs; /* [N] */
    uint32_t indexer_flags;
    void (*error_callback)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra);
    void *error_extra;
    int  (*poll_func)(void *poll_extra, float progress);
    void *poll_extra;
    uint64_t estimated_rows; // current estimate of table size
    uint64_t loop_mod;       // how often to call poll_func
    LE_CURSOR lec;
    FILENUM  *fnums; /* [N] */
    FILENUMS filenums;

    // undo state
    struct indexer_commit_keys commit_keys; // set of keys to commit
    DBT_ARRAY *hot_keys;
    DBT_ARRAY *hot_vals;

    // test functions
    int (*undo_do)(DB_INDEXER *indexer, DB *hotdb, DBT* key, ULEHANDLE ule);
    TOKUTXN_STATE (*test_xid_state)(DB_INDEXER *indexer, TXNID xid);
    void (*test_lock_key)(DB_INDEXER *indexer, TXNID xid, DB *hotdb, DBT *key);
    int (*test_delete_provisional)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
    int (*test_delete_committed)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
    int (*test_insert_provisional)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
    int (*test_insert_committed)(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
    int (*test_commit_any)(DB_INDEXER *indexer, DB *db, DBT *key, XIDS xids);

    // test flags
    int test_only_flags;
};

void indexer_undo_do_init(DB_INDEXER *indexer);

void indexer_undo_do_destroy(DB_INDEXER *indexer);

int indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info *prov_info, DBT_ARRAY *hot_keys, DBT_ARRAY *hot_vals);

#endif
