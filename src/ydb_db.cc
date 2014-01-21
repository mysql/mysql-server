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

#include <ctype.h>

#include <db.h>
#include <locktree/locktree.h>
#include <ft/ft.h>
#include <ft/ft-flusher.h>
#include <ft/checkpoint.h>
#include <ft/log_header.h>

#include "ydb_cursor.h"
#include "ydb_row_lock.h"
#include "ydb_db.h"
#include "ydb_write.h"
#include "ydb-internal.h"
#include "ydb_load.h"
#include "indexer.h"
#include <portability/toku_atomic.h>
#include <util/status.h>
#include <ft/le-cursor.h>

static YDB_DB_LAYER_STATUS_S ydb_db_layer_status;
#ifdef STATUS_VALUE
#undef STATUS_VALUE
#endif
#define STATUS_VALUE(x) ydb_db_layer_status.status[x].value.num

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(ydb_db_layer_status, k, c, t, l, inc)

static void
ydb_db_layer_status_init (void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(YDB_LAYER_DIRECTORY_WRITE_LOCKS,      nullptr, UINT64,   "directory write locks", TOKU_ENGINE_STATUS);
    STATUS_INIT(YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL, nullptr, UINT64,   "directory write locks fail", TOKU_ENGINE_STATUS);
    STATUS_INIT(YDB_LAYER_LOGSUPPRESS,                nullptr, UINT64,   "log suppress", TOKU_ENGINE_STATUS);
    STATUS_INIT(YDB_LAYER_LOGSUPPRESS_FAIL,           nullptr, UINT64,   "log suppress fail", TOKU_ENGINE_STATUS);
    ydb_db_layer_status.initialized = true;
}
#undef STATUS_INIT

void
ydb_db_layer_get_status(YDB_DB_LAYER_STATUS statp) {
    if (!ydb_db_layer_status.initialized)
        ydb_db_layer_status_init();
    *statp = ydb_db_layer_status;
}

static void
create_iname_hint(const char *dname, char *hint) {
    //Requires: size of hint array must be > strlen(dname)
    //Copy alphanumeric characters only.
    //Replace strings of non-alphanumeric characters with a single underscore.
    bool underscored = false;
    while (*dname) {
        if (isalnum(*dname)) {
            char c = *dname++;
            *hint++ = c;
            underscored = false;
        }
        else {
            if (!underscored)
                *hint++ = '_';
            dname++;
            underscored = true;
        }
    }
    *hint = '\0';
}

// n < 0  means to ignore mark and ignore n
// n >= 0 means to include mark ("_B_" or "_P_") with hex value of n in iname
// (intended for use by loader, which will create many inames using one txnid).
static char *
create_iname(DB_ENV *env, uint64_t id1, uint64_t id2, char *hint, const char *mark, int n) {
    int bytes;
    char inamebase[strlen(hint) +
                   8 +  // hex file format version
                   24 + // hex id (normally the txnid's parent and child)
                   8  + // hex value of n if non-neg
                   sizeof("_B___.") + // extra pieces
                   strlen(toku_product_name)];
    if (n < 0)
        bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%" PRIx64 "_%" PRIx64 "_%" PRIx32            ".%s",
                         hint, id1, id2, FT_LAYOUT_VERSION, toku_product_name);
    else {
        invariant(strlen(mark) == 1);
        bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%" PRIx64 "_%" PRIx64 "_%" PRIx32 "_%s_%" PRIx32 ".%s",
                         hint, id1, id2, FT_LAYOUT_VERSION, mark, n, toku_product_name);
    }
    assert(bytes>0);
    assert(bytes<=(int)sizeof(inamebase)-1);
    char *rval;
    if (env->i->data_dir)
        rval = toku_construct_full_name(2, env->i->data_dir, inamebase);
    else
        rval = toku_construct_full_name(1, inamebase);
    assert(rval);
    return rval;
}

static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, uint32_t flags, int mode);

// Effect: Do the work required of DB->close().
// requires: the multi_operation client lock is held.
int 
toku_db_close(DB * db) {
    int r = 0;
    if (db_opened(db) && db->i->dname) {
        // internal (non-user) dictionary has no dname
        env_note_db_closed(db->dbenv, db);  // tell env that this db is no longer in use by the user of this api (user-closed, may still be in use by fractal tree internals)
    }
    // close the ft handle, and possibly close the locktree
    toku_ft_handle_close(db->i->ft_handle);
    if (db->i->lt) {
        db->dbenv->i->ltm.release_lt(db->i->lt);
    }
    toku_sdbt_cleanup(&db->i->skey);
    toku_sdbt_cleanup(&db->i->sval);
    if (db->i->dname) {
        toku_free(db->i->dname);
    }
    toku_free(db->i);
    toku_free(db);
    return r;
}

///////////
//db_getf_XXX is equivalent to c_getf_XXX, without a persistent cursor

int
db_getf_set(DB *db, DB_TXN *txn, uint32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    DBC *c;
    uint32_t create_flags = flags & (DB_ISOLATION_FLAGS | DB_RMW);
    flags &= ~DB_ISOLATION_FLAGS;
    int r = toku_db_cursor_internal(db, txn, &c, create_flags | DBC_DISABLE_PREFETCHING, 1);
    if (r==0) {
        r = toku_c_getf_set(c, flags, key, f, extra);
        int r2 = toku_c_close(c);
        if (r==0) r = r2;
    }
    return r;
}

