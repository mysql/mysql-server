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
#include "toku_config.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <db.h>
#include <toku_byteswap.h>

static int verbose = 0;
static int commit_flag = 0;
static int do_verify = 0;
static int nthread = 1;
static int maxk = 1000000;
static int do_multidb = 0;

static int usage(const char *prog) {
    fprintf(stderr, "%s: run multi client single row insertions\n", prog);
    fprintf(stderr, "[--n %d]\n", maxk);
    fprintf(stderr, "[--nthread %d]\n", nthread);
    fprintf(stderr, "[--verify] (%d)\n", do_verify);
    fprintf(stderr, "[--nosync] (%d)\n", commit_flag);
    fprintf(stderr, "[--multidb] (%d)\n", do_multidb);
    return 1;
}

struct keygen {
    pthread_mutex_t lock;
    uint64_t k;
    uint64_t maxk;
};

static void keygen_init(struct keygen *key, uint64_t _maxk) {
    pthread_mutex_init(&key->lock, NULL);
    key->k = 0;
    key->maxk = _maxk;
}

static void keygen_destroy(struct keygen *key) {
    pthread_mutex_destroy(&key->lock);
}

static int keygen_next(struct keygen *key, uint64_t *k) {
    int r;
    pthread_mutex_lock(&key->lock);
    if (key->k >= key->maxk)
        r = 1;
    else {
        *k = key->k++;
        r = 0;
    }
    pthread_mutex_unlock(&key->lock);
    return r;
}

struct inserter_arg {
    struct keygen *keygen;
    DB_ENV *env;
    DB *db;
};

static int inserter(struct keygen *keygen, DB_ENV *env, DB *db) {
    if (verbose) printf("%p %p %p\n", keygen, env, db);
    while (1) {
        uint64_t k;
        int r = keygen_next(keygen, &k);
        if (r != 0)
            break;
        
        if (verbose) printf("%" PRIdPTR ": %" PRIu64 "\n", (intptr_t) pthread_self(), k);

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        assert(r == 0);

        uint64_t kk = bswap_64(k);
        DBT key = { .data = &kk, .size = sizeof kk };
        DBT val = { .data = &k, .size = sizeof k };
        r = db->put(db, txn, &key, &val, 0);
        assert(r == 0);

        r = txn->commit(txn, commit_flag);
        assert(r == 0);
    }
    return 0;
}

static void *inserter_wrap(void *arg) {
    if (verbose) printf("%" PRIdPTR "\n", (intptr_t) pthread_self());
    struct inserter_arg *inserter_arg = (struct inserter_arg *) arg;
    int r = inserter(inserter_arg->keygen, inserter_arg->env, inserter_arg->db);
    assert(r == 0);
    return arg;
}

static int verify(DB_ENV *env, DB *db, uint64_t _maxk) {
    int r;

    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    DBC *cursor;
    r = db->cursor(db, txn, &cursor, 0);
    assert(r == 0);

    DBT key; memset(&key, 0, sizeof key);
    DBT val; memset(&val, 0, sizeof val);
    uint64_t i;
    for (i=0; 1; i++) {
        r = cursor->c_get(cursor, &key, &val, DB_NEXT);
        if (r != 0)
            break;
        uint64_t k, v;
        assert(key.size == sizeof k);
        assert(val.size == sizeof v);
        memcpy(&k, key.data, key.size);
        k = bswap_64(k);
        assert(i == k);
        memcpy(&v, val.data, val.size);
        assert(v == i);
    }
    assert(i == _maxk);

    r = cursor->c_close(cursor);
    assert(r == 0);
    r = txn->commit(txn, 0);
    assert(r == 0);
    return 0;
}

static int env_init(DB_ENV **envptr, const char *envdir) {
    int r;
    DB_ENV *env;

    r = db_env_create(&env, 0);
    if (r == 0) {
        // env setup

        // env open
        r = env->open(env, envdir, DB_CREATE+DB_PRIVATE+DB_INIT_LOCK+DB_INIT_LOG+DB_INIT_MPOOL+DB_INIT_TXN, 0777);
    }
    if (r == 0)
        *envptr = env;
    return r;
}

static int db_init(DB_ENV *env, const char *dbname, DB **dbptr) {
    int r;
    DB *db;

    r = db_create(&db, env, 0);
    if (r == 0) {
        // db create
        r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0777);
        if (r != 0) {
            r = db->close(db, 0);
            assert(r == 0);
        }
    }
    if (r == 0)
        *dbptr = db;
    return r;
}   

int main(int argc, char *argv[]) {
    int r;

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "--nthread") == 0 && i+1 < argc) {
            nthread = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--n") == 0 && i+1 < argc) {
            maxk = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "--nosync") == 0) {
            commit_flag = DB_TXN_NOSYNC;
            continue;
        }
        if (strcmp(arg, "--verify") == 0) {
            do_verify++;
            continue;
        }
        if (strcmp(arg, "--multidb") == 0) {
            do_multidb++;
            continue;
        }

        return usage(argv[0]);
    }

    int ndb = 1;
    int nkeygen = 1;
    if (do_multidb) {
        ndb = nthread;
        nkeygen = nthread;
    }

    const char *envdir = ENVDIR;
    r = system("rm -rf " ENVDIR);
    assert(r == 0);
    r = mkdir(envdir, 0777);
    assert(r == 0);

    struct keygen keygen[nkeygen];
    for (int i = 0; i < nkeygen; i++) 
        keygen_init(&keygen[i], maxk);

    DB_ENV *env;
    r = env_init(&env, envdir);
    assert(r == 0);

    DB *db[ndb];
    for (int i = 0 ; i < ndb; i++) {
        char dbname[32]; sprintf(dbname, "db%d", i);
        r = db_init(env, dbname, &db[i]);
        assert(r == 0);
    }

    pthread_t tids[nthread];
    struct inserter_arg args[nthread];
    for (int i = 0; i < nthread; i++) {
        struct inserter_arg this_arg = { .keygen = &keygen[i % nkeygen], .env = env, .db = db[i % ndb] };
        args[i] = this_arg;
        pthread_create(&tids[i], NULL, inserter_wrap, &args[i]);
    }
    for (int i = 0; i < nthread; i++) {
        void *retptr;
        pthread_join(tids[i], &retptr);
    }

    for (int i = 0; i < ndb; i++) {
        if (do_verify) 
            verify(env, db[i], maxk);
        r = db[i]->close(db[i], 0);
        assert(r == 0);
    }

    r = env->close(env, 0);
    assert(r == 0);

    for (int i = 0; i < nkeygen; i++)
        keygen_destroy(&keygen[i]);

    return 0;
}
