/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
/*
 * Copyright (c) 2007, 2008, 2009, 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

/*
 *   The loader
 */

#include <toku_portability.h>
#include <stdio.h>
#include <string.h>
#include "ydb-internal.h"
#include <ft/ftloader.h>
#include "loader.h"
#include "ydb_load.h"
#include <ft/checkpoint.h>
#include <ft/ft-internal.h>
#include <ft/ft.h>
#include "ydb_db.h"


#define lazy_assert(a) assert(a) // indicates code is incomplete 
#define invariant(a) assert(a) // indicates a code invariant that must be true
#define resource_assert(a) assert(a) // indicates resource must be available, otherwise unrecoverable

enum {MAX_FILE_SIZE=256};

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static LOADER_STATUS_S loader_status;

#define STATUS_INIT(k,t,l) { \
        loader_status.status[k].keyname = #k; \
        loader_status.status[k].type    = t;  \
        loader_status.status[k].legend  = "loader: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(LOADER_CREATE,      UINT64, "number of loaders successfully created");
    STATUS_INIT(LOADER_CREATE_FAIL, UINT64, "number of calls to toku_loader_create_loader() that failed");
    STATUS_INIT(LOADER_PUT,       UINT64, "number of calls to loader->put() succeeded");
    STATUS_INIT(LOADER_PUT_FAIL,  UINT64, "number of calls to loader->put() failed");
    STATUS_INIT(LOADER_CLOSE,       UINT64, "number of calls to loader->close() that succeeded");
    STATUS_INIT(LOADER_CLOSE_FAIL,  UINT64, "number of calls to loader->close() that failed");
    STATUS_INIT(LOADER_ABORT,       UINT64, "number of calls to loader->abort()");
    STATUS_INIT(LOADER_CURRENT,     UINT64, "number of loaders currently in existence");
    STATUS_INIT(LOADER_MAX,         UINT64, "max number of loaders that ever existed simultaneously");
    loader_status.initialized = true;
}
#undef STATUS_INIT

void
toku_loader_get_status(LOADER_STATUS statp) {
    if (!loader_status.initialized)
        status_init();
    *statp = loader_status;
}

#define STATUS_VALUE(x) loader_status.status[x].value.num


struct __toku_loader_internal {
    DB_ENV *env;
    DB_TXN *txn;
    FTLOADER ft_loader;
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
 *  Requires that the ft_loader is closed or destroyed before calling this function.
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

static const char *loader_temp_prefix = "tokuld"; // #2536
static const char *loader_temp_suffix = "XXXXXX";

static int ft_loader_close_and_redirect(DB_LOADER *loader) {
    int r;
    // use the bulk loader
    // in case you've been looking - here is where the real work is done!
    r = toku_ft_loader_close(loader->i->ft_loader,
                              loader->i->error_callback, loader->i->error_extra,
                              loader->i->poll_func,      loader->i->poll_extra);
    if ( r==0 ) {
        for (int i=0; i<loader->i->N; i++) {
            toku_ydb_lock(); //Must hold ydb lock for dictionary_redirect.
            r = toku_dictionary_redirect(loader->i->inames_in_env[i],
                                         loader->i->dbs[i]->i->ft_handle,
                                         db_txn_struct_i(loader->i->txn)->tokutxn);
            toku_ydb_unlock();
            if ( r!=0 ) break;
        }
    }
    return r;
}

// loader_flags currently has three possible values:
//   0                   use brt loader
//   USE_PUTS            do not use brt loader, use log suppression mechanism (2440)
//                       which results in recursive call here via toku_db_pre_acquire_table_lock()
//   DB_PRELOCKED_WRITE  do not use brt loader, this is the recursive (inner) call via 
//                       toku_db_pre_acquire_table_lock()
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
    BOOL use_ft_loader = (loader_flags == 0); 

    *blp = NULL;           // set later when created

