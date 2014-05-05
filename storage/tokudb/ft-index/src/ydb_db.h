/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_YDB_DB_H
#define TOKU_YDB_DB_H

#include <ft/ft.h>

#include "ydb-internal.h"
#include "ydb_txn.h"

typedef enum {
    YDB_LAYER_DIRECTORY_WRITE_LOCKS = 0,        /* total directory write locks taken */
    YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL,   /* total directory write locks unable to be taken */
    YDB_LAYER_LOGSUPPRESS,                  /* number of times logs are suppressed for empty table (2440) */
    YDB_LAYER_LOGSUPPRESS_FAIL,             /* number of times unable to suppress logs for empty table (2440) */
    YDB_DB_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_db_lock_layer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_DB_LAYER_STATUS_NUM_ROWS];
} YDB_DB_LAYER_STATUS_S, *YDB_DB_LAYER_STATUS;

void ydb_db_layer_get_status(YDB_DB_LAYER_STATUS statp);

//
// export the following locktree create/destroy callbacks so
// the environment can pass them to the locktree manager.
//
struct lt_on_create_callback_extra {
    DB_TXN *txn;
    FT_HANDLE ft_handle;
};
int toku_db_lt_on_create_callback(toku::locktree *lt, void *extra);
void toku_db_lt_on_destroy_callback(toku::locktree *lt);

/* db methods */
static inline int db_opened(DB *db) {
    return db->i->opened != 0;
}

static inline ft_compare_func
toku_db_get_compare_fun(DB* db) {
    return toku_ft_get_bt_compare(db->i->ft_handle);
}

int toku_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn);
int toku_db_open_iname(DB * db, DB_TXN * txn, const char *iname, uint32_t flags, int mode);
int toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn);
int toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, uint32_t flags);
int toku_db_create(DB ** db, DB_ENV * env, uint32_t flags);
int toku_db_close(DB * db);
int toku_setup_db_internal (DB **dbp, DB_ENV *env, uint32_t flags, FT_HANDLE ft_handle, bool is_open);
int db_getf_set(DB *db, DB_TXN *txn, uint32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra);
int autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, uint32_t flags);

//TODO: DB_AUTO_COMMIT.
//TODO: Nowait only conditionally?
//TODO: NOSYNC change to SYNC if DB_ENV has something in set_flags
static inline int 
toku_db_construct_autotxn(DB* db, DB_TXN **txn, bool* changed, bool force_auto_commit) {
    assert(db && txn && changed);
    DB_ENV* env = db->dbenv;
    if (*txn || !(env->i->open_flags & DB_INIT_TXN)) {
        *changed = false;
        return 0;
    }
    bool nosync = (bool)(!force_auto_commit && !(env->i->open_flags & DB_AUTO_COMMIT));
    uint32_t txn_flags = DB_TXN_NOWAIT | (nosync ? DB_TXN_NOSYNC : 0);
    int r = toku_txn_begin(env, NULL, txn, txn_flags);
    if (r!=0) return r;
    *changed = true;
    return 0;
}

static inline int 
toku_db_destruct_autotxn(DB_TXN *txn, int r, bool changed) {
    if (!changed) return r;
    if (r==0) {
        r = locked_txn_commit(txn, 0);
    }
    else {
        locked_txn_abort(txn);
    }
    return r; 
}

#endif /* TOKU_YDB_DB_H */
