/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <ctype.h>
#include <db.h>
#include "ydb-internal.h"
#include <ft/ft-flusher.h>
#include <ft/checkpoint.h>
#include "indexer.h"
#include "ydb_load.h"
#include <ft/log_header.h>
#include "ydb_cursor.h"
#include "ydb_row_lock.h"
#include "ydb_db.h"
#include "ydb_write.h"


static YDB_DB_LAYER_STATUS_S ydb_db_layer_status;
#ifdef STATUS_VALUE
#undef STATUS_VALUE
#endif
#define STATUS_VALUE(x) ydb_db_layer_status.status[x].value.num

#define STATUS_INIT(k,t,l) { \
        ydb_db_layer_status.status[k].keyname = #k; \
        ydb_db_layer_status.status[k].type    = t;  \
        ydb_db_layer_status.status[k].legend  = l; \
    }

static void
ydb_db_layer_status_init (void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(YDB_LAYER_DIRECTORY_WRITE_LOCKS,      UINT64,   "directory write locks");
    STATUS_INIT(YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL, UINT64,   "directory write locks fail");
    STATUS_INIT(YDB_LAYER_LOGSUPPRESS,                UINT64,   "log suppress");
    STATUS_INIT(YDB_LAYER_LOGSUPPRESS_FAIL,           UINT64,   "log suppress fail");
    ydb_db_layer_status.initialized = true;
}
#undef STATUS_INIT

void
ydb_db_layer_get_status(YDB_DB_LAYER_STATUS statp) {
    if (!ydb_db_layer_status.initialized)
        ydb_db_layer_status_init();
    *statp = ydb_db_layer_status;
}

static inline DBT*
init_dbt_realloc(DBT *dbt) {
    memset(dbt, 0, sizeof(*dbt));
    dbt->flags = DB_DBT_REALLOC;
    return dbt;
}

static void
create_iname_hint(const char *dname, char *hint) {
    //Requires: size of hint array must be > strlen(dname)
    //Copy alphanumeric characters only.
    //Replace strings of non-alphanumeric characters with a single underscore.
    BOOL underscored = FALSE;
    while (*dname) {
        if (isalnum(*dname)) {
            char c = *dname++;
            *hint++ = c;
            underscored = FALSE;
        }
        else {
            if (!underscored)
                *hint++ = '_';
            dname++;
            underscored = TRUE;
        }
    }
    *hint = '\0';
}


// n < 0  means to ignore mark and ignore n
// n >= 0 means to include mark ("_B_" or "_P_") with hex value of n in iname
// (intended for use by loader, which will create many inames using one txnid).
static char *
create_iname(DB_ENV *env, u_int64_t id, char *hint, char *mark, int n) {
    int bytes;
    char inamebase[strlen(hint) +
		   8 +  // hex file format version
		   16 + // hex id (normally the txnid)
		   8  + // hex value of n if non-neg
		   sizeof("_B___.tokudb")]; // extra pieces
    if (n < 0)
	bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%"PRIx64"_%"PRIx32            ".tokudb",
                         hint, id, FT_LAYOUT_VERSION);
    else {
	invariant(strlen(mark) == 1);
	bytes = snprintf(inamebase, sizeof(inamebase),
                         "%s_%"PRIx64"_%"PRIx32"_%s_%"PRIx32".tokudb",
                         hint, id, FT_LAYOUT_VERSION, mark, n);
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

static int toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode);

void
toku_db_add_ref(DB *db) {
    db->i->refs++;
}

void
toku_db_release_ref(DB *db){
    db->i->refs--;
}

