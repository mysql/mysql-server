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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// test the loader write dbfile function


#include "test.h"
#include "ftloader-internal.h"
#include <inttypes.h>
#include <portability/toku_path.h>


static void traceit(const char *s) {
    time_t t = time(NULL);
    printf("%.24s %s\n", ctime(&t), s);
    fflush(stdout);
}

static int qsort_compare_ints (const void *a, const void *b) {
    int avalue = *(int*)a;
    int bvalue = *(int*)b;
    if (avalue<bvalue) return -1;
    if (avalue>bvalue) return +1;
    return 0;
}

static int compare_ints (DB *UU(desc), const DBT *akey, const DBT *bkey) {
    assert(akey->size==sizeof(int));
    assert(bkey->size==sizeof(int));
    return qsort_compare_ints(akey->data, bkey->data);
}

static void err_cb(DB *db UU(), int dbn UU(), int err UU(), DBT *key UU(), DBT *val UU(), void *extra UU()) {
    fprintf(stderr, "error in test");
    abort();
}

static void verify_dbfile(int n, const char *name) {
    if (verbose) traceit("verify");

    int r;

    CACHETABLE ct;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    TOKUTXN const null_txn = NULL;
    FT_HANDLE t = NULL;
    toku_ft_handle_create(&t);
    toku_ft_set_bt_compare(t, compare_ints);
    r = toku_ft_handle_open(t, name, 0, 0, ct, null_txn); assert(r==0);

    if (verbose) traceit("Verifying brt internals");
    r = toku_verify_ft(t);
    if (verbose) traceit("Verified brt internals");

    FT_CURSOR cursor = NULL;
    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    size_t userdata = 0;
    int i;
    for (i=0; ; i++) {
	int kk = i;
	int vv = i;
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
        userdata += pair.keylen + pair.vallen;
    }

    assert(i == n);

    toku_ft_cursor_close(cursor);

    struct ftstat64_s s;
    toku_ft_handle_stat64(t, NULL, &s);
    assert(s.nkeys == (uint64_t)n && s.ndata == (uint64_t)n && s.dsize == userdata);

    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
    if (verbose) traceit("verify done");
}

