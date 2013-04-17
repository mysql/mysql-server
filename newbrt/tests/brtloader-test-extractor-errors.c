/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The purpose of this test is to test the error recovery of the extractor.  We inject errors into the extractor and
// verify that the extractor error state is set.

#define DONT_DEPRECATE_MALLOC
#define DONT_DEPRECATE_WRITES
#include "test.h"
#include "brtloader.h"
#include "brtloader-internal.h"
#include "brtloader-error-injector.h"
#include "memory.h"

#if defined(__cplusplus)
extern "C" {
#if 0 
}
#endif
#endif

static void copy_dbt(DBT *dest, const DBT *src) {
    assert(dest->flags & DB_DBT_REALLOC);
    dest->data = toku_realloc(dest->data, src->size);
    dest->size = src->size;
    memcpy(dest->data, src->data, src->size);
}

static int generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_val = dest_val; src_key = src_key; src_val = src_val;

    copy_dbt(dest_key, src_key);
    copy_dbt(dest_val, src_val);
    
    return 0;
}

static int qsort_compare_ints (const void *a, const void *b) {
    int avalue = *(int*)a;
    int bvalue = *(int*)b;
    if (avalue<bvalue) return -1;
    if (avalue>bvalue) return +1;
    return 0;
}

static int compare_int(DB *desc, const DBT *akey, const DBT *bkey) {
    assert(desc == NULL);
    assert(akey->size == sizeof (int));
    assert(bkey->size == sizeof (int));
    return qsort_compare_ints(akey->data, bkey->data);
}

static void populate_rowset(struct rowset *rowset, int seq, int nrows, int keys[]) {
    for (int i = 0; i < nrows; i++) {
        int k = keys[i];
        int v = seq * nrows + i;
        DBT key = { .size = sizeof k, .data = &k };
        DBT val = { .size = sizeof v, .data = &v };
        add_row(rowset, &key, &val);
    }
}

static void shuffle(int a[], int n) {
    for (int i = 0; i < n; i++) {
        int r = random() % n;
        int t = a[i]; a[i] = a[r]; a[r] = t;
    }
}

static int ascending_keys = 0;
static int descending_keys = 0;
static int random_keys = 0;

static void test_extractor(int nrows, int nrowsets, BOOL expect_fail, const char *testdir) {
    if (verbose) printf("%s %d %d %s\n", __FUNCTION__, nrows, nrowsets, testdir);

    int r;

    int nkeys = nrows * nrowsets;
    int *keys = toku_malloc(nkeys * sizeof (int)); assert(keys);
    for (int i = 0; i < nkeys; i++)
        keys[i] = ascending_keys ? i : nkeys - i;
    if (random_keys)
        shuffle(keys, nkeys);

    // open the brtloader. this runs the extractor.
    const int N = 1;
    BRT brts[N];
    const char *fnames[N];
    brt_compare_func compares[N];
    for (int i = 0; i < N; i++) {
        brts[i] = NULL;
        fnames[i] = "";
        compares[i] = compare_int;
    }

    char temp[strlen(testdir) + 1 + strlen("tempXXXXXX") + 1];
    sprintf(temp, "%s/%s", testdir, "tempXXXXXX");

    BRTLOADER loader;
    r = toku_brt_loader_open(&loader, NULL, generate, NULL, N, brts, fnames, compares, "tempXXXXXX", ZERO_LSN, TXNID_NONE);
    assert(r == 0);

    struct rowset *rowset[nrowsets];
    for (int i = 0 ; i < nrowsets; i++) {
        rowset[i] = (struct rowset *) toku_malloc(sizeof (struct rowset));
        assert(rowset[i]);
        init_rowset(rowset[i], toku_brtloader_get_rowset_budget_for_testing());
        populate_rowset(rowset[i], i, nrows, &keys[i*nrows]);
    }

    // setup error injection
    toku_set_func_malloc(my_malloc);
    toku_set_func_realloc(my_realloc);
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);
    brt_loader_set_poll_function(&loader->poll_callback, loader_poll_callback, NULL);

    // feed rowsets to the extractor
    for (int i = 0; i < nrowsets; i++) {
        r = queue_enq(loader->primary_rowset_queue, rowset[i], 1, NULL);
        assert(r == 0);
    }

    r = toku_brt_loader_finish_extractor(loader);
    assert(r == 0);

    toku_set_func_malloc(NULL);
    toku_set_func_realloc(NULL);
    brtloader_set_os_fwrite(NULL);
    toku_set_func_write(NULL);
    toku_set_func_pwrite(NULL);

    int error;
    r = toku_brt_loader_get_error(loader, &error);
    assert(r == 0);
    assert(expect_fail ? error != 0 : error == 0);

    // verify the temp files

    // abort the brtloader.  this ends the test
    r = toku_brt_loader_abort(loader, TRUE);
    assert(r == 0);

    toku_free(keys);
}
static int nrows = 1;
static int nrowsets = 2;

