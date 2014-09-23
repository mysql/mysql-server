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
// Test that isolation works right for subtransactions.
// In particular, check to see what happens if a subtransaction has different isolation level from its parent.

#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

int test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    uint32_t i = 0;
    uint32_t num_read_txns = 1000;
    r = db_env_create(&env, 0);                                                         CKERR(r);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
    DB *db;

    DB_TXN* create_txn;
    DB_TXN* read_txns[num_read_txns];
    DB_TXN* read_uncommitted_txn;
    memset(read_txns, 0, sizeof(read_txns));

    r = env->txn_begin(env, NULL, &create_txn, 0);                                        CKERR(r);

    r = db_create(&db, env, 0);                                                     CKERR(r);
    r = db->open(db, create_txn, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);
    r = create_txn->commit(create_txn, 0);                                                      CKERR(r);

    DBT key,val;

    for (i = 0; i < num_read_txns; i++) {
        DB_TXN* put_txn = NULL;
        uint32_t data = i;
        r = env->txn_begin(env, NULL, &put_txn, DB_TXN_SNAPSHOT);
        CKERR(r);
        r = db->put(
            db, 
            put_txn, 
            dbt_init(&key, "a", 2), 
            dbt_init(&val, &data, 4), 
            0
            );       
        CKERR(r);
        r = put_txn->commit(put_txn, 0);
        CKERR(r);
        //this should read the above put
        r = env->txn_begin(env, NULL, &read_txns[i], DB_TXN_SNAPSHOT);
        CKERR(r);
            
    }

    for (i = 0; i < num_read_txns; i++) {
        DBT curr_key, curr_val;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));
        DBC* snapshot_cursor = NULL;
        r = db->cursor(db, read_txns[i], &snapshot_cursor, 0); CKERR(r);        
        r = snapshot_cursor->c_get(snapshot_cursor, &curr_key, &curr_val, DB_NEXT); CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'a');
        assert((*(uint32_t *)(curr_val.data)) == i);
        assert(curr_key.size == 2);
        assert(curr_val.size == 4);
        snapshot_cursor->c_close(snapshot_cursor);
    }
    {
        DBT curr_key, curr_val;
        memset(&curr_key, 0, sizeof(curr_key));
        memset(&curr_val, 0, sizeof(curr_val));
        r = env->txn_begin(env, NULL, &read_uncommitted_txn, DB_READ_UNCOMMITTED);
        CKERR(r);
        DBC* read_uncommitted_cursor = NULL;
        r = db->cursor(db, read_uncommitted_txn, &read_uncommitted_cursor, 0); CKERR(r);        
        r = read_uncommitted_cursor->c_get(
            read_uncommitted_cursor, 
            &curr_key, 
            &curr_val, 
            DB_NEXT
            ); 
        CKERR(r);
        assert(((char *)(curr_key.data))[0] == 'a');
        assert((*(uint32_t *)(curr_val.data)) == (num_read_txns - 1));
        assert(curr_key.size == 2);
        assert(curr_val.size == 4);
        read_uncommitted_cursor->c_close(read_uncommitted_cursor);
    }
    for (i = 0; i < num_read_txns; i++) {
        r = read_txns[i]->commit(read_txns[i], 0);
        CKERR(r);
    }
    r = read_uncommitted_txn->commit(read_uncommitted_txn, 0);

    r = db->close(db, 0);                                                               CKERR(r);
    r = env->close(env, 0);                                                             CKERR(r);
    
    return 0;
}