//DB->close()
int 
toku_db_close(DB * db) {
    int r = 0;
    // the magic number one comes from the fact that only one loader
    // or hot indexer may reference a DB at a time. when that changes,
    // this will break.
    if (db->i->refs != 1) {
        r = EBUSY;
    } else {
        // TODO: assert(db->i->refs == 0) because we're screwed otherwise
        db->i->refs = 0;
        if (db_opened(db) && db->i->dname) {
            // internal (non-user) dictionary has no dname
            env_note_db_closed(db->dbenv, db);  // tell env that this db is no longer in use by the user of this api (user-closed, may still be in use by fractal tree internals)
        }
        //Remove from transaction's list of 'must close' if necessary.
        if (!toku_list_empty(&db->i->dbs_that_must_close_before_abort))
            toku_list_remove(&db->i->dbs_that_must_close_before_abort);

        r = toku_ft_handle_close(db->i->ft_handle, FALSE, ZERO_LSN);
        if (r == 0) {
            // go ahead and close this DB handle right away.
            if (db->i->lt) {
                toku_lt_remove_db_ref(db->i->lt);
            }
            toku_sdbt_cleanup(&db->i->skey);
            toku_sdbt_cleanup(&db->i->sval);
            if (db->i->dname) {
                toku_free(db->i->dname);
            }
            toku_free(db->i);
            toku_free(db);
        }
    }
    return r;
}


///////////
//db_getf_XXX is equivalent to c_getf_XXX, without a persistent cursor

int
db_getf_set(DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
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
toku_db_get (DB * db, DB_TXN * txn, DBT * key, DBT * data, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;
    u_int32_t iso_flags = flags & DB_ISOLATION_FLAGS;

    if ((db->i->open_flags & DB_THREAD) && db_thread_need_flags(data))
        return EINVAL;

    u_int32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);
    flags &= ~lock_flags;
    flags &= ~DB_ISOLATION_FLAGS;
    // And DB_GET_BOTH is no longer supported. #2862.
    if (flags != 0) return EINVAL;


    DBC *dbc;
    r = toku_db_cursor_internal(db, txn, &dbc, iso_flags | DBC_DISABLE_PREFETCHING, 1);
    if (r!=0) return r;
    u_int32_t c_get_flags = DB_SET;
    r = toku_c_get(dbc, key, data, c_get_flags | lock_flags);
    int r2 = toku_c_close(dbc);
    return r ? r : r2;
}

#if 0
static int 
toku_db_key_range(DB * db, DB_TXN * txn, DBT * dbt, DB_KEY_RANGE * kr, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    txn=txn; dbt=dbt; kr=kr; flags=flags;
    toku_ydb_barf();
    abort();
}
#endif