static int usage(const char *progname) {
    fprintf(stderr, "Usage: %s [options] directory\n", progname);
    fprintf(stderr, "[-v] turn on verbose\n");
    fprintf(stderr, "[-q] turn off verbose\n");
    fprintf(stderr, "[-r %d] set the number of rows\n", nrows);
    fprintf(stderr, "[--rowsets %d] set the number of rowsets\n", nrowsets);
    fprintf(stderr, "[-s] set the small loader size factor\n");
    fprintf(stderr, "[-m] inject big malloc and realloc errors\n");
    fprintf(stderr, "[--malloc_limit %u] set the threshold for failing malloc and realloc\n", (unsigned) my_big_malloc_limit);
    fprintf(stderr, "[-w] inject write errors\n");
    fprintf(stderr, "[-u] inject user errors\n");
    return 1;
}

int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    int max_error_limit = -1;
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0],"-h")==0) {
            return usage(progname);
        } else if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r") == 0 && argc >= 1) {
            argc--; argv++;
            nrows = atoi(argv[0]);
        } else if (strcmp(argv[0],"--rowsets") == 0 && argc >= 1) {
            argc--; argv++;
            nrowsets = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
        } else if (strcmp(argv[0],"-w") == 0) {
            do_write_errors = 1;
        } else if (strcmp(argv[0],"-m") == 0) {
            do_malloc_errors = 1;
        } else if (strcmp(argv[0],"-u") == 0) {
            do_user_errors = 1;
        } else if (strcmp(argv[0],"--malloc_limit") == 0 && argc > 1) {
            argc--; argv++;
            my_big_malloc_limit = atoi(argv[0]);
        } else if (strcmp(argv[0],"--max_error_limit") == 0 && argc >= 1) {
            argc--; argv++;
            max_error_limit = atoi(argv[0]);
        } else if (strcmp(argv[0],"--asc") == 0) {
            ascending_keys = 1;
        } else if (strcmp(argv[0],"--dsc") == 0) {
            descending_keys = 1;
        } else if (strcmp(argv[0],"--random") == 0) {
            random_keys = 1;
	} else if (argc!=1) {
            return usage(progname);
	    exit(1);
	}
        else {
            break;
        }
	argc--; argv++;
    }

    assert(argc == 1);
    const char *testdir = argv[0];

    if (ascending_keys + descending_keys + random_keys == 0)
        ascending_keys = 1;

    // callibrate
    test_extractor(nrows, nrowsets, FALSE, testdir);

    // run tests
    int error_limit = event_count;
    if (verbose) printf("error_limit=%d\n", error_limit);

    if (max_error_limit != -1 && error_limit > max_error_limit)
        error_limit = max_error_limit;
    for (int i = 1; i <= error_limit; i++) {
        reset_event_counts();
        reset_my_malloc_counts();
        event_count_trigger = i;
        test_extractor(nrows, nrowsets, TRUE, testdir);
    }

    return 0;
}

#if defined(__cplusplus)
}
#endif
