/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"


#include "test.h"
#include "toku_pthread.h"
#include <portability/toku_atomic.h>

static int my_compare (DB *db, const DBT *a, const DBT *b) {
    assert(db);
    assert(db->cmp_descriptor);
    assert(db->cmp_descriptor->dbt.size >= 3);
    char *CAST_FROM_VOIDP(data, db->cmp_descriptor->dbt.data);
    assert(data[0]=='f');
    assert(data[1]=='o');
    assert(data[2]=='o');
    if (verbose) printf("compare descriptor=%s\n", data);
    sched_yield();
    return uint_dbt_cmp(db, a, b);
}

DB_ENV *env;
DB     *db;
const char   *env_dir = TOKU_TEST_FILENAME;
volatile int done = 0;

static void *startA (void *ignore __attribute__((__unused__))) {
    for (int i=0;i<999; i++) {
	DBT k,v;
	int a = (random()<<16) + i;
	dbt_init(&k, &a, sizeof(a));
	dbt_init(&v, &a, sizeof(a));
	DB_TXN *txn;
    again:
	{ int chk_r = env->txn_begin(env, NULL, &txn, DB_TXN_NOSYNC); CKERR(chk_r); }
	{
	    int r = db->put(db, txn, &k, &v, 0);
	    if (r==DB_LOCK_NOTGRANTED) {
		if (verbose) printf("lock not granted on %d\n", i);
		{ int chk_r = txn->abort(txn); CKERR(chk_r); }
		goto again;
	    }
	    assert(r==0);
	}
	{ int chk_r = txn->commit(txn, 0); CKERR(chk_r); }
    }
    int r __attribute__((__unused__)) = toku_sync_fetch_and_add(&done, 1);
    return NULL;
}
static void change_descriptor (DB_TXN *txn, int i) {
    DBT desc;
    char foo[100];
    snprintf(foo, 99, "foo%d", i);
    dbt_init(&desc, foo, 1+strlen(foo));
    int r;
    if (verbose) printf("trying to change to %s\n", foo);
    while ((r=db->change_descriptor(db, txn, &desc, 0))) {
	if (verbose) printf("Change failed r=%d, try again\n", r);
    }
    if (verbose) printf("ok\n");
}
static void startB (void) {
    for (int i=0; !done; i++) {
	IN_TXN_COMMIT(env, NULL, txn, 0,
		      change_descriptor(txn, i));
	sched_yield();
    }
}

static void my_parse_args (int argc, char * const argv[]) {
    const char *argv0=argv[0];
    while (argc>1) {
	int resultcode=0;
	if (strcmp(argv[1], "-v")==0) {
	    verbose++;
	} else if (strcmp(argv[1],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[1],"--envdir")==0) {
	    assert(argc>2);
	    env_dir = argv[2];
	    argc--;
	    argv++;
	} else if (strcmp(argv[1], "-h")==0) {
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q] [-h] [--envdir <envdir>]\n", argv0);
	    exit(resultcode);
	} else {
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
}

int test_main(int argc, char * const argv[]) {
    my_parse_args(argc, argv);

    db_env_set_num_bucket_mutexes(32);
    { int chk_r = db_env_create(&env, 0); CKERR(chk_r); }
    { int chk_r = env->set_redzone(env, 0); CKERR(chk_r); }
    { int chk_r = env->set_default_bt_compare(env, my_compare); CKERR(chk_r); }
    {
	const int size = 10+strlen(env_dir);
	char cmd[size];
	snprintf(cmd, size, "rm -rf %s", env_dir);
	int r = system(cmd);
        CKERR(r);
    }
    { int chk_r = toku_os_mkdir(env_dir, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
    const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
    { int chk_r = env->open(env, env_dir, envflags, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }

    { int chk_r = db_create(&db, env, 0); CKERR(chk_r); }
    { int chk_r = db->set_pagesize(db, 1024); CKERR(chk_r); }
    { int chk_r = db->open(db, NULL, "db", NULL, DB_BTREE, DB_CREATE, 0666); CKERR(chk_r); }
    DBT desc;
    dbt_init(&desc, "foo", sizeof("foo"));
    IN_TXN_COMMIT(env, NULL, txn, 0,
                  { int chk_r = db->change_descriptor(db, txn, &desc, DB_UPDATE_CMP_DESCRIPTOR); CKERR(chk_r); });
    pthread_t thd;
    { int chk_r = toku_pthread_create(&thd, NULL, startA, NULL); CKERR(chk_r); }

    startB();

    void *retval;
    { int chk_r = toku_pthread_join(thd, &retval); CKERR(chk_r); }
    assert(retval==NULL);

    { int chk_r = db->close(db, 0); CKERR(chk_r); }


    { int chk_r = env->close(env, 0); CKERR(chk_r); }

    return 0;
}
