/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: pqueue.c$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"
#include <toku_assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "brtloader-internal.h"
#include "memory.h"

#if defined(__cplusplus)
extern "C" {
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

BOOL founddup;
static void expect_dups_cb(DB *db UU(), int dbn UU(), int err UU(), DBT *key UU(), DBT *val UU(), void *extra UU()) {
    founddup=TRUE;
}

static void test_merge_internal (int a[], int na, int b[], int nb, BOOL dups) {
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
    struct brtloader_s bl;
    brt_loader_init_error_callback(&bl);
    brt_loader_set_error_function(&bl, dups ? expect_dups_cb : err_cb, NULL);
    struct rowset rs = {.data=(char*)ab};
    merge_row_arrays_base(cr, ar, na, br, nb, 0, dest_db, compare_ints, &bl, &rs);
    brt_loader_call_error_function(&bl);
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
    brt_loader_destroy_error_callback(&bl);
}

/* Test the basic merger. */
static void test_merge (void) {
    {
	int avals[]={1,2,3,4,5};
	int *bvals = NULL; //icc won't let us use a zero-sized array explicitly or by [] = {} construction.
	test_merge_internal(avals, 5, bvals, 0, FALSE);
	test_merge_internal(bvals, 0, avals, 5, FALSE);
    }
    {
	int avals[]={1,3,5,7};
	int bvals[]={2,4};
	test_merge_internal(avals, 4, bvals, 2, FALSE);
	test_merge_internal(bvals, 2, avals, 4, FALSE);
    }
    {
	int avals[]={1,2,3,5,6,7};
	int bvals[]={2,4,5,6,8};
	test_merge_internal(avals, 6, bvals, 5, TRUE);
	test_merge_internal(bvals, 5, avals, 6, TRUE);
    }
}

static void test_internal_mergesort_row_array (int a[], int n) {
    struct row *MALLOC_N(n, ar);
    for (int i=0; i<n; i++) {
	ar[i].off  = i*sizeof(a[0]);
	ar[i].klen = sizeof(a[i]);
	ar[i].vlen = 0;
    }
    struct rowset rs = {.data=(char*)a};
    brt_loader_mergesort_row_array (ar, n, 0, NULL, compare_ints, NULL, &rs);
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
	BOOL used[MAX_VAL];
	for (int j=0; j<MAX_VAL; j++) used[j]=FALSE; 
	int len=1+random()%MAX_LEN;
	int avals[len];
	for (int j=0; j<len; j++) {
	    int v;
	    do {
		v = random()%MAX_VAL;
	    } while (used[v]); 
	    avals[j] = v;
	    used[v] = TRUE;
	}
	test_internal_mergesort_row_array(avals, len);
    }
}

