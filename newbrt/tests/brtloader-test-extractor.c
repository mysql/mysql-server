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

static char **get_temp_files(const char *testdir) {
    int ntemp = 0;
    int maxtemp = 32;
    char **tempfiles = toku_malloc(maxtemp * sizeof (char *)); assert(tempfiles);

    DIR *d = opendir(testdir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strncmp(de->d_name, "temp", 4) == 0) {
                if (ntemp >= maxtemp-1) {
                    maxtemp = 2*maxtemp;
                    tempfiles = toku_realloc(tempfiles, 2*maxtemp*sizeof (char *));
                    assert(tempfiles);
                }
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

static int read_row(FILE *f, DBT *key, DBT *val) {
    size_t r;
    int len;
    r = fread(&len, sizeof len, 1, f);
    if (r != 1)
        return EOF;
    assert(key->flags == DB_DBT_REALLOC);
    key->data = toku_realloc(key->data, len); key->size = len;
    r = fread(key->data, len, 1, f);
    if (r != 1)
        return EOF;
    r = fread(&len, sizeof len, 1, f);
    if (r != 1)
        return EOF;
    assert(val->flags == DB_DBT_REALLOC);
    val->data = toku_realloc(val->data, len); val->size = len;
    r = fread(val->data, len, 1, f);
    if (r != 1)
        return EOF;
    return 0;
}

static void write_row(FILE *f, DBT *key, DBT *val) {
    size_t r;
    int len = key->size;
    r = fwrite(&len, sizeof len, 1, f);
    assert(r == 1);
    r = fwrite(key->data, len, 1, f);
    assert(r == 1);
    len = val->size;
    r = fwrite(&len, sizeof len, 1, f);
    assert(r == 1);
    r = fwrite(val->data, len, 1, f);
    assert(r == 1);
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
        DBT key = { .flags = DB_DBT_REALLOC };
        DBT val = { .flags = DB_DBT_REALLOC };
        while (read_row(f, &key, &val) == 0) {
            if (nkeys >= maxkeys) {
                maxkeys *= 2;
                keys = toku_realloc(keys, maxkeys * sizeof (int));
            }
            assert(key.size == sizeof (int));
            memcpy(&keys[nkeys], key.data, key.size);
            nkeys++;
        }
        toku_free(key.data);
        toku_free(val.data);
        fclose(f);
    }

    *tempkeys = keys;
    *ntempkeys = nkeys;
}

static void verify_sorted(int a[], int n) {
    for (int i = 1; i < n; i++) 
        assert(a[i-1] <= a[i]);
}

struct merge_file {
    FILE *f;
    DBT key, val;
    BOOL row_valid;
};

static DBT zero_dbt;

static void merge_file_init(struct merge_file *mf) {
    mf->f = NULL;
    mf->key = zero_dbt; mf->key.flags = DB_DBT_REALLOC;
    mf->val = zero_dbt; mf->val.flags = DB_DBT_REALLOC;
    mf->row_valid = FALSE;
}

static void merge_file_destroy(struct merge_file *mf) {
    if (mf->f) {
        fclose(mf->f);
        mf->f = NULL;
    }
    toku_free(mf->key.data);
    toku_free(mf->val.data);
}

static char *merge(char **tempfiles, int ntempfiles, const char *testdir) {
    char fname[strlen(testdir) + 1 + strlen("result") + 1];
    sprintf(fname, "%s/%s", testdir, "result");
    FILE *mergef = fopen(fname, "w"); assert(mergef != NULL);

    struct merge_file f[ntempfiles];
    for (int i = 0; i < ntempfiles; i++) {
        merge_file_init(&f[i]);
        char tname[strlen(testdir) + 1 + strlen(tempfiles[i]) + 1];
        sprintf(tname, "%s/%s", testdir, tempfiles[i]);
        f[i].f = fopen(tname, "r"); assert(f[i].f != NULL);
        if (read_row(f[i].f, &f[i].key, &f[i].val) == 0)
            f[i].row_valid = TRUE;
    }

    while (1) {
        // get min 
        int mini = -1;
        for (int i = 0; i < ntempfiles; i++) {
            if (f[i].row_valid) {
                if (mini == -1) {
                    mini = i;
                } else {
                    int r = compare_int(NULL, &f[mini].key, &f[i].key);
                    assert(r != 0);
                    if (r > 0)
                        mini = i;
                }
            }
        }
        if (mini == -1)
            break;

        // write min
        write_row(mergef, &f[mini].key, &f[mini].val);

        // refresh mini
        if (read_row(f[mini].f, &f[mini].key, &f[mini].val) != 0)
            f[mini].row_valid = FALSE;
    }

    for (int i = 0; i < ntempfiles; i++) {
        merge_file_destroy(&f[i]);
    }

    fclose(mergef);
    return toku_strdup("result");
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
    for (int i = 0; i < ntempfiles; i++) {
        int *tempkeys; int ntempkeys;
        read_tempfile(testdir, tempfiles[i], &tempkeys, &ntempkeys);
        verify_sorted(tempkeys, ntempkeys);
        toku_free(tempkeys);
    }

    // merge
    char *result_file = merge(tempfiles, ntempfiles, testdir);
    assert(result_file);

    int *result_keys; int n_result_keys;
    read_tempfile(testdir, result_file, &result_keys, &n_result_keys);
    toku_free(result_file);

    // compare 
    assert(nkeys == n_result_keys);
    for (int i = 0; i < nkeys; i++) 
        assert(inkey[i] == result_keys[i]);

    toku_free(result_keys);
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
    DESCRIPTOR descriptors[N];
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
    char unlink_all[strlen(testdir)+20];
    snprintf(unlink_all, strlen(testdir)+20, "rm -rf %s", testdir);
    int r;
    r = system(unlink_all); CKERR(r);
    r = toku_os_mkdir(testdir, 0755); CKERR(r);

    if (ascending_keys + descending_keys + random_keys == 0)
        ascending_keys = 1;

    // run test
    test_extractor(nrows, nrowsets, testdir);

    return 0;
}

#if defined(__cplusplus)
}
#endif
