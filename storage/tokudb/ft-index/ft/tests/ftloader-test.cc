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

#include "test.h"
#include <toku_assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "ftloader-internal.h"
#include "memory.h"
#include <portability/toku_path.h>


static int qsort_compare_ints (const void *a, const void *b) {
    int avalue = *(int*)a;
    int bvalue = *(int*)b;
    if (avalue<bvalue) return -1;
    if (avalue>bvalue) return +1;
    return 0;

}

static int compare_ints (DB* UU(desc), const DBT *akey, const DBT *bkey) {
    assert(akey->size==sizeof(int));
    assert(bkey->size==sizeof(int));
    return qsort_compare_ints(akey->data, bkey->data);
}

static void err_cb(DB *db UU(), int dbn UU(), int err UU(), DBT *key UU(), DBT *val UU(), void *extra UU()) {
    fprintf(stderr, "error in test");
    abort();
}

bool founddup;
static void expect_dups_cb(DB *db UU(), int dbn UU(), int err UU(), DBT *key UU(), DBT *val UU(), void *extra UU()) {
    founddup=true;
}

static void test_merge_internal (int a[], int na, int b[], int nb, bool dups) {
    int *MALLOC_N(na+nb, ab); // the combined array a and b
    for (int i=0; i<na; i++) {
	ab[i]=a[i];
    }
    for (int i=0; i<nb; i++) {
	ab[na+i] = b[i];
    }
    struct row *MALLOC_N(na, ar);
    struct row *MALLOC_N(nb, br);
    for (int i=0; i<na; i++) {
	ar[i].off = i*sizeof(a[0]);
	ar[i].klen = sizeof(a[i]);
	ar[i].vlen = 0;
    }
    for (int i=0; i<nb; i++) {
	br[i].off  = (na+i)*sizeof(b[0]);
	br[i].klen = sizeof(b[i]);
	br[i].vlen = 0;
    }
    struct row *MALLOC_N(na+nb, cr);
    DB *dest_db = NULL;
    struct ft_loader_s bl;
    ft_loader_init_error_callback(&bl.error_callback);
    ft_loader_set_error_function(&bl.error_callback, dups ? expect_dups_cb : err_cb, NULL);
    struct rowset rs = { .memory_budget = 0, .n_rows = 0, .n_rows_limit = 0, .rows = NULL, .n_bytes = 0, .n_bytes_limit = 0,
                         .data=(char*)ab};
    merge_row_arrays_base(cr, ar, na, br, nb, 0, dest_db, compare_ints, &bl, &rs);
    ft_loader_call_error_function(&bl.error_callback);
    if (dups) {
	assert(founddup);
    } else {
	// verify the merge
	int i=0;
	int j=0;
	for (int k=0; k<na+nb; k++) {
	    int voff = cr[k].off;
	    int vc   = *(int*)(((char*)ab)+voff);
	    if (i<na && j<nb) {
		if (vc==a[i]) {
		    assert(a[i]<=b[j]);
		    i++;
		} else if (vc==b[j]) {
		    assert(a[i]>b[j]);
		    j++;
		} else {
		    assert(0);
		}
	    }
	}
    }
    toku_free(cr);
    toku_free(ar);
    toku_free(br);
    toku_free(ab);
    ft_loader_destroy_error_callback(&bl.error_callback);
}

/* Test the basic merger. */
static void test_merge (void) {
    {
	int avals[]={1,2,3,4,5};
	int *bvals = NULL; //icc won't let us use a zero-sized array explicitly or by [] = {} construction.
	test_merge_internal(avals, 5, bvals, 0, false);
	test_merge_internal(bvals, 0, avals, 5, false);
    }
    {
	int avals[]={1,3,5,7};
	int bvals[]={2,4};
	test_merge_internal(avals, 4, bvals, 2, false);
	test_merge_internal(bvals, 2, avals, 4, false);
    }
    {
	int avals[]={1,2,3,5,6,7};
	int bvals[]={2,4,5,6,8};
	test_merge_internal(avals, 6, bvals, 5, true);
	test_merge_internal(bvals, 5, avals, 6, true);
    }
}

static void test_internal_mergesort_row_array (int a[], int n) {
    struct row *MALLOC_N(n, ar);
    for (int i=0; i<n; i++) {
	ar[i].off  = i*sizeof(a[0]);
	ar[i].klen = sizeof(a[i]);
	ar[i].vlen = 0;
    }
    struct rowset rs = { .memory_budget = 0, .n_rows = 0, .n_rows_limit = 0, .rows = NULL, .n_bytes = 0, .n_bytes_limit = 0,
                         .data=(char*)a};
    ft_loader_mergesort_row_array (ar, n, 0, NULL, compare_ints, NULL, &rs);
    int *MALLOC_N(n, tmp);
    for (int i=0; i<n; i++) {
	tmp[i]=a[i];
    }
    qsort(tmp, n, sizeof(a[0]), qsort_compare_ints);
    for (int i=0; i<n; i++) {
	int voff = ar[i].off;
	int v    = *(int*)(((char*)a)+voff);
	assert(tmp[i]==v);
    }
    toku_free(ar);
    toku_free(tmp);
}