static void test_write_dbfile (char *tf_template, int n, char *output_name, TXNID xid) {
    if (verbose) traceit("test start");

    DB *dest_db = NULL;
    struct ft_loader_s bl;
    ZERO_STRUCT(bl);
    bl.temp_file_template = tf_template;
    bl.reserved_memory = 512*1024*1024;
    bl.load_root_xid = xid;
    if (xid) {
        XCALLOC_N(1, bl.root_xids_that_created);
        bl.root_xids_that_created[0] = 0;
    }
    int r = ft_loader_init_file_infos(&bl.file_infos); CKERR(r);
    ft_loader_lock_init(&bl);
    ft_loader_set_fractal_workers_count_from_c(&bl);

    struct merge_fileset fs;
    init_merge_fileset(&fs);

    // put rows in the row set
    struct rowset aset;
    uint64_t size_est = 0;
    init_rowset(&aset, toku_ft_loader_get_rowset_budget_for_testing());
    for (int i=0; i<n; i++) {
	DBT key;
        toku_fill_dbt(&key, &i, sizeof i);
	DBT val;
        toku_fill_dbt(&val, &i, sizeof i);
	add_row(&aset, &key, &val);
	size_est += ft_loader_leafentry_size(key.size, val.size, xid);
     }

    toku_ft_loader_set_n_rows(&bl, n);

    ft_loader_init_error_callback(&bl.error_callback);
    ft_loader_set_error_function(&bl.error_callback, err_cb, NULL);
    r = ft_loader_sort_and_write_rows(&aset, &fs, &bl, 0, dest_db, compare_ints);  CKERR(r);
    // destroy_rowset(&aset);
    
    ft_loader_fi_close_all(&bl.file_infos);

    QUEUE q;
    r = queue_create(&q, 0xFFFFFFFF); // infinite queue.
    assert(r==0);
    r = merge_files(&fs, &bl, 0, dest_db, compare_ints, 0, q); CKERR(r);
    assert(fs.n_temp_files==0);

    QUEUE q2;
    r = queue_create(&q2, 0xFFFFFFFF); // infinite queue.
    assert(r==0);

    size_t num_found = 0;
    size_t found_size_est = 0;
    while (1) {
	void *v;
	r = queue_deq(q, &v, NULL, NULL);
	if (r==EOF) break;
	struct rowset *rs = (struct rowset *)v;
	if (verbose) printf("v=%p\n", v);

	for (size_t i=0; i<rs->n_rows; i++) {
	    struct row *row = &rs->rows[i];
	    assert(row->klen==sizeof(int));
	    assert(row->vlen==sizeof(int));
	    assert((int)(num_found+i)==*(int*)(rs->data+row->off));
	    found_size_est += ft_loader_leafentry_size(row->klen, row->vlen, xid);
 	}

	num_found += rs->n_rows;

	r = queue_enq(q2, v, 0, NULL);
	assert(r==0);
    }
    assert((int)num_found == n);
    assert(found_size_est == size_est);
 
    r = queue_eof(q2);
    assert(r==0);

    r = queue_destroy(q);
    assert(r==0);

    DESCRIPTOR_S desc;
    toku_fill_dbt(&desc.dbt, "abcd", 4);

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);

    if (verbose) traceit("write to file");
    r = toku_loader_write_brt_from_q_in_C(&bl, &desc, fd, 1000, q2, size_est, 0, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, 16);
    assert(r==0);

    r = queue_destroy(q2);
    assert_zero(r);
   
    destroy_merge_fileset(&fs);
    ft_loader_fi_destroy(&bl.file_infos, false);

    // walk a cursor through the dbfile and verify the rows
    verify_dbfile(n, output_name);

    ft_loader_destroy_error_callback(&bl.error_callback);
    ft_loader_lock_destroy(&bl);

    toku_free(bl.root_xids_that_created);
}

static int nrows = 1;
static TXNID xid = 0;

static int usage(const char *progname) {
    fprintf(stderr, "Usage:\n %s [-h] [-v] [-q] [-r %d] [-x %" PRIu64 "] [-s] directory\n", progname, nrows, xid);
    return 1;
}

int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {
	    return usage(progname);
        } else if (strcmp(argv[0], "-v") == 0 || strcmp(argv[0], "--verbose") == 0) {
	    verbose=1;
	} else if (strcmp(argv[0], "-q") == 0) {
	    verbose=0;
        } else if (strcmp(argv[0], "-r") == 0) {
            argc--; argv++;
            nrows = atoi(argv[0]);
        } else if (strcmp(argv[0], "-x") == 0) {
            argc--; argv++;
            xid = atol(argv[0]);
        } else if (strcmp(argv[0], "-s") == 0) {
            toku_ft_loader_set_size_factor(1);
	} else if (argv[0][0] == '-' || argc != 1) {
            return usage(progname);
	} else {
            break;
        }
	argc--; argv++;
    }
    const char* directory = TOKU_TEST_FILENAME;
    char unlink_all[strlen(directory)+20];
    snprintf(unlink_all, strlen(directory)+20, "rm -rf %s", directory);
    int r;
    r = system(unlink_all);
    CKERR(r);
    r = toku_os_mkdir(directory, 0755);
    CKERR(r);

    int  templen = strlen(directory)+15;
    char tf_template[templen];
    int tlen = snprintf(tf_template, templen, "%s/tempXXXXXX", directory);
    assert (tlen>0 && tlen<templen);

    char output_name[templen];
    int  olen = snprintf(output_name, templen, "%s/test.tokudb", directory);
    assert (olen>0 && olen<templen);

    test_write_dbfile(tf_template, nrows, output_name, xid);

#if 0
    r = system(unlink_all);
    CKERR(r);
#endif
    return 0;
}

