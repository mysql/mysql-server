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

#ifndef TOKU_LOADER_H
#define TOKU_LOADER_H

/*
Create and set up a loader.
 - The loader will operate in environment env, and the load will happen within transaction txn.
 - You must remember to close (or abort) the loader eventually (otherwise the resulting DBs will 
   not be valid, and you will have a memory leak).
 - The number of databases to be loaded is N.
 - The databases must already be open, and their handles are passed in in the array dbs.   
   In particular dbs[i] is the ith database.
 - The loader will work right whether the DBs are empty or full.  However if any of the DBs are not empty, 
   it may not be fast (e.g., the loader may simply perform DB->put() operations).   
 - For each row that is put into the loader, for i over each of the N DBs, generate_row is invoked on the 
   row to generate a secondary row.
 - The DBTs passed to generate_row() will have the DB_DBT_REALLOC flag set, and the extract 
   function should realloc the memory passed in.  The ulen field indicates how large the realloc'd 
   storage is, and if the extract function does perform a realloc it should update the ulen field.
 - We require that the extract function always return 0.  
 - The generate_row function must be thread safe.
 - Whenever two rows in dbs[i] need to be compared we use that db's comparison function.  The 
   comparison function must be thread safe.
 - DBs must have been set up with descriptors and comparison functions before calling any extract 
   or compare functions.
 - loader_flags is used to specify loader specific behavior.  For instance, LOADER_USE_PUTS tells the 
   loader to use traditional puts to save disk space while loading (at the cost of performance)
 - The new loader is returned in *blp.

 Modifies: :: env, txn, blp, and dbs.
*/
int toku_loader_create_loader(DB_ENV *env, DB_TXN *txn, DB_LOADER **blp, DB *src_db, int N, DB *dbs[/*N*/], uint32_t db_flags[/*N*/], uint32_t dbt_flags[/*N*/], uint32_t loader_flags, bool check_empty);


/*
Set a error callback. 
 - If at any point during the load the system notices that an error has occurred, error information is recorded. 
 - The callback function may be called during DB_LOADER->close() or DB_LOADER->abort(), at which time the error 
   information is returned. 
 - A key-val pair for one of the errors is returned along with the db, and the index i indicating which db 
   had the problem. 
 - This function will be called at most once (so even if there are many problems, only one call will be made.) 
 - If a duplicate is discovered, the error is DB_KEYEXIST. 
 - The error_extra passed at the time of set_error_callback is the value passed as the error_extra when an error occurs.
*/
int toku_loader_set_error_callback(DB_LOADER *loader, void (*error_cb)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra), void *error_extra);


/*
Set the polling function. 
 - During the DB_LOADER->close operation, the poll function is called periodically. 
 - If it ever returns nonzero, then the loader stops as soon as possible. 
 - The poll function is called with the extra passed into the loader create function. 
 - A floating point number is also returned, which ranges from 0.0 to 1.0, indicating progress. Progress of 0.0 means 
   no progress so far. Progress of 0.5 means that the job is about half done. Progress of 1.0 means the job is done. 
   The progress is just an estimate.
*/
int toku_loader_set_poll_function(DB_LOADER *loader, int (*poll_func)(void *poll_extra, float progress), void *poll_extra);


/*
Give a row to the loader.
 - Returns zero if no error, non-zero if error.
 - When the application sees a non-zero return from put(), it must abort(), which would then call the error callback. 
 - Once put() returns a non-zero value, any loader calls other than abort() are unsupported and will result in undefined behavior.
*/
int toku_loader_put(DB_LOADER *loader, DBT *key, DBT *val);


/*
Finish the load, 
 - Take all the rows and put them into dictionaries which are returned as open handlers through the original dbs array. 
 - Frees all the memory allocated by the loader. 
 - You may not use the loader handle again after calling close.
 - The system will return an DB_KEYEXIST if in any of the resulting databases, there are two different rows with keys 
   that compare to be equal (and the duplicate callback function, if set, is called first).
 - If the polling function has been set, the loader will periodically call the polling function. If the polling function 
   ever returns a nonzero value, then the loader will return immediately, possibly with the dictionaries in some 
   inconsistent state. (To get them to a consistent state, the enclosing transaction should abort.)
 - To free the resources used by a loader, either DB_LOADER->close or DB_LOADER->abort must be called. After calling either 
   of those functions, no further loader operations can be performed with that loader.
 - The DBs remain open after the loader is closed.
*/
int toku_loader_close(DB_LOADER *loader);


/*
Abort the load, 
 - Possibly leave none, some, or all of the puts in effect. You may need to abort the enclosing transaction to get 
   back to a sane state.
 - To free the resources used by a loader, either DB_LOADER->close or DB_LOADER->abort must be called. After calling either 
   of those functions, no further loader operations can be performed with that loader.
 - The DBs remain open after the loader is aborted.
 */
int toku_loader_abort(DB_LOADER *loader);

// Remove any loader temp files that may have been left from a crashed system
int toku_loader_cleanup_temp_files(DB_ENV *env);


typedef enum {
    LOADER_CREATE = 0,      // number of loaders successfully created
    LOADER_CREATE_FAIL,     // number of calls to toku_loader_create_loader() that failed
    LOADER_PUT,             // number of calls to toku_loader_put() that succeeded
    LOADER_PUT_FAIL,        // number of calls to toku_loader_put() that failed
    LOADER_CLOSE,           // number of calls to toku_loader_close()
    LOADER_CLOSE_FAIL,      // number of calls to toku_loader_close() that failed
    LOADER_ABORT,           // number of calls to toku_loader_abort()
    LOADER_CURRENT,         // number of loaders currently in existence
    LOADER_MAX,             // max number of loaders that ever existed simultaneously
    LOADER_STATUS_NUM_ROWS
} loader_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LOADER_STATUS_NUM_ROWS];
} LOADER_STATUS_S, *LOADER_STATUS;


void toku_loader_get_status(LOADER_STATUS s);


#endif
