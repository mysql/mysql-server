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
/* Transaction consistency:  
 *  fork a process:
 *   Open two tables, A and B
 *   begin transaction U
 *   begin transaction V
 *   store U.A into A using U
 *   store V.B into B using V
 *   checkpoint
 *   store U.C into A using U
 *   store V.D into B using V
 *   commit U
 *   maybe commit V
 *   abort the process abruptly
 *  wait for the process to finish
 *   open the environment doing recovery
 *   check to see if both rows are present in A and maybe present in B
 */
#include <sys/stat.h>
#include "test.h"


const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_TXN|DB_PRIVATE;
const char *namea="a.db";
const char *nameb="b.db";

static void
put (DB_TXN *txn, DB *db, const char *key, const char *data) {
    DBT k,d;
    dbt_init(&k, key, 1+strlen(key));
    dbt_init(&d, data, 1+strlen(data));
    int r = db->put(db, txn, &k, &d, 0);
    CKERR(r);
}
   
static void
do_x2_shutdown (bool do_commit) {
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    DB_ENV *env;
    DB *dba, *dbb; // Use two DBs so that BDB doesn't get a lock conflict
    r = db_env_create(&env, 0);                                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);                      CKERR(r);
    r = db_create(&dba, env, 0);                                                        CKERR(r);
    r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    r = db_create(&dbb, env, 0);                                                        CKERR(r);
    r = dba->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
    DB_TXN *txnU, *txnV;
    r = env->txn_begin(env, NULL, &txnU, 0);                                            CKERR(r);
    r = env->txn_begin(env, NULL, &txnV, 0);                                            CKERR(r);
    put(txnU, dba, "u.a", "u.a.data");
    put(txnV, dbb, "v.b", "v.b.data");
    r = env->txn_checkpoint(env, 0, 0, 0);                                              CKERR(r);
    put(txnU, dba, "u.c", "u.c.data");
    put(txnV, dbb, "v.d", "v.d.data");
    r = txnU->commit(txnU, 0);                                                          CKERR(r);
    if (do_commit) {
	r = txnV->commit(txnV, 0);                                                      CKERR(r);
    }
    toku_hard_crash_on_purpose();
}

static void
checkcurs (DBC *curs, int cursflags, const char *key, const char *val, bool expect_it) {
    DBT k,v;
    dbt_init(&k, NULL, 0);
    dbt_init(&v, NULL, 0);
    int r = curs->c_get(curs, &k, &v, cursflags);
    if (expect_it) {
	assert(r==0);
	printf("Got %s expected %s\n", (char*)k.data, key);
	assert(strcmp((char*)k.data, key)==0);
	assert(strcmp((char*)v.data, val)==0);
    } else {
	printf("Expected nothing, got r=%d\n", r);
	assert(r!=0);
    }
}

static void
do_x2_recover (bool did_commit) {
    DB_ENV *env;
    int r;
    r = db_env_create(&env, 0);                                                             CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags|DB_RECOVER, S_IRWXU+S_IRWXG+S_IRWXO);                          CKERR(r);
    DB_TXN *txn;
    r = env->txn_begin(env, NULL, &txn, 0);                                                 CKERR(r);
    {
	DB *dba;
	r = db_create(&dba, env, 0);                                                        CKERR(r);
	r = dba->open(dba, NULL, namea, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
	DBC *c;
	r = dba->cursor(dba, txn, &c, 0);                                                   CKERR(r);
	checkcurs(c, DB_FIRST, "u.a", "u.a.data", true);
	checkcurs(c, DB_NEXT,  "u.c", "u.c.data", true);
	checkcurs(c, DB_NEXT,  NULL,  NULL,       false);
	r = c->c_close(c);                                                                  CKERR(r);	
	r = dba->close(dba, 0);                                                             CKERR(r);
    }
    {
	DB *dbb;
	r = db_create(&dbb, env, 0);                                                        CKERR(r);
	r = dbb->open(dbb, NULL, nameb, NULL, DB_BTREE, DB_AUTO_COMMIT|DB_CREATE, 0666);    CKERR(r);
	DBC *c;
	r = dbb->cursor(dbb, txn, &c, 0);                                                   CKERR(r);
	checkcurs(c, DB_FIRST, "v.b", "v.b.data", did_commit);
	checkcurs(c, DB_NEXT,  "v.d", "v.d.data", did_commit);
	checkcurs(c, DB_NEXT,  NULL,  NULL,       false);
	r = c->c_close(c);                                                                  CKERR(r);	
	r = dbb->close(dbb, 0);                                                             CKERR(r);
    }
 
    r = txn->commit(txn, 0);                                                                CKERR(r);
    r = env->close(env, 0);                                                                 CKERR(r);
    exit(0);
}