static inline int 
db_thread_need_flags(DBT *dbt) {
    return (dbt->flags & (DB_DBT_MALLOC+DB_DBT_REALLOC+DB_DBT_USERMEM)) == 0;
}

int 
toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, uint32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;
    uint32_t iso_flags = flags & DB_ISOLATION_FLAGS;

    if ((db->i->open_flags & DB_THREAD) && db_thread_need_flags(data))
        return EINVAL;

    uint32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);
    flags &= ~lock_flags;
    flags &= ~DB_ISOLATION_FLAGS;
    // And DB_GET_BOTH is no longer supported. #2862.
    if (flags != 0) return EINVAL;

    DBC *dbc;
    r = toku_db_cursor_internal(db, txn, &dbc, iso_flags | DBC_DISABLE_PREFETCHING, 1);
    if (r!=0) return r;
    uint32_t c_get_flags = DB_SET;
    r = toku_c_get(dbc, key, data, c_get_flags | lock_flags);
    int r2 = toku_c_close(dbc);
    return r ? r : r2;
}

static int
db_open_subdb(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, uint32_t flags, int mode) {
    int r;
    if (!fname || !dbname) r = EINVAL;
    else {
        char subdb_full_name[strlen(fname) + sizeof("/") + strlen(dbname)];
        int bytes = snprintf(subdb_full_name, sizeof(subdb_full_name), "%s/%s", fname, dbname);
        assert(bytes==(int)sizeof(subdb_full_name)-1);
        const char *null_subdbname = NULL;
        r = toku_db_open(db, txn, subdb_full_name, null_subdbname, dbtype, flags, mode);
    }
    return r;
}

static uint64_t nontransactional_open_id = 0;

// inames are created here.
// algorithm:
//  begin txn
//  convert dname to iname (possibly creating new iname)
//  open file (toku_ft_handle_open() will handle logging)
//  close txn
//  if created a new iname, take full range lock
// Requires: no checkpoint may take place during this function, which is enforced by holding the multi_operation_client_lock.
static int 
toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, uint32_t flags, int mode) {
    HANDLE_PANICKED_DB(db);
    HANDLE_READ_ONLY_TXN(txn);
    if (dbname != NULL) {
        return db_open_subdb(db, txn, fname, dbname, dbtype, flags, mode);
    }

    // at this point fname is the dname
    //This code ONLY supports single-db files.
    assert(dbname == NULL);
    const char * dname = fname;  // db_open_subdb() converts (fname, dbname) to dname

    ////////////////////////////// do some level of parameter checking.
    uint32_t unused_flags = flags;
    int r;
    if (dbtype!=DB_BTREE && dbtype!=DB_UNKNOWN) return EINVAL;
    int is_db_excl    = flags & DB_EXCL;    unused_flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  unused_flags&=~DB_CREATE;
    int is_db_hot_index  = flags & DB_IS_HOT_INDEX;  unused_flags&=~DB_IS_HOT_INDEX;

    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
    unused_flags&=~DB_READ_UNCOMMITTED;
    unused_flags&=~DB_READ_COMMITTED;
    unused_flags&=~DB_SERIALIZABLE;

    // DB_THREAD is implicitly supported and DB_BLACKHOLE is supported at the ft-layer
    unused_flags &= ~DB_THREAD;
    unused_flags &= ~DB_BLACKHOLE;

    // check for unknown or conflicting flags
    if (unused_flags) return EINVAL; // unknown flags
    if (is_db_excl && !is_db_create) return EINVAL;
    if (dbtype==DB_UNKNOWN && is_db_excl) return EINVAL;

    if (db_opened(db)) {
        // it was already open
        return EINVAL;
    }
    //////////////////////////////

    // convert dname to iname
    //  - look up dname, get iname
    //  - if dname does not exist, create iname and make entry in directory
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds iname_in_env
    toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
    toku_init_dbt_flags(&iname_dbt, DB_DBT_REALLOC);
    r = toku_db_get(db->dbenv->i->directory, txn, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = (char *) iname_dbt.data;
    if (r == DB_NOTFOUND && !is_db_create) {
        r = ENOENT;
    } else if (r==0 && is_db_excl) {
        r = EEXIST;
    } else if (r == DB_NOTFOUND) {
        char hint[strlen(dname) + 1];

        // create iname and make entry in directory
        uint64_t id1 = 0;
        uint64_t id2 = 0;

        if (txn) {
            id1 = toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn).parent_id64;
            id2 = toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn).child_id64;
        } else {
            id1 = toku_sync_fetch_and_add(&nontransactional_open_id, 1);
        }
        create_iname_hint(dname, hint);
        iname = create_iname(db->dbenv, id1, id2, hint, NULL, -1);  // allocated memory for iname
        toku_fill_dbt(&iname_dbt, iname, strlen(iname) + 1);
        //
        // put_flags will be 0 for performance only, avoid unnecessary query
        // if we are creating a hot index, per #3166, we do not want the write lock  in directory grabbed.
        // directory read lock is grabbed in toku_db_get above
        //
        uint32_t put_flags = 0 | ((is_db_hot_index) ? DB_PRELOCKED_WRITE : 0); 
        r = toku_db_put(db->dbenv->i->directory, txn, &dname_dbt, &iname_dbt, put_flags, true);  
    }

    // we now have an iname
    if (r == 0) {
        r = toku_db_open_iname(db, txn, iname, flags, mode);
        if (r == 0) {
            db->i->dname = toku_xstrdup(dname);
            env_note_db_opened(db->dbenv, db);  // tell env that a new db handle is open (using dname)
        }
    }

    if (iname) {
        toku_free(iname);
    }
    return r;
}

