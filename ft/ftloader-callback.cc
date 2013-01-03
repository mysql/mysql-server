/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include <errno.h>
#include <string.h>

#include "ftloader-internal.h"
#include "ybt.h"

static void error_callback_lock(ft_loader_error_callback loader_error) {
    toku_mutex_lock(&loader_error->mutex);
}

static void error_callback_unlock(ft_loader_error_callback loader_error) {
    toku_mutex_unlock(&loader_error->mutex);
}

void ft_loader_init_error_callback(ft_loader_error_callback loader_error) {
    memset(loader_error, 0, sizeof *loader_error);
    toku_init_dbt(&loader_error->key);
    toku_init_dbt(&loader_error->val);
    toku_mutex_init(&loader_error->mutex, NULL);
}

void ft_loader_destroy_error_callback(ft_loader_error_callback loader_error) { 
    toku_mutex_destroy(&loader_error->mutex);
    toku_destroy_dbt(&loader_error->key);
    toku_destroy_dbt(&loader_error->val);
    memset(loader_error, 0, sizeof *loader_error);
}

int ft_loader_get_error(ft_loader_error_callback loader_error) {
    error_callback_lock(loader_error);
    int r = loader_error->error;
    error_callback_unlock(loader_error);
    return r;
}

void ft_loader_set_error_function(ft_loader_error_callback loader_error, ft_loader_error_func error_function, void *error_extra) {
    loader_error->error_callback = error_function;
    loader_error->extra = error_extra;
}

int ft_loader_set_error(ft_loader_error_callback loader_error, int error, DB *db, int which_db, DBT *key, DBT *val) {
    int r;
    error_callback_lock(loader_error);
    if (loader_error->error) {              // there can be only one
        r = EEXIST;
    } else {
        r = 0;
        loader_error->error = error;        // set the error 
        loader_error->db = db;
        loader_error->which_db = which_db;
        if (key != nullptr) {
            toku_clone_dbt(&loader_error->key, *key);
        }
        if (val != nullptr) {
            toku_clone_dbt(&loader_error->val, *val);
        }
    }
    error_callback_unlock(loader_error);
    return r;
}

int ft_loader_call_error_function(ft_loader_error_callback loader_error) {
    int r;
    error_callback_lock(loader_error);
    r = loader_error->error;
    if (r && loader_error->error_callback && !loader_error->did_callback) {
        loader_error->did_callback = true;
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

int ft_loader_set_error_and_callback(ft_loader_error_callback loader_error, int error, DB *db, int which_db, DBT *key, DBT *val) {
    int r = ft_loader_set_error(loader_error, error, db, which_db, key, val);
    if (r == 0)
        r = ft_loader_call_error_function(loader_error);
    return r;
}

int ft_loader_init_poll_callback(ft_loader_poll_callback p) {
    memset(p, 0, sizeof *p);
    return 0;
}

void ft_loader_destroy_poll_callback(ft_loader_poll_callback p) {
    memset(p, 0, sizeof *p);
}

void ft_loader_set_poll_function(ft_loader_poll_callback p, ft_loader_poll_func poll_function, void *poll_extra) {
    p->poll_function = poll_function;
    p->poll_extra = poll_extra;
}

int ft_loader_call_poll_function(ft_loader_poll_callback p, float progress) {
    int r = 0;
    if (p->poll_function)
	r = p->poll_function(p->poll_extra, progress);
    return r;
}
