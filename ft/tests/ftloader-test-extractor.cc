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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// The purpose of this test is to test the extractor component of the ft loader.  We insert rowsets into the extractor queue and verify temp files 
// after the extractor is finished.

#define DONT_DEPRECATE_MALLOC
#define DONT_DEPRECATE_WRITES
#include "test.h"
#include "loader/loader.h"
#include "loader/loader-internal.h"
#include "memory.h"
#include <portability/toku_path.h>


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

static char **get_temp_files(const char *testdir) {
    int ntemp = 0;
    int maxtemp = 32;
    char **XMALLOC_N(maxtemp, tempfiles);

    DIR *d = opendir(testdir);
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (strncmp(de->d_name, "temp", 4) == 0) {
                if (ntemp >= maxtemp-1) {
                    maxtemp = 2*maxtemp;
                    XREALLOC_N(2*maxtemp, tempfiles);
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
    int *XCALLOC_N(maxkeys, keys);

    char fname[strlen(testdir) + 1 + strlen(tempfile) + 1];
    sprintf(fname, "%s/%s", testdir, tempfile);
    FILE *f = fopen(fname, "r");
    if (f) {
        DBT key;
        toku_init_dbt_flags(&key, DB_DBT_REALLOC);
        DBT val;
        toku_init_dbt_flags(&val, DB_DBT_REALLOC);
        while (read_row(f, &key, &val) == 0) {
            if (nkeys >= maxkeys) {
                maxkeys *= 2;
                XREALLOC_N(maxkeys, keys);
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
    bool row_valid;
};

static DBT zero_dbt;

static void merge_file_init(struct merge_file *mf) {
    mf->f = NULL;
    mf->key = zero_dbt; mf->key.flags = DB_DBT_REALLOC;
    mf->val = zero_dbt; mf->val.flags = DB_DBT_REALLOC;
    mf->row_valid = false;
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
        f[i].f = fopen(tname, "r"); 
	if (f[i].f == NULL) {
	    int error = errno;
	    fprintf(stderr, "%s:%d errno=%d %s\n", __FILE__, __LINE__, error, strerror(error));
	    if (error == EMFILE)
		fprintf(stderr, "may need to increase the nofile ulimit\n");
	}
	assert(f[i].f != NULL);
        if (read_row(f[i].f, &f[i].key, &f[i].val) == 0)
            f[i].row_valid = true;
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
            f[mini].row_valid = false;
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

static int generate(DB *dest_db, DB *src_db, DBT_ARRAY *dest_keys, DBT_ARRAY *dest_vals, const DBT *src_key, const DBT *src_val) {
    toku_dbt_array_resize(dest_keys, 1);
    toku_dbt_array_resize(dest_vals, 1);
    DBT *dest_key = &dest_keys->dbts[0];
    DBT *dest_val = &dest_vals->dbts[0];
    assert(dest_db == NULL); assert(src_db == NULL);

    copy_dbt(dest_key, src_key);
    copy_dbt(dest_val, src_val);
    
    return 0;
}

static void populate_rowset(struct rowset *rowset, int seq, int nrows, int keys[]) {
    for (int i = 0; i < nrows; i++) {
        int k = keys[i];
        int v = seq * nrows + i;
        DBT key;
        toku_fill_dbt(&key, &k, sizeof k);
        DBT val;
        toku_fill_dbt(&val, &v, sizeof v);
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
static int ascending_keys_poison = 0;
static int descending_keys = 0;
static int random_keys = 0;

static void test_extractor(int nrows, int nrowsets, const char *testdir) {
    if (verbose) printf("%s %d %d %s\n", __FUNCTION__, nrows, nrowsets, testdir);

    int r;

    int nkeys = nrows * nrowsets;
    int *XCALLOC_N(nkeys, keys);
    for (int i = 0; i < nkeys; i++)
        keys[i] = ascending_keys ? 2*i : nkeys - i;
    if (ascending_keys_poison) {
        if (verbose)
            printf("poison %d %d %d\n", nrows*(nrowsets-1), keys[nrows*(nrowsets-1)], keys[nrows-1] -1);
        keys[nrows*(nrowsets-1)] = keys[nrows-1] - 1;
    }
    if (random_keys)
        shuffle(keys, nkeys);

    // open the ft_loader. this runs the extractor.
    const int N = 1;
    FT_HANDLE fts[N];
    DB* dbs[N];
    const char *fnames[N];
    ft_compare_func compares[N];
    for (int i = 0; i < N; i++) {
        fts[i] = NULL;
        dbs[i] = NULL;
        fnames[i] = "";
        compares[i] = compare_int;
    }

    char temp[strlen(testdir) + 1 + strlen("tempXXXXXX") + 1];
    sprintf(temp, "%s/%s", testdir, "tempXXXXXX");

    FTLOADER loader;
    r = toku_ft_loader_open(&loader, NULL, generate, NULL, N, fts, dbs, fnames, compares, temp, ZERO_LSN, nullptr, true, 0, false, true);
    assert(r == 0);

    struct rowset *rowset[nrowsets];
    for (int i = 0 ; i < nrowsets; i++) {
        rowset[i] = (struct rowset *) toku_malloc(sizeof (struct rowset));
        assert(rowset[i]);
        init_rowset(rowset[i], toku_ft_loader_get_rowset_budget_for_testing());
        populate_rowset(rowset[i], i, nrows, &keys[i*nrows]);
    }

    // feed rowsets to the extractor
    for (int i = 0; i < nrowsets; i++) {
        r = toku_queue_enq(loader->primary_rowset_queue, rowset[i], 1, NULL);
        assert(r == 0);
    }
    r = toku_ft_loader_finish_extractor(loader);
    assert(r == 0);

    int error;
    r = toku_ft_loader_get_error(loader, &error);
    assert(r == 0);
    assert(error == 0);

    // sort the input keys
    qsort(keys, nkeys, sizeof (int), qsort_compare_ints);

    // verify the temp files
    verify(keys, nkeys, testdir);

    // abort the ft_loader.  this ends the test
    r = toku_ft_loader_abort(loader, true);
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
            toku_ft_loader_set_size_factor(1);
        } else if (strcmp(argv[0],"--asc") == 0) {
            ascending_keys = 1;
        } else if (strcmp(argv[0],"--dsc") == 0) {
            descending_keys = 1;
        } else if (strcmp(argv[0],"--random") == 0) {
            random_keys = 1;
        } else if (strcmp(argv[0], "--asc-poison") == 0) {
            ascending_keys = 1;
            ascending_keys_poison = 1;
	} else if (argc!=1) {
            return usage(progname);
	    exit(1);
	}
        else {
            break;
        }
	argc--; argv++;
    }

    const char *testdir = TOKU_TEST_FILENAME;
    char unlink_all[strlen(testdir)+20];
    snprintf(unlink_all, strlen(testdir)+20, "rm -rf %s", testdir);
    int r;
    r = system(unlink_all); CKERR(r);
    r = toku_os_mkdir(testdir, 0755); CKERR(r);

    if (ascending_keys + descending_keys + random_keys == 0)
        ascending_keys = 1;

    // run test
    test_extractor(nrows, nrowsets, testdir);

    r = system(unlink_all); CKERR(r);

    return 0;
}
