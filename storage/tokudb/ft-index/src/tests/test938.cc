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
#include <db.h>
#include <sys/stat.h>

unsigned char N=5;

static int
fact(int n) {
    if (n<=2) return n;
    else return n*fact(n-1);
}

static void
swapc (unsigned char *a, unsigned char *b) {
    unsigned char tmp=*a;
    *a=*b;
    *b=tmp;
}

DB_ENV *env;
DB *db;

static void
run (int choice) {
    unsigned char v[N];
    int i;
    int r;
    for (i=0; i<N; i++) {
	v[i]=(unsigned char)(10*i);
    }
    for (i=0; i<N; i++) {
	int nchoices=N-i;
	swapc(&v[i], &v[i+choice%nchoices]);
	choice=choice/nchoices;
    }
    if (0) {
	for (i=0; i<N; i++) {
	    printf("%d ", v[i]);
	}

	printf("\n");
    }
    DB_TXN *txn;
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	for (i=0; i<N; i++) {
	    DBT kdbt,vdbt;
	    char key[2]={25,(char)v[i]};
	    char val=v[i];
	    //printf("put %d %d\n", key, val);
	    r=db->put(db, txn, dbt_init(&kdbt, &key, 2), dbt_init(&vdbt, &val, 1), 0); CKERR(r);
	}
	r=txn->commit(txn, DB_TXN_NOSYNC);                                        CKERR(r);
    }
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);	
	DBC *c;
	r=db->cursor(db, txn, &c, 0);                                 CKERR(r);
	DBT kdbt,vdbt;
	memset(&kdbt, 0, sizeof(kdbt));
	memset(&vdbt, 0, sizeof(vdbt));
	i=0;
	while (0==(r=c->c_get(c, &kdbt, &vdbt, DB_NEXT))) {
	    //printf("Got %d %d\n", *(unsigned char*)kdbt.data, *(unsigned char*)vdbt.data);
	    i++;
	    kdbt.data=0;
	    vdbt.data=0;
	}
	CKERR2(r, DB_NOTFOUND);
	//printf("i=%d N=%d\n", i, N);
	assert(i==N);
	r=c->c_close(c);                                                          CKERR(r);
	r=txn->commit(txn, DB_TXN_NOSYNC);                                        CKERR(r);
    }
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBC *c;
	r=db->cursor(db, txn, &c, 0);                                 CKERR(r);
	DBT kdbt,vdbt;
	memset(&kdbt, 0, sizeof(kdbt));
	memset(&vdbt, 0, sizeof(vdbt));
	i=0;
	while (0==(r=(c->c_get(c, &kdbt, &vdbt, DB_FIRST)))) {
	    i++;
	    r = db->del(db, txn, &kdbt, DB_DELETE_ANY);
	    CKERR(r);
	}
	assert(r==DB_NOTFOUND);
	r=c->c_close(c);                                                          CKERR(r);
	r=txn->commit(txn, DB_TXN_NOSYNC);                                        CKERR(r);
    }
    return;
#if 0
    char v101=101, v102=102, v1=1, v2=2;
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBT k,v;
	r=db->put(db, txn, dbt_init(&k, &v1, 1), dbt_init(&v, &v101, 1), 0); CKERR(r);
	r=db->put(db, txn, dbt_init(&k, &v2, 1), dbt_init(&v, &v102, 1), 0); CKERR(r);
	r=txn->commit(txn, 0);                                        CKERR(r);
    }
    {
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	DBC *c;
	r=db->cursor(db, txn, &c, 0);                                 CKERR(r);
	DBT k,v;
	r=c->c_get(c, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_FIRST); CKERR(r);
	assert(*(char*)k.data==v1); assert(*(char*)v.data==v101);
	r=c->c_get(c, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_NEXT);  CKERR(r);
	assert(*(char*)k.data==v2); assert(*(char*)v.data==v102);
	r=c->c_get(c, dbt_init_malloc(&k), dbt_init_malloc(&v), DB_NEXT);  assert(r!=0);
	r=c->c_close(c);                                                   CKERR(r);
	r=txn->commit(txn, 0);                                             CKERR(r);
    }
#endif
}

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_TXN *txn;
    {
        r = db_env_create(&env, 0);                                   CKERR(r);
	r = env->set_redzone(env, 0);                                 CKERR(r);
	r=env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(r);
	env->set_errfile(env, stderr);
	r=env->txn_begin(env, 0, &txn, 0);                            CKERR(r);
	r=db_create(&db, env, 0);                                     CKERR(r);
	r=db->open(db, txn, "foo.db", 0, DB_BTREE, DB_CREATE, S_IRWXU+S_IRWXG+S_IRWXO);  CKERR(r);
	r=txn->commit(txn, 0);                                        CKERR(r);
    }
    int i;
    //printf("fact(%d)=%d\n", N, fact(N));
    for (i=0; i<fact(N); i++) {
	run(i);
    }
    {
	r=db->close(db, 0);                                           CKERR(r);
	r=env->close(env, 0);                                         CKERR(r);
    }

    return 0;
}
