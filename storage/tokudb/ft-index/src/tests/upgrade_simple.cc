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

/* Purpose of this test is to verify simplest part of upgrade logic.
 * Start by creating two very simple 4.x environments,
 * one in each of two states:
 *  - after a clean shutdown
 *  - without a clean shutdown
 *
 * The two different environments will be used to exercise upgrade logic
 * for 5.x.
 *
 */


#include "test.h"
#include <db.h>

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

#define OLDDATADIR "../../../../tokudb.data/"

static char *env_dir = TOKU_TEST_FILENAME; // the default env_dir.

static char * dir_v41_clean = OLDDATADIR "env_simple.4.1.1.cleanshutdown";
static char * dir_v42_clean = OLDDATADIR "env_simple.4.2.0.cleanshutdown";
static char * dir_v42_dirty = OLDDATADIR "env_simple.4.2.0.dirtyshutdown";
static char * dir_v41_dirty_multilogfile = OLDDATADIR "env_preload.4.1.1.multilog.dirtyshutdown";
static char * dir_v42_dirty_multilogfile = OLDDATADIR "env_preload.4.2.0.multilog.dirtyshutdown";


static void
setup (uint32_t flags, bool clean, bool too_old, char * src_db_dir) {
    int r;
    int len = 256;
    char syscmd[len];

    if (env)
        test_shutdown();

    r = snprintf(syscmd, len, "rm -rf %s", env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                  
    CKERR(r);
    
    r = snprintf(syscmd, len, "cp -r %s %s", src_db_dir, env_dir);
    assert(r<len);
    r = system(syscmd);                                                                                 
    CKERR(r);

    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, flags, mode); 
    if (clean)
	CKERR(r);
    else {
	if (too_old)
	    CKERR2(r, TOKUDB_DICTIONARY_TOO_OLD);
	else
	    CKERR2(r, TOKUDB_UPGRADE_FAILURE);
    }
}



static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}


static void
test_env_startup(void) {
    uint32_t flags;
    
    flags = FLAGS_LOG;

    setup(flags, true, false, dir_v42_clean);
    print_engine_status(env);
    test_shutdown();

    setup(flags, false, true, dir_v41_clean);
    print_engine_status(env);
    test_shutdown();

    setup(flags, false, false, dir_v42_dirty);
    if (verbose) {
	printf("\n\nEngine status after aborted env->open() will have some garbage values:\n");
    }
    print_engine_status(env);
    test_shutdown();

    setup(flags, false, true, dir_v41_dirty_multilogfile);
    if (verbose) {
	printf("\n\nEngine status after aborted env->open() will have some garbage values:\n");
    }
    print_engine_status(env);
    test_shutdown();

    setup(flags, false, false, dir_v42_dirty_multilogfile);
    if (verbose) {
	printf("\n\nEngine status after aborted env->open() will have some garbage values:\n");
    }
    print_engine_status(env);
    test_shutdown();
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    test_env_startup();
    return 0;
}