// set the descriptor and cmp_descriptor to the
// descriptors from the given ft, updating the
// locktree's descriptor pointer if necessary
static void
db_set_descriptors(DB *db, FT_HANDLE ft_handle) {
    db->descriptor = toku_ft_get_descriptor(ft_handle);
    db->cmp_descriptor = toku_ft_get_cmp_descriptor(ft_handle);
    if (db->i->lt) {
        db->i->lt->set_descriptor(db->cmp_descriptor);
    }
}

// callback that sets the descriptors when 
// a dictionary is redirected at the ft layer
static void
db_on_redirect_callback(FT_HANDLE ft_handle, void* extra) {
    DB *db = (DB *) extra;
    db_set_descriptors(db, ft_handle);
}

// when a locktree is created, clone a ft handle and store it
// as userdata so we can close it later.
int toku_db_lt_on_create_callback(toku::locktree *lt, void *extra) {
    int r;
    struct lt_on_create_callback_extra *info = (struct lt_on_create_callback_extra *) extra;
    TOKUTXN ttxn = info->txn ? db_txn_struct_i(info->txn)->tokutxn : NULL;
    FT_HANDLE ft_handle = info->ft_handle;

    FT_HANDLE cloned_ft_handle;
    r = toku_ft_handle_clone(&cloned_ft_handle, ft_handle, ttxn);
    if (r == 0) {
        assert(lt->get_userdata() == NULL);
        lt->set_userdata(cloned_ft_handle);
    }
    return r;
}

// when a locktree is about to be destroyed, 
// close the ft handle stored as userdata.
void toku_db_lt_on_destroy_callback(toku::locktree *lt) {
    FT_HANDLE ft_handle = (FT_HANDLE) lt->get_userdata();
    assert(ft_handle);
    toku_ft_handle_close(ft_handle);
}

int 
toku_db_open_iname(DB * db, DB_TXN * txn, const char *iname_in_env, uint32_t flags, int mode) {
    //Set comparison functions if not yet set.
    HANDLE_READ_ONLY_TXN(txn);
    if (!db->i->key_compare_was_set && db->dbenv->i->bt_compare) {
        toku_ft_set_bt_compare(db->i->ft_handle, db->dbenv->i->bt_compare);
        db->i->key_compare_was_set = true;
    }
    if (db->dbenv->i->update_function) {
        toku_ft_set_update(db->i->ft_handle,db->dbenv->i->update_function);
    }
    toku_ft_set_redirect_callback(
        db->i->ft_handle,
        db_on_redirect_callback,
        db
        );
    bool need_locktree = (bool)((db->dbenv->i->open_flags & DB_INIT_LOCK) &&
                                (db->dbenv->i->open_flags & DB_INIT_TXN));

    int is_db_excl    = flags & DB_EXCL;    flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  flags&=~DB_CREATE;
    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
                                            flags&=~DB_READ_UNCOMMITTED;
                                            flags&=~DB_READ_COMMITTED;
                                            flags&=~DB_SERIALIZABLE;
                                            flags&=~DB_IS_HOT_INDEX;
    // unknown or conflicting flags are bad
    int unknown_flags = flags & ~DB_THREAD;
    unknown_flags &= ~DB_BLACKHOLE;
    if (unknown_flags || (is_db_excl && !is_db_create)) {
        return EINVAL;
    }

    if (db_opened(db)) {
        return EINVAL;              /* It was already open. */
    }
    
    db->i->open_flags = flags;
    db->i->open_mode = mode;

    FT_HANDLE ft_handle = db->i->ft_handle;
    int r = toku_ft_handle_open(ft_handle, iname_in_env,
                      is_db_create, is_db_excl,
                      db->dbenv->i->cachetable,
                      txn ? db_txn_struct_i(txn)->tokutxn : NULL_TXN);
    if (r != 0) {
        goto error_cleanup;
    }

    // if the dictionary was opened as a blackhole, mark the
    // fractal tree as blackhole too.
    if (flags & DB_BLACKHOLE) {
        toku_ft_set_blackhole(ft_handle);
    }

    db->i->opened = 1;

    // now that the handle has successfully opened, a valid descriptor
    // is in the ft. we need to set the db's descriptor pointers
    db_set_descriptors(db, ft_handle);

    if (need_locktree) {
        db->i->dict_id = toku_ft_get_dictionary_id(db->i->ft_handle);
        struct lt_on_create_callback_extra on_create_extra = {
            .txn = txn,
            .ft_handle = db->i->ft_handle,
        };
        db->i->lt = db->dbenv->i->ltm.get_lt(
                db->i->dict_id,
                db->cmp_descriptor,
                toku_ft_get_bt_compare(db->i->ft_handle),
                &on_create_extra);
        if (db->i->lt == nullptr) {
            r = errno;
            if (r == 0)
                r = EINVAL;
            goto error_cleanup;
        }
    }
    return 0;
 
error_cleanup:
    db->i->dict_id = DICTIONARY_ID_NONE;
    db->i->opened = 0;
    if (db->i->lt) {
        db->dbenv->i->ltm.release_lt(db->i->lt);
        db->i->lt = NULL;
    }
    return r;
}

