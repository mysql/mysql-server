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
/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

// Test cardinality algorithm on a 2 level key where the first level is random in a space of size maxrand
// and the second level is unique.

#ident "Copyright (c) 2013 Tokutek Inc.  All rights reserved."
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>
#include <db.h>
#if __linux__
#include <endian.h>
#endif
#include <sys/stat.h>
typedef unsigned long long ulonglong;
#include "tokudb_status.h"
#include "tokudb_buffer.h"
#include "fake_mysql.h"
#if __APPLE__
typedef unsigned long ulong;
#endif
#include "tokudb_card.h"

static uint32_t hton32(uint32_t n) {
#if BYTE_ORDER == LITTLE_ENDIAN
    return __builtin_bswap32(n);
#else
    return n;
#endif
}

struct key {
    uint32_t r;
    uint32_t seq;
}; //  __attribute__((packed));

struct val {
    uint32_t v0;
}; //  __attribute__((packed));

// load nrows into the db
static void load_db(DB_ENV *env, DB *db, uint32_t nrows, uint32_t maxrand) {
    DB_TXN *txn = NULL;
    int r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    DB_LOADER *loader = NULL;
    uint32_t db_flags[1] = { 0 };
    uint32_t dbt_flags[1] = { 0 };
    uint32_t loader_flags = 0;
    r = env->create_loader(env, txn, &loader, db, 1, &db, db_flags, dbt_flags, loader_flags);
    assert(r == 0);

    for (uint32_t seq = 0; seq < nrows ; seq++) {
        struct key k = { hton32(random() % maxrand), hton32(seq) };
        struct val v = { seq };
        DBT key = { .data = &k, .size = sizeof k };
        DBT val = { .data = &v, .size = sizeof v };
        r = loader->put(loader, &key, &val);
        assert(r == 0);
    }

    r = loader->close(loader);
    assert(r == 0);

    r = txn->commit(txn, 0);
    assert(r == 0);
}

static int analyze_key_compare(DB *db __attribute__((unused)), const DBT *a, const DBT *b, uint level) {
    assert(a->size == b->size);
    switch (level) {
    default:
        assert(0);
    case 1:
        return memcmp(a->data, b->data, sizeof (uint32_t));
    case 2:
        assert(a->size == sizeof (struct key));
        return memcmp(a->data, b->data, sizeof (struct key));
    }
}

static void test_card(DB_ENV *env, DB *db, uint64_t expect[]) {
    int r;
    
    DB_TXN *txn = NULL;
    r = env->txn_begin(env, NULL, &txn, 0);
    assert(r == 0);

    uint64_t num_key_parts = 2;
    uint64_t rec_per_key[num_key_parts];

    r = tokudb::analyze_card(db, txn, false, num_key_parts, rec_per_key, analyze_key_compare, NULL, NULL);
    assert(r == 0);

    assert(rec_per_key[0] == expect[0]);
    assert(rec_per_key[1] == expect[1]);

    r = txn->commit(txn, 0);
    assert(r == 0);
}

int main(int argc, char * const argv[]) {
    uint64_t nrows = 1000000;
    uint32_t maxrand = 10;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--nrows") == 0 && i+1 < argc) {
            nrows = atoll(argv[++i]);
            continue;
        }
        if (strcmp(argv[i], "--maxrand") == 0 && i+1 < argc) {
            maxrand = atoi(argv[++i]);
            continue;
        }
    }

    int r;
    r = system("rm -rf " __FILE__ ".testdir");
    assert(r == 0);
    r = mkdir(__FILE__ ".testdir", S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);
    assert(r == 0);

    r = env->open(env, __FILE__ ".testdir", DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    assert(r == 0);

    // create the db
    DB *db = NULL;
    r = db_create(&db, env, 0);
    assert(r == 0);

    r = db->open(db, NULL, "test.db", 0, DB_BTREE, DB_CREATE + DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    // load the db
    load_db(env, db, nrows, maxrand);

    uint64_t expect[2] = { nrows/maxrand, 1 };
    test_card(env, db, expect);

    r = db->close(db, 0);
    assert(r == 0);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
