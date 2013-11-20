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
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

typedef struct {
    int64_t left;
    int64_t right;
    int64_t last;
    int found;
    int direction;
    int error_to_expect;
} cont_extra;

static int
getf_continue(DBT const* key, DBT const* val, void* context) {
    assert(key); // prob wrong?
    assert(val); // prob wrong? make ifs if this fails
    cont_extra *CAST_FROM_VOIDP(c, context);

    assert(c->found >= 0);
    assert(c->found < 3);
    c->found++;
    assert(key->size == 8);
    assert(val->size == 8);
    int64_t k = *(int64_t*)key->data;
    int64_t v = *(int64_t*)val->data;
    assert(k==v);
    assert(k==c->last+c->direction);
    c->last = k;
    if (c->error_to_expect) {
        assert(c->left <= k);
        assert(k <= c->right);
    }
    if (c->found < 3) {
        return TOKUDB_CURSOR_CONTINUE;
    } else {
        return 0;
    }
}

static void
test_restrict (int64_t n, int offset, int error_to_expect) {
    assert(n > 30);
    DB_TXN * const null_txn = 0;
    int r;


    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    /* create the dup database file */
    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r=env->set_default_bt_compare(env, int64_dbt_cmp); CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE+DB_PRIVATE+DB_INIT_MPOOL, 0); assert(r == 0);

    DB *db;
    r = db_create(&db, env, 0);
    assert(r == 0);
    r = db->set_flags(db, 0);
    assert(r == 0);
    r = db->open(db, null_txn, "restrict.db", NULL, DB_BTREE, DB_CREATE, 0666);
    assert(r == 0);

    int64_t keys[n];
    int64_t i;
    for (i=0; i<n; i++) {
        keys[i] = i;
    }

    DBT key, val;
    for (i=0; i<n; i++) {
        r = db->put(db, null_txn, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, &i, sizeof i), 0);
        assert(r == 0);
    }

    DBC* cursor;

    r = db->cursor(db, NULL, &cursor, 0);
    CKERR(r);

    DBT dbt_left, dbt_right;
    int64_t int_left, int_right;
    int_left = n / 3 + offset;
    int_right = 2 * n / 3 + offset;

    dbt_init(&dbt_left, &keys[int_left], sizeof keys[int_left]);
    dbt_init(&dbt_right, &keys[int_right], sizeof keys[int_right]);

    r = cursor->c_set_bounds(
        cursor,
        &dbt_left,
        &dbt_right,
        true,
        error_to_expect);
    CKERR(r);


    for (i=0; i<n; i++) {
        r = cursor->c_get(cursor, dbt_init(&key, &keys[i], sizeof keys[i]), dbt_init(&val, NULL, 0), DB_SET);
        if (i < int_left || i > int_right) {
            CKERR2(r, error_to_expect);
        } else {
            CKERR(r);
            assert(val.size == 8);
            assert(*(int64_t*)val.data == i);
        }
    }
    // Forwards

    r = cursor->c_get(cursor, dbt_init(&key, &keys[int_left], sizeof keys[int_left]), dbt_init(&val, NULL, 0), DB_SET);
    CKERR(r);
    assert(val.size == 8);
    assert(*(int64_t*)val.data == int_left);

    for (i=int_left+1; i < n; i++) {
        r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_NEXT);
        if (i >= int_left && i <= int_right) {
            CKERR(r);
            assert(key.size == 8);
            assert(*(int64_t*)key.data == i);
            assert(val.size == 8);
            assert(*(int64_t*)val.data == i);
        } else {
            CKERR2(r, error_to_expect);
            break;
        }
    }

    r = cursor->c_get(cursor, dbt_init(&key, &keys[int_right], sizeof keys[int_right]), dbt_init(&val, NULL, 0), DB_SET);
    CKERR(r);
    assert(val.size == 8);
    assert(*(int64_t*)val.data == int_right);

    for (i=int_right-1; i >= 0; i--) {
        r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_PREV);
        if (i >= int_left && i <= int_right) {
            CKERR(r);
            assert(key.size == 8);
            assert(*(int64_t*)key.data == i);
            assert(val.size == 8);
            assert(*(int64_t*)val.data == i);
        } else {
            CKERR2(r, error_to_expect);
            break;
        }
    }

    // Forwards

    r = cursor->c_get(cursor, dbt_init(&key, &keys[int_left], sizeof keys[int_left]), dbt_init(&val, NULL, 0), DB_SET);
    CKERR(r);
    assert(val.size == 8);
    assert(*(int64_t*)val.data == int_left);

    cont_extra c;
    c.left = int_left;
    c.right = int_right;
    c.error_to_expect = error_to_expect;
    c.direction = 1;
    c.last = int_left;
    for (i=int_left+1; i < n; i+=3) {
        c.found = 0;

        r = cursor->c_getf_next(cursor, 0, getf_continue, &c);
        if (i >= int_left && i <= int_right) {
            CKERR(r);
            if (!error_to_expect) {
                assert(c.found == 3);
                assert(c.last == i+2);
            } else if (i+2 >= int_left && i+2 <= int_right) {
                assert(c.found == 3);
                assert(c.last == i+2);
            } else if (i+1 >= int_left && i+1 <= int_right) {
                assert(c.found == 2);
                assert(c.last == i+1);
                r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_CURRENT);
                CKERR2(r, error_to_expect);
                break;
            } else {
                assert(c.found == 1);
                assert(c.last == i);
                r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_CURRENT);
                CKERR2(r, error_to_expect);
                break;
            }
        } else {
            if (error_to_expect == 0) {
                assert(c.found == 3);
                assert(c.last == i+2);
            } else {
                assert(c.found == 0);
                assert(c.last == i-1);
            }
            CKERR2(r, error_to_expect);
            break;
        }
    }

    r = cursor->c_get(cursor, dbt_init(&key, &keys[int_right], sizeof keys[int_right]), dbt_init(&val, NULL, 0), DB_SET);
    CKERR(r);
    assert(val.size == 8);
    assert(*(int64_t*)val.data == int_right);

    c.direction = -1;
    c.last = int_right;
    for (i=int_right-1; i >= 0; i -= 3) {
        c.found = 0;

        r = cursor->c_getf_prev(cursor, 0, getf_continue, &c);
        if (i >= int_left && i <= int_right) {
            CKERR(r);
            if (!error_to_expect) {
                assert(c.found == 3);
                assert(c.last == i-2);
            } else if (i-2 >= int_left && i-2 <= int_right) {
                assert(c.found == 3);
                assert(c.last == i-2);
            } else if (i-1 >= int_left && i-1 <= int_right) {
                assert(c.found == 2);
                assert(c.last == i-1);
                r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_CURRENT);
                CKERR2(r, error_to_expect);
                break;
            } else {
                assert(c.found == 1);
                assert(c.last == i);
                r = cursor->c_get(cursor, dbt_init(&key, NULL, 0), dbt_init(&val, NULL, 0), DB_CURRENT);
                CKERR2(r, error_to_expect);
                break;
            }
        } else {
            if (error_to_expect == 0) {
                assert(c.found == 3);
                assert(c.last == i-2);
            } else {
                assert(c.found == 0);
                assert(c.last == i+1);
            }
            CKERR2(r, error_to_expect);
            break;
        }
    }

    r = cursor->c_close(cursor); CKERR(r);
    r = db->close(db, 0); CKERR(r);
    r = env->close(env, 0); CKERR(r);
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    for (int i = 3*64; i < 3*1024; i *= 2) {
        for (int offset = -2; offset <= 2; offset++) {
            test_restrict(i, offset, DB_NOTFOUND);
            test_restrict(i, offset, TOKUDB_OUT_OF_RANGE);
            test_restrict(i, offset, 0);
        }
    }
    return 0;
}
