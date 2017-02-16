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

/* Idea: 
 *  create a dictionary
 *  repeat:  
 *    lots of inserts
 *    checkpoint
 *    note file size
 *    lots of deletes
 *    optimize (flatten tree)
 *    checkpoint 
 *    note file size
 *
 */

#include "test.h"

#define PATHSIZE 1024

DB_ENV *env;
DB *db;
char dbname[] = "foo.db";
char path[PATHSIZE];

const int envflags = DB_INIT_MPOOL|DB_CREATE|DB_THREAD |DB_INIT_LOCK|DB_PRIVATE;

int ninsert, nread, nread_notfound, nread_failed, ndelete, ndelete_notfound, ndelete_failed;

static TOKU_DB_FRAGMENTATION_S report;

static void
check_fragmentation(void) {
    int r = db->get_fragmentation(db, &report);
    CKERR(r);
}

static void
print_fragmentation(void) {
    printf("Fragmentation:\n");
    printf("\tTotal file size in bytes (file_size_bytes): %" PRIu64 "\n", report.file_size_bytes);
    printf("\tCompressed User Data in bytes (data_bytes): %" PRIu64 "\n", report.data_bytes);
    printf("\tNumber of blocks of compressed User Data (data_blocks): %" PRIu64 "\n", report.data_blocks);
    printf("\tAdditional bytes used for checkpoint system (checkpoint_bytes_additional): %" PRIu64 "\n", report.checkpoint_bytes_additional);
    printf("\tAdditional blocks used for checkpoint system  (checkpoint_blocks_additional): %" PRIu64 "\n", report.checkpoint_blocks_additional);
    printf("\tUnused space in file (unused_bytes): %" PRIu64 "\n", report.unused_bytes);
    printf("\tNumber of contiguous regions of unused space (unused_blocks): %" PRIu64 "\n", report.unused_blocks);
    printf("\tSize of largest contiguous unused space (largest_unused_block): %" PRIu64 "\n", report.largest_unused_block);
}

static void
close_em (void)
{
    int r;
    r = db->close(db, 0);   CKERR(r);
    r = env->close(env, 0); CKERR(r);
}


static void
setup(void)
{
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    r = db_env_create(&env, 0);                                         CKERR(r);
    r = env->open(env, TOKU_TEST_FILENAME, envflags, S_IRWXU+S_IRWXG+S_IRWXO);      CKERR(r);
    r = db_create(&db, env, 0);                                         CKERR(r);
    r = db->open(db, NULL, dbname, NULL, DB_BTREE, DB_CREATE, 0666);    CKERR(r);
}


static void
fill_rand(int n, uint64_t * d) {
    for (int i = 0; i < n; i++){
	*(d+i) = random64();
    }
}

#define INSERT_BIG 1500
#define INSERT_SMALL 0
static void
insert_n (uint32_t ah, int datasize) {
    uint64_t vdata[datasize];
    fill_rand(datasize, vdata);
    uint32_t an = htonl(ah);
    //    if (verbose) printf("insert an = %0X (ah = %0X)\n", an, ah);
    DBT key;
    dbt_init(&key, &an, 4);
    DBT val;
    dbt_init(&val, vdata, sizeof vdata);
    int r = db->put(db, NULL, &key, &val, 0);
    CKERR(r);
    ninsert++;
}

static void
delete_n (uint32_t ah)
{
    uint32_t an = htonl(ah);
    //    if (verbose) printf("delete an = %0X (ah = %0X)\n", an, ah);
    DBT key;
    dbt_init(&key, &an, 4);
    int r = db->del(db, NULL, &key, DB_DELETE_ANY);
    if (r == 0)
	ndelete++;
    else if (r == DB_NOTFOUND)
	ndelete_notfound++;
    else
	ndelete_failed++;
    CKERR(r);
}

static void
optimize(void) {
    if (verbose) printf("Filesize: begin optimize dictionary\n");
    uint64_t loops_run;
    int r = db->hot_optimize(db, NULL, NULL, NULL, NULL, &loops_run);
    CKERR(r);
    if (verbose) printf("Filesize: end optimize dictionary\n");
}


static void
get_file_pathname(void) {
    DBT dname;
    DBT iname;
    dbt_init(&dname, dbname, sizeof(dbname));
    dbt_init(&iname, NULL, 0);
    iname.flags |= DB_DBT_MALLOC;
    int r = env->get_iname(env, &dname, &iname);
    CKERR(r);
    sprintf(path, "%s/%s", TOKU_TEST_FILENAME, (char*)iname.data);
    toku_free(iname.data);
    if (verbose) printf("path = %s\n", path);
}


static int
getsizeM(void) {
    toku_struct_stat buf;
    int r = toku_stat(path, &buf);
    CKERR(r);
    int sizeM = (int)buf.st_size >> 20;
    check_fragmentation();
    if (verbose>1)
        print_fragmentation();
    return sizeM;
}

static void
test_filesize (bool sequential)
{
    int N=1<<14;
    int r, i, sizeM;

    get_file_pathname();

    for (int iter = 0; iter < 3; iter++) {
        int offset = N * iter;

        if (sequential) {
            for (i=0; i<N; i++) {
                insert_n(i + offset, INSERT_BIG);
            }
        } else {
            for (i=N-1; i>=0; --i) {
                insert_n(i + offset, INSERT_BIG);
            }
        }

        r = env->txn_checkpoint(env, 0, 0, 0);
        CKERR(r);
        int sizefirst = sizeM = getsizeM();
        if (verbose) printf("Filesize after iteration %d insertion and checkpoint = %dM\n", iter, sizeM);

        int preserve = 2;
        for (i = preserve; i<(N); i++) {  // leave a little at the beginning
            delete_n(i + offset);
        }
        optimize();

        r = env->txn_checkpoint(env, 0, 0, 0);
        CKERR(r);
        sizeM = getsizeM();
        if (verbose) printf("Filesize after iteration %d deletion and checkpoint 1 = %dM\n", iter, sizeM);

        if (sequential) {
            for (i=0; i<N; i++) {
                insert_n(i + offset, INSERT_SMALL);
            }
        } else {
            for (i=N-1; i>=0; --i) {
                insert_n(i + offset, INSERT_SMALL);
            }
        }
        for (i = preserve; i<(N); i++) {  // leave a little at the beginning
            delete_n(i + offset);
        }
        optimize();
        r = env->txn_checkpoint(env, 0, 0, 0);
        CKERR(r);
        sizeM = getsizeM();
        if (verbose) printf("Filesize after iteration %d deletion and checkpoint 2 = %dM\n", iter, sizeM);
        assert(sizeM <= sizefirst);

        if (verbose) printf("ninsert = %d\n", ninsert);
        if (verbose) printf("nread = %d, nread_notfound = %d, nread_failed = %d\n", nread, nread_notfound, nread_failed);
        if (verbose) printf("ndelete = %d, ndelete_notfound = %d, ndelete_failed = %d\n", ndelete, ndelete_notfound, ndelete_failed);
    }
}

int test_main (int argc __attribute__((__unused__)), char * const argv[] __attribute__((__unused__))) {
    parse_args(argc, argv);
    setup();
    if (verbose) print_engine_status(env);
    test_filesize(true);
    if (verbose) {
        print_engine_status(env);
    }
    check_fragmentation();
    if (verbose) print_fragmentation();
    close_em();
    setup();
    if (verbose) print_engine_status(env);
    test_filesize(false);
    if (verbose) {
        print_engine_status(env);
    }
    check_fragmentation();
    if (verbose) print_fragmentation();
    close_em();
    return 0;
}
