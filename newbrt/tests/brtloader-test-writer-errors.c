/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: pqueue.c$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// test the loader write dbfile function

#define DONT_DEPRECATE_WRITES
#define DONT_DEPRECATE_MALLOC
#include "includes.h"
#include "test.h"
#include "brtloader-internal.h"

#if defined(__cplusplus)
extern "C" {
#endif


static int event_count, event_count_trigger;

static void reset_event_counts(void) {
    event_count = event_count_trigger = 0;
}

static void event_hit(void) {
}

static int loader_poll_callback(void *UU(extra), float UU(progress)) {
    int r;
    event_count++;
    if (event_count_trigger == event_count) {
        event_hit();
        r = 1;
    } else {
        r = 0;
    }
    return r;
}

static size_t bad_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    event_count++;
    size_t r;
    if (event_count_trigger == event_count) {
        event_hit();
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
    event_count++;
    if (event_count_trigger == event_count) {
        event_hit();
	errno = ENOSPC;
	r = -1;
    } else {
	r = write(fd, bp, len);
    }
    return r;
}

static ssize_t bad_pwrite(int fd, const void * bp, size_t len, toku_off_t off) {
    ssize_t r;
    event_count++;
    if (event_count_trigger == event_count) {
        event_hit();
	errno = ENOSPC;
	r = -1;
    } else {
	r = pwrite(fd, bp, len, off);
    }
    return r;
}

static int my_malloc_event = 0;
static int my_malloc_count = 0, my_big_malloc_count = 0;

static void reset_my_malloc_counts(void) {
    my_malloc_count = my_big_malloc_count = 0;
}

static void *my_malloc(size_t n) {
    void *caller = __builtin_return_address(0);
    if (!((void*)toku_malloc <= caller && caller <= (void*)toku_free))
        goto skip;
    my_malloc_count++;
    if (n >= 64*1024) {
        my_big_malloc_count++;
        if (my_malloc_event) {
            caller = __builtin_return_address(1);
            if ((void*)toku_xmalloc <= caller && caller <= (void*)toku_malloc_report)
                goto skip;
            event_count++;
            if (event_count == event_count_trigger) {
                event_hit();
                errno = ENOMEM;
                return NULL;
            }
        }
    }
 skip:
    return malloc(n);
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

static void write_dbfile (char *template, int n, char *output_name, BOOL expect_error) {
    if (verbose) printf("test start %d %d\n", n, expect_error);

    DB *dest_db = NULL;
    struct brtloader_s bl = {
        .panic              = 0,
        .temp_file_template = template,
        .reserved_memory = 512*1024*1024,
    };
    int r = brtloader_init_file_infos(&bl.file_infos);
    CKERR(r);
    struct merge_fileset fs;
    init_merge_fileset(&fs);

    // put rows in the row set
    struct rowset aset;
    init_rowset(&aset, toku_brtloader_get_rowset_budget_for_testing());
    for (int i=0; i<n; i++) {
	DBT key = {.size=sizeof i,
		   .data=&i};
	DBT val = {.size=sizeof i,
		   .data=&i};
	add_row(&aset, &key, &val);
    }

    toku_brt_loader_set_n_rows(&bl, n);

    brt_loader_init_error_callback(&bl.error_callback);
    brt_loader_set_error_function(&bl.error_callback, err_cb, NULL);
    brt_loader_init_poll_callback(&bl.poll_callback);
    r = brt_loader_sort_and_write_rows(&aset, &fs, &bl, 0, dest_db, compare_ints, 0);  CKERR(r);
    // destroy_rowset(&aset);

    QUEUE q;
    r = queue_create(&q, 0xFFFFFFFF); // infinite queue.
    assert(r==0);
    r = merge_files(&fs, &bl, 0, dest_db, compare_ints, 0, q); CKERR(r);
    assert(fs.n_temp_files==0);

    QUEUE q2;
    r = queue_create(&q2, 0xFFFFFFFF); // infinite queue.
    assert(r==0);

    size_t num_found = 0;
    while (1) {
	void *v;
	r = queue_deq(q, &v, NULL, NULL);
	if (r==EOF) break;
	struct rowset *rs = (struct rowset *)v;
	if (verbose) printf("v=%p\n", v);

	for (size_t i=num_found; i<rs->n_rows; i++) {
	    struct row *row = &rs->rows[i];
	    assert(row->klen==sizeof(int));
	    assert(row->vlen==sizeof(int));
	    assert((int)i==*(int*)(rs->data+row->off));
	}

	num_found += rs->n_rows;

	r = queue_enq(q2, v, 0, NULL);
	assert(r==0);
    }
    assert((int)num_found == n);

    r = queue_eof(q2);
    assert(r==0);

    r = queue_destroy(q);
    assert(r==0);

    struct descriptor desc = {.version = 1, .dbt = (DBT){.size = 4, .data="abcd"}};

    int fd = open(output_name, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);

    toku_set_func_malloc(my_malloc);
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);
    brt_loader_set_poll_function(&bl.poll_callback, loader_poll_callback, NULL);

    r = toku_loader_write_brt_from_q_in_C(&bl, &desc, fd, 1000, q2);
    // if (!(expect_error ? r != 0 : r == 0)) printf("WARNING%%d expect_error=%d r=%d\n", __LINE__, expect_error, r); 
    assert(expect_error ? r != 0 : r == 0);

    toku_set_func_malloc(NULL);
    brtloader_set_os_fwrite(NULL);
    toku_set_func_write(NULL);
    toku_set_func_pwrite(NULL);

    brt_loader_destroy_error_callback(&bl.error_callback);
    brt_loader_destroy_poll_callback(&bl.poll_callback);
    
    r = queue_destroy(q2);
    //if (r != 0) printf("WARNING%d r=%d\n", __LINE__, r);
    assert(r==0);
   
    destroy_merge_fileset(&fs);
    brtloader_fi_destroy(&bl.file_infos, expect_error);
}

static int usage(const char *progname, int n) {
    fprintf(stderr, "Usage:\n %s [-v] [-q] [-r %d] [-s] [-m] directory\n", progname, n);
    fprintf(stderr, "[-v] turn on verbose\n");
    fprintf(stderr, "[-q] turn off verbose\n");
    fprintf(stderr, "[-r %d] set the number of rows\n", n);
    fprintf(stderr, "[-s] set the small loader size factor\n");
    fprintf(stderr, "[-m] inject big malloc failures\n");
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
        } else if (strcmp(argv[0],"-m") == 0) {
            my_malloc_event = 1;
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
    write_dbfile(template, n, output_name, FALSE);

    if (verbose) printf("my_malloc_count=%d big_count=%d\n", my_malloc_count, my_big_malloc_count);

    int event_limit = event_count;
    if (verbose) printf("event_limit=%d\n", event_limit);

    for (int i = 1; i <= event_limit; i++) {
        reset_event_counts();
        reset_my_malloc_counts();
        event_count_trigger = i;
        r = system(unlink_all); CKERR(r);
        r = toku_os_mkdir(directory, 0755); CKERR(r);
        write_dbfile(template, n, output_name, TRUE);
    }

    return 0;
}

#if defined(__cplusplus)
}
#endif
