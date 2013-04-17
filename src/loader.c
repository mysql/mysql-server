/* -*- mode: C; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2007, 2008, 2009, 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */

/*
 *   The loader
 */

#include <toku_portability.h>
#include <stdio.h>
#include <string.h>
#include "ydb-internal.h"
#include "../newbrt/brtloader.h"
#include "loader.h"
#include "ydb_load.h"
#include "checkpoint.h"
#include "brt-internal.h"
#include "toku_atomic.h"


#define lazy_assert(a) assert(a) // indicates code is incomplete 
#define invariant(a) assert(a) // indicates a code invariant that must be true
#define resource_assert(a) assert(a) // indicates resource must be available, otherwise unrecoverable

enum {MAX_FILE_SIZE=256};

static LOADER_STATUS_S status;  // accountability

struct __toku_loader_internal {
    DB_ENV *env;
    DB_TXN *txn;
    BRTLOADER brt_loader;
    int N;
    DB **dbs; /* [N] */
    DB *src_db;
    uint32_t *db_flags;
    uint32_t *dbt_flags;
    uint32_t loader_flags;
    void (*error_callback)(DB *db, int i, int err, DBT *key, DBT *val, void *error_extra);
    void *error_extra;
    int  (*poll_func)(void *poll_extra, float progress);
    void *poll_extra;
    char *temp_file_template;

    DBT *ekeys;
    DBT *evals;

    DBT err_key;   /* error key */
    DBT err_val;   /* error val */
    int err_i;     /* error i   */
    int err_errno;

    char **inames_in_env; /* [N]  inames of new files to be created */
};

/*
 *  free_loader_resources() frees all of the resources associated with
 *      struct __toku_loader_internal 
 *  assumes any previously freed items set the field pointer to NULL
 *  Requires that the brt_loader is closed or destroyed before calling this function.
 */
static void free_loader_resources(DB_LOADER *loader) 
{
    if ( loader->i ) {
        for (int i=0; i<loader->i->N; i++) {
            if (loader->i->ekeys &&
                loader->i->ekeys[i].data &&
                loader->i->ekeys[i].flags == DB_DBT_REALLOC) {
                toku_free(loader->i->ekeys[i].data);
            }
            if (loader->i->evals &&
                loader->i->evals[i].data &&
                loader->i->evals[i].flags == DB_DBT_REALLOC) {
                toku_free(loader->i->evals[i].data);
            }
        }
        if (loader->i->ekeys)              toku_free(loader->i->ekeys);
        if (loader->i->evals)              toku_free(loader->i->evals);

        if (loader->i->err_key.data)       toku_free(loader->i->err_key.data);
        if (loader->i->err_val.data)       toku_free(loader->i->err_val.data);

        if (loader->i->inames_in_env) {
            for (int i=0; i<loader->i->N; i++) {
                if (loader->i->inames_in_env[i]) toku_free(loader->i->inames_in_env[i]);
            }
            toku_free(loader->i->inames_in_env);
        }
        if (loader->i->temp_file_template) toku_free(loader->i->temp_file_template);

        // loader->i
        toku_free(loader->i);
        loader->i = NULL;
    }
}

static void free_loader(DB_LOADER *loader)
{
    if ( loader ) free_loader_resources(loader);
    toku_free(loader);
}

// excuse the convolution of error messages - in the end returns
//             0 if empty
//   DB_KEYEXIST if not empty
//   DB_NOTFOUND if problem with DB
static int verify_empty(DB *db, DB_TXN *txn) 
{
    int r, r2;
    DBC *cursor;
    DBT k, v;
    toku_init_dbt(&k);
    toku_init_dbt(&v);

    r  = db->cursor(db, txn, &cursor, 0);
    if ( r!=0 )  return DB_NOTFOUND;
    r  = cursor->c_get(cursor, &k, &v, DB_NEXT);
    r2 = cursor->c_close(cursor);
    if ( r2!=0 ) return DB_NOTFOUND; 
    if (r==DB_NOTFOUND) r = 0; // this is correct
    else if (r==0)      r = DB_KEYEXIST;
    return r;
}

static const char *loader_temp_prefix = "tokuld"; // #2536
static const char *loader_temp_suffix = "XXXXXX";

