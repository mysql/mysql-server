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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#ifndef TOKU_INDEXER_H
#define TOKU_INDEXER_H


// locking and unlocking functions to synchronize cursor position with
// XXX_multiple APIs
void toku_indexer_lock(DB_INDEXER* indexer);

void toku_indexer_unlock(DB_INDEXER* indexer);
bool toku_indexer_may_insert(DB_INDEXER* indexer, const DBT* key);
void toku_indexer_update_estimate(DB_INDEXER* indexer);

// The indexer populates multiple destination db's from the contents of one source db.
// While the indexes are being built by the indexer, the application may continue to 
// change the contents of the source db.  The changes will be reflected into the destination
// db's by the indexer.
//
// Each indexer references one source db.
// A source db may have multiple indexers referencing it.
// Each indexer references one or more destination db's.
// Each destination db references the one and only indexer that is building it.
//
// env must be set to the YDB environment
// txn must be set to the transaction under which the indexer will run
// *indexer is set to the address of the indexer object returned by the create function
// src_db is the source db
// N is the number of destination db's
// dest_dbs is an array of pointers to destination db's
// db_flags is currently unused
// indexer_flags is currently unused
//
// Returns 0 if the indexer has been created and sets *indexer to the indexer object.
// If an error occurred while creating the indexer object, a non-zero error number is returned.
//
// Clients must not operate on any of the dest_dbs concurrently with create_indexer();
int toku_indexer_create_indexer(DB_ENV *env,
                                DB_TXN *txn,
                                DB_INDEXER **indexer,
                                DB *src_db,
                                int N,
                                DB *dest_dbs[/*N*/],
                                uint32_t db_flags[/*N*/],
                                uint32_t indexer_flags) __attribute__((__visibility__("default")));

// Set the indexer poll function
int toku_indexer_set_poll_function(DB_INDEXER *indexer,
                                   int (*poll_function)(void *poll_extra,
                                                        float progress),
                                   void *poll_extra);

// Set the indexer error callback
int toku_indexer_set_error_callback(DB_INDEXER *indexer,
                                    void (*error_cb)(DB *db, int i, int err,
                                                     DBT *key, DBT *val,
                                                     void *error_extra),
                                    void *error_extra);

// Is the key right of the indexer's leaf entry cursor?
// Returns true  if right of le_cursor
// Returns false if left or equal to le_cursor
bool toku_indexer_should_insert_key(DB_INDEXER *indexer, const DBT *key);

// Get the indexer's source db
DB *toku_indexer_get_src_db(DB_INDEXER *indexer);

// TEST set the indexer's test flags
extern "C" void toku_indexer_set_test_only_flags(DB_INDEXER *indexer, int flags) __attribute__((__visibility__("default")));

#define INDEXER_TEST_ONLY_ERROR_CALLBACK 1

typedef enum {
    INDEXER_CREATE = 0,     // number of indexers successfully created
    INDEXER_CREATE_FAIL,    // number of calls to toku_indexer_create_indexer() that failed
    INDEXER_BUILD,          // number of calls to indexer->build() succeeded
    INDEXER_BUILD_FAIL,     // number of calls to indexer->build() failed
    INDEXER_CLOSE,          // number of calls to indexer->close() that succeeded
    INDEXER_CLOSE_FAIL,     // number of calls to indexer->close() that failed
    INDEXER_ABORT,          // number of calls to indexer->abort()
    INDEXER_CURRENT,        // number of indexers currently in existence
    INDEXER_MAX,            // max number of indexers that ever existed simultaneously
    INDEXER_STATUS_NUM_ROWS
} indexer_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[INDEXER_STATUS_NUM_ROWS];
} INDEXER_STATUS_S, *INDEXER_STATUS;

void toku_indexer_get_status(INDEXER_STATUS s);


#endif // TOKU_INDEXER_H
