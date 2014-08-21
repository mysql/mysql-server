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

#include <memory.h>
#include <db.h>

#include <errno.h>
#include <sys/stat.h>


// TOKU_TEST_FILENAME is defined in the Makefile

DB *db;
DB_ENV* dbenv;
DBC*    cursors[(int)256];
DB_TXN* null_txn = NULL;

static void
put (int _key, int _data) {
    int r;
    DBT key;
    DBT data;
    dbt_init(&key,  &_key,  sizeof(int));
    dbt_init(&data, &_data, sizeof(int));
    if (_key == -1) {
        key.data = NULL;
        key.size = 0;
    }
    if (_data == -1) {
        data.data = NULL;
        data.size = 0;
    }
    
    r = db->put(db, null_txn, &key, &data, 0);
    CKERR(r);
}

static void
cget (uint32_t flag, bool find, char txn, int _key, int _data) {
    assert(cursors[(int)txn]);

    int r;
    DBT key;
    DBT data;
    if (flag == DB_CURRENT) {
        _key++;
        _data++;
        dbt_init(&key,  &_key,  sizeof(int));
        dbt_init(&data, &_data, sizeof(int));
        _key--;
        _data--;
    }
    else if (flag == DB_SET) {
        dbt_init(&key,  &_key,  sizeof(int));
        if (_key == -1) {
            key.data = NULL;
            key.size = 0;
        }
        _data++;
        dbt_init(&data, &_data, sizeof(int));
        _data--;
    }
    else assert(false);
    r = cursors[(int)txn]->c_get(cursors[(int)txn], &key, &data, flag);
    if (find) {
        CKERR(r);
        if (_key == -1) {
            assert(key.data == NULL);
            assert(key.size == 0);
        }
        else {
            assert(key.size == sizeof(int));
            assert(*(int*)key.data == _key);
        }
        if (_data == -1) {
            assert(data.data == NULL);
            assert(data.size == 0);
        }
        else {
            assert(data.size == sizeof(int));
            assert(*(int*)data.data == _data);
        }
    }
    else        CKERR2(r, DB_NOTFOUND);
}

static void
init_dbc (char name) {
    int r;

    assert(!cursors[(int)name]);
    r = db->cursor(db, null_txn, &cursors[(int)name], 0);
        CKERR(r);
    assert(cursors[(int)name]);
}

static void
close_dbc (char name) {
    int r;

    assert(cursors[(int)name]);
    r = cursors[(int)name]->c_close(cursors[(int)name]);
        CKERR(r);
    cursors[(int)name] = NULL;
}

static void
setup_dbs (void) {
    int r;

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    dbenv   = NULL;
    db      = NULL;
    /* Open/create primary */
    r = db_env_create(&dbenv, 0);
        CKERR(r);
    uint32_t env_txn_flags  = 0;
    uint32_t env_open_flags = DB_CREATE | DB_PRIVATE | DB_INIT_MPOOL;
	r = dbenv->open(dbenv, TOKU_TEST_FILENAME, env_open_flags | env_txn_flags, 0600);
        CKERR(r);
    
    r = db_create(&db, dbenv, 0);
        CKERR(r);

    char a;
    r = db->open(db, null_txn, "foobar.db", NULL, DB_BTREE, DB_CREATE, 0600);
        CKERR(r);
    for (a = 'a'; a <= 'z'; a++) init_dbc(a);
}

static void
close_dbs (void) {
    char a;
    for (a = 'a'; a <= 'z'; a++) {
        if (cursors[(int)a]) close_dbc(a);
    }

    int r;
    r = db->close(db, 0);
        CKERR(r);
    db      = NULL;
    r = dbenv->close(dbenv, 0);
        CKERR(r);
    dbenv   = NULL;
}

static void
test (void) {
    /* ********************************************************************** */
    int key;
    int data;
    int i;
    for (i = 0; i < 4; i++) {
        if (i & 0x1) key  = -1;
        else         key  = 1;
        if (i & 0x2) data = -1;
        else         data = 1;
        setup_dbs();
        put(key, data);
        cget(DB_SET,     true, 'a', key, data);
        cget(DB_CURRENT, true, 'a', key, data);
        close_dbs();
    }
    /* ********************************************************************** */
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    test();
    return 0;
}
