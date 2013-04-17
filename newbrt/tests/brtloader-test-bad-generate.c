/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The purpose of this test is to test the error return from the generate callback

#define DONT_DEPRECATE_MALLOC
#define DONT_DEPRECATE_WRITES
#include "test.h"
#include "brtloader.h"
#include "brtloader-internal.h"
#include "memory.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if 0

static int my_malloc_count = 0;
static int my_malloc_trigger = 0;

static void set_my_malloc_trigger(int n) {
    my_malloc_count = 0;
    my_malloc_trigger = n;
}

static void *my_malloc(size_t n) {
    my_malloc_count++;
    if (my_malloc_count == my_malloc_trigger) {
        errno = ENOSPC;
        return NULL;
    } else
        return malloc(n);
}

#endif

static int write_count, write_count_trigger, write_enospc;

static void reset_write_counts(void) {
    write_count = write_count_trigger = write_enospc = 0;
}

static void count_enospc(void) {
    write_enospc++;
}

static size_t bad_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    write_count++;
    size_t r;
    if (write_count_trigger == write_count) {
        count_enospc();
	errno = ENOSPC;
	r = -1;
    } else {
	r = fwrite(ptr, size, nmemb, stream);
	if (r!=nmemb) {
	    errno = ferror(stream);
	}
    }
    return r;
}

static ssize_t bad_write(int fd, const void * bp, size_t len) {
    ssize_t r;
    write_count++;
    if (write_count_trigger == write_count) {
        count_enospc();
	errno = ENOSPC;
	r = -1;
    } else {
	r = write(fd, bp, len);
    }
    return r;
}

static ssize_t bad_pwrite(int fd, const void * bp, size_t len, toku_off_t off) {
    ssize_t r;
    write_count++;
    if (write_count_trigger == write_count) {
        count_enospc();
	errno = ENOSPC;
	r = -1;
    } else {
	r = pwrite(fd, bp, len, off);
    }
    return r;
}

static int generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {
    dest_db = dest_db; src_db = src_db; dest_key = dest_key; dest_val = dest_val; src_key = src_key; src_val = src_val; extra = extra;
    return EINVAL;
}

static int qsort_compare_ints (const void *a, const void *b) {
    int avalue = *(int*)a;
    int bvalue = *(int*)b;
    if (avalue<bvalue) return -1;
    if (avalue>bvalue) return +1;
    return 0;
}

static int compare_int(DB *dest_db, const DBT *akey, const DBT *bkey) {
    assert(dest_db == NULL);
    assert(akey->size == sizeof (int));
    assert(bkey->size == sizeof (int));
    return qsort_compare_ints(akey->data, bkey->data);
}

static void populate_rowset(struct rowset *rowset, int seq, int nrows) {
    for (int i = 0; i < nrows; i++) {
        int k = htonl(seq + i);
        int v = seq + i;
        DBT key = { .size = sizeof k, .data = &k };
        DBT val = { .size = sizeof v, .data = &v };
        add_row(rowset, &key, &val);
    }
}

static void test_extractor(int nrows, int nrowsets, BOOL expect_fail) {
    if (verbose) printf("%s %d %d\n", __FUNCTION__, nrows, nrowsets);

    int r;

    // open the brtloader. this runs the extractor.
    const int N = 1;
    DB *dbs[N];
    const struct descriptor *descriptors[N];
    const char *fnames[N];
    brt_compare_func compares[N];
    for (int i = 0; i < N; i++) {
        dbs[i] = NULL;
        descriptors[i] = NULL;
        fnames[i] = "";
        compares[i] = compare_int;
    }

    BRTLOADER loader;
    r = toku_brt_loader_open(&loader, NULL, generate, NULL, N, dbs, descriptors, fnames, compares, "tempXXXXXX", ZERO_LSN);
    assert(r == 0);

    struct rowset *rowset[nrowsets];
    for (int i = 0 ; i < nrowsets; i++) {
        rowset[i] = (struct rowset *) toku_malloc(sizeof (struct rowset));
        assert(rowset[i]);
        init_rowset(rowset[i], toku_brtloader_get_rowset_budget_for_testing());
        populate_rowset(rowset[i], i, nrows);
    }

    // setup error injection
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);

    // feed rowsets to the extractor
    for (int i = 0; i < nrowsets; i++) {
        r = queue_enq(loader->primary_rowset_queue, rowset[i], 1, NULL);
        assert(r == 0);
    }

    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);

    // verify the temp files

    // abort the brtloader.  this ends the test
    r = toku_brt_loader_abort(loader, TRUE);
    assert(r == 0);

    expect_fail = expect_fail;
}

static int nrows = 1;
static int nrowsets = 2;

static int usage(const char *progname) {
    fprintf(stderr, "Usage:\n %s [-h] [-v] [-q] [-s] [-r %d] [--rowsets %d]\n", progname, nrows, nrowsets);
    return 1;
}

int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
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
        } else if (strcmp(argv[0],"--nrowsets") == 0 && argc >= 1) {
            argc--; argv++;
            nrowsets = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
	} else if (argc!=1) {
            return usage(progname);
	    exit(1);
	}
        else {
            break;
        }
	argc--; argv++;
    }

    // callibrate
    test_extractor(nrows, nrowsets, FALSE);

    // run tests
    int write_error_limit = write_count;
    if (verbose) printf("write_error_limit=%d\n", write_error_limit);

    for (int i = 1; i <= write_error_limit; i++) {
        reset_write_counts();
        write_count_trigger = i;
        test_extractor(nrows, nrowsets, TRUE);
    }

    return 0;
}

#if defined(__cplusplus)
}
#endif
