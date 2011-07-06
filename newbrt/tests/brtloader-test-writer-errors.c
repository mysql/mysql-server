/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// test the loader write dbfile function

#define DONT_DEPRECATE_WRITES
#define DONT_DEPRECATE_MALLOC
#include "includes.h"
#include "test.h"
#include "brtloader-internal.h"
#include "brtloader-error-injector.h"

#if defined(__cplusplus)
extern "C" {
#if 0 
}
#endif
#endif

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

static void write_dbfile (char *template, int n, char *output_name, BOOL expect_error, int testno) {
    if (verbose) printf("test start %d %d testno=%d\n", n, expect_error, testno);

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
	size_est += brtloader_leafentry_size(key.size, val.size, TXNID_NONE);
    }

    toku_brt_loader_set_n_rows(&bl, n);

    brt_loader_init_error_callback(&bl.error_callback);
    brt_loader_set_error_function(&bl.error_callback, err_cb, NULL);
    brt_loader_init_poll_callback(&bl.poll_callback);
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
	    found_size_est += brtloader_leafentry_size(row->klen, row->vlen, TXNID_NONE);
	}

	num_found += rs->n_rows;

	r = queue_enq(q2, v, 0, NULL);
	assert(r==0);
    }
    assert((int)num_found == n);
    if (!expect_error) assert(found_size_est == size_est);

    r = queue_eof(q2);
    assert(r==0);

    r = queue_destroy(q);
    assert(r==0);

    DESCRIPTOR_S desc = {.dbt = (DBT){.size = 4, .data="abcd"}};

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);

    toku_set_func_malloc_only(my_malloc);
    toku_set_func_realloc_only(my_realloc);
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);
    brt_loader_set_error_function(&bl.error_callback, NULL, NULL);
    brt_loader_set_poll_function(&bl.poll_callback, loader_poll_callback, NULL);

    r = toku_loader_write_brt_from_q_in_C(&bl, &desc, fd, 1000, q2, size_est, 0, 0);
    // if (!(expect_error ? r != 0 : r == 0)) printf("WARNING%%d expect_error=%d r=%d\n", __LINE__, expect_error, r); 
    assert(expect_error ? r != 0 : r == 0);

    toku_set_func_malloc_only(NULL);
    toku_set_func_realloc_only(NULL);
    brtloader_set_os_fwrite(NULL);
    toku_set_func_write(NULL);
    toku_set_func_pwrite(NULL);

    brt_loader_destroy_error_callback(&bl.error_callback);
    brt_loader_destroy_poll_callback(&bl.poll_callback);
    brt_loader_lock_destroy(&bl);
    
    r = queue_destroy(q2);
    //if (r != 0) printf("WARNING%d r=%d\n", __LINE__, r);
    assert(r==0);
   
    destroy_merge_fileset(&fs);
    brtloader_fi_destroy(&bl.file_infos, expect_error);
}

static int usage(const char *progname, int n) {
    fprintf(stderr, "Usage: %s [options] directory\n", progname);
    fprintf(stderr, "[-v] turn on verbose\n");
    fprintf(stderr, "[-q] turn off verbose\n");
    fprintf(stderr, "[-r %d] set the number of rows\n", n);
    fprintf(stderr, "[-s] set the small loader size factor\n");
    fprintf(stderr, "[-m] inject big malloc and realloc errors\n");
    fprintf(stderr, "[--malloc_limit %u] set the threshold for failing malloc and realloc\n", (unsigned) my_big_malloc_limit);
    fprintf(stderr, "[--realloc_errors] inject realloc errors\n");
    fprintf(stderr, "[-w] inject write errors\n");
    fprintf(stderr, "[-u] inject user errors\n");
    return 1;
}

int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    int n = 1;
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0],"-h")==0) {
            return usage(progname, n);
	} else if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r") == 0) {
            argc--; argv++;
            n = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
        } else if (strcmp(argv[0],"-w") == 0) {
            do_write_errors = 1;
        } else if (strcmp(argv[0],"-m") == 0) {
            do_malloc_errors = 1;
            do_realloc_errors = 1;
        } else if (strcmp(argv[0],"-u") == 0) {
            do_user_errors = 1;
        } else if (strcmp(argv[0],"--realloc_errors") == 0) {
            do_realloc_errors = 1;
        } else if (strcmp(argv[0],"--malloc_limit") == 0 && argc > 1) {
            argc--; argv++;
            my_big_malloc_limit = atoi(argv[0]);
	} else if (argc!=1) {
            return usage(progname, n);
	}
        else {
            break;
        }
	argc--; argv++;
    }
    assert(argc==1); // argv[1] is the directory in which to do the test.
    const char* directory = argv[0];
    char unlink_all[strlen(directory)+20];
    snprintf(unlink_all, strlen(directory)+20, "rm -rf %s", directory);

    int  templen = strlen(directory)+15;
    char template[templen];
    int tlen = snprintf(template, templen, "%s/tempXXXXXX", directory);
    assert (tlen>0 && tlen<templen);

    char output_name[templen];
    int  olen = snprintf(output_name, templen, "%s/test.tokudb", directory);
    assert (olen>0 && olen<templen);

    // callibrate
    int r;
    r = system(unlink_all); CKERR(r);
    r = toku_os_mkdir(directory, 0755); CKERR(r);
    write_dbfile(template, n, output_name, FALSE, 0);

    if (verbose) printf("my_malloc_count=%d big_count=%d\n", my_malloc_count, my_big_malloc_count);
    if (verbose) printf("my_realloc_count=%d big_count=%d\n", my_realloc_count, my_big_realloc_count);

    int event_limit = event_count;
    if (verbose) printf("event_limit=%d\n", event_limit);

    for (int i = 1; i <= event_limit; i++) {
        reset_event_counts();
        reset_my_malloc_counts();
        event_count_trigger = i;
        r = system(unlink_all); CKERR(r);
        r = toku_os_mkdir(directory, 0755); CKERR(r);
        write_dbfile(template, n, output_name, TRUE, i);
    }

    r = system(unlink_all); CKERR(r);

    return 0;
}

#if defined(__cplusplus)
}
#endif
