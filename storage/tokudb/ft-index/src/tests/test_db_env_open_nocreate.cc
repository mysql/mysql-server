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


// Try to open an environment where the directory does not exist 
// Try when the dir exists but is not an initialized env
// Try when the dir exists and we do DB_CREATE: it should work.
// And after that the open should work without a DB_CREATE
//   However, in BDB, after doing an DB_ENV->open and then a close, no state has changed
//   One must actually create a DB I think...


#include <db.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

// TOKU_TEST_FILENAME is defined in the Makefile

int
test_main(int argc, char *const argv[]) {
    parse_args(argc, argv);
    DB_ENV *dbenv;
    int r;
    int do_private;

    for (do_private=0; do_private<2; do_private++) {
#ifdef USE_TDB
	if (do_private==0) continue; // See #208.
#else
	if (do_private==1) continue; // See #530.  BDB 4.6.21 segfaults if DB_PRIVATE is passed when no environment previously exists.
#endif
	int private_flags = do_private ? (DB_CREATE|DB_PRIVATE) : 0;
	
	toku_os_recursive_delete(TOKU_TEST_FILENAME);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, TOKU_TEST_FILENAME, private_flags|DB_INIT_MPOOL, 0);
	assert(r==ENOENT);
	dbenv->close(dbenv,0); // free memory
	
	toku_os_recursive_delete(TOKU_TEST_FILENAME);
	toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
	r = db_env_create(&dbenv, 0);
	CKERR(r);
	r = dbenv->open(dbenv, TOKU_TEST_FILENAME, private_flags|DB_INIT_MPOOL, 0);
#ifdef USE_TDB
	// TokuDB has no trouble opening an environment if the directory exists.
	CKERR(r);
	assert(r==0);
#else
	if (r!=ENOENT) printf("%s:%d %d: %s\n", __FILE__, __LINE__, r,db_strerror(r));
	assert(r==ENOENT);
#endif
	dbenv->close(dbenv,0); // free memory
    }

#ifndef USE_TDB
    // Now make sure that if we have a non-private DB that we can tell if it opened or not.
    DB *db;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, TOKU_TEST_FILENAME, DB_CREATE|DB_INIT_MPOOL, 0);
    CKERR(r);
    r=db_create(&db, dbenv, 0);
    CKERR(r);
    db->close(db, 0);
    dbenv->close(dbenv,0); // free memory
    r = db_env_create(&dbenv, 0);
    CKERR(r);
    r = dbenv->open(dbenv, TOKU_TEST_FILENAME, DB_INIT_MPOOL, 0);
    CKERR(r);
    dbenv->close(dbenv,0); // free memory
#endif

    return 0;
}

