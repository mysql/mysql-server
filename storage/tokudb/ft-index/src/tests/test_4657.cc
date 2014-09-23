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
#include "test.h"

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;

//
// test that nodes written out for checkpointing properly update the stats
// insert a bunch of elements, but not too many. The amount of data should
// fit into a single leaf node. Then we :
//  - checkpoint
//  - close dictionary
//  - reopen dictionary
//  - call stat64
// prior to the fix for 4657, the stats would return
// 0 rows. After the fix, the stats should return an
// accurate number of rows
//

int test_main (int argc, char * const argv[]) {
  parse_args(argc, argv);
  int r;
  toku_os_recursive_delete(TOKU_TEST_FILENAME);
  toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
  DB_ENV *env;
  r = db_env_create(&env, 0);                                                         CKERR(r);
  env->set_errfile(env, stderr);
  r = env->set_default_bt_compare(env, int64_dbt_cmp); CKERR(r);
  r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    
  DB *db;
  {
    DB_TXN *txna;
    r = env->txn_begin(env, NULL, &txna, 0);                                        CKERR(r);

    r = db_create(&db, env, 0);                                                     CKERR(r);
    CKERR(r);
    r = db->open(db, txna, "foo.db", NULL, DB_BTREE, DB_CREATE, 0666);              CKERR(r);

    r = txna->commit(txna, 0);                                                      CKERR(r);
  }
  if (verbose) printf("starting insertion of even elements\n");
  //
  // now insert 1000 elements
  //
  DB_TXN* txn;
  r = env->txn_begin(env, NULL, &txn, 0);
  CKERR(r);
  for (uint32_t i = 0; i < 1000; i++) {
      DBT key,val;
      uint64_t key_data = i;
      uint64_t val_data = i;
      r = db->put(
          db,
          txn,
          dbt_init(&key, &key_data, sizeof(key_data)),
          dbt_init(&val, &val_data, sizeof(val_data)),
          0
          );       
      CKERR(r);
  }
  r = txn->commit(txn, 0);
  CKERR(r);

  //
  // the assumption here is that the db consists
  // of a single leaf node that is the root.
  //

  r = env->txn_checkpoint(env, 0, 0, 0);
  CKERR(r);
  r = db->close(db, 0);
  CKERR(r);

  // now reopen 
  r = db_create(&db, env, 0);
  CKERR(r);
  r = db->open(db, NULL, "foo.db", NULL, DB_BTREE, DB_THREAD, 0666);
  CKERR(r);
  DB_BTREE_STAT64 dict_stats;
  r = db->stat64(
      db, 
      NULL, 
      &dict_stats
      );
  CKERR(r);
  // check that stats are correct
  assert(dict_stats.bt_nkeys == 1000);
  
  r = db->close(db, 0);
  CKERR(r);
  
  r = env->close(env, 0);
  CKERR(r);
    
  return 0;
}
