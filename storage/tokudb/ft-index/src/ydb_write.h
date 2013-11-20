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
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_WRITE_H)
#define TOKU_YDB_WRITE_H


typedef enum {
    YDB_LAYER_NUM_INSERTS = 0,
    YDB_LAYER_NUM_INSERTS_FAIL,
    YDB_LAYER_NUM_DELETES,
    YDB_LAYER_NUM_DELETES_FAIL,
    YDB_LAYER_NUM_UPDATES,
    YDB_LAYER_NUM_UPDATES_FAIL,
    YDB_LAYER_NUM_UPDATES_BROADCAST,
    YDB_LAYER_NUM_UPDATES_BROADCAST_FAIL,
    YDB_LAYER_NUM_MULTI_INSERTS,
    YDB_LAYER_NUM_MULTI_INSERTS_FAIL,
    YDB_LAYER_NUM_MULTI_DELETES,
    YDB_LAYER_NUM_MULTI_DELETES_FAIL,
    YDB_LAYER_NUM_MULTI_UPDATES,
    YDB_LAYER_NUM_MULTI_UPDATES_FAIL,
    YDB_WRITE_LAYER_STATUS_NUM_ROWS              /* number of rows in this status array */
} ydb_write_lock_layer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[YDB_WRITE_LAYER_STATUS_NUM_ROWS];
} YDB_WRITE_LAYER_STATUS_S, *YDB_WRITE_LAYER_STATUS;

void ydb_write_layer_get_status(YDB_WRITE_LAYER_STATUS statp);


int toku_db_del(DB *db, DB_TXN *txn, DBT *key, uint32_t flags, bool holds_mo_lock);
int toku_db_put(DB *db, DB_TXN *txn, DBT *key, DBT *val, uint32_t flags, bool holds_mo_lock);
int autotxn_db_del(DB* db, DB_TXN* txn, DBT* key, uint32_t flags);
int autotxn_db_put(DB* db, DB_TXN* txn, DBT* key, DBT* data, uint32_t flags);
int autotxn_db_update(DB *db, DB_TXN *txn, const DBT *key, const DBT *update_function_extra, uint32_t flags);
int autotxn_db_update_broadcast(DB *db, DB_TXN *txn, const DBT *update_function_extra, uint32_t flags);
int env_put_multiple(
    DB_ENV *env,
    DB *src_db,
    DB_TXN *txn,
    const DBT *src_key, const DBT *src_val,
    uint32_t num_dbs,
    DB **db_array,
    DBT_ARRAY *keys, DBT_ARRAY *vals,
    uint32_t *flags_array
    );
int env_del_multiple(
    DB_ENV *env,
    DB *src_db,
    DB_TXN *txn,
    const DBT *src_key,
    const DBT *src_val,
    uint32_t num_dbs,
    DB **db_array,
    DBT_ARRAY *keys,
    uint32_t *flags_array
    );
int env_update_multiple(
    DB_ENV *env,
    DB *src_db,
    DB_TXN *txn,
    DBT *old_src_key, DBT *old_src_data,
    DBT *new_src_key, DBT *new_src_data,
    uint32_t num_dbs,
    DB **db_array,
    uint32_t* flags_array,
    uint32_t num_keys, DBT_ARRAY keys[],
    uint32_t num_vals, DBT_ARRAY vals[]
    );




#endif