// Return the maximum key and val size in 
// *key_size and *val_size respectively
static void
toku_db_get_max_row_size(DB * UU(db), uint32_t * max_key_size, uint32_t * max_val_size) {
    *max_key_size = 0;
    *max_val_size = 0;
    toku_ft_get_maximum_advised_key_value_lengths(max_key_size, max_val_size);
}

int toku_db_pre_acquire_fileops_lock(DB *db, DB_TXN *txn) {
    // bad hack because some environment dictionaries do not have a dname
    char *dname = db->i->dname;
    if (!dname)
        return 0;

    DBT key_in_directory = { .data = dname, .size = (uint32_t) strlen(dname)+1 };
    //Left end of range == right end of range (point lock)
    int r = toku_db_get_range_lock(db->dbenv->i->directory, txn,
            &key_in_directory, &key_in_directory,
            toku::lock_request::type::WRITE);
    if (r == 0)
        STATUS_VALUE(YDB_LAYER_DIRECTORY_WRITE_LOCKS)++;  // accountability 
    else
        STATUS_VALUE(YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL)++;  // accountability 
    return r;
}

//
// This function is used both to set an initial descriptor of a DB and to
// change a descriptor. (only way to set a descriptor of a DB)
//
// Requires:
//  - The caller must not call put_multiple, del_multiple, or update_multiple concurrently
//  - The caller must not have a hot index running concurrently on db
//  - If the caller has passed DB_UPDATE_CMP_DESCRIPTOR as a flag, then he is calling this function
//     ONLY immediately after creating the dictionary and before doing any actual work on the dictionary.
//
static int 
toku_db_change_descriptor(DB *db, DB_TXN* txn, const DBT* descriptor, uint32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_READ_ONLY_TXN(txn);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r = 0;
    TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
    DBT old_descriptor;
    bool is_db_hot_index  = ((flags & DB_IS_HOT_INDEX) != 0);
    bool update_cmp_descriptor = ((flags & DB_UPDATE_CMP_DESCRIPTOR) != 0);

    toku_init_dbt(&old_descriptor);
    if (!db_opened(db) || !descriptor || (descriptor->size>0 && !descriptor->data)){
        r = EINVAL;
        goto cleanup;
    }
    // For a hot index, this is an initial descriptor.
    // We do not support (yet) hcad with hot index concurrently on a single table, which
    // would require changing a descriptor for a hot index.
    if (!is_db_hot_index) {
        r = toku_db_pre_acquire_table_lock(db, txn);
        if (r != 0) { goto cleanup; }    
    }

    // TODO: use toku_clone_dbt(&old-descriptor, db->descriptor);
    old_descriptor.size = db->descriptor->dbt.size;
    old_descriptor.data = toku_memdup(db->descriptor->dbt.data, db->descriptor->dbt.size);

    toku_ft_change_descriptor(
        db->i->ft_handle, 
        &old_descriptor, 
        descriptor, 
        true, 
        ttxn, 
        update_cmp_descriptor
        );

cleanup:
    if (old_descriptor.data) {
        toku_free(old_descriptor.data);
    }
    return r;
}

static int 
toku_db_set_flags(DB *db, uint32_t flags) {
    HANDLE_PANICKED_DB(db);

    /* the following matches BDB */
    if (db_opened(db) && flags != 0) return EINVAL;

    return 0;
}

static int 
toku_db_get_flags(DB *db, uint32_t *pflags) {
    HANDLE_PANICKED_DB(db);
    if (!pflags) return EINVAL;
    *pflags = 0;
    return 0;
}

static int 
toku_db_change_pagesize(DB *db, uint32_t pagesize) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    toku_ft_handle_set_nodesize(db->i->ft_handle, pagesize);
    return 0;
}

static int 
toku_db_set_pagesize(DB *db, uint32_t pagesize) {
    HANDLE_PANICKED_DB(db);
    if (db_opened(db)) return EINVAL;
    toku_ft_handle_set_nodesize(db->i->ft_handle, pagesize);
    return 0;
}

static int 
toku_db_get_pagesize(DB *db, uint32_t *pagesize_ptr) {
    HANDLE_PANICKED_DB(db);
    toku_ft_handle_get_nodesize(db->i->ft_handle, pagesize_ptr);
    return 0;
}

static int 
toku_db_change_readpagesize(DB *db, uint32_t readpagesize) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    toku_ft_handle_set_basementnodesize(db->i->ft_handle, readpagesize);
    return 0;
}

