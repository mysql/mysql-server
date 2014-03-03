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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include "test.h"
#include <vector>
#include <db.h>
#include "toku_time.h"

static void open_dbs(DB_ENV *env, int max_dbs) {
    std::vector<DB *> dbs;

    uint64_t t_start = toku_current_time_microsec();
    // open db's
    {
        uint64_t t0 = toku_current_time_microsec();
        for (int i = 1; i <= max_dbs; i++) {
            int r;
            DB *db = NULL;
            r = db_create(&db, env, 0);
            assert(r == 0);
            char db_name[32];
            sprintf(db_name, "db%d", i);
            r = db->open(db, NULL, db_name, NULL, DB_BTREE, DB_CREATE, 0666);
            assert(r == 0);
            dbs.push_back(db);
            if ((i % 100) == 0) {
                uint64_t t = toku_current_time_microsec();
                fprintf(stderr, "open %d %" PRIu64 "\n", i, t - t0);
                t0 = t;
            }
        }
    }
    uint64_t t_end = toku_current_time_microsec();
    fprintf(stderr, "%" PRIu64 "\n", t_end - t_start);

    // close db's
    {
        uint64_t t0 = toku_current_time_microsec();
        int i = 1;
        for (std::vector<DB *>::iterator dbi = dbs.begin(); dbi != dbs.end(); dbi++, i++) {
            DB *db = *dbi;
            int r = db->close(db, 0);
            assert(r == 0);
            if ((i % 100) == 0) {
                uint64_t t = toku_current_time_microsec();
                printf("close %d %" PRIu64 "\n", i, t - t0);
                t0 = t;
            }
        }
    }
}

int test_main (int argc, char * const argv[]) {
    int r;
    int max_dbs = 1;

    // parse_args(argc, argv);
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0) verbose--;
            continue;
        }
        max_dbs = atoi(argv[i]);
        continue;
    }

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);

    DB_ENV *env = NULL;
    r = db_env_create(&env, 0);
    assert(r == 0);
    env->set_errfile(env, stderr);
    r = env->open(env, TOKU_TEST_FILENAME, DB_INIT_LOCK+DB_INIT_MPOOL+DB_INIT_TXN+DB_INIT_LOG + DB_CREATE + DB_PRIVATE, S_IRWXU+S_IRWXG+S_IRWXO);
    assert(r == 0);

    open_dbs(env, max_dbs);

    r = env->close(env, 0);
    assert(r == 0);

    return 0;
}
