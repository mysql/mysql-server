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

#include "checkpoint_test.h"


// Purpose of test is to verify that callbacks are called correctly
// without breaking a simple checkpoint (copied from tests/checkpoint_1.c).

static const char * string_1 = "extra1";
static const char * string_2 = "extra2";
static int callback_1_count = 0;
static int callback_2_count = 0;

static void checkpoint_callback_1(void * extra) {
    if (verbose) printf("checkpoint callback 1 called with extra = %s\n", *((char**) extra));
    assert(extra == &string_1);
    callback_1_count++;
}

static void checkpoint_callback_2(void * extra) {
    if (verbose) printf("checkpoint callback 2 called with extra = %s\n", *((char**) extra));
    assert(extra == &string_2);
    callback_2_count++;
}

static void
checkpoint_test_1(uint32_t flags, uint32_t n, int snap_all) {
    if (verbose>1) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags); 
        fflush(stdout); 
    }
    dir_create(TOKU_TEST_FILENAME);
    env_startup(TOKU_TEST_FILENAME, 0, false);
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
        uint32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
	assert(callback_1_count == run+1);
	assert(callback_2_count == run+1);
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, NULL, NULL);
        db_replace(TOKU_TEST_FILENAME, &db_test, NULL);
        r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    db_shutdown(&db_test);
    db_shutdown(&db_control);
    env_shutdown();
}

int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);

    db_env_set_checkpoint_callback(checkpoint_callback_1, &string_1);
    db_env_set_checkpoint_callback2(checkpoint_callback_2, &string_2);

    checkpoint_test_1(0,4096,1);
    return 0;
}