static int 
toku_db_set_readpagesize(DB *db, uint32_t readpagesize) {
    HANDLE_PANICKED_DB(db);
    if (db_opened(db)) return EINVAL;
    toku_ft_handle_set_basementnodesize(db->i->ft_handle, readpagesize);
    return 0;
}

static int 
toku_db_get_readpagesize(DB *db, uint32_t *readpagesize_ptr) {
    HANDLE_PANICKED_DB(db);
    toku_ft_handle_get_basementnodesize(db->i->ft_handle, readpagesize_ptr);
    return 0;
}

static int 
toku_db_change_compression_method(DB *db, enum toku_compression_method compression_method) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    toku_ft_handle_set_compression_method(db->i->ft_handle, compression_method);
    return 0;
}

static int 
toku_db_set_compression_method(DB *db, enum toku_compression_method compression_method) {
    HANDLE_PANICKED_DB(db);
    if (db_opened(db)) return EINVAL;
    toku_ft_handle_set_compression_method(db->i->ft_handle, compression_method);
    return 0;
}

static int 
toku_db_get_compression_method(DB *db, enum toku_compression_method *compression_method_ptr) {
    HANDLE_PANICKED_DB(db);
    toku_ft_handle_get_compression_method(db->i->ft_handle, compression_method_ptr);
    return 0;
}

static int 
toku_db_change_fanout(DB *db, unsigned int fanout) {
    HANDLE_PANICKED_DB(db);
    if (!db_opened(db)) return EINVAL;
    toku_ft_handle_set_fanout(db->i->ft_handle, fanout);
    return 0;
}

static int 
toku_db_set_fanout(DB *db, unsigned int fanout) {
    HANDLE_PANICKED_DB(db);
    if (db_opened(db)) return EINVAL;
    toku_ft_handle_set_fanout(db->i->ft_handle, fanout);
    return 0;
}

static int 
toku_db_get_fanout(DB *db, unsigned int *fanout) {
    HANDLE_PANICKED_DB(db);
    toku_ft_handle_get_fanout(db->i->ft_handle, fanout);
    return 0;
}

static int
toku_db_get_fractal_tree_info64(DB *db, uint64_t *num_blocks_allocated, uint64_t *num_blocks_in_use, uint64_t *size_allocated, uint64_t *size_in_use) {
    HANDLE_PANICKED_DB(db);
    struct ftinfo64 ftinfo;
    toku_ft_handle_get_fractal_tree_info64(db->i->ft_handle, &ftinfo);
    *num_blocks_allocated = ftinfo.num_blocks_allocated;
    *num_blocks_in_use = ftinfo.num_blocks_in_use;
    *size_allocated = ftinfo.size_allocated;
    *size_in_use = ftinfo.size_in_use;
    return 0;
}

static int
toku_db_iterate_fractal_tree_block_map(DB *db, int (*iter)(uint64_t,int64_t,int64_t,int64_t,int64_t,void*), void *iter_extra) {
    HANDLE_PANICKED_DB(db);
    return toku_ft_handle_iterate_fractal_tree_block_map(db->i->ft_handle, iter, iter_extra);
}

static int 
toku_db_stat64(DB * db, DB_TXN *txn, DB_BTREE_STAT64 *s) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    struct ftstat64_s ftstat;
    TOKUTXN tokutxn = NULL;
    if (txn != NULL) {
        tokutxn = db_txn_struct_i(txn)->tokutxn;
    }
    toku_ft_handle_stat64(db->i->ft_handle, tokutxn, &ftstat);
    s->bt_nkeys = ftstat.nkeys;
    s->bt_ndata = ftstat.ndata;
    s->bt_dsize = ftstat.dsize;
    s->bt_fsize = ftstat.fsize;
    s->bt_create_time_sec = ftstat.create_time_sec;
    s->bt_modify_time_sec = ftstat.modify_time_sec;
    s->bt_verify_time_sec = ftstat.verify_time_sec;
    return 0;
}

static const char *
toku_db_get_dname(DB *db) {
    if (!db_opened(db)) {
        return nullptr;
    }
    if (db->i->dname == nullptr) {
        return ""; 
    }
    return db->i->dname;
}

static int 
toku_db_keys_range64(DB* db, DB_TXN* txn __attribute__((__unused__)), DBT* keyleft, DBT* keyright, uint64_t* less, uint64_t* left, uint64_t* between, uint64_t *right, uint64_t *greater, bool* middle_3_exact) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);

    // note that we ignore the txn param.  It would be more complicated to support it.
    // TODO(yoni): Maybe add support for txns later?  How would we do this?  ydb lock comment about db_keyrange64 is obsolete.
    toku_ft_keysrange(db->i->ft_handle, keyleft, keyright, less, left, between, right, greater, middle_3_exact);
    return 0;
}

