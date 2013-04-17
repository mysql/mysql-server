/* -*- mode: C; c-basic-offset: 4 -*-
 *
 * Copyright (c) 2007, 2008, 2009, 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */

/*
 *   a REFERENCE IMPLEMENTATION of the loader
 */

#include "includes.h"
#include "ydb-internal.h"
#include "loader.h"


enum {VERBOSE=0};

struct __toku_loader_internal {
    DB_ENV *env;
    DB_TXN *txn;
    int N;
    DB **dbs; /* [N] */
    DB *src_db;
    uint32_t *flags;
    uint32_t *dbt_flags;
    void *extra;
    void (*duplicate_callback)(DB *db, int i, DBT *key, DBT *val);
    int  (*poll_func)(void *extra, float progress);

    DBT dkey;   /* duplicate key */
    DBT dval;   /* duplicate val */
    int di;     /* duplicate i   */
};



int toku_loader_create_loader(DB_ENV *env, 
                              DB_TXN *txn, 
                              DB_LOADER **blp, 
                              DB *src_db, 
                              int N, 
                              DB *dbs[], 
                              uint32_t flags[N], 
                              uint32_t dbt_flags[N], 
                              void *extra)
{
    if (VERBOSE) printf("toku_loader_create_loader\n");
    DB_LOADER *loader;
    loader = toku_malloc(sizeof(DB_LOADER));
    assert(loader != NULL);
    loader->i = toku_malloc(sizeof(*loader->i));
    assert(loader->i != NULL);

    loader->i->env       = env;
    loader->i->txn       = txn;
    loader->i->N         = N;
    loader->i->src_db    = src_db;
    loader->i->dbs       = dbs;
    loader->i->flags     = flags;
    loader->i->dbt_flags = dbt_flags;
    loader->i->extra     = extra;

    memset(&loader->i->dkey, 0, sizeof(loader->i->dkey));
    memset(&loader->i->dval, 0, sizeof(loader->i->dval));
    loader->i->di      = 0;

    loader->set_poll_function      = toku_loader_set_poll_function;
    loader->set_duplicate_callback = toku_loader_set_duplicate_callback;
    loader->put                    = toku_loader_put;
    loader->close                  = toku_loader_close;

    *blp = loader;

    return 0;
}

int toku_loader_set_poll_function(DB_LOADER *loader,
                                  int (*poll_func)(void *extra, float progress)) 
{
    if (VERBOSE) printf("toku_loader_set_poll_function\n");
    loader->i->poll_func = poll_func;
    return 0;
}

int toku_loader_set_duplicate_callback(DB_LOADER *loader, 
                                       void (*duplicate)(DB *db, int i, DBT *key, DBT *val)) 
{
    if (VERBOSE) printf("toku_loader_set_duplicate_callback\n");
    loader->i->duplicate_callback = duplicate;
    return 0;
}

int toku_loader_put(DB_LOADER *loader, DBT *key, DBT *val) 
{
    if (VERBOSE) printf("toku_loader_put\n");
    int i;
    int r;
    DBT ekey, eval;
    
    if ( loader->i->dkey.data != NULL ) {
        if (VERBOSE) printf("skipping put\n");
        return 0;
    }
    
    for (i=0; i<loader->i->N; i++) {
        r = loader->i->env->i->generate_row_for_put(loader->i->dbs[i], // dest_db 
                                                    loader->i->dbs[0], // src_db,
                                                    &ekey, &eval,       // dest_key, dest_val
                                                    key, val,          // src_key, src_val
                                                    NULL);             // extra
        assert(r == 0);
        r = loader->i->dbs[i]->put(loader->i->dbs[i], loader->i->txn, &ekey, &eval, DB_NOOVERWRITE);
        if ( r != 0 ) {
            if (VERBOSE) printf("put returns %d\n", r);
            // spec says errors all happen on close
            //   - have to save key, val, and i for duplicate callback
            loader->i->dkey.size = key->size;
            loader->i->dkey.data = toku_malloc(key->size);
            memcpy(loader->i->dkey.data, key->data, key->size);
            loader->i->dval.size = val->size;
            loader->i->dval.data = toku_malloc(val->size);
            memcpy(loader->i->dval.data, val->data, val->size);
            loader->i->di = i;
        }
    }
    return 0;
}

int toku_loader_close(DB_LOADER *loader) 
{
    if (VERBOSE) printf("toku_loader_close\n");
    // as per spec, if you found a duplicate during load, call duplicate_callback
    if ( loader->i->dkey.data != NULL ) {
        if ( loader->i->duplicate_callback != NULL ) {
            loader->i->duplicate_callback(loader->i->dbs[loader->i->di], loader->i->di, &loader->i->dkey, &loader->i->dval);
        }
        toku_free(loader->i->dkey.data);
        toku_free(loader->i->dval.data);
    }
        
    toku_free(loader->i);
    toku_free(loader);
    return 0;
}

int toku_loader_abort(DB_LOADER *loader) {
    return toku_loader_close(loader);
}



