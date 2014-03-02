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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."
#ident "$Id$"

// Test for #3681: iibench hangs.  The scenario is
//  * Thread 1 calls root_put_msg, get_and_pin_root, 1 holds read lock on the root.
//  * Thread 2 calls checkpoint, marks the root for checkpoint.
//  * Thread 2 calls end_checkpoint, tries to write lock the root, sets want_write, and blocks on the rwlock because there is a reader.
//  * Thread 1 calls apply_msg_to_in_memory_leaves, calls get_and_pin_if_in_memory, tries to get a read lock on the root node and blocks on the rwlock because there is a write request on the lock.


#include "checkpoint.h"
#include "test.h"

CACHETABLE ct;
FT_HANDLE t;

static DB * const null_db = 0;
static TOKUTXN const null_txn = 0;

volatile bool done = false;

static void setup (void) {
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);
    const char *fname = TOKU_TEST_FILENAME;
    unlink(fname);
    { int r = toku_open_ft_handle(fname, 1, &t, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);         assert(r==0); }
}


static void finish (void) {
    { int r = toku_close_ft_handle_nolsn(t, 0);                                                                       assert(r==0); };
    toku_cachetable_close(&ct);
}

static void *starta (void *n) {
    assert(n==NULL);
    for (int i=0; i<10000; i++) {
	DBT k,v;
	char ks[20], vs[20];
	snprintf(ks, sizeof(ks), "hello%03d", i);
	snprintf(vs, sizeof(vs), "there%03d", i);
	toku_ft_insert(t, toku_fill_dbt(&k, ks, strlen(ks)), toku_fill_dbt(&v, vs, strlen(vs)), null_txn);
	usleep(1);
    }
    done = true;
    return NULL;
}
static void *startb (void *n) {
    assert(n==NULL);
    int count=0;
    while (!done) {
        CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
        int r = toku_checkpoint(cp, NULL, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT); assert(r==0);
        count++;
    }
    printf("count=%d\n", count);
    return NULL;
}

static void test3681 (void) {
    setup();
    toku_pthread_t a,b;
    { int r; r = toku_pthread_create(&a, NULL, starta, NULL); assert(r==0); }
    { int r; r = toku_pthread_create(&b, NULL, startb, NULL); assert(r==0); }
    { int r; void *v; r = toku_pthread_join(a, &v);           assert(r==0); assert(v==NULL); }
    { int r; void *v; r = toku_pthread_join(b, &v);           assert(r==0); assert(v==NULL);  }
    finish();
}

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test3681();
    return 0;
}