static int 
toku_db_key_range64(DB* db, DB_TXN* txn, DBT* key, uint64_t* less_p, uint64_t* equal_p, uint64_t* greater_p, int* is_exact) {
    uint64_t less, equal_left, middle, equal_right, greater;
    bool ignore;
    int r = toku_db_keys_range64(db, txn, key, NULL, &less, &equal_left, &middle, &equal_right, &greater, &ignore);
    if (r == 0) {
        *less_p = less;
        *equal_p = equal_left;
        *greater_p = middle;
        paranoid_invariant_zero(greater);  // no keys are greater than positive infinity
        paranoid_invariant_zero(equal_right);  // no keys are equal to positive infinity
        // toku_ft_keysrange does not know when all 3 are exact, so set is_exact to false
        *is_exact = false;
    }
    return 0;
}

static int toku_db_get_key_after_bytes(DB *db, DB_TXN *txn, const DBT *start_key, uint64_t skip_len, void (*callback)(const DBT *end_key, uint64_t actually_skipped, void *extra), void *cb_extra, uint32_t UU(flags)) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    return toku_ft_get_key_after_bytes(db->i->ft_handle, start_key, skip_len, callback, cb_extra);
}

// needed by loader.c
int 
toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return 0;
    int r;
    r = toku_db_get_range_lock(db, txn, 
            toku_dbt_negative_infinity(), toku_dbt_positive_infinity(),
            toku::lock_request::type::WRITE);
    return r;
}

static int 
locked_db_close(DB * db, uint32_t UU(flags)) {
    // cannot begin a checkpoint
    toku_multi_operation_client_lock();
    int r = toku_db_close(db);
    toku_multi_operation_client_unlock();
    return r;
}

int 
autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, uint32_t flags) {
    bool changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, false);
    if (r!=0) return r;
    r = toku_db_get(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static inline int 
autotxn_db_getf_set (DB *db, DB_TXN *txn, uint32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    bool changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, false);
    if (r!=0) return r;
    r = db_getf_set(db, txn, flags, key, f, extra);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int 
locked_db_open(DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, uint32_t flags, int mode) {
    int ret, r;
    HANDLE_READ_ONLY_TXN(txn);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);

    //
    // Note that this function opens a db with a transaction. Should
    // the transaction abort, the user is responsible for closing the DB
    // before aborting the transaction. Not doing so results in undefined
    // behavior.
    //    
    DB_ENV *env = db->dbenv;
    DB_TXN *child_txn = NULL;
    int using_txns = env->i->open_flags & DB_INIT_TXN;
    if (using_txns) {
        ret = toku_txn_begin(env, txn, &child_txn, DB_TXN_NOSYNC);
        invariant_zero(ret);
    }

    // cannot begin a checkpoint
    toku_multi_operation_client_lock();
    r = toku_db_open(db, child_txn, fname, dbname, dbtype, flags & ~DB_AUTO_COMMIT, mode);
    toku_multi_operation_client_unlock();

    if (using_txns) {
        if (r == 0) {
            ret = locked_txn_commit(child_txn, DB_TXN_NOSYNC);
            invariant_zero(ret);
        } else {
            ret = locked_txn_abort(child_txn);
            invariant_zero(ret);
        }
    }
    return r;
}

static int 
locked_db_change_descriptor(DB *db, DB_TXN *txn, const DBT *descriptor, uint32_t flags) {
    // cannot begin a checkpoint
    toku_multi_operation_client_lock();
    int r = toku_db_change_descriptor(db, txn, descriptor, flags);
    toku_multi_operation_client_unlock();
    return r;
}

static int
autotxn_db_change_descriptor(DB *db, DB_TXN *txn, const DBT *descriptor, uint32_t flags) {
    bool changed; int r;
    r = toku_db_construct_autotxn(db, &txn, &changed, false);
    if (r != 0) { return r; }
    r = locked_db_change_descriptor(db, txn, descriptor, flags);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static void 
toku_db_set_errfile (DB *db, FILE *errfile) {
    db->dbenv->set_errfile(db->dbenv, errfile);
}

// TODO 2216 delete this
static int 
toku_db_fd(DB * UU(db), int * UU(fdp)) {
    return 0;
}

static const DBT* toku_db_dbt_pos_infty(void) __attribute__((pure));
static const DBT*
toku_db_dbt_pos_infty(void) {
    return toku_dbt_positive_infinity();
}

static const DBT* toku_db_dbt_neg_infty(void) __attribute__((pure));
static const DBT* 
toku_db_dbt_neg_infty(void) {
    return toku_dbt_negative_infinity();
}

static int
toku_db_optimize(DB *db) {
    HANDLE_PANICKED_DB(db);
    toku_ft_optimize(db->i->ft_handle);
    return 0;
}

static int
toku_db_hot_optimize(DB *db, DBT* left, DBT* right,
                     int (*progress_callback)(void *extra, float progress),
                     void *progress_extra, uint64_t* loops_run)
{
    HANDLE_PANICKED_DB(db);
    int r = 0;
    r = toku_ft_hot_optimize(db->i->ft_handle, left, right,
                              progress_callback,
                              progress_extra, loops_run);

    return r;
}

static int 
locked_db_optimize(DB *db) {
    // need to protect from checkpointing because
    // toku_db_optimize does a message injection
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    int r = toku_db_optimize(db);
    toku_multi_operation_client_unlock();
    return r;
}


struct last_key_extra {
    YDB_CALLBACK_FUNCTION func;
    void* extra;
};

static int
db_get_last_key_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen UU(), bytevec val UU(), void *extra, bool lock_only) {
    if (!lock_only) {
        DBT keydbt;
        toku_fill_dbt(&keydbt, key, keylen);
        struct last_key_extra * CAST_FROM_VOIDP(info, extra);
        info->func(&keydbt, NULL, info->extra);
    }
    return 0;
}