    DB_LOADER *loader = NULL;
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
            r = toku_db_pre_acquire_table_lock(dbs[i], txn);
            if (r!=0) break;
        }
        r = !toku_ft_is_empty_fast(dbs[i]->i->ft_handle);
        if (r!=0) break;
    }
    if ( r!=0 ) {
        rval = -1;
        goto create_exit;
    }

    {
        ft_compare_func compare_functions[N];
        for (int i=0; i<N; i++) {
            compare_functions[i] = env->i->bt_compare;
        }

        // time to open the big kahuna
        char **XMALLOC_N(N, new_inames_in_env);
        FT_HANDLE *XMALLOC_N(N, brts);
        for (int i=0; i<N; i++) {
            brts[i] = dbs[i]->i->ft_handle;
        }
        loader->i->ekeys = NULL;
        loader->i->evals = NULL;
        LSN load_lsn;
        r = ydb_load_inames(env, txn, N, dbs, new_inames_in_env, &load_lsn, use_ft_loader);
        if ( r!=0 ) {
            toku_free(new_inames_in_env);
            toku_free(brts);
            rval = r;
            goto create_exit;
        }
        TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
        r = toku_ft_loader_open(&loader->i->ft_loader,
                                 loader->i->env->i->cachetable,
                                 loader->i->env->i->generate_row_for_put,
                                 src_db,
                                 N,
                                 brts, dbs,
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

        if (loader->i->loader_flags & LOADER_USE_PUTS) {
            XCALLOC_N(loader->i->N, loader->i->ekeys);
            XCALLOC_N(loader->i->N, loader->i->evals);
            toku_ydb_unlock();
            // the following function grabs the ydb lock, so we
            // first unlock before calling it
            rval = ft_loader_close_and_redirect(loader);
            toku_ydb_lock();
            assert_zero(rval);
            for (int i=0; i<N; i++) {
                loader->i->ekeys[i].flags = DB_DBT_REALLOC;
                loader->i->evals[i].flags = DB_DBT_REALLOC;
                toku_ft_suppress_recovery_logs(dbs[i]->i->ft_handle, db_txn_struct_i(txn)->tokutxn);
            }
            loader->i->ft_loader = NULL;
            // close the ft_loader and skip to the redirection
            rval = 0;
        }

        rval = 0;
    }
    *blp = loader;
 create_exit:
    if (rval == 0) {
        (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_CREATE), 1);
        (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_CURRENT), 1);
        if (STATUS_VALUE(LOADER_CURRENT) > STATUS_VALUE(LOADER_MAX) )
            STATUS_VALUE(LOADER_MAX) = STATUS_VALUE(LOADER_CURRENT);  // not worth a lock to make threadsafe, may be inaccurate
    }
    else {
        (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_CREATE_FAIL), 1);
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
    int r = 0;
    int i = 0;
    //      err_i is unused now( always 0).  How would we know which dictionary
    //      the error happens in?  (put_multiple and toku_ft_loader_put do NOT report
    //      which dictionary). 

    // skip put if error already found
    if ( loader->i->err_errno != 0 ) {
        r = -1;
        goto cleanup;
    }

    if (loader->i->loader_flags & LOADER_USE_PUTS) {
        r = loader->i->env->put_multiple(loader->i->env,
                                         loader->i->src_db, // src_db
                                         loader->i->txn,
                                         key, val,
                                         loader->i->N, // num_dbs
                                         loader->i->dbs, // (DB**)db_array
                                         loader->i->ekeys, 
                                         loader->i->evals,
                                         loader->i->db_flags); // flags_array
    }
    else {
        // calling toku_ft_loader_put without a lock assumes that the 
        //  handlerton is guaranteeing single access to the loader
        // future multi-threaded solutions may need to protect this call
        r = toku_ft_loader_put(loader->i->ft_loader, key, val);
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
        r = -1;
    }
 cleanup:
    if (r==0)
        STATUS_VALUE(LOADER_PUT)++;  // executed too often to be worth making threadsafe
    else
        STATUS_VALUE(LOADER_PUT_FAIL)++;
    return r;
}

int toku_loader_close(DB_LOADER *loader) 
{
    (void) __sync_fetch_and_sub(&STATUS_VALUE(LOADER_CURRENT), 1);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
        if (!(loader->i->loader_flags & LOADER_USE_PUTS ) ) {
            r = toku_ft_loader_abort(loader->i->ft_loader, TRUE);
        }
        else {
            r = loader->i->err_errno;
        }
    } 
    else { // no error outstanding 
        if (!(loader->i->loader_flags & LOADER_USE_PUTS ) ) {
            r = ft_loader_close_and_redirect(loader);
        }
    }
    toku_ydb_lock();
    free_loader(loader);
    toku_ydb_unlock();
    if (r==0)
        (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_CLOSE), 1);
    else
        (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_CLOSE_FAIL), 1);
    return r;
}

int toku_loader_abort(DB_LOADER *loader) 
{
    (void) __sync_fetch_and_sub(&STATUS_VALUE(LOADER_CURRENT), 1);
    (void) __sync_fetch_and_add(&STATUS_VALUE(LOADER_ABORT), 1);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
    }

    if (!(loader->i->loader_flags & LOADER_USE_PUTS) ) {
        r = toku_ft_loader_abort(loader->i->ft_loader, TRUE);
    }
    toku_ydb_lock();
    free_loader(loader);
    toku_ydb_unlock();
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



#undef STATUS_VALUE