static void test_mergesort_row_array (void) {
    {
	int avals[]={5,2,1,7};
	for (int i=0; i<=4; i++)
	    test_internal_mergesort_row_array(avals, i);
    }
    const int MAX_LEN = 100;
    enum { MAX_VAL = 1000 };
    for (int i=0; i<MAX_LEN; i++) {
	bool used[MAX_VAL];
	for (int j=0; j<MAX_VAL; j++) used[j]=false; 
	int len=1+random()%MAX_LEN;
	int avals[len];
	for (int j=0; j<len; j++) {
	    int v;
	    do {
		v = random()%MAX_VAL;
	    } while (used[v]); 
	    avals[j] = v;
	    used[v] = true;
	}
	test_internal_mergesort_row_array(avals, len);
    }
}

static void test_read_write_rows (char *tf_template) {
    struct ft_loader_s bl;
    ZERO_STRUCT(bl);
    bl.temp_file_template = tf_template;
    int r = ft_loader_init_file_infos(&bl.file_infos);
    CKERR(r);
    FIDX file;
    r = ft_loader_open_temp_file(&bl, &file);
    CKERR(r);

    uint64_t dataoff=0;

    const char *keystrings[] = {"abc", "b", "cefgh"};
    const char *valstrings[] = {"defg", "", "xyz"};
    uint64_t actual_size=0;
    for (int i=0; i<3; i++) {
	DBT key;
        toku_fill_dbt(&key, keystrings[i], strlen(keystrings[i]));
	DBT val;
        toku_fill_dbt(&val, valstrings[i], strlen(valstrings[i]));
	r = loader_write_row(&key, &val, file, toku_bl_fidx2file(&bl, file), &dataoff, nullptr, &bl);
	CKERR(r);
	actual_size+=key.size + val.size + 8;
    }
    if (actual_size != dataoff) fprintf(stderr, "actual_size=%" PRIu64 ", dataoff=%" PRIu64 "\n", actual_size, dataoff);
    assert(actual_size == dataoff);

    r = ft_loader_fi_close(&bl.file_infos, file, true);
    CKERR(r);

    r = ft_loader_fi_reopen(&bl.file_infos, file, "r");
    CKERR(r);

    {
	int n_read=0;
	DBT key, val;
        toku_init_dbt(&key);
        toku_init_dbt(&val);
	while (0==loader_read_row(toku_bl_fidx2file(&bl, file), &key, &val)) {
	    assert(strlen(keystrings[n_read])==key.size);
	    assert(strlen(valstrings[n_read])==val.size);
	    assert(0==memcmp(keystrings[n_read], key.data, key.size));
	    assert(0==memcmp(valstrings[n_read], val.data, val.size));
	    assert(key.size<=key.ulen);
	    assert(val.size<=val.ulen);
	    n_read++;
	}
	assert(n_read==3);
	toku_free(key.data);
	toku_free(val.data);
    }
    r = ft_loader_fi_close(&bl.file_infos, file, true);
    CKERR(r);

    r = ft_loader_fi_unlink(&bl.file_infos, file);
    CKERR(r);

    assert(bl.file_infos.n_files_open==0);
    assert(bl.file_infos.n_files_extant==0);

    ft_loader_fi_destroy(&bl.file_infos, false);
}

static void fill_rowset (struct rowset *rows,
			 int keys[],
			 const char *vals[],
			 int n,
			 uint64_t *size_est) {
    init_rowset(rows, toku_ft_loader_get_rowset_budget_for_testing());
    for (int i=0; i<n; i++) {
	DBT key;
        toku_fill_dbt(&key, &keys[i], sizeof keys[i]);
	DBT val;
        toku_fill_dbt(&val, vals[i], strlen(vals[i]));
	add_row(rows, &key, &val);
	*size_est += ft_loader_leafentry_size(key.size, val.size, TXNID_NONE);
    }
}

