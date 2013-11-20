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
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <sys/stat.h>
#include <db.h>
#include <toku_pthread.h>

static inline unsigned int getmyid(void) {
    return toku_os_gettid();
}

typedef unsigned int my_t;

struct db_inserter {
    toku_pthread_t tid;
    DB *db;
    my_t startno, endno;
    int do_exit;
};

static int
db_put (DB *db, my_t k, my_t v) {
    DBT key, val;
    int r = db->put(db, 0, dbt_init(&key, &k, sizeof k), dbt_init(&val, &v, sizeof v), 0);
    return r;
}

static void *
do_inserts (void *arg) {
    struct db_inserter *mywork = (struct db_inserter *) arg;
    if (verbose) {
        toku_pthread_t self = toku_pthread_self();
        printf("%lu:%u:do_inserts:start:%u-%u\n", *(unsigned long*)&self, getmyid(), mywork->startno, mywork->endno);
    }
    my_t i;
    for (i=mywork->startno; i < mywork->endno; i++) {
        int r = db_put(mywork->db, htonl(i), i); assert(r == 0);
    }
    
    if (verbose) {
        toku_pthread_t self = toku_pthread_self();
        printf("%lu:%u:do_inserts:end\n", *(unsigned long*)&self, getmyid());
    }
    // Don't call toku_pthread_exit(), since it has a memory leak.
    // if (mywork->do_exit) toku_pthread_exit(arg);
    return 0;
}

static int
usage (void) {
    fprintf(stderr, "test [-n NTUPLES] [-p NTHREADS]\n");
    fprintf(stderr, "default NTUPLES=1000000\n");
    fprintf(stderr, "default NTHREADS=2\n");
    return 1;
}

int
test_main(int argc, char *const argv[]) {
    const char *dbfile = "test.db";
    const char *dbname = "main";
    int nthreads = 2;
    my_t n = 1000000;

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    int i;
    for (i=1; i<argc; i++) {
        const char *arg = argv[i];
        if (0 == strcmp(arg, "-h") || 0 == strcmp(arg, "--help")) {
            return usage();
        }
        if (0 == strcmp(arg, "-v") || 0 == strcmp(arg, "--verbose")) {
            verbose = 1; 
            continue;
        }
        if (0 == strcmp(arg, "-p")) {
            if (i+1 >= argc) return usage();
            nthreads = atoi(argv[++i]);
            continue;
        }
        if (0 == strcmp(arg, "-n")) {
            if (i+1 >= argc) return usage();
            n = atoi(argv[++i]);
            continue;
        }
    }

    DB_ENV *env;

    r = db_env_create(&env, 0); assert(r == 0);
    r = env->set_cachesize(env, 0, 128000000, 1); assert(r == 0);
    r = env->open(env, TOKU_TEST_FILENAME, DB_CREATE + DB_THREAD + DB_PRIVATE + DB_INIT_MPOOL + DB_INIT_LOCK, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    DB *db;

    r = db_create(&db, env, 0); assert(r == 0);
    r = db->open(db, 0, dbfile, dbname, DB_BTREE, DB_CREATE + DB_THREAD, S_IRWXU+S_IRWXG+S_IRWXO); assert(r == 0);

    struct db_inserter work[nthreads];

    for (i=0; i<nthreads; i++) {
        work[i].db = db;
        work[i].startno = i*(n/nthreads);
        work[i].endno = work[i].startno + (n/nthreads);
        work[i].do_exit =1 ;
        if (i+1 == nthreads)  
            work[i].endno = n;
    }

    if (verbose) printf("pid:%d\n", toku_os_getpid());

    for (i=1; i<nthreads; i++) {
        r = toku_pthread_create(&work[i].tid, 0, do_inserts, &work[i]); assert(r == 0);
    }

    work[0].do_exit = 0;
    do_inserts(&work[0]);

    for (i=1; i<nthreads; i++) {
        void *ret;
        r = toku_pthread_join(work[i].tid, &ret); assert(r == 0);
    }

    r = db->close(db, 0); assert(r == 0);
    r = env->close(env, 0); assert(r == 0);

    return 0;
}
