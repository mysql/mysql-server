/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <string.h>
#include <errno.h>
#include "memory.h"
#include "brtloader-internal.h"

int brt_loader_init_error_callback(brtloader_error_callback loader_error) {
    memset(loader_error, 0, sizeof *loader_error);
    int r = toku_pthread_mutex_init(&loader_error->mutex, NULL); assert(r == 0);
    return r;
}

void brt_loader_destroy_error_callback(brtloader_error_callback loader_error) { 
    int r = toku_pthread_mutex_destroy(&loader_error->mutex); assert(r == 0);
    toku_free(loader_error->key.data);
    toku_free(loader_error->val.data);
    memset(loader_error, 0, sizeof *loader_error);
}

int brt_loader_get_error(brtloader_error_callback loader_error) {
    return loader_error->error;
}

void brt_loader_set_error_function(brtloader_error_callback loader_error, brt_loader_error_func error_function, void *error_extra) {
    loader_error->error_callback = error_function;
    loader_error->extra = error_extra;
}

static void error_callback_lock(brtloader_error_callback loader_error) {
    int r = toku_pthread_mutex_lock(&loader_error->mutex); assert(r == 0);
}

static void error_callback_unlock(brtloader_error_callback loader_error) {
    int r = toku_pthread_mutex_unlock(&loader_error->mutex); assert(r == 0);
}

static void copy_dbt(DBT *dest, DBT *src) {
    if (src) {
        dest->data = toku_malloc(src->size);
        memcpy(dest->data, src->data, src->size);
        dest->size = src->size;
    }
}

int brt_loader_set_error(brtloader_error_callback loader_error, int error, DB *db, int which_db, DBT *key, DBT *val) {
    int r;
    error_callback_lock(loader_error);
    if (loader_error->error) {              // there can be only one
        r = EEXIST;
    } else {
        r = 0;
        loader_error->error = error;        // set the error 
        loader_error->db = db;
        loader_error->which_db = which_db;
        copy_dbt(&loader_error->key, key);  // copy the data
        copy_dbt(&loader_error->val, val);
    }
    error_callback_unlock(loader_error);
    return r;
}

int brt_loader_call_error_function(brtloader_error_callback loader_error) {
    int r;
    error_callback_lock(loader_error);
    r = loader_error->error;
    if (r && loader_error->error_callback && !loader_error->did_callback) {
        loader_error->did_callback = TRUE;
        loader_error->error_callback(loader_error->db, 
                                     loader_error->which_db,
                                     loader_error->error,
                                     &loader_error->key,
                                     &loader_error->val,
                                     loader_error->extra);
    }
    error_callback_unlock(loader_error);    
    return r;
}

int brt_loader_set_error_and_callback(brtloader_error_callback loader_error, int error, DB *db, int which_db, DBT *key, DBT *val) {
    int r = brt_loader_set_error(loader_error, error, db, which_db, key, val);
    if (r == 0)
        r = brt_loader_call_error_function(loader_error);
    return r;
}

int brt_loader_init_poll_callback(brtloader_poll_callback p) {
    memset(p, 0, sizeof *p);
    return 0;
}

void brt_loader_destroy_poll_callback(brtloader_poll_callback p) {
    memset(p, 0, sizeof *p);
}

void brt_loader_set_poll_function(brtloader_poll_callback p, brt_loader_poll_func poll_function, void *poll_extra) {
    p->poll_function = poll_function;
    p->poll_extra = poll_extra;
}

int brt_loader_call_poll_function(brtloader_poll_callback p, float progress) {
    int r = 0;
    if (p->poll_function)
	r = p->poll_function(p->poll_extra, progress);
    return r;
}

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif
