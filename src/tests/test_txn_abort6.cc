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
#include <sys/stat.h>
#include <db.h>


#define N_TXNS 4

static void
test_txn_abort (int n, int which_guys_to_abort) {
    if (verbose>1) printf("test_txn_abort(%d,%x)\n", n, which_guys_to_abort);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env;
    r = db_env_create(&env, 0); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_MPOOL + DB_INIT_LOG + DB_INIT_LOCK + DB_INIT_TXN + DB_PRIVATE + DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); 
    if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
    assert(r == 0);

    DB *db;
    {
	DB_TXN *txn;
	r = env->txn_begin(env, 0, &txn, 0); assert(r == 0);
	
	r = db_create(&db, env, 0); assert(r == 0);
	r = db->open(db, txn, "test.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);
	r = txn->commit(txn, 0); assert(r == 0);
    }
    {
	DB_TXN *txns[N_TXNS];
	{
	    int j;
	    for (j=0; j<N_TXNS; j++) {
		r = env->txn_begin(env, 0, &txns[j], 0); assert(r == 0);
	    }
	}
	
	{
	    int i;
	    for (i=0; i<n; i++) {
		int j;
		for (j=N_TXNS; j>0; j--) {
		    if (i%j==0) { // This is guaranteed to be true when j==1, so someone will do it.
			DBT key, val;
			r = db->put(db, txns[j-1], dbt_init(&key, &i, sizeof i), dbt_init(&val, &i, sizeof i), 0); 
			if (r != 0) printf("%s:%d:%d:%s\n", __FILE__, __LINE__, r, db_strerror(r));
			assert(r == 0);
			goto didit;
		    }
		}
	        toku_hard_crash_on_purpose();
	    didit: ;
	    }
	}
	{
	    int j;
	    for (j=0; j<N_TXNS; j++) {
		if (which_guys_to_abort&(1<<j)) {
		    r = txns[j]->abort(txns[j]);
		} else {
		    r = txns[j]->commit(txns[j], 0);
		}
		if (r != 0) printf("%s:%d:abort:%d\n", __FILE__, __LINE__, r);
		assert(r == 0);
	    }
	}
    }
    {
	DB_TXN *txn;
	int i;
	r = env->txn_begin(env, 0, &txn, 0); assert(r==0);
	if (verbose>1) printf("Now see what's there:  which_guys_to_abort=%x: ", which_guys_to_abort);
	for (i=0; i<n; i++) {
	    DBT key,val;
	    memset(&val, 0, sizeof val);
	    r = db->get(db, txn, dbt_init(&key, &i, sizeof i), &val, 0);
	    if (r==0) { if (verbose>1) printf(" %d", i); }
	}
	if (verbose>1) printf("\n");
	for (i=0; i<n; i++) {
	    DBT key,val;
	    memset(&val, 0, sizeof val);
	    r = db->get(db, txn, dbt_init(&key, &i, sizeof i), &val, 0);
	    int j;
	    for (j=N_TXNS; j>0; j--) {
		if (i%j==0) {
		    if (which_guys_to_abort&(1<<(j-1))) assert(r==DB_NOTFOUND);
		    else assert(r==0);
		    break;
		}
	    }
	}
	r = txn->commit(txn, 0);             assert(r==0);
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);
}

int
test_main(int argc, char *const argv[]) {
    int i,j;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            verbose++;
            continue;
        }
    }
    if (verbose>0) printf("%s:", __FILE__);
    if (verbose==1) printf("\n");
    for (j=0; j<(1<<N_TXNS); j++)
	for (i=1; i<100; i*=2) 
	    test_txn_abort(i, j);
    if (verbose>0) printf("OK\n");
    return 0;
}