const char *cmd;

#if 0

static void
do_test_internal (bool commit) {
    pid_t pid;
    if (0 == (pid=fork())) {
	int r=execl(cmd, verbose ? "-v" : "-q", commit ? "--commit" : "--abort", NULL);
	assert(r==-1);
	printf("execl failed: %d (%s)\n", errno, strerror(errno));
	assert(0);
    }
    {
	int r;
	int status;
	r = waitpid(pid, &status, 0);
	//printf("signaled=%d sig=%d\n", WIFSIGNALED(status), WTERMSIG(status));
	assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    // Now find out what happend
    
    if (0 == (pid = fork())) {
	int r=execl(cmd, verbose ? "-v" : "-q", commit ? "--recover-committed" : "--recover-aborted", NULL);
	assert(r==-1);
	printf("execl failed: %d (%s)\n", errno, strerror(errno));
	assert(0);
    }
    {
	int r;
	int status;
	r = waitpid(pid, &status, 0);
	//printf("recovery exited=%d\n", WIFEXITED(status));
	assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
}

static void
do_test (void) {
    do_test_internal(true);
    do_test_internal(false);
}

#endif

bool do_commit=false, do_abort=false, do_recover_committed=false,  do_recover_aborted=false;

static void
x2_parse_args (int argc, char * const argv[]) {
    int resultcode;
    cmd = argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-v") == 0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	    if (verbose<0) verbose=0;
	} else if (strcmp(argv[0],"--abort")==0) {
	    do_abort=true;
	} else if (strcmp(argv[0],"--commit")==0 || strcmp(argv[0], "--test") == 0) {
	    do_commit=true;
	} else if (strcmp(argv[0],"--recover-committed")==0 || strcmp(argv[0], "--recover") == 0) {
	    do_recover_committed=true;
	} else if (strcmp(argv[0],"--recover-aborted")==0) {
	    do_recover_aborted=true;
	} else if (strcmp(argv[0], "-h")==0) {
	    resultcode=0;
	do_usage:
	    fprintf(stderr, "Usage:\n%s [-v|-q]* [-h] {--abort | --commit | --recover-committed | --recover-aborted } \n", cmd);
	    exit(resultcode);
	} else {
	    fprintf(stderr, "Unknown arg: %s\n", argv[0]);
	    resultcode=1;
	    goto do_usage;
	}
	argc--;
	argv++;
    }
    {
	int n_specified=0;
	if (do_commit)            n_specified++;
	if (do_abort)             n_specified++;
	if (do_recover_committed) n_specified++;
	if (do_recover_aborted)   n_specified++;
	if (n_specified>1) {
	    printf("Specify only one of --commit or --abort or --recover-committed or --recover-aborted\n");
	    resultcode=1;
	    goto do_usage;
	}
    }
}

int
test_main (int argc, char * const argv[]) {
    x2_parse_args(argc, argv);
    if (do_commit) {
	do_x2_shutdown (true);
    } else if (do_abort) {
	do_x2_shutdown (false);
    } else if (do_recover_committed) {
	do_x2_recover(true);
    } else if (do_recover_aborted) {
	do_x2_recover(false);
    } 
#if 0
    else {
	do_test();
    }
#endif
    return 0;
}
