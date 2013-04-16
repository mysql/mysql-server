/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
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

/* This test ensures that a db->get DB_NOTFOUND does not cause an
   exception when exceptions are enabled, which is consistent with
   Berkeley DB */

#include <db_cxx.h>
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#define DIR __FILE__ ".dir"
#define fname "foo.tdb"

void db_put(Db *db, int k, int v) {
    Dbt key(&k, sizeof k);
    Dbt val(&v, sizeof v);
    int r = db->put(0, &key, &val, 0); 
    assert(r == 0);
}

void db_del(Db *db, int k) {
    Dbt key(&k, sizeof k);
    int r = db->del(0, &key, 0); 
    assert(r == 0);
}

void db_get(Db *db, int k, int v, int expectr) {
    Dbt key(&k, sizeof k);
    Dbt val; val.set_data(&v);  val.set_ulen(sizeof v); val.set_flags(DB_DBT_USERMEM);
    int r = db->get(0, &key, &val, 0); 
    assert(r == expectr);
    if (r == 0) {
        assert(val.get_size() == sizeof v);
        int vv;
        memcpy(&vv, val.get_data(), val.get_size());
        assert(vv == v);
    }
}

DbEnv *env = NULL;
void reset_env (void) {
    system("rm -rf " DIR);
    toku_os_mkdir(DIR, 0777);
    if (env) delete env;
    env = new DbEnv(DB_CXX_NO_EXCEPTIONS);
    { int r = env->set_redzone(0); assert(r==0); }
    int r = env->open(DIR, DB_INIT_MPOOL + DB_CREATE + DB_PRIVATE, 0777); 
    assert(r == 0);
}

void test_not_found(void) {
    int r;
    reset_env();
    Db *db = new Db(env, DB_CXX_NO_EXCEPTIONS); assert(db);
    r = db->open(0, fname, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    db_put(db, 1, 2);
    db_get(db, 1, 2, 0);
    db_del(db, 1);
    db_get(db, 1, 0, DB_NOTFOUND);
    r = db->close(0); assert(r == 0);
    delete db;
}

void test_exception_not_found(void) {
    int r;
    
    reset_env();
    Db *db = new Db(env, 0); assert(db);
    r = db->open(0, fname, 0, DB_BTREE, DB_CREATE, 0777); assert(r == 0);
    db_put(db, 1, 2);
    db_get(db, 1, 2, 0);
    db_del(db, 1);
    try {
        db_get(db, 1, 0, DB_NOTFOUND);
    } catch(...) {
        assert(0);
    }
    r = db->close(0); assert(r == 0);
    delete db;
}

int main(int argc, char *argv[]) {
    test_not_found();
    test_exception_not_found();
    if (env) delete env;
    return 0;
}
