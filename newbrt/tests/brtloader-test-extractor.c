/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brtloader-test-extractor-errors.c 20466 2010-05-20 17:45:19Z prohaska $"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The purpose of this test is to test the extractor component of the brt loader.  We insert rowsets into the extractor queue and verify temp files 
// after the extractor is finished.

#define DONT_DEPRECATE_MALLOC
#define DONT_DEPRECATE_WRITES
#include "test.h"
#include "brtloader.h"
#include "brtloader-internal.h"
#include "memory.h"

#if defined(__cplusplus)
extern "C" {
#if 0
}
#endif
#endif

static char **get_temp_files(const char *testdir) {
    int ntemp = 0;
    int maxtemp = 32;
    char **tempfiles = toku_calloc(maxtemp, sizeof (char *));
    assert(tempfiles);

    DIR *d = opendir(testdir);
    if (d) {
        while (1) {
            struct dirent *de = readdir(d);
            if (de == NULL)
                break;
            if (strncmp(de->d_name, "temp", 4) == 0) {
                if (ntemp >= maxtemp-1) {
                    maxtemp = 2*maxtemp;
                    tempfiles = toku_realloc(tempfiles, 2*maxtemp);
                    assert(tempfiles);
                }
                assert(ntemp < maxtemp-1);
                tempfiles[ntemp++] = toku_strdup(de->d_name);
            }
        }
        closedir(d);
    }
    tempfiles[ntemp] = NULL;
    return tempfiles;
}

static void free_temp_files(char **tempfiles) {
    for (int i = 0; tempfiles[i] != NULL; i++)
        toku_free(tempfiles[i]);
    toku_free(tempfiles);
}

static void read_tempfile(const char *testdir, const char *tempfile, int **tempkeys, int *ntempkeys) {
    int maxkeys = 32;
    int nkeys = 0;
    int *keys = toku_calloc(maxkeys, sizeof (int));
    assert(keys);

    char fname[strlen(testdir) + 1 + strlen(tempfile) + 1];
    sprintf(fname, "%s/%s", testdir, tempfile);
    FILE *f = fopen(fname, "r");
    if (f) {
        void *vp = NULL;
        while (1) {
            size_t r;
            int len;

            // key
            r = fread(&len, sizeof (int), 1, f);
            if (r == 0)
                break;
            assert(len == sizeof (int));
            vp = toku_realloc(vp, len);
            r = fread(vp, len, 1, f);
            if (r == 0)
                break;
            if (nkeys >= maxkeys) {
                maxkeys *= 2;
                keys = toku_realloc(keys, maxkeys * sizeof (int));
            }
            memcpy(&keys[nkeys], vp, len);
            nkeys++;
            
            // val
            r = fread(&len, sizeof (int), 1, f);
            if (r == 0)
                break;
            assert(len == sizeof (int));
            vp = toku_realloc(vp, len);
            r = fread(vp, len, 1, f);
            if (r == 0)
                break;

        }
        toku_free(vp);
        fclose(f);
    }

    *tempkeys = keys;
    *ntempkeys = nkeys;
}

static void verify(int inkey[], int nkeys, const char *testdir) {
    // find the temp files
    char **tempfiles = get_temp_files(testdir);
    int ntempfiles = 0;
    for (int i = 0; tempfiles[i] != NULL; i++) {
        if (verbose) printf("%s\n", tempfiles[i]);
        ntempfiles++;
    }

    // verify each is sorted
    assert(ntempfiles == 1);
    int *tempkeys; int ntempkeys;
    read_tempfile(testdir, tempfiles[0], &tempkeys, &ntempkeys);

    // compare 
    assert(nkeys == ntempkeys);
    for (int i = 0; i < nkeys; i++) 
        assert(inkey[i] == tempkeys[i]);

    toku_free(tempkeys);
    free_temp_files(tempfiles);
}

static void copy_dbt(DBT *dest, const DBT *src) {
    assert(dest->flags & DB_DBT_REALLOC);
    dest->data = toku_realloc(dest->data, src->size);
    dest->size = src->size;
    memcpy(dest->data, src->data, src->size);
}

static int generate(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra) {
    assert(dest_db == NULL); assert(src_db == NULL); assert(extra == NULL);

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

static int compare_int(DB *dest_db, const DBT *akey, const DBT *bkey) {
    assert(dest_db == NULL);
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

static void test_extractor(int nrows, int nrowsets, const char *testdir) {
    if (verbose) printf("%s %d %d %s\n", __FUNCTION__, nrows, nrowsets, testdir);

    int r;

    int nkeys = nrows * nrowsets;
    int *keys = toku_calloc(nkeys, sizeof (int)); assert(keys);
    for (int i = 0; i < nkeys; i++)
        keys[i] = ascending_keys ? i : nkeys - i;
    if (random_keys)
        shuffle(keys, nkeys);

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

    char temp[strlen(testdir) + 1 + strlen("tempXXXXXX") + 1];
    sprintf(temp, "%s/%s", testdir, "tempXXXXXX");

    BRTLOADER loader;
    r = toku_brt_loader_open(&loader, NULL, generate, NULL, N, dbs, descriptors, fnames, compares, temp, ZERO_LSN);
    assert(r == 0);

    struct rowset *rowset[nrowsets];
    for (int i = 0 ; i < nrowsets; i++) {
        rowset[i] = (struct rowset *) toku_malloc(sizeof (struct rowset));
        assert(rowset[i]);
        init_rowset(rowset[i], toku_brtloader_get_rowset_budget_for_testing());
        populate_rowset(rowset[i], i, nrows, &keys[i*nrows]);
    }

    // feed rowsets to the extractor
    for (int i = 0; i < nrowsets; i++) {
        r = queue_enq(loader->primary_rowset_queue, rowset[i], 1, NULL);
        assert(r == 0);
    }
    r = toku_brt_loader_finish_extractor(loader);
    assert(r == 0);

    int error;
    r = toku_brt_loader_get_error(loader, &error);
    assert(r == 0);
    assert(error == 0);

    // sort the input keys
    qsort(keys, nkeys, sizeof (int), qsort_compare_ints);

    // verify the temp files
    verify(keys, nkeys, testdir);

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
    fprintf(stderr, "[--asc] [--dsc] [--random]\n");
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
        } else if (strcmp(argv[0],"--rowsets") == 0 && argc >= 1) {
            argc--; argv++;
            nrowsets = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
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

    // run test
    test_extractor(nrows, nrowsets, testdir);

    return 0;
}

#if defined(__cplusplus)
}
#endif