static int
toku_db_get_last_key(DB * db, DB_TXN *txn, YDB_CALLBACK_FUNCTION func, void* extra) {
    int r;
    LE_CURSOR cursor = nullptr;
    struct last_key_extra last_extra = { .func = func, .extra = extra };

    r = toku_le_cursor_create(&cursor, db->i->ft_handle, db_txn_struct_i(txn)->tokutxn);
    if (r != 0) { goto cleanup; }

    // Goes in reverse order.  First key returned is last in dictionary.
    r = toku_le_cursor_next(cursor, db_get_last_key_callback, &last_extra);
    if (r != 0) { goto cleanup; }

cleanup:
    if (cursor) {
        toku_le_cursor_close(cursor);
    }
    return r;
}

static int
autotxn_db_get_last_key(DB* db, YDB_CALLBACK_FUNCTION func, void* extra) {
    bool changed; int r;
    DB_TXN *txn = nullptr;
    // Cursors inside require transactions, but this is _not_ a transactional function.
    // Create transaction in a wrapper and then later close it.
    r = toku_db_construct_autotxn(db, &txn, &changed, false);
    if (r!=0) return r;
    r = toku_db_get_last_key(db, txn, func, extra);
    return toku_db_destruct_autotxn(txn, r, changed);
}

static int
toku_db_get_fragmentation(DB * db, TOKU_DB_FRAGMENTATION report) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (!db_opened(db))
        r = toku_ydb_do_error(db->dbenv, EINVAL, "Fragmentation report available only on open DBs.\n");
    else
        r = toku_ft_get_fragmentation(db->i->ft_handle, report);
    return r;
}

int 
toku_db_set_indexer(DB *db, DB_INDEXER * indexer) {
    int r = 0;
    if ( db->i->indexer != NULL && indexer != NULL ) {
        // you are trying to overwrite a valid indexer
        r = EINVAL;
    }
    else {
        db->i->indexer = indexer;
    }
    return r;
}

DB_INDEXER *
toku_db_get_indexer(DB *db) {
    return db->i->indexer;
}

static void 
db_get_indexer(DB *db, DB_INDEXER **indexer_ptr) {
    *indexer_ptr = toku_db_get_indexer(db);
}

struct ydb_verify_context {
    int (*progress_callback)(void *extra, float progress);
    void *progress_extra;
};

static int
ydb_verify_progress_callback(void *extra, float progress) {
    struct ydb_verify_context *context = (struct ydb_verify_context *) extra;
    int r = 0;
    if (context->progress_callback) {
        r = context->progress_callback(context->progress_extra, progress);
    }
    return r;
}

static int
toku_db_verify_with_progress(DB *db, int (*progress_callback)(void *extra, float progress), void *progress_extra, int verbose, int keep_going) {
    struct ydb_verify_context context = { progress_callback, progress_extra };
    int r = toku_verify_ft_with_progress(db->i->ft_handle, ydb_verify_progress_callback, &context, verbose, keep_going);
    return r;
}

int toku_setup_db_internal (DB **dbp, DB_ENV *env, uint32_t flags, FT_HANDLE brt, bool is_open) {
    if (flags || env == NULL) 
        return EINVAL;

    if (!env_opened(env))
        return EINVAL;
    
    DB *MALLOC(result);
    if (result == 0) {
        return ENOMEM;
    }
    memset(result, 0, sizeof *result);
    result->dbenv = env;
    MALLOC(result->i);
    if (result->i == 0) {
        toku_free(result);
        return ENOMEM;
    }
    memset(result->i, 0, sizeof *result->i);
    result->i->ft_handle = brt;
    result->i->opened = is_open;
    *dbp = result;
    return 0;
}