int toku_loader_create_loader(DB_ENV *env, 
                              DB_TXN *txn, 
                              DB_LOADER **blp, 
                              DB *src_db, 
                              int N, 
                              DB *dbs[], 
                              uint32_t db_flags[N], 
                              uint32_t dbt_flags[N], 
                              uint32_t loader_flags)
{
    int rval;

    *blp = NULL;           // set later when created

    DB_LOADER *loader;
    XCALLOC(loader);       // init to all zeroes (thus initializing the error_callback and poll_func)
    XCALLOC(loader->i);    // init to all zeroes (thus initializing all pointers to NULL)

    loader->i->env                = env;
    loader->i->txn                = txn;
    loader->i->N                  = N;
    loader->i->dbs                = dbs;
    loader->i->src_db             = src_db;
    loader->i->db_flags           = db_flags;
    loader->i->dbt_flags          = dbt_flags;
    loader->i->loader_flags       = loader_flags;
    loader->i->temp_file_template = (char *)toku_malloc(MAX_FILE_SIZE);

    int n = snprintf(loader->i->temp_file_template, MAX_FILE_SIZE, "%s/%s%s", env->i->real_tmp_dir, loader_temp_prefix, loader_temp_suffix);
    if ( !(n>0 && n<MAX_FILE_SIZE) ) {
        rval = -1;
	goto create_exit;
    }

    memset(&loader->i->err_key, 0, sizeof(loader->i->err_key));
    memset(&loader->i->err_val, 0, sizeof(loader->i->err_val));
    loader->i->err_i      = 0;
    loader->i->err_errno  = 0;

    loader->set_error_callback     = toku_loader_set_error_callback;
    loader->set_poll_function      = toku_loader_set_poll_function;
    loader->put                    = toku_loader_put;
    loader->close                  = toku_loader_close;
    loader->abort                  = toku_loader_abort;

    int r = 0;
    // lock tables and check empty
    for(int i=0;i<N;i++) {
        if (!(loader_flags&DB_PRELOCKED_WRITE)) {
            toku_ydb_lock(); //Must hold ydb lock for acquiring locks
            BOOL using_puts = (loader->i->loader_flags & LOADER_USE_PUTS) != 0;
            r = toku_db_pre_acquire_table_lock(dbs[i], txn, !using_puts);
            toku_ydb_unlock();
            if (r!=0) break;
        }
        r = verify_empty(dbs[i], txn);
        if (r!=0) break;
    }
    if ( r!=0 ) {
        rval = -1;
	goto create_exit;
    }

    {
	brt_compare_func compare_functions[N];
	for (int i=0; i<N; i++) {
	    compare_functions[i] = dbs[i]->i->key_compare_was_set ? toku_brt_get_bt_compare(dbs[i]->i->brt) : env->i->bt_compare;
	}

	// time to open the big kahuna
	if ( loader->i->loader_flags & LOADER_USE_PUTS ) {
	    XCALLOC_N(loader->i->N, loader->i->ekeys);
	    XCALLOC_N(loader->i->N, loader->i->evals);
	    for (int i=0; i<N; i++) {
		loader->i->ekeys[i].flags = DB_DBT_REALLOC;
		loader->i->evals[i].flags = DB_DBT_REALLOC;
	    }
	    loader->i->brt_loader = NULL;
	    rval = 0;
	}
	else {
	    char **XMALLOC_N(N, new_inames_in_env);
	    BRT *XMALLOC_N(N, brts);
	    for (int i=0; i<N; i++) {
		brts[i] = dbs[i]->i->brt;
	    }
	    loader->i->ekeys = NULL;
	    loader->i->evals = NULL;
	    LSN load_lsn;
	    r = locked_ydb_load_inames (env, txn, N, dbs, new_inames_in_env, &load_lsn);
	    if ( r!=0 ) {
		toku_free(new_inames_in_env);
		toku_free(brts);
		rval = r;
		goto create_exit;
	    }
            TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
	    r = toku_brt_loader_open(&loader->i->brt_loader,
				     loader->i->env->i->cachetable,
				     loader->i->env->i->generate_row_for_put,
				     src_db,
				     N,
				     brts,
				     (const char **)new_inames_in_env,
				     compare_functions,
				     loader->i->temp_file_template,
				     load_lsn,
                                     ttxn);
            if ( r!=0 ) {
		toku_free(new_inames_in_env);
		toku_free(brts);
                rval = r;
                goto create_exit;
            }
	    loader->i->inames_in_env = new_inames_in_env;
	    toku_free(brts);
	    rval = 0;
	}
    }
    *blp = loader;
 create_exit:
    if (rval == 0) {
	(void) toku_sync_fetch_and_increment_uint64(&status.create);
	(void) toku_sync_fetch_and_increment_uint32(&status.current);
	if (status.current > status.max)
	    status.max = status.current;   // not worth a lock to make threadsafe, may be inaccurate
    }
    else {
	(void) toku_sync_fetch_and_increment_uint64(&status.create_fail);
        free_loader(loader);
    }
    return rval;
}

int toku_loader_set_poll_function(DB_LOADER *loader,
                                  int (*poll_func)(void *extra, float progress),
				  void *poll_extra) 
{
    invariant(loader != NULL);
    loader->i->poll_func = poll_func;
    loader->i->poll_extra = poll_extra;
    return 0;
}

int toku_loader_set_error_callback(DB_LOADER *loader, 
                                   void (*error_cb)(DB *db, int i, int err, DBT *key, DBT *val, void *extra),
				   void *error_extra) 
{
    invariant(loader != NULL);
    loader->i->error_callback = error_cb;
    loader->i->error_extra    = error_extra;
    return 0;
}