static int
db_open_subdb(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
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

// inames are created here.
// algorithm:
//  begin txn
//  convert dname to iname (possibly creating new iname)
//  open file (toku_ft_handle_open() will handle logging)
//  close txn
//  if created a new iname, take full range lock
static int 
toku_db_open(DB * db, DB_TXN * txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    if (dbname!=NULL) 
        return db_open_subdb(db, txn, fname, dbname, dbtype, flags, mode);

    // at this point fname is the dname
    //This code ONLY supports single-db files.
    assert(dbname==NULL);
    const char * dname = fname;  // db_open_subdb() converts (fname, dbname) to dname

    ////////////////////////////// do some level of parameter checking.
    u_int32_t unused_flags = flags;
    int using_txns = db->dbenv->i->open_flags & DB_INIT_TXN;
    int r;
    if (dbtype!=DB_BTREE && dbtype!=DB_UNKNOWN) return EINVAL;
    int is_db_excl    = flags & DB_EXCL;    unused_flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  unused_flags&=~DB_CREATE;
    int is_db_hot_index  = flags & DB_IS_HOT_INDEX;  unused_flags&=~DB_IS_HOT_INDEX;

    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
                                            unused_flags&=~DB_READ_UNCOMMITTED;
                                            unused_flags&=~DB_READ_COMMITTED;
                                            unused_flags&=~DB_SERIALIZABLE;
    if (unused_flags & ~DB_THREAD) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;
    if (dbtype==DB_UNKNOWN && is_db_excl) return EINVAL;

    /* tokudb supports no duplicates and sorted duplicates only */
    unsigned int tflags;
    r = toku_ft_get_flags(db->i->ft_handle, &tflags);
    if (r != 0) 
        return r;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    //////////////////////////////

    DB_TXN *child = NULL;
    // begin child (unless transactionless)
    if (using_txns) {
        r = toku_txn_begin(db->dbenv, txn, &child, DB_TXN_NOSYNC, 1, true);
        assert(r==0);
    }

    // convert dname to iname
    //  - look up dname, get iname
    //  - if dname does not exist, create iname and make entry in directory
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds iname_in_env
    toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
    init_dbt_realloc(&iname_dbt);  // sets iname_dbt.data = NULL
    r = toku_db_get(db->dbenv->i->directory, child, &dname_dbt, &iname_dbt, DB_SERIALIZABLE);  // allocates memory for iname
    char *iname = iname_dbt.data;
    if (r==DB_NOTFOUND && !is_db_create)
        r = ENOENT;
    else if (r==0 && is_db_excl) {
        r = EEXIST;
    }
    else if (r==DB_NOTFOUND) {
        char hint[strlen(dname) + 1];

        // create iname and make entry in directory
        u_int64_t id = 0;

        if (using_txns) {
            id = toku_txn_get_txnid(db_txn_struct_i(child)->tokutxn);
        }
        create_iname_hint(dname, hint);
        iname = create_iname(db->dbenv, id, hint, NULL, -1);  // allocated memory for iname
        toku_fill_dbt(&iname_dbt, iname, strlen(iname) + 1);
        //
        // 0 for performance only, avoid unnecessary query
        // if we are creating a hot index, per #3166, we do not want the write lock  in directory grabbed.
        // directory read lock is grabbed in toku_db_get above
        //
        u_int32_t put_flags = 0 | ((is_db_hot_index) ? DB_PRELOCKED_WRITE : 0); 
        r = toku_db_put(db->dbenv->i->directory, child, &dname_dbt, &iname_dbt, put_flags, TRUE);  
    }

    // we now have an iname
    if (r == 0) {
        r = db_open_iname(db, child, iname, flags, mode);
        if (r==0) {
            db->i->dname = toku_xstrdup(dname);
            env_note_db_opened(db->dbenv, db);  // tell env that a new db handle is open (using dname)
        }
    }

    // free string holding iname
    if (iname) toku_free(iname);

    if (using_txns) {
        // close txn
        if (r == 0) {  // commit
            r = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL, false);
            invariant(r==0);  // TODO panic
        }
        else {         // abort
            int r2 = toku_txn_abort(child, NULL, NULL, false);
            invariant(r2==0);  // TODO panic
        }
    }

    return r;
}

// callback that sets the descriptors when 
// a dictionary is redirected at the brt layer
// I wonder if client applications can safely access
// the descriptor via db->descriptor, because
// a redirect may be happening underneath the covers.
// Need to investigate further.
static void db_on_redirect_callback(FT_HANDLE brt, void* extra) {
    DB* db = extra;
    db->descriptor = &brt->ft->descriptor;
    db->cmp_descriptor = &brt->ft->cmp_descriptor;
}