int 
toku_db_create(DB ** db, DB_ENV * env, uint32_t flags) {
    if (flags || env == NULL) 
        return EINVAL;

    if (!env_opened(env))
        return EINVAL;
    

    FT_HANDLE brt;
    toku_ft_handle_create(&brt);

    int r = toku_setup_db_internal(db, env, flags, brt, false);
    if (r != 0) return r;

    DB *result=*db;
    // methods that grab the ydb lock
#define SDB(name) result->name = locked_db_ ## name
    SDB(close);
    SDB(open);
    SDB(optimize);
#undef SDB
    // methods that do not take the ydb lock
#define USDB(name) result->name = toku_db_ ## name
    USDB(set_errfile);
    USDB(set_pagesize);
    USDB(get_pagesize);
    USDB(change_pagesize);
    USDB(set_readpagesize);
    USDB(get_readpagesize);
    USDB(change_readpagesize);
    USDB(set_compression_method);
    USDB(get_compression_method);
    USDB(change_compression_method);
    USDB(set_fanout);
    USDB(get_fanout);
    USDB(change_fanout);
    USDB(set_flags);
    USDB(get_flags);
    USDB(fd);
    USDB(get_max_row_size);
    USDB(set_indexer);
    USDB(pre_acquire_table_lock);
    USDB(pre_acquire_fileops_lock);
    USDB(key_range64);
    USDB(keys_range64);
    USDB(get_key_after_bytes);
    USDB(hot_optimize);
    USDB(stat64);
    USDB(get_fractal_tree_info64);
    USDB(iterate_fractal_tree_block_map);
    USDB(get_dname);
    USDB(verify_with_progress);
    USDB(cursor);
    USDB(dbt_pos_infty);
    USDB(dbt_neg_infty);
    USDB(get_fragmentation);
#undef USDB
    result->get_indexer = db_get_indexer;
    result->del = autotxn_db_del;
    result->put = autotxn_db_put;
    result->update = autotxn_db_update;
    result->update_broadcast = autotxn_db_update_broadcast;
    result->change_descriptor = autotxn_db_change_descriptor;
    result->get_last_key = autotxn_db_get_last_key;
    
    // unlocked methods
    result->get = autotxn_db_get;
    result->getf_set = autotxn_db_getf_set;
    
    result->i->dict_id = DICTIONARY_ID_NONE;
    result->i->opened = 0;
    result->i->open_flags = 0;
    result->i->open_mode = 0;
    result->i->indexer = NULL;
    *db = result;
    return 0;
}

// When the loader is created, it makes this call (toku_env_load_inames).
// For each dictionary to be loaded, replace old iname in directory
// with a newly generated iname.  This will also take a write lock
// on the directory entries.  The write lock will be released when
// the transaction of the loader is completed.
// If the transaction commits, the new inames are in place.
// If the transaction aborts, the old inames will be restored.
// The new inames are returned to the caller.  
// It is the caller's responsibility to free them.
// If "mark_as_loader" is true, then include a mark in the iname
// to indicate that the file is created by the brt loader.
// Return 0 on success (could fail if write lock not available).
static int
load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[/*N*/], const char * new_inames_in_env[/*N*/], LSN *load_lsn, bool mark_as_loader) {
    int rval = 0;
    int i;
    
    TXNID_PAIR xid = TXNID_PAIR_NONE;
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds new iname
    
    const char *mark;

    if (mark_as_loader) {
        mark = "B";
    } else {
        mark = "P";
    }

    for (i=0; i<N; i++) {
        new_inames_in_env[i] = NULL;
    }

    if (txn) {
        xid = toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn);
    }
    for (i = 0; i < N; i++) {
        char * dname = dbs[i]->i->dname;
        toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
        // now create new iname
        char hint[strlen(dname) + 1];
        create_iname_hint(dname, hint);
        const char *new_iname = create_iname(env, xid.parent_id64, xid.child_id64, hint, mark, i);               // allocates memory for iname_in_env
        new_inames_in_env[i] = new_iname;
        toku_fill_dbt(&iname_dbt, new_iname, strlen(new_iname) + 1);      // iname_in_env goes in directory
        rval = toku_db_put(env->i->directory, txn, &dname_dbt, &iname_dbt, 0, true);
        if (rval) break;
    }

    // Generate load log entries.
    if (!rval && txn) {
        TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
        int do_fsync = 0;
        LSN *get_lsn = NULL;
        for (i = 0; i < N; i++) {
            FT_HANDLE brt  = dbs[i]->i->ft_handle;
            //Fsync is necessary for the last one only.
            if (i==N-1) {
                do_fsync = 1; //We only need a single fsync of logs.
                get_lsn  = load_lsn; //Set pointer to capture the last lsn.
            }
            toku_ft_load(brt, ttxn, new_inames_in_env[i], do_fsync, get_lsn);
        }
    }
    return rval;
}

int
locked_load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[/*N*/], char * new_inames_in_env[/*N*/], LSN *load_lsn, bool mark_as_loader) {
    int ret, r;
    HANDLE_READ_ONLY_TXN(txn);

    DB_TXN *child_txn = NULL;
    int using_txns = env->i->open_flags & DB_INIT_TXN;
    if (using_txns) {
        ret = toku_txn_begin(env, txn, &child_txn, 0);
        invariant_zero(ret);
    }

    // cannot begin a checkpoint
    toku_multi_operation_client_lock();
    r = load_inames(env, child_txn, N, dbs, (const char **) new_inames_in_env, load_lsn, mark_as_loader);
    toku_multi_operation_client_unlock();

    if (using_txns) {
        if (r == 0) {
            ret = locked_txn_commit(child_txn, DB_TXN_NOSYNC);
            invariant_zero(ret);
        } else {
            ret = locked_txn_abort(child_txn);
            invariant_zero(ret);
            for (int i = 0; i < N; i++) {
                if (new_inames_in_env[i]) {
                    toku_free(new_inames_in_env[i]);
                    new_inames_in_env[i] = NULL;
                }
            }
        }
    }
    return r;

}

#undef STATUS_VALUE

#include <toku_race_tools.h>
void __attribute__((constructor)) toku_ydb_db_helgrind_ignore(void);
void
toku_ydb_db_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&ydb_db_layer_status, sizeof ydb_db_layer_status);
}
