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
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>
#include <db.h>


static DBC* cursor      = NULL;
static DB*  db          = NULL;
static DB_ENV* env      = NULL;
static int r            = 0;
static DB_TXN* null_txn = NULL;

static void setup_env(void) {
    assert(!env && !db && !cursor);
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);
        CKERR(r);
    assert(env);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL|DB_THREAD|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    assert(env);
}

static void close_env(void) {
    assert(env && !db && !cursor);
    r = env->close(env, 0);
        CKERR(r);
    env = NULL;
}

static void setup_db(void) {
    assert(env && !db && !cursor);
    r = db_create(&db, env, 0);
        CKERR(r);
    assert(db);
    db->set_errfile(db, stderr);
    r = db->open(db, null_txn, "foo.db", "main", DB_BTREE, DB_CREATE, 0666);
        CKERR(r);
    assert(db);
}

static void close_db(void) {
    assert(env && db && !cursor);
    r = db->close(db, 0);
        CKERR(r);
    db = NULL;
}

static void setup_cursor(void) {
    assert(env && db && !cursor);
    r = db->cursor(db, NULL, &cursor, 0);
        CKERR(r);
    assert(cursor);
}

static void close_cursor(void) {
    assert(env && db && cursor);
    r = cursor->c_close(cursor);
        CKERR(r);
    cursor = NULL;
}

static void insert(char k, char d) {
    DBT key;
    DBT data;
    r = db->put(db, null_txn, dbt_init(&key, &k, sizeof(k)), dbt_init(&data, &d, sizeof(d)), 0);
        CKERR(r);
}

static void c_get(uint32_t flag, char key_expect, char data_expect) {
    DBT key;
    DBT data;

    r = cursor->c_get(cursor, dbt_init(&key, 0, 0), dbt_init(&data, 0, 0), flag);
        CKERR(r);
    assert(key.size == sizeof(key_expect));
    assert(data.size == sizeof(data_expect));
    char got_key  = *(char*)key.data;
    char got_data = *(char*)data.data;
    if (verbose &&
        (got_key != key_expect || got_data != data_expect)) {
        printf("c_get(%u) Expect (%c,%c)\n"
               "   Got (%c,%c)\n",
               flag, key_expect, data_expect, got_key, got_data);
    }
    assert(got_key  == key_expect);
    assert(got_data == data_expect);
}

static void test_skip_key(uint32_t flag, bool is_next) {
    setup_env();
    setup_db();
    setup_cursor();

    /* ********************************************************************** */

    char key           = 'g';
    char data          = 'g';
    int forward = is_next ? 1 : -1;

    insert(key, data);
    insert((char)(key + forward), data);
    c_get(flag, key, data);
    insert(key, (char)(data + forward));
    c_get(flag, (char)(key + forward), data);

    /* ********************************************************************** */
    close_cursor();
    close_db();
    close_env();
}

static void run_test(void) {
    /* ********************************************************************** */
    /* Test DB_NEXT works properly. */
    test_skip_key(DB_NEXT, true);
    /* ********************************************************************** */
    /* Test DB_PREV works properly. */
    test_skip_key(DB_PREV, false);
}

int
test_main(int argc, char *const argv[]) {

    parse_args(argc, argv);
  
    
    run_test();

    return 0;
}