int 
db_open_iname(DB * db, DB_TXN * txn, const char *iname_in_env, u_int32_t flags, int mode) {
    int r;

    //Set comparison functions if not yet set.
    if (!db->i->key_compare_was_set && db->dbenv->i->bt_compare) {
        r = toku_ft_set_bt_compare(db->i->ft_handle, db->dbenv->i->bt_compare);
        assert(r==0);
        db->i->key_compare_was_set = TRUE;
    }
    if (db->dbenv->i->update_function) {
        r = toku_ft_set_update(db->i->ft_handle,db->dbenv->i->update_function);
        assert(r==0);
    }
    toku_ft_set_redirect_callback(
        db->i->ft_handle,
        db_on_redirect_callback,
        db
        );
    BOOL need_locktree = (BOOL)((db->dbenv->i->open_flags & DB_INIT_LOCK) &&
                                (db->dbenv->i->open_flags & DB_INIT_TXN));

    int is_db_excl    = flags & DB_EXCL;    flags&=~DB_EXCL;
    int is_db_create  = flags & DB_CREATE;  flags&=~DB_CREATE;
    //We support READ_UNCOMMITTED and READ_COMMITTED whether or not the flag is provided.
                                            flags&=~DB_READ_UNCOMMITTED;
                                            flags&=~DB_READ_COMMITTED;
                                            flags&=~DB_SERIALIZABLE;
                                            flags&=~DB_IS_HOT_INDEX;
    if (flags & ~DB_THREAD) return EINVAL; // unknown flags

    if (is_db_excl && !is_db_create) return EINVAL;

    /* tokudb supports no duplicates and sorted duplicates only */
    unsigned int tflags;
    r = toku_ft_get_flags(db->i->ft_handle, &tflags);
    if (r != 0) 
        return r;

    if (db_opened(db))
        return EINVAL;              /* It was already open. */
    
    db->i->open_flags = flags;
    db->i->open_mode = mode;

    FT_HANDLE brt = db->i->ft_handle;
    r = toku_ft_handle_open(brt, iname_in_env,
		      is_db_create, is_db_excl,
		      db->dbenv->i->cachetable,
		      txn ? db_txn_struct_i(txn)->tokutxn : NULL_TXN);
    if (r != 0)
        goto error_cleanup;

    db->i->opened = 1;

    // now that the brt has successfully opened, a valid descriptor
    // is in the brt header. we need a copy of the pointer in the DB.
    // TODO: there may be a cleaner way to do this. 
    // toku_ft_get_descriptor(db, &cmp_desc, &desc); ??
    db->descriptor = &brt->ft->descriptor;
    db->cmp_descriptor = &brt->ft->cmp_descriptor;

    if (need_locktree) {
	db->i->dict_id = toku_ft_get_dictionary_id(db->i->ft_handle);
        r = toku_ltm_get_lt(db->dbenv->i->ltm, &db->i->lt, db->i->dict_id, db->cmp_descriptor, toku_ft_get_bt_compare(db->i->ft_handle));
        if (r!=0) { goto error_cleanup; }
    }
    //Add to transaction's list of 'must close' if necessary.
    if (txn) {
        //Do last so we don't have to undo.
        toku_list_push(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort,
                  &db->i->dbs_that_must_close_before_abort);
    }

    return 0;
 
error_cleanup:
    db->i->dict_id = DICTIONARY_ID_NONE;
    db->i->opened = 0;
    if (db->i->lt) {
        toku_lt_remove_db_ref(db->i->lt);
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

    DBT key_in_directory = { .data = dname, .size = strlen(dname)+1 };
    //Left end of range == right end of range (point lock)
    int r = get_range_lock(db->dbenv->i->directory, txn, &key_in_directory, &key_in_directory, LOCK_REQUEST_WRITE);
    if (r == 0)
	STATUS_VALUE(YDB_LAYER_DIRECTORY_WRITE_LOCKS)++;  // accountability 
    else
	STATUS_VALUE(YDB_LAYER_DIRECTORY_WRITE_LOCKS_FAIL)++;  // accountability 
    return r;
}

//
// This function is the only way to set a descriptor of a DB.
//
static int 
toku_db_change_descriptor(DB *db, DB_TXN* txn, const DBT* descriptor, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    int r;
    TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
    DBT old_descriptor;
    BOOL is_db_hot_index  = ((flags & DB_IS_HOT_INDEX) != 0);
    BOOL update_cmp_descriptor = ((flags & DB_UPDATE_CMP_DESCRIPTOR) != 0);

    toku_init_dbt(&old_descriptor);
    if (!db_opened(db) || !txn || !descriptor || (descriptor->size>0 && !descriptor->data)){
        r = EINVAL;
        goto cleanup;
    }
    if (txn->parent != NULL) {
        r = EINVAL; // cannot have a parent if you are a resetting op
        goto cleanup;
    }
    if (!is_db_hot_index) {
        r = toku_db_pre_acquire_fileops_lock(db, txn);
        if (r != 0) { goto cleanup; }    
    }

    old_descriptor.size = db->descriptor->dbt.size;
    old_descriptor.data = toku_memdup(db->descriptor->dbt.data, db->descriptor->dbt.size);
    r = toku_ft_change_descriptor(
        db->i->ft_handle, 
        &old_descriptor, 
        descriptor, 
        TRUE, 
        ttxn, 
        update_cmp_descriptor
        );
    if (r != 0) { goto cleanup; }

    // the lock tree uses a copy of the header's descriptor for comparisons.
    // if we need to update the cmp descriptor, we need to make sure the lock
    // tree can get a copy of the new descriptor.
    if (update_cmp_descriptor) {
        toku_lt_update_descriptor(db->i->lt, db->cmp_descriptor);
    }
cleanup:
    if (old_descriptor.data) toku_free(old_descriptor.data);
    return r;
}

static int 
toku_db_set_flags(DB *db, u_int32_t flags) {
    HANDLE_PANICKED_DB(db);

    /* the following matches BDB */
    if (db_opened(db) && flags != 0) return EINVAL;

    return 0;
}

static int 
toku_db_get_flags(DB *db, u_int32_t *pflags) {
    HANDLE_PANICKED_DB(db);
    if (!pflags) return EINVAL;
    *pflags = 0;
    return 0;
}

static int 
toku_db_set_pagesize(DB *db, u_int32_t pagesize) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_set_nodesize(db->i->ft_handle, pagesize);
    return r;
}