static void verify_dbfile(int n, int sorted_keys[], const char *sorted_vals[], const char *name) {
    int r;

    CACHETABLE ct;
    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    TOKUTXN const null_txn = NULL;
    FT_HANDLE t = NULL;
    toku_ft_handle_create(&t);
    toku_ft_set_bt_compare(t, compare_ints);
    r = toku_ft_handle_open(t, name, 0, 0, ct, null_txn); assert(r==0);

    FT_CURSOR cursor = NULL;
    r = toku_ft_cursor(t, &cursor, NULL, false, false); assert(r == 0);

    size_t userdata = 0;
    int i;
    for (i=0; i<n; i++) {
	struct check_pair pair = {sizeof sorted_keys[i], &sorted_keys[i], (ITEMLEN) strlen(sorted_vals[i]), sorted_vals[i], 0};
        r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
        userdata += pair.keylen + pair.vallen;
    }
    
    struct check_pair pair; memset(&pair, 0, sizeof pair);
    r = toku_ft_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
    assert(r != 0);

    toku_ft_cursor_close(cursor);

    struct ftstat64_s s;
    toku_ft_handle_stat64(t, NULL, &s);
    assert(s.nkeys == (uint64_t) n && s.ndata == (uint64_t) n && s.dsize == userdata);
    
    r = toku_close_ft_handle_nolsn(t, 0); assert(r==0);
    toku_cachetable_close(&ct);
}

static void test_merge_files (const char *tf_template, const char *output_name) {
    DB *dest_db = NULL;
    struct ft_loader_s bl;
    ZERO_STRUCT(bl);
    bl.temp_file_template = tf_template;
    bl.reserved_memory = 512*1024*1024;
    int r = ft_loader_init_file_infos(&bl.file_infos); CKERR(r);
    ft_loader_lock_init(&bl);
    ft_loader_init_error_callback(&bl.error_callback);
    ft_loader_set_fractal_workers_count_from_c(&bl);

    struct merge_fileset fs;
    init_merge_fileset(&fs);

    int a_keys[] = {   1,    3,    5,    7, 8, 9};
    int b_keys[] = { 0,   2,    4,    6         };
    const char *a_vals[] = {"a", "c", "e", "g", "h", "i"};
    const char *b_vals[] = {"0", "b", "d", "f"};
    int sorted_keys[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const char *sorted_vals[] = { "0", "a", "b", "c", "d", "e", "f", "g", "h", "i" };
    struct rowset aset, bset;
    uint64_t size_est = 0;
    fill_rowset(&aset, a_keys, a_vals, 6, &size_est);
    fill_rowset(&bset, b_keys, b_vals, 4, &size_est);

    toku_ft_loader_set_n_rows(&bl, 6+4);

    ft_loader_set_error_function(&bl.error_callback, err_cb, NULL);
    r = ft_loader_sort_and_write_rows(&aset, &fs, &bl, 0, dest_db, compare_ints);  CKERR(r);
    r = ft_loader_sort_and_write_rows(&bset, &fs, &bl, 0, dest_db, compare_ints);  CKERR(r);
    assert(fs.n_temp_files==2 && fs.n_temp_files_limit >= fs.n_temp_files);
    // destroy_rowset(&aset);
    // destroy_rowset(&bset);
    for (int i=0; i<2; i++) assert(fs.data_fidxs[i].idx != -1);

    ft_loader_fi_close_all(&bl.file_infos);

    QUEUE q;
    r = queue_create(&q, 0xFFFFFFFF); // infinite queue.
    assert(r==0);

    r = merge_files(&fs, &bl, 0, dest_db, compare_ints, 0, q); CKERR(r);

    assert(fs.n_temp_files==0);

    DESCRIPTOR_S desc;
    toku_fill_dbt(&desc.dbt, "abcd", 4);

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    
    r = toku_loader_write_ft_from_q_in_C(&bl, &desc, fd, 1000, q, size_est, 0, 0, 0, TOKU_DEFAULT_COMPRESSION_METHOD, 16);
    assert(r==0);

    destroy_merge_fileset(&fs);
    ft_loader_fi_destroy(&bl.file_infos, false);
    ft_loader_destroy_error_callback(&bl.error_callback);
    ft_loader_lock_destroy(&bl);

    // verify the dbfile
    verify_dbfile(10, sorted_keys, sorted_vals, output_name);

    r = queue_destroy(q);
    assert(r==0);
}

/* Test to see if we can open temporary files. */
int test_main (int argc, const char *argv[]) {
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	}
        else {
            break;
        }
	argc--; argv++;
    }
    const char* directory = TOKU_TEST_FILENAME;
    int r = toku_os_mkdir(directory, 0755);
    if (r!=0) CKERR2(errno, EEXIST);

    int  templen = strlen(directory)+15;
    char tf_template[templen];
    {
	int n = snprintf(tf_template, templen, "%s/tempXXXXXX", directory);
	assert (n>0 && n<templen);
    }
    char output_name[templen];
    {
	int n = snprintf(output_name, templen, "%s/data.tokudb", directory);
	assert (n>0 && n<templen);
    }
    test_read_write_rows(tf_template);
    test_merge();
    test_mergesort_row_array();
    test_merge_files(tf_template, output_name);
    
    {
	char deletecmd[templen];
	int n =  snprintf(deletecmd, templen, "rm -rf %s", directory);
	assert(n>0 && n<templen);
	r = system(deletecmd);
        CKERR(r);
    }

    return 0;
}

