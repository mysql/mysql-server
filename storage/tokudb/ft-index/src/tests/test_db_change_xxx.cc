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

/* Can I close a db without opening it? */

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <db.h>


int
test_main (int UU(argc), char UU(*const argv[])) {
    int r;
    DB_ENV *env;
    DB *db;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);       assert(r==0);
    r=db_env_create(&env, 0); assert(r==0);
    r=env->open(env, TOKU_TEST_FILENAME, DB_PRIVATE|DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r==0);

    uint32_t ret_val = 0;
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->set_pagesize(db, 112024);
    CKERR(r);
    r = db->change_pagesize(db, 202433);
    CKERR2(r, EINVAL);
    r = db->get_pagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 112024);
    r = db->set_readpagesize(db, 33024);
    CKERR(r);
    r = db->change_readpagesize(db, 202433);
    CKERR2(r, EINVAL);
    r = db->get_readpagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 33024);

    enum toku_compression_method method = TOKU_ZLIB_METHOD;
    enum toku_compression_method ret_method = TOKU_NO_COMPRESSION;
    r = db->set_compression_method(db, method);
    CKERR(r);
    r = db->change_compression_method(db, method);
    CKERR2(r, EINVAL);
    r = db->get_compression_method(db, &ret_method);
    CKERR(r);
    assert(ret_method == TOKU_ZLIB_METHOD);

    // now do the open
    const char * const fname = "test.change_xxx";
    r = db->open(db, NULL, fname, "main", DB_BTREE, DB_CREATE, 0666);
    CKERR(r);
    
    r = db->get_pagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 112024);
    r = db->get_readpagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 33024);
    ret_method = TOKU_NO_COMPRESSION;
    r = db->get_compression_method(db, &ret_method);
    CKERR(r);
    assert(ret_method == TOKU_ZLIB_METHOD);

    r = db->set_pagesize(db, 2024);
    CKERR2(r, EINVAL);
    r = db->set_readpagesize(db, 1111);
    CKERR2(r, EINVAL);
    r = db->set_compression_method(db, TOKU_NO_COMPRESSION);
    CKERR2(r, EINVAL);

    r = db->change_pagesize(db, 100000);
    CKERR(r);
    r = db->change_readpagesize(db, 10000);
    CKERR(r);
    r = db->change_compression_method(db, TOKU_LZMA_METHOD);
    CKERR(r);
    
    r = db->get_pagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 100000);
    r = db->get_readpagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 10000);
    ret_method = TOKU_NO_COMPRESSION;
    r = db->get_compression_method(db, &ret_method);
    CKERR(r);
    assert(ret_method == TOKU_LZMA_METHOD);

    r = db->close(db, 0);
    
    r = db_create(&db, env, 0);
    CKERR(r);
    r = db->open(db, NULL, fname, "main", DB_BTREE, DB_AUTO_COMMIT, 0666);
    CKERR(r);

    r = db->get_pagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 100000);
    r = db->get_readpagesize(db, &ret_val);
    CKERR(r);
    assert(ret_val == 10000);
    ret_method = TOKU_NO_COMPRESSION;
    r = db->get_compression_method(db, &ret_method);
    CKERR(r);
    assert(ret_method == TOKU_LZMA_METHOD);

    r = db->close(db, 0);

    r=env->close(env, 0);     assert(r==0);
    return 0;
}
