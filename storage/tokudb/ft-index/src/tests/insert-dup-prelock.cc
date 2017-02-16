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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <db.h>
#include <toku_byteswap.h>
#include <portability/toku_path.h>

static int verbose = 0;
static uint64_t maxk = 100000;

static int usage(const char *prog) {
    fprintf(stderr, "%s: run single row insertions with prelocking\n", prog);
    fprintf(stderr, "[--n %" PRIu64 "]\n", maxk);
    return 1;
}

static int inserter(DB_ENV *env, DB *db, uint64_t _maxk, int putflags, int expectr) {
    if (verbose) printf("%p %p\n", env, db);
    int r;
    for (uint64_t k = 0; k < _maxk; k++) {
        
        if (verbose) printf("%" PRIu64 "\n", k);

        DB_TXN *txn;
        r = env->txn_begin(env, NULL, &txn, 0);
        assert(r == 0);

        r = db->pre_acquire_table_lock(db, txn);
        assert(r == 0);

        uint64_t kk = bswap_64(k);
        DBT key = { .data = &kk, .size = sizeof kk };
        DBT val = { .data = &k, .size = sizeof k };
        r = db->put(db, txn, &key, &val, putflags);
        assert(r == expectr);

        r = txn->commit(txn, DB_TXN_NOSYNC);
        assert(r == 0);
    }

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
        if (strcmp(arg, "--n") == 0 && i+1 < argc) {
            maxk = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--verbose") == 0 || strcmp(arg, "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(arg, "-q") == 0) {
            verbose = 0;
            continue;
        }

        return usage(argv[0]);
    }

    const char *envdir = TOKU_TEST_FILENAME;
    char cmd[TOKU_PATH_MAX+sizeof("rm -rf ")];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", TOKU_TEST_FILENAME);
    r = system(cmd);
    assert(r == 0);
    r = mkdir(envdir, 0777);
    assert(r == 0);

    DB_ENV *env;
    r = env_init(&env, envdir);
    assert(r == 0);

    DB *db;
    r = db_init(env, "db0", &db);
    assert(r == 0);

    r = inserter(env, db, maxk, 0, 0);
    assert(r == 0);

    r = inserter(env, db, maxk, DB_NOOVERWRITE, DB_KEYEXIST);
    assert(r == 0);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
