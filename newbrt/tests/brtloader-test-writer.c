/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// test the loader write dbfile function

#include "includes.h"
#include "test.h"
#include "brtloader-internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

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

static int compare_ints (DB *dest_db, const DBT *akey, const DBT *bkey) {
    assert(dest_db==NULL);
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
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);


    TOKUTXN const null_txn = NULL;
    BRT t = NULL;
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_bt_compare(t, compare_ints);
    assert(r==0);
    r = toku_brt_open(t, name, 0, 0, ct, null_txn, 0); assert(r==0);

    if (verbose) traceit("Verifying brt internals");
    r = toku_verify_brt(t);
    if (verbose) traceit("Verified brt internals");

    BRT_CURSOR cursor = NULL;
    r = toku_brt_cursor(t, &cursor, NULL, FALSE); assert(r == 0);

    int i;
    for (i=0; ; i++) {
	int kk = i;
	int vv = i;
	struct check_pair pair = {sizeof kk, &kk, sizeof vv, &vv, 0};
        r = toku_brt_cursor_get(cursor, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
    }

    assert(i == n);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
    if (verbose) traceit("verify done");
}

static void test_write_dbfile (char *template, int n, char *output_name) {
    if (verbose) traceit("test start");

    DB *dest_db = NULL;
    struct brtloader_s bl = {
        .temp_file_template = template,
        .reserved_memory = 512*1024*1024,
    };
    int r = brtloader_init_file_infos(&bl.file_infos); CKERR(r);
    r = brt_loader_lock_init(&bl); CKERR(r);
    brt_loader_set_fractal_workers_count_from_c(&bl);

    struct merge_fileset fs;
    init_merge_fileset(&fs);

    // put rows in the row set
    struct rowset aset;
    uint64_t size_est = 0;
    init_rowset(&aset, toku_brtloader_get_rowset_budget_for_testing());
    for (int i=0; i<n; i++) {
	DBT key = {.size=sizeof i,
		   .data=&i};
	DBT val = {.size=sizeof i,
		   .data=&i};
	add_row(&aset, &key, &val);
	size_est += key.size + val.size + disksize_row_overhead;
     }

    toku_brt_loader_set_n_rows(&bl, n);

    brt_loader_init_error_callback(&bl.error_callback);
    brt_loader_set_error_function(&bl.error_callback, err_cb, NULL);
    r = brt_loader_sort_and_write_rows(&aset, &fs, &bl, 0, dest_db, compare_ints);  CKERR(r);
    // destroy_rowset(&aset);
    
    brtloader_fi_close_all(&bl.file_infos);

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
	    found_size_est += row->klen + row->vlen + disksize_row_overhead; 
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

    DESCRIPTOR_S desc = {.dbt = (DBT){.size = 4, .data="abcd"}};

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);

    if (verbose) traceit("write to file");
    r = toku_loader_write_brt_from_q_in_C(&bl, &desc, fd, 1000, q2, size_est, 0, 0);
    assert(r==0);

    r = queue_destroy(q2);
    assert(r==0);
   
    destroy_merge_fileset(&fs);
    brtloader_fi_destroy(&bl.file_infos, FALSE);

    // walk a cursor through the dbfile and verify the rows
    verify_dbfile(n, output_name);

    brt_loader_destroy_error_callback(&bl.error_callback);
    brt_loader_lock_destroy(&bl);
}

static int nrows = 1;

static int usage(const char *progname) {
     fprintf(stderr, "Usage:\n %s [-h] [-v] [-q] [-r %d] [-s] directory\n", progname, nrows);
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
        } else if (strcmp(argv[0], "-s") == 0) {
            toku_brtloader_set_size_factor(1);
	} else if (argv[0][0] == '-' || argc != 1) {
            return usage(progname);
	} else {
            break;
        }
	argc--; argv++;
    }
    assert(argc == 1); // argv[1] is the directory in which to do the test.
    const char* directory = argv[0];
    char unlink_all[strlen(directory)+20];
    snprintf(unlink_all, strlen(directory)+20, "rm -rf %s", directory);
    int r;
    r = system(unlink_all);
    CKERR(r);
    r = toku_os_mkdir(directory, 0755);
    CKERR(r);

    int  templen = strlen(directory)+15;
    char template[templen];
    int tlen = snprintf(template, templen, "%s/tempXXXXXX", directory);
    assert (tlen>0 && tlen<templen);

    char output_name[templen];
    int  olen = snprintf(output_name, templen, "%s/test.tokudb", directory);
    assert (olen>0 && olen<templen);

    test_write_dbfile(template, nrows, output_name);

#if 0
    r = system(unlink_all);
    CKERR(r);
#endif
    return 0;
}

#if defined(__cplusplus)
}
#endif
