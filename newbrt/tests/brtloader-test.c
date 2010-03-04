#include "test.h"
#include <toku_assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "brtloader-internal.h"
#include "memory.h"

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

static void test_merge_internal (int a[], int na, int b[], int nb) {
    struct row *MALLOC_N(na, ar);
    struct row *MALLOC_N(nb, br);
    for (int i=0; i<na; i++) {
	ar[i].data = (void*)&a[i];
	ar[i].klen = sizeof(a[i]);
	ar[i].vlen = 0;
    }
    for (int i=0; i<nb; i++) {
	br[i].data = (void*)&b[i];
	br[i].klen = sizeof(b[i]);
	br[i].vlen = 0;
    }
    struct row *MALLOC_N(na+nb, cr);
    DB *dest_db = NULL;
    merge(cr, ar, na, br, nb, dest_db, compare_ints);
    int i=0;
    int j=0;
    for (int k=0; k<na+nb; k++) {
	int vc = *(int*)cr[k].data;
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
    toku_free(cr);
    toku_free(ar);
    toku_free(br);
}

/* Test the basic merger. */
static void test_merge (void) {
    {
	int avals[]={1,2,3,4,5};
	int *bvals = NULL; //icc won't let us use a zero-sized array explicitly or by [] = {} construction.
	test_merge_internal(avals, 5, bvals, 0);
	test_merge_internal(bvals, 0, avals, 5);
    }
    {
	int avals[]={1,3,5,7};
	int bvals[]={2,4};
	test_merge_internal(avals, 4, bvals, 2);
	test_merge_internal(bvals, 2, avals, 4);
    }
    {
	int avals[]={1,2,3,5,6,7};
	int bvals[]={2,4,5,6,8};
	test_merge_internal(avals, 6, bvals, 5);
	test_merge_internal(bvals, 5, avals, 6);
    }
}

static void test_internal_mergesort_row_array (int a[], int n) {
    struct row *MALLOC_N(n, ar);
    for (int i=0; i<n; i++) {
	ar[i].data = (void*)&a[i];
	ar[i].klen = sizeof(a[i]);
	ar[i].vlen = 0;
    }
    mergesort_row_array (ar, n, NULL, compare_ints);
    int *MALLOC_N(n, tmp);
    for (int i=0; i<n; i++) {
	tmp[i]=a[i];
    }
    qsort(tmp, n, sizeof(a[0]), qsort_compare_ints);
    for (int i=0; i<n; i++) {
	assert(tmp[i]==*(int*)ar[i].data);
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
    for (int i=0; i<100; i++) {
	int len=1+random()%100;
	int avals[len];
	for (int j=0; j<len; j++) {
	    avals[j] = random()%len;
	}
	test_internal_mergesort_row_array(avals, len);
    }
}

static void test_read_write_rows (char *template) {
    struct brtloader_s bl = {.panic              = 0,
			     .temp_file_template = template};
    int r;
    FILE *file;
    char *fname;
    r = brtloader_open_temp_file(&bl, &file, &fname);
    CKERR(r);

    FILE *idx;
    char *idxname;
    r = brtloader_open_temp_file(&bl, &idx, &idxname);
    CKERR(r);

    size_t dataoff=0;

    char *keystrings[] = {"abc", "b", "cefgh"};
    char *valstrings[] = {"defg", "", "xyz"};
    size_t actual_size=0;
    for (int i=0; i<3; i++) {
	DBT key = {.size=strlen(keystrings[i]), .data=keystrings[i]};
	DBT val = {.size=strlen(valstrings[i]), .data=valstrings[i]};
	r = loader_write_row(&key, &val, file, idx, &dataoff, &bl);
	CKERR(r);
	actual_size+=key.size + val.size + 8;
    }
    if (actual_size != dataoff) fprintf(stderr, "actual_size=%ld, dataoff=%ld\n", actual_size, dataoff);
    assert(actual_size == dataoff);

    r = fclose(file);
    CKERR(r);

    file = fopen(fname, "r");
    assert(file);

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
    r = fclose(file);
    CKERR(r);
    r = fclose(idx);
    CKERR(r);

    r = unlink(fname);
    CKERR(r);
    r = unlink(idxname);
    CKERR(r);

    toku_free(fname);
    toku_free(idxname);
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
		   .data=&vals[i]};
	add_row(rows, &key, &val);
    }
}

static void test_merge_files (char *template) {
    DB *dest_db = NULL;
    struct brtloader_s bl = {.panic              = 0,
			     .temp_file_template = template};
    int r;
    struct fileset fs;
    init_fileset(&fs);

    int a_keys[] = {   1,    3,    5,    7, 8, 9};
    int b_keys[] = {      2,    4,    6         };
    const char *a_vals[] = {"a", "c", "e", "g", "h", "i"};
    const char *b_vals[] = {"b", "d", "f"};
    struct rowset aset, bset;
    fill_rowset(&aset, a_keys, a_vals, 6);
    fill_rowset(&bset, b_keys, b_vals, 3);

    r = sort_and_write_rows(&aset, &fs, &bl, dest_db, compare_ints);  CKERR(r);
    r = sort_and_write_rows(&bset, &fs, &bl, dest_db, compare_ints);  CKERR(r);
    assert(fs.n_temp_files==2 && fs.n_temp_files_limit >= fs.n_temp_files);
    destroy_rowset(&aset);
    destroy_rowset(&bset);
    for (int i=0; i<2; i++) assert(fs.temp_data_names[i] != NULL && fs.temp_idx_names[i] != NULL);

    r = merge_files(&fs, &bl, dest_db, compare_ints); CKERR(r);

    assert(fs.n_temp_files==1);

    FILE *inf = fopen(fs.temp_data_names[0], "r");
    char *name = toku_strdup(template);
    int   fd  = mkstemp(name);
    fprintf(stderr, "Final data in %s\n", name);
    assert(r>=0);
    struct descriptor desc = {.version = 1, .dbt = (DBT){.size = 4, .data="abcd"}};
    r = write_file_to_dbfile(fd, inf, &bl, &desc);
    CKERR(r);
    r = fclose(inf);
    CKERR(r);
    r = unlink(fs.temp_data_names[0]);
    CKERR(r);
    r = unlink(fs.temp_idx_names[0]);
    CKERR(r);
    

    toku_free(fs.temp_data_names[0]);
    toku_free(fs.temp_idx_names[0]);
    toku_free(fs.temp_data_names);
    toku_free(fs.temp_idx_names);
    toku_free(name);
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
    int n = snprintf(template, templen, "%s/tempXXXXXX", directory);
    assert (n>0 && n<templen);

    test_read_write_rows(template);
    test_merge();
    test_mergesort_row_array();
    test_merge_files(template);
    
    return 0;
}
