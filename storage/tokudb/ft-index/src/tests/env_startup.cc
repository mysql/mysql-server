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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify correct behavior of
 * environment startup:
 *
 * All three of the following should exist or all three should not exist:
 *  - persistent environment
 *  - fileops directory
 *  - recovery log  (if DB_INIT_LOG)
 *
 * If all three are missing, env->open() should create a new environment.
 * If any one is present and any other is missing, env->open() should return ENOENT.
 *
 * TODO: experiment with DB_INIT_LOG off.
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

static void
setup (uint32_t flags) {
    int r;
    if (env)
        test_shutdown();
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, flags, mode); 
    CKERR(r);
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}


static void
reopen_env(uint32_t flags, int expected_r) {
    int r;
    if (env)
        test_shutdown();
    r = db_env_create(&env, 0);                                           
    CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, flags, mode);
    CKERR2(r, expected_r);
}

static void
delete_persistent(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", TOKU_TEST_FILENAME, "/", "tokudb.environment");
    int r = system(cmd);
    CKERR(r);
}


static void
delete_directory(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", TOKU_TEST_FILENAME, "/", "tokudb.directory");
    int r = system(cmd);
    CKERR(r);
}


static void
delete_log(void) {
    char cmd[1024];
    sprintf(cmd, "rm -rf %s%s%s", TOKU_TEST_FILENAME, "/", "*.tokulog*");
    int r = system(cmd);
    CKERR(r);
}


static void
create_env(uint32_t flags) {
    setup(flags);                     // create new environment
    test_shutdown();
    reopen_env(flags, 0);             // reopen existing environment, should have log now
    test_shutdown();
}


static void
test_env_startup(int logging) {
    uint32_t flags;
    
    if (logging)
	flags = FLAGS_LOG;
    else
	flags = FLAGS_NOLOG;

    create_env(flags);

    // delete persistent info and try to reopen
    delete_persistent();
    reopen_env(flags, ENOENT);

    // recreate, then try to open with missing fileops directory
    create_env(flags);
    delete_directory();
    reopen_env(flags, ENOENT);
    

    if (logging) {
	// recreate, then try to open with missing recovery log
	create_env(flags);
	delete_log();
	reopen_env(flags, ENOENT);

	
	// now try two missing items, if log can be present

	// log is only item present
	create_env(flags);
	delete_persistent();
	delete_directory();
	reopen_env(flags, ENOENT);

	// persistent env is only item present
	create_env(flags);
	delete_log();
	delete_directory();
	reopen_env(flags, ENOENT);
	
	// directory is only item present
	create_env(flags);
	delete_persistent();
	delete_log();
	reopen_env(flags, ENOENT);
    }

    test_shutdown();
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    test_env_startup(0);  // transactionless env
    test_env_startup(1);  // with transactions and logging
    return 0;
}