int toku_loader_put(DB_LOADER *loader, DBT *key, DBT *val) 
{
    status.put++;  // not worth the extra cycles to keep threadsafe

    int r = 0;
    int i = 0;
    //      err_i is unused now( always 0).  How would we know which dictionary
    //      the error happens in?  (put_multiple and toku_brt_loader_put do NOT report
    //      which dictionary). 

    // skip put if error already found
    if ( loader->i->err_errno != 0 ) {
        return -1;
    }

    if ( loader->i->loader_flags & LOADER_USE_PUTS ) {
        r = loader->i->env->put_multiple(loader->i->env,
                                         loader->i->src_db, // src_db
                                         loader->i->txn,
                                         key, val,
                                         loader->i->N, // num_dbs
                                         loader->i->dbs, // (DB**)db_array
                                         loader->i->ekeys, 
                                         loader->i->evals,
                                         loader->i->db_flags, // flags_array
                                         NULL);
    }
    else {
        r = toku_brt_loader_put(loader->i->brt_loader, key, val);
    }
    if ( r != 0 ) {
        // spec says errors all happen on close
        //   - have to save key, val, errno (r) and i for duplicate callback
        loader->i->err_key.size = key->size;
        loader->i->err_key.data = toku_malloc(key->size);
        memcpy(loader->i->err_key.data, key->data, key->size);

        loader->i->err_val.size = val->size;
        loader->i->err_val.data = toku_malloc(val->size);
        memcpy(loader->i->err_val.data, val->data, val->size);

        loader->i->err_i = i;
        loader->i->err_errno = r;
        
        // deliberately return content free value
        //   - must call error_callback to get error info
        return -1;
    }
    return 0;
}

int toku_loader_close(DB_LOADER *loader) 
{
    (void) toku_sync_fetch_and_decrement_uint32(&status.current);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
        if ( !(loader->i->loader_flags & LOADER_USE_PUTS ) ) {
            r = toku_brt_loader_abort(loader->i->brt_loader, TRUE);
        }
        else {
            r = loader->i->err_errno;
        }
    } 
    else { // no error outstanding 
        if ( !(loader->i->loader_flags & LOADER_USE_PUTS ) ) {
            // use the bulk loader
            // in case you've been looking - here is where the real work is done!
            r = toku_brt_loader_close(loader->i->brt_loader,
                                      loader->i->error_callback, loader->i->error_extra,
                                      loader->i->poll_func,      loader->i->poll_extra);
            if ( r==0 ) {
                for (int i=0; i<loader->i->N; i++) {
                    toku_ydb_lock(); //Must hold ydb lock for dictionary_redirect.
                    r = toku_dictionary_redirect(loader->i->inames_in_env[i],
                                                 loader->i->dbs[i]->i->brt,
                                                 db_txn_struct_i(loader->i->txn)->tokutxn);
                    toku_ydb_unlock();
                    if ( r!=0 ) break;
                }
            }
        }
    }
    free_loader(loader);
    if (r==0)
	(void) toku_sync_fetch_and_increment_uint64(&status.close);
    else
	(void) toku_sync_fetch_and_increment_uint64(&status.close_fail);
    return r;
}

int toku_loader_abort(DB_LOADER *loader) 
{
    (void) toku_sync_fetch_and_decrement_uint32(&status.current);
    (void) toku_sync_fetch_and_increment_uint64(&status.abort);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
    }

    if ( !(loader->i->loader_flags & LOADER_USE_PUTS) ) {
        r = toku_brt_loader_abort(loader->i->brt_loader, TRUE);
    }
    free_loader(loader);
    return r;
}


// find all of the files in the environments home directory that match the loader temp name and remove them
int toku_loader_cleanup_temp_files(DB_ENV *env) {
    int result;
    struct dirent *de;
    char * dir = env->i->real_tmp_dir;
    DIR *d = opendir(dir);
    if (d==0) {
        result = errno; goto exit;
    }

    result = 0;
    while ((de = readdir(d))) {
        int r = memcmp(de->d_name, loader_temp_prefix, strlen(loader_temp_prefix));
        if (r == 0 && strlen(de->d_name) == strlen(loader_temp_prefix) + strlen(loader_temp_suffix)) {
            int fnamelen = strlen(dir) + 1 + strlen(de->d_name) + 1; // One for the slash and one for the trailing NUL.
            char fname[fnamelen];
            int l = snprintf(fname, fnamelen, "%s/%s", dir, de->d_name);
            assert(l+1 == fnamelen);
            r = unlink(fname);
            if (r!=0) {
                result = errno;
                perror("Trying to delete a rolltmp file");
            }
        }
    }
    {
        int r = closedir(d);
        if (r == -1) 
            result = errno;
    }

exit:
    return result;
}

void 
toku_loader_get_status(LOADER_STATUS s) {
    *s = status;
}
