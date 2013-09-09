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


// Only useful for single threaded testing, 
// but can be accessed from checkpoint_callback.
static DICTIONARY test_dictionary = NULL;
static int iter = 0;  // horrible technique, but quicker than putting in extra (this is just test code, not product code)


static void
checkpoint_test_1(uint32_t flags, uint32_t n, int snap_all) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, snap_all, flags); 
        fflush(stdout); 
    }
    dir_create(TOKU_TEST_FILENAME);
    env_startup(TOKU_TEST_FILENAME, 0, 0);
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
        uint32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
        snapshot(&db_test, snap_all);
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

static void
checkpoint_test_2(uint32_t flags, uint32_t n) {
    if (verbose) { 
        printf("%s(%s):%d, n=0x%03x, checkpoint=%01x, flags=0x%05x\n", 
               __FILE__, __FUNCTION__, __LINE__, 
               n, 1, flags); 
	printf("Verify that inserts done during checkpoint are effective\n");
        fflush(stdout); 
    }
    dir_create(TOKU_TEST_FILENAME);
    env_startup(TOKU_TEST_FILENAME, 0, 0);
    int run;
    int r;
    DICTIONARY_S db_control;
    init_dictionary(&db_control, flags, "control");
    DICTIONARY_S db_test;
    init_dictionary(&db_test, flags, "test");
    test_dictionary = &db_test;

    db_startup(&db_test, NULL);
    db_startup(&db_control, NULL);
    const int num_runs = 4;
    for (run = 0; run < num_runs; run++) {
	iter = run;
        uint32_t i;
        for (i=0; i < n/2/num_runs; i++)
            insert_random(db_test.db, db_control.db, NULL);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
        snapshot(&db_test, true);  // take checkpoint, insert into test db during checkpoint callback
	r = compare_dbs(db_test.db, db_control.db);
	// test and control should be different 
	assert(r!=0);
	// now insert same rows into control and they should be same 
	insert_n_fixed(db_control.db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
	r = compare_dbs(db_test.db, db_control.db);
	assert(r==0);
    }
    // now close db_test via checkpoint callback (i.e. during checkpoint)
    iter = -1;  
    snapshot(&db_test, true);
    db_shutdown(&db_control);
    env_shutdown();
}


 

// Purpose is to scribble over test db while checkpoint is 
// in progress.
static void checkpoint_callback_1(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    int i;
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (verbose) {
	printf("checkpoint_callback_1 inserting randomly into %s\n",
	       name);
	fflush(stdout);
    }
    for (i=0; i < 1024; i++)
	insert_random(d->db, NULL, NULL);
    
}

static void checkpoint_callback_2(void * extra) {
    DICTIONARY d = *(DICTIONARY*) extra;
    assert(d==test_dictionary);
    char name[MAX_NAME*2];
    fill_name(d, name, sizeof(name));

    if (iter >= 0) {
	if (verbose) {
	    printf("checkpoint_callback_2 inserting fixed rows into %s\n",
		   name);
	    fflush(stdout);
	}
	insert_n_fixed(d->db, NULL, NULL, iter * NUM_FIXED_ROWS, NUM_FIXED_ROWS);
    }
    else {
	DICTIONARY_S db_temp;
	init_dictionary(&db_temp, 0, "temp");
	int i;
	if (verbose) {
	    printf("checkpoint_callback_2 closing %s\n",
		   name);
	    fflush(stdout);
	}
	db_shutdown(d);
	if (verbose) {
	    printf("checkpoint_callback_2 opening and closing unrelated dictionary\n");
	    fflush(stdout);
	}
	db_startup(&db_temp, NULL);
	for (i=0; i<1025; i++)
	    insert_random(db_temp.db, NULL, NULL);
	db_shutdown(&db_temp);	
    }
}


int
test_main (int argc, char * const argv[]) {
    parse_args(argc, argv);
    uint32_t n;
    int snap;

    n = 0;
    for (snap = 0; snap < 2; snap++) {
        checkpoint_test_1(0, n, snap);
    }
    for (n = 1; n <= 1<<9; n*= 2) {
        for (snap = 0; snap < 2; snap++) {
            checkpoint_test_1(0, n, snap);
        }
    }

    db_env_set_checkpoint_callback(checkpoint_callback_1, &test_dictionary);
    checkpoint_test_1(0,4096,1);
    db_env_set_checkpoint_callback(checkpoint_callback_2, &test_dictionary);
    checkpoint_test_2(0,4096);
    db_env_set_checkpoint_callback(NULL, NULL);

    return 0;
}

#if 0
 checkpoint_1:
   create two dbs, db_test (observed) and db_control (expected)
   loop n times:
     modify both dbs
     checkpoint db_test
     modify db_test only
     copy db_test file (system(cp)) to db_temp
     compare db_temp with db_control
     continue test using db_temp as db_test instead of db_test
     delete old db_test
 checkpoint_2,3 were subsumed into 1.


TODO: Add callback to toku_checkpoint(), called after ydb lock is released.
      (Note, checkpoint_safe_lock is still held.)

 checkpoint_4:
   Callback can do some inserts, guaranteeing that we are testing that inserts
   are done "during" a checkpoint.

 checkpoint_5:
    Callback does unrelated open, close, and related close.

 checkpoint_6: 
    Callback triggers a thread that will perform same operations as:
     * checkpoint_4
     * checkpoint_5
     * delete (relevant db)
     * delete (irrelevant db)
     * take checkpoint safe lock using YDB api, do something like insert and release it
    but those operations happen during execution of toku_cachetable_end_checkpoint().
    
 checkpoint_7
   take atomic operation lock
   perform some inserts
   on another thread, call toku_checkpoint()
   sleep a few seconds
   perform more inserts
   release atomic operations lock
   wait for checkpoint thread to complete
   verify that checkpointed db has all data inserted


#endif
