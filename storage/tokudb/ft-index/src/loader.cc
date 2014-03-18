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

/*
 *   The loader
 */

#include <toku_portability.h>
#include <portability/toku_atomic.h>
#include <stdio.h>
#include <string.h>

#include <ft/ft.h>
#include <ft/ftloader.h>
#include <ft/checkpoint.h>

#include "ydb-internal.h"
#include "ydb_db.h"
#include "ydb_load.h"

#include "loader.h"
#include <util/status.h>

enum {MAX_FILE_SIZE=256};

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static LOADER_STATUS_S loader_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(loader_status, k, c, t, "loader: " l, inc)

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(LOADER_CREATE,      LOADER_NUM_CREATED, UINT64, "number of loaders successfully created", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(LOADER_CREATE_FAIL, nullptr, UINT64, "number of calls to toku_loader_create_loader() that failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_PUT,       nullptr, UINT64, "number of calls to loader->put() succeeded", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_PUT_FAIL,  nullptr, UINT64, "number of calls to loader->put() failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_CLOSE,       nullptr, UINT64, "number of calls to loader->close() that succeeded", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_CLOSE_FAIL,  nullptr, UINT64, "number of calls to loader->close() that failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_ABORT,       nullptr, UINT64, "number of calls to loader->abort()", TOKU_ENGINE_STATUS);
    STATUS_INIT(LOADER_CURRENT,     LOADER_NUM_CURRENT, UINT64, "number of loaders currently in existence", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(LOADER_MAX,         LOADER_NUM_MAX, UINT64, "max number of loaders that ever existed simultaneously", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
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
        toku_destroy_dbt(&loader->i->err_key);
        toku_destroy_dbt(&loader->i->err_val);

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
            toku_multi_operation_client_lock(); //Must hold MO lock for dictionary_redirect.
            r = toku_dictionary_redirect(loader->i->inames_in_env[i],
                                         loader->i->dbs[i]->i->ft_handle,
                                         db_txn_struct_i(loader->i->txn)->tokutxn);
            toku_multi_operation_client_unlock();
            if ( r!=0 ) break;
        }
    }
    return r;
}


// loader_flags currently has the following flags:
//   LOADER_DISALLOW_PUTS     loader->put is not allowed.
//                            Loader is only being used for its side effects
//   DB_PRELOCKED_WRITE       Table lock is already held, no need to relock.
int
toku_loader_create_loader(DB_ENV *env,
                          DB_TXN *txn,
                          DB_LOADER **blp,
                          DB *src_db,
                          int N,
                          DB *dbs[],
                          uint32_t db_flags[/*N*/],
                          uint32_t dbt_flags[/*N*/],
                          uint32_t loader_flags,
                          bool check_empty) {
    int rval;
    HANDLE_READ_ONLY_TXN(txn);

    *blp = NULL;           // set later when created

    DB_LOADER *loader = NULL;
    bool puts_allowed = !(loader_flags & LOADER_DISALLOW_PUTS);
    bool compress_intermediates = (loader_flags & LOADER_COMPRESS_INTERMEDIATES) != 0;
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
        rval = ENAMETOOLONG;
        goto create_exit;
    }

    toku_init_dbt(&loader->i->err_key);
    toku_init_dbt(&loader->i->err_val);
    loader->i->err_i      = 0;
    loader->i->err_errno  = 0;

    loader->set_error_callback     = toku_loader_set_error_callback;
    loader->set_poll_function      = toku_loader_set_poll_function;
    loader->put                    = toku_loader_put;
    loader->close                  = toku_loader_close;
    loader->abort                  = toku_loader_abort;

    // lock tables and check empty
    for(int i=0;i<N;i++) {
        if (!(loader_flags&DB_PRELOCKED_WRITE)) {
            rval = toku_db_pre_acquire_table_lock(dbs[i], txn);
            if (rval!=0) {
                goto create_exit;
            }
        }
        if (check_empty) {
            bool empty = toku_ft_is_empty_fast(dbs[i]->i->ft_handle);
            if (!empty) {
                rval = ENOTEMPTY;
                goto create_exit;
            }
        }
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
        LSN load_lsn;
        rval = locked_load_inames(env, txn, N, dbs, new_inames_in_env, &load_lsn, puts_allowed);
        if ( rval!=0 ) {
            toku_free(new_inames_in_env);
            toku_free(brts);
            goto create_exit;
        }
        TOKUTXN ttxn = txn ? db_txn_struct_i(txn)->tokutxn : NULL;
        rval = toku_ft_loader_open(&loader->i->ft_loader,
                                 env->i->cachetable,
                                 env->i->generate_row_for_put,
                                 src_db,
                                 N,
                                 brts, dbs,
                                 (const char **)new_inames_in_env,
                                 compare_functions,
                                 loader->i->temp_file_template,
                                 load_lsn,
                                 ttxn,
                                 puts_allowed,
                                 env->get_loader_memory_size(env),
                                 compress_intermediates);
        if ( rval!=0 ) {
            toku_free(new_inames_in_env);
            toku_free(brts);
            goto create_exit;
        }
        loader->i->inames_in_env = new_inames_in_env;
        toku_free(brts);

        if (!puts_allowed) {
            rval = ft_loader_close_and_redirect(loader);
            assert_zero(rval);
            loader->i->ft_loader = NULL;
            // close the ft_loader and skip to the redirection
            rval = 0;
        }

        rval = 0;
    }
    *blp = loader;
 create_exit:
    if (rval == 0) {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_CREATE), 1);
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_CURRENT), 1);
        if (STATUS_VALUE(LOADER_CURRENT) > STATUS_VALUE(LOADER_MAX) )
            STATUS_VALUE(LOADER_MAX) = STATUS_VALUE(LOADER_CURRENT);  // not worth a lock to make threadsafe, may be inaccurate
    }
    else {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_CREATE_FAIL), 1);
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

    if (loader->i->loader_flags & LOADER_DISALLOW_PUTS) {
        r = EINVAL;
        goto cleanup;
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
        toku_clone_dbt(&loader->i->err_key, *key);
        toku_clone_dbt(&loader->i->err_val, *val);

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

static void redirect_loader_to_empty_dictionaries(DB_LOADER *loader) {
    DB_LOADER* tmp_loader = NULL;
    int r = toku_loader_create_loader(
        loader->i->env,
        loader->i->txn,
        &tmp_loader,
        loader->i->src_db,
        loader->i->N,
        loader->i->dbs,
        loader->i->db_flags,
        loader->i->dbt_flags,
        0,
        false
        );
    lazy_assert_zero(r);
    r = toku_loader_close(tmp_loader);
}

int toku_loader_close(DB_LOADER *loader) 
{
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(LOADER_CURRENT), 1);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
        if (!(loader->i->loader_flags & LOADER_DISALLOW_PUTS ) ) {
            r = toku_ft_loader_abort(loader->i->ft_loader, true);
            redirect_loader_to_empty_dictionaries(loader);
        }
        else {
            r = loader->i->err_errno;
        }
    } 
    else { // no error outstanding 
        if (!(loader->i->loader_flags & LOADER_DISALLOW_PUTS ) ) {
            r = ft_loader_close_and_redirect(loader);
            if (r) {
                redirect_loader_to_empty_dictionaries(loader);
            }
        }
    }
    free_loader(loader);
    if (r==0)
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_CLOSE), 1);
    else
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_CLOSE_FAIL), 1);
    return r;
}

int toku_loader_abort(DB_LOADER *loader) 
{
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(LOADER_CURRENT), 1);
    (void) toku_sync_fetch_and_add(&STATUS_VALUE(LOADER_ABORT), 1);
    int r=0;
    if ( loader->i->err_errno != 0 ) {
        if ( loader->i->error_callback != NULL ) {
            loader->i->error_callback(loader->i->dbs[loader->i->err_i], loader->i->err_i, loader->i->err_errno, &loader->i->err_key, &loader->i->err_val, loader->i->error_extra);
        }
    }

    if (!(loader->i->loader_flags & LOADER_DISALLOW_PUTS) ) {
        r = toku_ft_loader_abort(loader->i->ft_loader, true);
        lazy_assert_zero(r);
    }

    redirect_loader_to_empty_dictionaries(loader);
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
        result = get_error_errno(); goto exit;
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
                result = get_error_errno();
                perror("Trying to delete a rolltmp file");
            }
        }
    }
    {
        int r = closedir(d);
        if (r == -1) 
            result = get_error_errno();
    }

exit:
    return result;
}



#undef STATUS_VALUE

