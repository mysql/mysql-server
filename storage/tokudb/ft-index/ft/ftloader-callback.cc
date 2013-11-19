/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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
