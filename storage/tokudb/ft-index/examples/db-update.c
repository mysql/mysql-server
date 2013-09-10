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

// measure the performance of a simulated "insert on duplicate key update" operation
// the table schema is t(a int, b int, c int, d int, primary key(a, b))
// a and b are random
// c is the sum of the observations
// d is the first observation

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "db.h"

static size_t key_size = 8;
static size_t val_size = 8;
static int verbose = 0;

static void db_error(const DB_ENV *env, const char *prefix, const char *msg) {
    printf("%s: %p %s %s\n", __FUNCTION__, env, prefix, msg);
}

static int get_int(void *p) {
    int v; 
    memcpy(&v, p, sizeof v);
    return htonl(v);
}

#if defined(TOKUDB)
static int my_update_callback(DB *db, const DBT *key, const DBT *old_val, const DBT *extra, void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    assert(db);
    assert(key);
    if (old_val == NULL) {
        // insert new_val = extra
        set_val(extra, set_extra);
    } else {
        if (verbose) printf("u");
        // update new_val = old_val + extra
        assert(old_val->size == val_size && extra->size == val_size);
        char new_val_buffer[val_size];
        memcpy(new_val_buffer, old_val->data, sizeof new_val_buffer);
        int newc = htonl(get_int(old_val->data) + get_int(extra->data)); // newc = oldc + newc
        memcpy(new_val_buffer, &newc, sizeof newc);
        DBT new_val = { .data = new_val_buffer, .size = sizeof new_val_buffer };
        set_val(&new_val, set_extra);
    }
    return 0;
}
#endif

static void insert_and_update(DB *db, DB_TXN *txn, int a, int b, int c, int d, bool do_update_callback) {
#if !defined(TOKUDB)
    assert(!do_update_callback);
#endif
    int r;

    // generate the key
    assert(key_size >= 8);
    char key_buffer[key_size];
    int newa = htonl(a);
    memcpy(key_buffer, &newa, sizeof newa);
    int newb = htonl(b);
    memcpy(key_buffer+4, &newb, sizeof newb);

    // generate the value
    assert(val_size >= 8);
    char val_buffer[val_size];
    int newc = htonl(c);
    memcpy(val_buffer, &newc, sizeof newc);
    int newd = htonl(d);
    memcpy(val_buffer+4, &newd, sizeof newd);

#if defined(TOKUDB)
    if (do_update_callback) {
        // extra = value_buffer, implicit combine column c update function
        DBT key = { .data = key_buffer, .size = sizeof key_buffer };
        DBT extra = { .data = val_buffer, .size = sizeof val_buffer };
        r = db->update(db, txn, &key, &extra, 0); assert(r == 0);
    } else
#endif
    {
        DBT key = { .data = key_buffer, .size = sizeof key_buffer };
        DBT value = { .data = val_buffer, .size = sizeof val_buffer };
        DBT oldvalue = { };
        r = db->get(db, txn, &key, &oldvalue, 0);
        assert(r == 0 || r == DB_NOTFOUND);
        if (r == 0) {
            // update it
            if (verbose) printf("U");
            int oldc = get_int(oldvalue.data);
            newc = htonl(oldc + c); // newc = oldc + newc
            memcpy(val_buffer, &newc, sizeof newc);
            r = db->put(db, txn, &key, &value, 0);
            assert(r == 0);
        } else if (r == DB_NOTFOUND) {
            r = db->put(db, txn, &key, &value, 0);
            assert(r == 0);
        }
    }
}

static inline float tdiff (struct timeval *a, struct timeval *b) {
    return (a->tv_sec - b->tv_sec) +1e-6*(a->tv_usec - b->tv_usec);
}

static void insert_and_update_all(DB_ENV *db_env, DB *db, long nrows, long max_rows_per_txn, int key_range, long rows_per_report, bool do_update_callback, bool do_txn) {
    int r;
    struct timeval tstart;
    r = gettimeofday(&tstart, NULL); assert(r == 0);
    struct timeval tlast = tstart;
    DB_TXN *txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
    }
    long n_rows_per_txn = 0;
    long rowi;
    for (rowi = 0; rowi < nrows; rowi++) {
        int a = random() % key_range;
        int b = random() % key_range;
        int c = 1;
        int d = 0; // timestamp
        insert_and_update(db, txn, a, b, c, d, do_update_callback);
        n_rows_per_txn++;
        
        // maybe commit
        if (do_txn && n_rows_per_txn == max_rows_per_txn) {
            r = txn->commit(txn, 0); assert(r == 0);
            r = db_env->txn_begin(db_env, NULL, &txn, 0); assert(r == 0);
            n_rows_per_txn = 0;
        }

        // maybe report performance
        if (((rowi + 1) % rows_per_report) == 0) {
            struct timeval tnow;
            r = gettimeofday(&tnow, NULL); assert(r == 0);
            float last_time = tdiff(&tnow, &tlast);
            float total_time = tdiff(&tnow, &tstart);
            printf("%ld %.3f %.0f/s %.0f/s\n", rowi + 1, last_time, rows_per_report/last_time, rowi/total_time); fflush(stdout);
            tlast = tnow;
        }
    }

    if (do_txn) {
        r = txn->commit(txn, 0); assert(r == 0);
    }
    struct timeval tnow;
    r = gettimeofday(&tnow, NULL); assert(r == 0);
    printf("total %ld %.3f %.0f/s\n", nrows, tdiff(&tnow, &tstart), nrows/tdiff(&tnow, &tstart)); fflush(stdout);
}

