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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#include "test.h"
#include <sys/wait.h>

// Verify that if we prepare a transaction, then commit a bunch more transactions so that the logs may have been rotated, then the transaction can commit or abort properly on recovery.

static void clean_env (const char *envdir) {
    const int len = strlen(envdir)+100;
    char cmd[len];
    snprintf(cmd, len, "rm -rf %s", envdir);
    int r = system(cmd);
    CKERR(r);
    CKERR(toku_os_mkdir(envdir, S_IRWXU+S_IRWXG+S_IRWXO));
}

static void setup_env (DB_ENV **envp, const char *envdir) {
    { int chk_r = db_env_create(envp, 0); CKERR(chk_r); }
    (*envp)->set_errfile(*envp, stderr);
    { int chk_r = (*envp)->set_redzone(*envp, 0); CKERR(chk_r); }
    { int chk_r = (*envp)->open(*envp, envdir, DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_INIT_TXN|DB_CREATE|DB_PRIVATE|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO); CKERR(chk_r); }
}

#define NTXNS 6

static void setup_env_and_prepare (DB_ENV **envp, const char *envdir) {
    DB *db;
    clean_env(envdir);
    setup_env(envp, envdir);
    CKERR(db_create(&db, *envp, 0));
    CKERR(db->open(db, NULL, "foo.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO));

    {
	DB_TXN *txn;
	CKERR((*envp)->txn_begin(*envp, 0, &txn, 0));
	for (int tnum=0; tnum<NTXNS; tnum++) {
	    for (int k=0; k<26; k++) {
		#define DSIZE 200
		char data[DSIZE];
		memset(data, ' ', DSIZE);
		data[0]='a'+tnum;
		data[1]='a'+k;
		data[DSIZE-1]=0;
                DBT key;
                dbt_init(&key, data, DSIZE);
		CKERR(db->put(db, txn, &key, &key, 0));
	    }
	}
	CKERR(txn->commit(txn, 0));
    }

    for (int tnum=0; tnum<NTXNS; tnum++) {
	DB_TXN *txn;
	CKERR((*envp)->txn_begin(*envp, 0, &txn, 0));
	char data[3]={(char)('a'+tnum),'_',0};
	DBT key;
        dbt_init(&key, data, 3);
	CKERR(db->put(db, txn, &key, &key, 0));
	uint8_t gid[DB_GID_SIZE];
	memset(gid, 0, DB_GID_SIZE);
	gid[0]='a'+tnum;
	CKERR(txn->prepare(txn, gid));
	// Drop txn on the ground, since we will commit or abort it after recovery
	if (tnum==0) {
	    //printf("commit %d\n", tnum);
	    CKERR(txn->commit(txn, 0));
	} else if (tnum==1) {
	    //printf("abort %d\n", tnum);
	    CKERR(txn->abort(txn));
	} else {
	    //printf("prepare %d\n", tnum);
	}
    }
    { int chk_r = db->close(db, 0); CKERR(chk_r); }
}

enum prepared_state {
    COMMITTED,
    ABORTED,
    MAYBE_COMMITTED,
    MAYBE_ABORTED,
    PREPARED};
    

static void check_prepared_list (enum prepared_state ps[NTXNS], long count, DB_PREPLIST *l) {
    int count_prepared=0;
    int count_maybe_prepared=0;
    for (int j=0; j<NTXNS; j++) {
	switch (ps[j]) {
	case COMMITTED:
	case ABORTED:
	    goto next;
	case PREPARED:
	    count_prepared++;
	case MAYBE_COMMITTED:
	case MAYBE_ABORTED:
	    count_maybe_prepared++;
	    goto next;
	}
	assert(0);
    next:;
    }
	    
    assert(count>=count_prepared && count<=count_maybe_prepared);

    bool found[NTXNS];
    for (int j=0; j<NTXNS; j++) {
	found[j] = (ps[j]!=PREPARED);
    }

    // now found[j] is false on those transactions that I hope to find in the prepared list.
    for (int j=0; j<count; j++) {
	int num = l[j].gid[0]-'a';
	assert(num>=0 && num<NTXNS);
	switch (ps[num]) {
	case PREPARED:
	    assert(!found[num]);
	    found[num]=true;
	    break;
	default:;
	}
	for (int i=1; i<DB_GID_SIZE; i++) {
		assert(l[j].gid[i]==0);
	}
    }
}

static void get_prepared (DB_ENV *env, long *count, DB_PREPLIST *l) {
    CKERR(env->txn_recover(env, l, NTXNS, count, DB_FIRST));
    //printf("%s:%d count=%ld\n", __FILE__, __LINE__, *count);
    assert(*count>=0);
}

static void check_prepared_txns (DB_ENV *env, enum prepared_state ps[NTXNS]) {
    DB_PREPLIST l[NTXNS];
    long count=-1;
    get_prepared(env, &count, l);
    check_prepared_list(ps, count, l);
}

static void check_state_after_full_recovery (DB_ENV *env) {
    DB *db;
    CKERR(db_create(&db, env, 0));
    CKERR(db->open(db, NULL, "foo.db", 0, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, S_IRWXU+S_IRWXG+S_IRWXO));

    for (int tnum=0; tnum<NTXNS; tnum++) {
	DB_TXN *txn;
	CKERR(env->txn_begin(env, 0, &txn, 0));
	char data[3]={(char)('a'+tnum),'_',0};
	DBT key;
        dbt_init(&key, data, 3);
	DBT dbt_data;
        dbt_init(&dbt_data, NULL, 0);
	int r = db->get(db, txn, &key, &dbt_data, 0);
	if (tnum%2==0) {
	    assert(r==0);
	    assert(dbt_data.size==3 && memcmp(dbt_data.data, data, 3)==0);
	} else {
	    assert(r==DB_NOTFOUND);
	}
	CKERR(txn->commit(txn, 0));
    }
    CKERR(db->close(db, 0));
}

static void waitfor (pid_t pid) {
    int status;
    pid_t pid2 = wait(&status);
    assert(pid2==pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
}

static void abort_number(int num, int count, DB_PREPLIST *l) {
    for (int j=0; j<count; j++) {
	if (l[j].gid[0]=='a'+num) {
	    CKERR(l[j].txn->abort(l[j].txn));
	    return;
	}
    }
    assert(0);
}
static void commit_number(int num, int count, DB_PREPLIST *l) {
    for (int j=0; j<count; j++) {
	if (l[j].gid[0]=='a'+num) {
	    CKERR(l[j].txn->commit(l[j].txn, 0));
	    return;
	}
    }
    assert(0);
}

static void test (void) {
    pid_t pid;

    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env_and_prepare(&env, TOKU_TEST_FILENAME);
	enum prepared_state prepared[NTXNS]={COMMITTED,ABORTED,PREPARED,PREPARED,PREPARED,PREPARED};
	check_prepared_txns(env, prepared);
	exit(0);
    }
    waitfor(pid);
    // Now run recovery and crash on purpose.
    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	enum prepared_state prepared[NTXNS]={COMMITTED,ABORTED,PREPARED,PREPARED,PREPARED,PREPARED};
	check_prepared_txns(env, prepared);
	exit(0);
    }
    waitfor(pid);

    // Now see if recovery works the second time.
    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	enum prepared_state prepared[NTXNS]={COMMITTED,ABORTED,PREPARED,PREPARED,PREPARED,PREPARED};
	check_prepared_txns(env, prepared);
	exit(0);
    }
    waitfor(pid);

    // Now see if recovery works the third time.
    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	enum prepared_state prepared[NTXNS]={COMMITTED,ABORTED,PREPARED,PREPARED,PREPARED,PREPARED};
	DB_PREPLIST l[NTXNS];
	long count=-1;
	get_prepared(env, &count, l);
	check_prepared_list(prepared, count, l);
	abort_number(3, count, l);
	commit_number(2, count, l); // do the commit second so it will make it to disk.
	exit(0);
    }
    waitfor(pid);
    // Now see if recovery works a third time, with number 2 and 3 no longer in the prepared state.
    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	enum prepared_state prepared[NTXNS]={COMMITTED,ABORTED,MAYBE_COMMITTED,MAYBE_ABORTED,PREPARED,PREPARED};
	DB_PREPLIST l[NTXNS];
	long count=-1;
	//printf("%s:%d count=%ld\n", __FILE__, __LINE__, count); // it's a little bit funky that the committed transactions in BDB (from commit_number(2,...) above) don't stay committed.  But whatever...
	get_prepared(env, &count, l);
	check_prepared_list(prepared, count, l);
	exit(0);
    }
    waitfor(pid);
    // Now see if recovery works a fourth time, with number 2 and 3 no longer in the prepared state.
    // This time we'll do get_prepared with a short count.
    if (0==(pid=fork())) {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	//printf("%s:%d count=%ld\n", __FILE__, __LINE__, count); // it's a little bit funky that the committed transactions in BDB (from commit_number(2,...) above) don't stay committed.  But whatever...

	long actual_count=0;

	for (int recover_num=0; 1; recover_num++) {
	    long count=-1;
	    DB_PREPLIST *MALLOC_N(1, l); // use malloc so that valgrind might notice a problem
	    CKERR(env->txn_recover(env, l, 1, &count, recover_num==0 ? DB_FIRST : DB_NEXT));
	    //printf("recover_num %d count=%ld\n", recover_num,count);
	    if (count==0) break;
	    actual_count++;
	    if ((l[0].gid[0]-'a')%2==0) {
		CKERR(l[0].txn->commit(l[0].txn, 0));
	    } else {
		CKERR(l[0].txn->abort(l[0].txn));
	    }
	    toku_free(l);
	}
	//printf("actual_count=%ld\n", actual_count);

	// Now let's see what the state is.
	check_state_after_full_recovery(env);

	CKERR(env->close(env, 0));
	exit(0);
    }
    waitfor(pid);
    // Now we should end up with nothing in the recovery list.
    {
	DB_ENV *env;
	setup_env(&env, TOKU_TEST_FILENAME);
	long count=-1;
	DB_PREPLIST l[1];
	CKERR(env->txn_recover(env, l, 1, &count, DB_FIRST));
	assert(count==0);
	check_state_after_full_recovery(env);
	CKERR(env->close(env, 0));
    }	
	

}

int test_main (int argc, char *const argv[]) {
    default_parse_args(argc, argv);
    // first test: open an environment, a db, a txn, and do a prepare.   Then do txn_prepare (without even closing the environment).
    test();
    
    return 0;
}