static int 
toku_db_get_pagesize(DB *db, u_int32_t *pagesize_ptr) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_get_nodesize(db->i->ft_handle, pagesize_ptr);
    return r;
}

static int 
toku_db_set_readpagesize(DB *db, u_int32_t readpagesize) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_set_basementnodesize(db->i->ft_handle, readpagesize);
    return r;
}

static int 
toku_db_get_readpagesize(DB *db, u_int32_t *readpagesize_ptr) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_get_basementnodesize(db->i->ft_handle, readpagesize_ptr);
    return r;
}

static int 
toku_db_set_compression_method(DB *db, enum toku_compression_method compression_method) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_set_compression_method(db->i->ft_handle, compression_method);
    return r;
}

static int 
toku_db_get_compression_method(DB *db, enum toku_compression_method *compression_method_ptr) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_get_compression_method(db->i->ft_handle, compression_method_ptr);
    return r;
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
    int r = toku_ft_handle_stat64(db->i->ft_handle, tokutxn, &ftstat);
    if (r==0) {
        s->bt_nkeys = ftstat.nkeys;
        s->bt_ndata = ftstat.ndata;
        s->bt_dsize = ftstat.dsize;
        s->bt_fsize = ftstat.fsize;
        // 4018
        s->bt_create_time_sec = ftstat.create_time_sec;
        s->bt_modify_time_sec = ftstat.modify_time_sec;
        s->bt_verify_time_sec = ftstat.verify_time_sec;
    }
    return r;
}

static int 
toku_db_key_range64(DB* db, DB_TXN* txn __attribute__((__unused__)), DBT* key, u_int64_t* less, u_int64_t* equal, u_int64_t* greater, int* is_exact) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);

    // note that toku_ft_keyrange does not have a txn param
    // this will be fixed later
    // temporarily, because the caller, locked_db_keyrange, 
    // has the ydb lock, we are ok
    int r = toku_ft_keyrange(db->i->ft_handle, key, less, equal, greater);
    if (r != 0) { goto cleanup; }
    // temporarily set is_exact to 0 because ft_keyrange does not have this parameter
    *is_exact = 0;
cleanup:
    return r;
}

// needed by loader.c
int 
toku_db_pre_acquire_table_lock(DB *db, DB_TXN *txn) {
    HANDLE_PANICKED_DB(db);
    if (!db->i->lt || !txn) return 0;
    int r;
    r = get_range_lock(db, txn, toku_lt_neg_infinity, toku_lt_infinity, LOCK_REQUEST_WRITE);
    return r;
}

static int 
locked_db_close(DB * db, u_int32_t UU(flags)) {
    toku_ydb_lock(); 
    int r = toku_db_close(db);
    toku_ydb_unlock(); 
    return r;
}

int 
autotxn_db_get(DB* db, DB_TXN* txn, DBT* key, DBT* data, u_int32_t flags) {
    BOOL changed; int r;
    // ydb lock is NOT held here
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE, FALSE);
    if (r!=0) return r;
    r = toku_db_get(db, txn, key, data, flags);
    return toku_db_destruct_autotxn(txn, r, changed, FALSE);
}

static inline int 
autotxn_db_getf_set (DB *db, DB_TXN *txn, u_int32_t flags, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    BOOL changed; int r;
    // ydb lock is NOT held here
    r = toku_db_construct_autotxn(db, &txn, &changed, FALSE, FALSE);
    if (r!=0) return r;
    r = db_getf_set(db, txn, flags, key, f, extra);
    return toku_db_destruct_autotxn(txn, r, changed, FALSE);
}

