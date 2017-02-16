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
#ident "$Id$"
#include "test.h"
#include <db.h>
#include <sys/stat.h>

#include "test.h"

static inline size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

static void
test_env (const char *envdir0, const char *envdir1, int expect_open_return) {
    int r;
    toku_os_recursive_delete(envdir0);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    if (strcmp(envdir0, envdir1) != 0) {
        toku_os_recursive_delete(envdir1);
        r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }
    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}

static void
test_datadir (const char *envdir0, const char *datadir0, const char *envdir1, const char *datadir1, int expect_open_return) {
    char s[256];

    int r;
    sprintf(s, "rm -rf %s", envdir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", envdir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    r = env->set_data_dir(env, datadir0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->set_data_dir(env2, datadir1);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}
static void
test_logdir (const char *envdir0, const char *datadir0, const char *envdir1, const char *datadir1, int expect_open_return) {
    char s[256];

    int r;
    sprintf(s, "rm -rf %s", envdir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir0);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir0, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", envdir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(envdir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    sprintf(s, "rm -rf %s", datadir1);
    r = system(s);
    CKERR(r);
    r = toku_os_mkdir(datadir1, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);

    DB_ENV *env;
    r = db_env_create(&env, 0);
        CKERR(r);
    r = env->set_redzone(env, 0);
        CKERR(r);
    r = env->set_lg_dir(env, datadir0);
        CKERR(r);
    int envflags = DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_CREATE | DB_PRIVATE | DB_RECOVER;
    r = env->open(env, envdir0, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);

    DB_ENV *env2;
    r = db_env_create(&env2, 0);
        CKERR(r);
    r = env2->set_redzone(env2, 0);
        CKERR(r);
    r = env2->set_lg_dir(env2, datadir1);
        CKERR(r);
    r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR2(r, expect_open_return);

    r = env->close(env, 0);
        CKERR(r);

    if (expect_open_return != 0) {
        r = env2->open(env2, envdir1, envflags, S_IRWXU+S_IRWXG+S_IRWXO);
        CKERR(r);
    }

    r = env2->close(env2, 0);
        CKERR(r);
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU|S_IRWXG|S_IRWXO);
    assert_zero(r);

    char env0[TOKU_PATH_MAX+1];
    char env1[TOKU_PATH_MAX+1];
    toku_path_join(env0, 2, TOKU_TEST_FILENAME, "e0");
    toku_path_join(env1, 2, TOKU_TEST_FILENAME, "e1");
    test_env(env0, env1, 0);
    test_env(env0, env0, EWOULDBLOCK);
    char wd[TOKU_PATH_MAX+1];
    char *cwd = getcwd(wd, sizeof wd);
    assert(cwd != nullptr);
    char data0[TOKU_PATH_MAX+1];
    toku_path_join(data0, 3, cwd, TOKU_TEST_FILENAME, "d0");
    char data1[TOKU_PATH_MAX+1];
    toku_path_join(data1, 3, cwd, TOKU_TEST_FILENAME, "d1");
    test_datadir(env0, data0, env1, data1, 0);
    test_datadir(env0, data0, env1, data0, EWOULDBLOCK);
    test_logdir(env0, data0, env1, data1, 0);
    test_logdir(env0, data0, env1, data0, EWOULDBLOCK);

    toku_os_recursive_delete(env0);
    toku_os_recursive_delete(env1);
    toku_os_recursive_delete(data0);
    toku_os_recursive_delete(data1);

    return 0;
}