int main(int argc, char *argv[]) {
#if defined(TOKUDB)
    char *db_env_dir = "update.env.tokudb";
#else
    char *db_env_dir = "update.env.bdb";
#endif
    int db_env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    char *db_filename = "update.db";
    long rows = 1000000000;
    long rows_per_txn = 100;
    long rows_per_report = 100000;
    int key_range = 1000000;
#if defined(TOKUDB)
    bool do_update_callback = true;
#else
    bool do_update_callback = false;
#endif
    bool do_txn = false;
    u_int64_t cachesize = 1000000000;
    u_int32_t pagesize = 0;
#if defined(TOKUDB)
    u_int32_t checkpoint_period = 60;
#endif

    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "--rows") == 0 && i+1 < argc) {
            rows = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_txn") == 0 && i+1 < argc) {
            rows_per_txn = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--rows_per_report") == 0 && i+1 < argc) {
            rows_per_report = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--key_range") == 0 && i+1 < argc) {
            key_range = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--txn") == 0 && i+1 < argc) {
            do_txn = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(arg, "--pagesize") == 0 && i+1 < argc) {
            pagesize = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--cachesize") == 0 && i+1 < argc) {
            cachesize = atol(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--update_callback") == 0 && i+1 < argc) {
            do_update_callback = atoi(argv[++i]) != 0;
            continue;
        }
        if (strcmp(arg, "--key_size") == 0 && i+1 < argc) {
            key_size = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--val_size") == 0 && i+1 < argc) {
            val_size = atoi(argv[++i]);
            continue;
        }
#if defined(TOKUDB)
        if (strcmp(arg, "--checkpoint_period") == 0 && i+1 < argc) {
            checkpoint_period = atoi(argv[++i]);
            continue;
        }
#endif

        assert(0);
    }

    int r;
    char rm_cmd[strlen(db_env_dir) + strlen("rm -rf ") + 1];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf %s", db_env_dir);
    r = system(rm_cmd); assert(r == 0);

    r = mkdir(db_env_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); assert(r == 0);

    // create and open the env
    DB_ENV *db_env = NULL;
    r = db_env_create(&db_env, 0); assert(r == 0);
#if defined(TOKUDB)
    db_env->set_update(db_env, my_update_callback);
#endif
    if (cachesize) {
        if (verbose) printf("cachesize %llu\n", (unsigned long long)cachesize);
        const u_int64_t gig = 1 << 30;
        r = db_env->set_cachesize(db_env, cachesize / gig, cachesize % gig, 1); assert(r == 0);
    }
    if (!do_txn)
        db_env_open_flags &= ~(DB_INIT_TXN | DB_INIT_LOG);
    db_env->set_errcall(db_env, db_error);
    if (verbose) printf("env %s\n", db_env_dir);
    r = db_env->open(db_env, db_env_dir, db_env_open_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
#if defined(TOKUDB)
    if (checkpoint_period) {
        r = db_env->checkpointing_set_period(db_env, checkpoint_period); assert(r == 0);
        u_int32_t period;
        r = db_env->checkpointing_get_period(db_env, &period); assert(r == 0 && period == checkpoint_period);
    }
#endif

    // create the db
    DB *db = NULL;
    r = db_create(&db, db_env, 0); assert(r == 0);
    DB_TXN *create_txn = NULL;
    if (do_txn) {
        r = db_env->txn_begin(db_env, NULL, &create_txn, 0); assert(r == 0);
    }
    if (pagesize) {
        r = db->set_pagesize(db, pagesize); assert(r == 0);
    }
    r = db->open(db, create_txn, db_filename, NULL, DB_BTREE, DB_CREATE, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH); assert(r == 0);
    if (do_txn) {
        r = create_txn->commit(create_txn, 0); assert(r == 0);
    }

    // insert on duplicate key update
    insert_and_update_all(db_env, db, rows, rows_per_txn, key_range, rows_per_report, do_update_callback, do_txn);

    // shutdown
    r = db->close(db, 0); assert(r == 0); db = NULL;
    r = db_env->close(db_env, 0); assert(r == 0); db_env = NULL;

    return 0;
}