static inline int 
autotxn_db_open(DB* db, DB_TXN* txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    BOOL changed; int r;
    // YDB lock is held when this function is called
    r = toku_db_construct_autotxn(db, &txn, &changed, (BOOL)((flags & DB_AUTO_COMMIT) != 0), TRUE);
    if (r!=0) return r;
    r = toku_db_open(db, txn, fname, dbname, dbtype, flags & ~DB_AUTO_COMMIT, mode);
    return toku_db_destruct_autotxn(txn, r, changed, TRUE);
}

static int 
locked_db_open(DB *db, DB_TXN *txn, const char *fname, const char *dbname, DBTYPE dbtype, u_int32_t flags, int mode) {
    toku_multi_operation_client_lock(); //Cannot begin checkpoint
    toku_ydb_lock(); int r = autotxn_db_open(db, txn, fname, dbname, dbtype, flags, mode); toku_ydb_unlock();
    toku_multi_operation_client_unlock(); //Can now begin checkpoint
    return r;
}

static int 
locked_db_change_descriptor(DB *db, DB_TXN* txn, const DBT* descriptor, u_int32_t flags) {
    toku_ydb_lock();
    int r = toku_db_change_descriptor(db, txn, descriptor, flags);
    toku_ydb_unlock();
    return r;
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
    return toku_lt_infinity;
}

static const DBT* toku_db_dbt_neg_infty(void) __attribute__((pure));
static const DBT* 
toku_db_dbt_neg_infty(void) {
    return toku_lt_neg_infinity;
}

static int
toku_db_optimize(DB *db) {
    HANDLE_PANICKED_DB(db);
    int r = toku_ft_optimize(db->i->ft_handle);
    return r;
}

static int
toku_db_hot_optimize(DB *db,
                     int (*progress_callback)(void *extra, float progress),
                     void *progress_extra)
{
    HANDLE_PANICKED_DB(db);
    int r = 0;
    // If we areunable to get a directory read lock, do nothing.
    r = toku_ft_hot_optimize(db->i->ft_handle,
                              progress_callback,
                              progress_extra);

    return r;
}

static int 
locked_db_optimize(DB *db) {
    toku_ydb_lock();
    int r = toku_db_optimize(db);
    toku_ydb_unlock();
    return r;
}

static int
db_get_fragmentation(DB * db, TOKU_DB_FRAGMENTATION report) {
    HANDLE_PANICKED_DB(db);
    int r;
    if (!db_opened(db))
        r = toku_ydb_do_error(db->dbenv, EINVAL, "Fragmentation report available only on open DBs.\n");
    else
        r = toku_ft_get_fragmentation(db->i->ft_handle, report);
    return r;
}