static void test_read_write_rows (char *template) {
    struct brtloader_s bl = {.panic              = 0,
			     .temp_file_template = template};
    int r = brtloader_init_file_infos(&bl.file_infos);
    CKERR(r);
    FIDX file;
    r = brtloader_open_temp_file(&bl, &file);
    CKERR(r);

    u_int64_t dataoff=0;

    char *keystrings[] = {"abc", "b", "cefgh"};
    char *valstrings[] = {"defg", "", "xyz"};
    u_int64_t actual_size=0;
    for (int i=0; i<3; i++) {
	DBT key = {.size=strlen(keystrings[i]), .data=keystrings[i]};
	DBT val = {.size=strlen(valstrings[i]), .data=valstrings[i]};
	r = loader_write_row(&key, &val, file, &dataoff, &bl);
	CKERR(r);
	actual_size+=key.size + val.size + 8;
    }
    if (actual_size != dataoff) fprintf(stderr, "actual_size=%"PRIu64", dataoff=%"PRIu64"\n", actual_size, dataoff);
    assert(actual_size == dataoff);

    r = brtloader_fi_close(&bl.file_infos, file);
    CKERR(r);

    r = brtloader_fi_reopen(&bl.file_infos, file, "r");
    CKERR(r);

    {
	int n_read=0;
	DBT key={.size=0}, val={.size=0};
	while (0==loader_read_row(file, &key, &val, &bl)) {
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
    r = brtloader_fi_close(&bl.file_infos, file);
    CKERR(r);

    r = brtloader_fi_unlink(&bl.file_infos, file);
    CKERR(r);

    assert(bl.file_infos.n_files_open==0);
    assert(bl.file_infos.n_files_extant==0);

    brtloader_fi_destroy(&bl.file_infos, FALSE);
}

static void fill_rowset (struct rowset *rows,
			 int keys[],
			 const char *vals[],
			 int n) {
    init_rowset(rows);
    for (int i=0; i<n; i++) {
	DBT key = {.size=sizeof(keys[i]),
		   .data=&keys[i]};
	DBT val = {.size=strlen(vals[i]),
		   .data=(void *)vals[i]};
	add_row(rows, &key, &val);
    }
}

static void verify_dbfile(int n, int sorted_keys[], const char *sorted_vals[], const char *name) {
    int r;

    CACHETABLE ct;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER); assert(r==0);

    TOKUTXN const null_txn = NULL;
    BRT t = NULL;
    r = toku_brt_create(&t); assert(r == 0);
    r = toku_brt_set_bt_compare(t, compare_ints); assert(r == 0);
    r = toku_brt_open(t, name, 0, 0, ct, null_txn, 0); assert(r==0);

    BRT_CURSOR cursor = NULL;
    r = toku_brt_cursor(t, &cursor, NULL, TXNID_NONE, FALSE); assert(r == 0);

    int i;
    for (i=0; i<n; i++) {
	struct check_pair pair = {sizeof sorted_keys[i], &sorted_keys[i], strlen(sorted_vals[i]), sorted_vals[i], 0};
        r = toku_brt_cursor_get(cursor, NULL, NULL, lookup_checkf, &pair, DB_NEXT);
        if (r != 0) {
	    assert(pair.call_count ==0);
	    break;
	}
	assert(pair.call_count==1);
    }
    
    struct check_pair pair; memset(&pair, 0, sizeof pair);
    r = toku_brt_cursor_get(cursor, NULL, NULL, lookup_checkf, &pair, DB_NEXT);
    assert(r != 0);

    r = toku_brt_cursor_close(cursor); assert(r == 0);
    r = toku_close_brt(t, 0); assert(r==0);
    r = toku_cachetable_close(&ct);assert(r==0);
}

    static void test_merge_files (const char *template, const char *output_name) {
    DB *dest_db = NULL;
    struct brtloader_s bl = {.panic              = 0,
			     .temp_file_template = template};
    int r = brtloader_init_file_infos(&bl.file_infos);
    CKERR(r);
    struct merge_fileset fs;
    init_merge_fileset(&fs);

    int a_keys[] = {   1,    3,    5,    7, 8, 9};
    int b_keys[] = { 0,   2,    4,    6         };
    const char *a_vals[] = {"a", "c", "e", "g", "h", "i"};
    const char *b_vals[] = {"0", "b", "d", "f"};
    int sorted_keys[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const char *sorted_vals[] = { "0", "a", "b", "c", "d", "e", "f", "g", "h", "i" };
    struct rowset aset, bset;
    fill_rowset(&aset, a_keys, a_vals, 6);
    fill_rowset(&bset, b_keys, b_vals, 4);
    toku_brt_loader_set_n_rows(&bl, 6+3);

    brt_loader_set_error_function(&bl, err_cb, NULL);
    r = brt_loader_sort_and_write_rows(&aset, &fs, &bl, 0, dest_db, compare_ints, 0);  CKERR(r);
    r = brt_loader_sort_and_write_rows(&bset, &fs, &bl, 0, dest_db, compare_ints, 0);  CKERR(r);
    assert(fs.n_temp_files==2 && fs.n_temp_files_limit >= fs.n_temp_files);
    destroy_rowset(&aset);
    destroy_rowset(&bset);
    for (int i=0; i<2; i++) assert(fs.data_fidxs[i].idx != -1);

    QUEUE q;
    r = queue_create(&q, 0xFFFFFFFF); // infinite queue.
    assert(r==0);

    r = merge_files(&fs, &bl, 0, dest_db, compare_ints, 0, q); CKERR(r);

    assert(fs.n_temp_files==0);

    struct descriptor desc = {.version = 1, .dbt = (DBT){.size = 4, .data="abcd"}};

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    
    r = toku_loader_write_brt_from_q_in_C(&bl, &desc, fd, 1000, q);
    assert(r==0);

    destroy_merge_fileset(&fs);
    brtloader_fi_destroy(&bl.file_infos, FALSE);

    // verify the dbfile
    verify_dbfile(10, sorted_keys, sorted_vals, output_name);

    r = queue_destroy(q);
    assert(r==0);
}

/* Test to see if we can open temporary files. */
int test_main (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
	} else if (argc!=1) {
	    fprintf(stderr, "Usage:\n %s [-v] [-q] directory\n", progname);
	    exit(1);
	}
        else {
            break;
        }
	argc--; argv++;
    }
    assert(argc==1); // argv[1] is the directory in which to do the test.
    const char* directory = argv[0];
    int r = toku_os_mkdir(directory, 0755);
    if (r!=0) CKERR2(errno, EEXIST);

    int  templen = strlen(directory)+15;
    char template[templen];
    {
	int n = snprintf(template, templen, "%s/tempXXXXXX", directory);
	assert (n>0 && n<templen);
    }
    char output_name[templen];
    {
	int n = snprintf(output_name, templen, "%s/data.tokudb", directory);
	assert (n>0 && n<templen);
    }
    test_read_write_rows(template);
    test_merge();
    test_mergesort_row_array();
    test_merge_files(template, output_name);
    
    return 0;
}

#if defined(__cplusplus)
};
#endif
