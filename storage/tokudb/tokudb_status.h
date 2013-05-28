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
#ifndef _TOKUDB_STATUS_H
#define _TOKUDB_STATUS_H

// These are keys that will be used for retrieving metadata in status.tokudb
// To get the version, one looks up the value associated with key hatoku_version
// in status.tokudb
typedef ulonglong HA_METADATA_KEY;
#define hatoku_old_version 0
#define hatoku_capabilities 1
#define hatoku_max_ai 2 //maximum auto increment value found so far
#define hatoku_ai_create_value 3
#define hatoku_key_name 4
#define hatoku_frm_data 5
#define hatoku_new_version 6
#define hatoku_cardinality 7

// use a very small pagesize for the status dictionary
#define status_dict_pagesize 1024

namespace tokudb {

    // get the value for a given key in the status dictionary. copy the value to the supplied buffer.
    // returns 0 if successful.
    int get_status(DB *status_db, DB_TXN *txn, HA_METADATA_KEY k, void *p, size_t s, size_t *sp) {
        DBT key = {}; key.data = &k; key.size = sizeof k;
        DBT val = {}; val.data = p; val.ulen = (uint32_t) s; val.flags = DB_DBT_USERMEM;
        int error = status_db->get(status_db, txn, &key, &val, 0);
        if (error == 0) {
            *sp = val.size;
        }
        return error;
    }

    // get the value for a given key in the status dictionary. put the value in a realloced buffer.
    // returns 0 if successful.
    int get_status_realloc(DB *status_db, DB_TXN *txn, HA_METADATA_KEY k, void **pp, size_t *sp) {
        DBT key = {}; key.data = &k; key.size = sizeof k;
        DBT val = {}; val.data = *pp; val.size = (uint32_t) *sp; val.flags = DB_DBT_REALLOC;
        int error = status_db->get(status_db, txn, &key, &val, 0);
        if (error == 0) {
            *pp = val.data;
            *sp = val.size;
        }
        return error;
    }

    // write a key value pair into the status dictionary, overwriting the previous value if any.
    // auto create a txn if necessary.
    // returns 0 if successful.
    int write_metadata(DB *status_db, void *key_data, uint key_size, void* val_data, uint val_size, DB_TXN *txn) {
        DBT key = {}; key.data = key_data; key.size = key_size;
        DBT value = {}; value.data = val_data; value.size = val_size;
        int error = status_db->put(status_db, txn, &key, &value, 0);
        return error;
    }

    // write a key value pair into the status dictionary, overwriting the previous value if any.
    // the key must be a HA_METADATA_KEY.
    // returns 0 if successful.
    int write_to_status(DB *status_db, HA_METADATA_KEY curr_key_data, void *val, size_t val_size, DB_TXN *txn) {
        return write_metadata(status_db, &curr_key_data, sizeof curr_key_data, val, val_size, txn);
    }

    // remove a key from the status dictionary.
    // auto create a txn if necessary.
    // returns 0 if successful.
    int remove_metadata(DB *status_db, void *key_data, uint key_size, DB_TXN *txn) {
        DBT key = {}; key.data = key_data; key.size = key_size;
        int error = status_db->del(status_db, txn, &key, DB_DELETE_ANY);
        return error;
    }

    // remove a key from the status dictionary.
    // the key must be a HA_METADATA_KEY
    // returns 0 if successful.
    int remove_from_status(DB *status_db, HA_METADATA_KEY curr_key_data, DB_TXN *txn) {
        return remove_metadata(status_db, &curr_key_data, sizeof curr_key_data, txn);
    }

    int close_status(DB **status_db_ptr) {
        int error = 0;
        DB *status_db = *status_db_ptr;
        if (status_db) {
            error = status_db->close(status_db, 0);
            if (error == 0)
                *status_db_ptr = NULL;
        }
        return error;
    }

    int create_status(DB_ENV *env, DB **status_db_ptr, const char *name, DB_TXN *txn) {
        int error;
        DB *status_db = NULL;

        error = db_create(&status_db, env, 0);
        if (error == 0) {
            error = status_db->set_pagesize(status_db, status_dict_pagesize);
        }
        if (error == 0) {
            error = status_db->open(status_db, txn, name, NULL, DB_BTREE, DB_CREATE | DB_EXCL, 0);
        }
        if (error == 0) {
            *status_db_ptr = status_db;
        } else {
            int r = close_status(&status_db);
            assert(r == 0);
        }
        return error;
    }

    int open_status(DB_ENV *env, DB **status_db_ptr, const char *name, DB_TXN *txn) {
        int error = 0;
        DB *status_db = NULL;
        error = db_create(&status_db, env, 0);
        if (error == 0) {
            error = status_db->open(status_db, txn, name, NULL, DB_BTREE, DB_THREAD, 0);
        }
        if (error == 0) {
            uint32_t pagesize = 0;
            error = status_db->get_pagesize(status_db, &pagesize);                
            if (error == 0 && pagesize > status_dict_pagesize) {
                error = status_db->change_pagesize(status_db, status_dict_pagesize);
            }
        }
        if (error == 0) {
            *status_db_ptr = status_db;
        } else {
            int r = close_status(&status_db);
            assert(r == 0);
        }
        return error;
    }
}

#endif