static int
locked_db_get_fragmentation(DB * db, TOKU_DB_FRAGMENTATION report) {
    toku_ydb_lock();
    int r = db_get_fragmentation(db, report);
    toku_ydb_unlock();
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

int toku_setup_db_internal (DB **dbp, DB_ENV *env, u_int32_t flags, FT_HANDLE brt, bool is_open) {
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
    toku_list_init(&result->i->dbs_that_must_close_before_abort);
    result->i->ft_handle = brt;
    result->i->refs = 1;
    result->i->opened = is_open;
    *dbp = result;
    return 0;
}

int 
toku_db_create(DB ** db, DB_ENV * env, u_int32_t flags) {
    if (flags || env == NULL) 
        return EINVAL;

    if (!env_opened(env))
        return EINVAL;
    

    FT_HANDLE brt;
    int r;
    r = toku_ft_handle_create(&brt);
    if (r!=0) return r;

    r = toku_setup_db_internal(db, env, flags, brt, false);
    if (r != 0) return r;


    DB *result=*db;
    // methods that grab the ydb lock
#define SDB(name) result->name = locked_db_ ## name
    SDB(close);
    SDB(open);
    SDB(change_descriptor);
    SDB(optimize);
    SDB(get_fragmentation);
#undef SDB
    // methods that do not take the ydb lock
#define USDB(name) result->name = toku_db_ ## name
    USDB(set_errfile);
    USDB(set_pagesize);
    USDB(get_pagesize);
    USDB(set_readpagesize);
    USDB(get_readpagesize);
    USDB(set_compression_method);
    USDB(get_compression_method);
    USDB(set_flags);
    USDB(get_flags);
    USDB(fd);
    USDB(get_max_row_size);
    USDB(set_indexer);
    USDB(pre_acquire_table_lock);
    USDB(pre_acquire_fileops_lock);
    USDB(key_range64);
    USDB(hot_optimize);
    USDB(stat64);
    USDB(verify_with_progress);
    USDB(cursor);
    USDB(dbt_pos_infty);
    USDB(dbt_neg_infty);
#undef USDB
    result->get_indexer = db_get_indexer;
    result->del = autotxn_db_del;
    result->put = autotxn_db_put;
    result->update = autotxn_db_update;
    result->update_broadcast = autotxn_db_update_broadcast;
    
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


/* Following functions (ydb_load_xxx()) are used by loader:
 */

// When the loader is created, it makes this call.
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
int
ydb_load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[N], char * new_inames_in_env[N], LSN *load_lsn, BOOL mark_as_loader) {
    int rval;
    int i;
    
    int using_txns = env->i->open_flags & DB_INIT_TXN;
    DB_TXN * child = NULL;
    TXNID xid = 0;
    DBT dname_dbt;  // holds dname
    DBT iname_dbt;  // holds new iname
    
    char * mark;

    if (mark_as_loader)
	mark = "B";
    else
	mark = "P";

    for (i=0; i<N; i++) {
	new_inames_in_env[i] = NULL;
    }

    // begin child (unless transactionless)
    if (using_txns) {
	rval = toku_txn_begin(env, txn, &child, DB_TXN_NOSYNC, 1, true);
	assert(rval == 0);
	xid = toku_txn_get_txnid(db_txn_struct_i(child)->tokutxn);
    }
    for (i = 0; i < N; i++) {
	char * dname = dbs[i]->i->dname;
	toku_fill_dbt(&dname_dbt, dname, strlen(dname)+1);
	// now create new iname
	char hint[strlen(dname) + 1];
	create_iname_hint(dname, hint);
	char * new_iname = create_iname(env, xid, hint, mark, i);               // allocates memory for iname_in_env
	new_inames_in_env[i] = new_iname;
        toku_fill_dbt(&iname_dbt, new_iname, strlen(new_iname) + 1);      // iname_in_env goes in directory
        rval = toku_db_put(env->i->directory, child, &dname_dbt, &iname_dbt, 0, TRUE);
	if (rval) break;
    }

    // Generate load log entries.
    if (!rval && using_txns) {
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
            rval = toku_ft_load(brt, ttxn, new_inames_in_env[i], do_fsync, get_lsn);
            if (rval) break;
        }
    }
	
    if (using_txns) {
	// close txn
	if (rval == 0) {  // all well so far, commit child
	    rval = toku_txn_commit(child, DB_TXN_NOSYNC, NULL, NULL, false);
	    assert(rval==0);
	}
	else {         // abort child
	    int r2 = toku_txn_abort(child, NULL, NULL, false);
	    assert(r2==0);
	    for (i=0; i<N; i++) {
		if (new_inames_in_env[i]) {
		    toku_free(new_inames_in_env[i]);
		    new_inames_in_env[i] = NULL;
		}
	    }
	}
    }

    return rval;
}

int
locked_ydb_load_inames(DB_ENV * env, DB_TXN * txn, int N, DB * dbs[N], char * new_inames_in_env[N], LSN *load_lsn, BOOL mark_as_loader) {
    toku_ydb_lock();
    int r = ydb_load_inames(env, txn, N, dbs, new_inames_in_env, load_lsn, mark_as_loader);
    toku_ydb_unlock();
    return r;
}

#undef STATUS_VALUE

#include <valgrind/helgrind.h>
void __attribute__((constructor)) toku_ydb_db_helgrind_ignore(void);
void
toku_ydb_db_helgrind_ignore(void) {
    VALGRIND_HG_DISABLE_CHECKING(&ydb_db_layer_status, sizeof ydb_db_layer_status);
}
