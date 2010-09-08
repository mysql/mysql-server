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

C_BEGIN

static int event_count, event_count_trigger;

static void my_assert_hook (void) {
    fprintf(stderr, "event_count=%d\n", event_count);
}

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
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
        r = TOKUDB_CANCELED;
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
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
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
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
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
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
	errno = ENOSPC;
	r = -1;
    } else {
	r = pwrite(fd, bp, len, off);
    }
    return r;
}

static FILE * 
bad_fdopen(int fd, const char * mode) {
    FILE * rval;
    event_count++;
    if (event_count_trigger == event_count) {
	event_hit();
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
	errno = EINVAL;
	rval  = NULL;
    } else {
	rval = fdopen(fd, mode);
    }
    return rval;
}

static FILE * 
bad_fopen(const char *filename, const char *mode) {
    FILE * rval;
    event_count++;
    if (event_count_trigger == event_count) {
	event_hit();
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
	errno = EINVAL;
	rval  = NULL;
    } else {
	rval = fopen(filename, mode);
    }
    return rval;
}


static int
bad_open(const char *path, int oflag, int mode) {
    int rval;
    event_count++;
    if (event_count_trigger == event_count) {
	event_hit();
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
	errno = EINVAL;
	rval = -1;
    } else {
	rval = open(path, oflag, mode);
    }
    return rval;
}



static int
bad_fclose(FILE * stream) {
    int rval;
    event_count++;
    // Must close the stream even in the "error case" because otherwise there is no way to get the memory back.
    rval = fclose(stream);
    if (rval==0) {
	if (event_count_trigger == event_count) {
            if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
	    errno = ENOSPC;
	    rval = -1;
	}
    }
    return rval;
}

int bad_read_errno = 0;

static ssize_t
bad_read(int fd, void *buf, size_t count) {
    ssize_t rval;
    event_count++;
    if (event_count_trigger == event_count) {
        event_hit();
        if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
        errno = bad_read_errno;
        rval = -1;
    } else
        rval = read(fd, buf, count);
    return rval;
}

static int my_malloc_event = 1;
static int my_malloc_count = 0, my_big_malloc_count = 0;
static int my_realloc_count = 0, my_big_realloc_count = 0;

static void reset_my_malloc_counts(void) {
    my_malloc_count = my_big_malloc_count = 0;
    my_realloc_count = my_big_realloc_count = 0;
}

size_t min_malloc_error_size = 0;

static void *my_malloc(size_t n) {
    void *caller = __builtin_return_address(0);
    if (!((void*)toku_malloc <= caller && caller <= (void*)toku_free))
        goto skip;
    my_malloc_count++;
    if (n >= min_malloc_error_size) {
        my_big_malloc_count++;
        if (my_malloc_event) {
            caller = __builtin_return_address(1);
            if ((void*)toku_xmalloc <= caller && caller <= (void*)toku_set_func_malloc)
                goto skip;
            event_count++;
            if (event_count == event_count_trigger) {
                event_hit();
                if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
                errno = ENOMEM;
                return NULL;
            }
        }
    }
 skip:
    return malloc(n);
}

static int do_realloc_errors = 1;

static void *my_realloc(void *p, size_t n) {
    void *caller = __builtin_return_address(0);
    if (!((void*)toku_realloc <= caller && caller <= (void*)toku_free))
        goto skip;
    my_realloc_count++;
    if (n >= min_malloc_error_size) {
        my_big_realloc_count++;
        if (do_realloc_errors) {
            caller = __builtin_return_address(1);
            if ((void*)toku_xrealloc <= caller && caller <= (void*)toku_set_func_malloc)
                goto skip;
            event_count++;
            if (event_count == event_count_trigger) {
                event_hit();
                if (verbose) printf("%s %d\n", __FUNCTION__, event_count);
                errno = ENOMEM;
                return NULL;
            }
        }
    }
 skip:
    return realloc(p, n);
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

static char *errorstr_static (int err) {
    static char errorstr[100];
    toku_brt_strerror_r(err, errorstr, sizeof(errorstr));
    return errorstr;
}
		       

static void err_cb(DB *db UU(), int dbn, int err, DBT *key UU(), DBT *val UU(), void *extra UU()) {
    fprintf(stderr, "error in test dbn=%d err=%d (%s)\n", dbn, err, errorstr_static(err));
    abort();
}

enum { N_SOURCES = 2, N_DEST_DBS=1 };

int N_RECORDS = 10;

static char *make_fname(const char *directory, const char *fname, int idx) {
    int len = strlen(directory)+strlen(fname)+20;
    char *XMALLOC_N(len, result);
    int r = snprintf(result, len, "%s/%s%d", directory, fname, idx);
    assert(r<len);
    return result; // don't care that it's a little too long.
}


struct consumer_thunk {
    QUEUE q;
    int64_t n_read;
};

static void *consumer_thread (void *ctv) {
    struct consumer_thunk *cthunk = (struct consumer_thunk *)ctv;
    while (1) {
	void *item;
	int r = queue_deq(cthunk->q, &item, NULL, NULL);
	if (r==EOF) return NULL;
	assert(r==0);
	struct rowset *rowset = (struct rowset *)item;
	cthunk->n_read += rowset->n_rows;
	destroy_rowset(rowset);
	toku_free(rowset);
    }
}


static void test (const char *directory, BOOL is_error) {

    int *XMALLOC_N(N_SOURCES, fds);

    char **XMALLOC_N(N_SOURCES, fnames);
    int *XMALLOC_N(N_SOURCES, n_records_in_fd);
    for (int i=0; i<N_SOURCES; i++) {
	fnames[i] = make_fname(directory, "temp", i);
	fds[i] = open(fnames[i], O_CREAT|O_RDWR, S_IRWXU);
	assert(fds[i]>=0);
	n_records_in_fd[i] = 0;
    }
    for (int i=0; i<N_RECORDS; i++) {
	int size=4;
	int fdi = random()%N_SOURCES;
	int fd  = fds[fdi];
	{ int r = write(fd, &size, 4);  assert(r==4); }
	{ int r = write(fd, &i,    4);  assert(r==4); }
	{ int r = write(fd, &size, 4);  assert(r==4); }
	{ int r = write(fd, &i,    4);  assert(r==4); }
	n_records_in_fd[fdi]++;
    }
    for (int i=0; i<N_SOURCES; i++) {
	toku_off_t r = lseek(fds[i], 0, SEEK_SET);
	assert(r==0);
    }

    BRTLOADER bl;
    BRT *XCALLOC_N(N_DEST_DBS, brts);
    const char **XMALLOC_N(N_DEST_DBS, new_fnames_in_env);
    for (int i=0; i<N_DEST_DBS; i++) {
	char s[100];
	snprintf(s, sizeof(s), "db%d.db", i);
	new_fnames_in_env[i] = toku_strdup(s);
	assert(new_fnames_in_env[i]);
    }
    brt_compare_func *XMALLOC_N(N_DEST_DBS, bt_compare_functions);
    bt_compare_functions[0] = compare_ints;
    CACHETABLE ct;
    enum {CACHETABLE_SIZE = 64*1024};
    {
	int r = toku_create_cachetable(&ct, CACHETABLE_SIZE, (LSN){1}, NULL);
	assert(r==0);
    }
    LSN *XMALLOC(lsnp);
    {
	int r = toku_brt_loader_internal_init (&bl,
					       ct,
					       (generate_row_for_put_func)NULL,
					       (DB*)NULL,
					       N_DEST_DBS, brts,
					       new_fnames_in_env,
					       bt_compare_functions,
					       "tempxxxxxx",
					       *lsnp,
                                               TXNID_NONE);
	assert(r==0);
    }

    brt_loader_init_error_callback(&bl->error_callback);
    brt_loader_set_error_function(&bl->error_callback, err_cb, NULL);
    brt_loader_init_poll_callback(&bl->poll_callback);
    brt_loader_set_poll_function(&bl->poll_callback, loader_poll_callback, NULL);
    brt_loader_set_fractal_workers_count_from_c(bl);

    QUEUE q;
    { int r = queue_create(&q, 1000); assert(r==0); }
    DBUFIO_FILESET bfs;
    const int MERGE_BUF_SIZE = 100000; // bigger than 64K so that we will trigger malloc issues.
    { int r = create_dbufio_fileset(&bfs, N_SOURCES, fds, MERGE_BUF_SIZE);  assert(r==0); }
    FIDX *XMALLOC_N(N_SOURCES, src_fidxs);
    assert(bl->file_infos.n_files==0);
    bl->file_infos.n_files = N_SOURCES;
    bl->file_infos.n_files_limit = N_SOURCES;
    bl->file_infos.n_files_open  = 0;
    bl->file_infos.n_files_extant = 0;
    XREALLOC_N(bl->file_infos.n_files_limit, bl->file_infos.file_infos);
    for (int i=0; i<N_SOURCES; i++) {
	// all we really need is the number of records in the file.  The rest of the file_info is unused by the dbufio code.n
	bl->file_infos.file_infos[i].n_rows = n_records_in_fd[i];
	// However we need these for the destroy method to work right.
	bl->file_infos.file_infos[i].is_extant = FALSE;
	bl->file_infos.file_infos[i].is_open   = FALSE;
	bl->file_infos.file_infos[i].buffer    = NULL;
	src_fidxs[i].idx = i;
    }
    toku_pthread_t consumer;
    struct consumer_thunk cthunk = {q, 0};
    {
	int r = toku_pthread_create(&consumer, NULL, consumer_thread, (void*)&cthunk);
	assert(r==0);
    }

    toku_set_func_malloc(my_malloc);
    toku_set_func_realloc(my_realloc);
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);
    toku_set_func_fdopen(bad_fdopen);
    toku_set_func_fopen(bad_fopen);
    toku_set_func_open(bad_open);
    toku_set_func_fclose(bad_fclose);
    if (bad_read_errno) toku_set_func_read(bad_read);

    int result = 0;
    {
	int r = toku_merge_some_files_using_dbufio(TRUE, FIDX_NULL, q, N_SOURCES, bfs, src_fidxs, bl, 0, (DB*)NULL, compare_ints, 10000);
	if (is_error && r!=0) {
	    result = r;
	} else {
	    if (r!=0) printf("%s:%d r=%d (%s)\n", __FILE__, __LINE__, r, errorstr_static(r));
	    assert(r==0);
	}
        if (r)
            panic_dbufio_fileset(bfs, r);
    }
    {
	int r = queue_eof(q);
	assert(r==0);
    }

    toku_set_func_malloc(NULL);
    toku_set_func_realloc(NULL);
    brtloader_set_os_fwrite(NULL);
    toku_set_func_write(NULL);
    toku_set_func_pwrite(NULL);
    toku_set_func_fdopen(NULL);
    toku_set_func_fopen(NULL);
    toku_set_func_open(NULL);
    toku_set_func_fclose(NULL);
    toku_set_func_read(NULL);
    do_assert_hook = my_assert_hook;

    {
	void *vresult;
	int r = toku_pthread_join(consumer, &vresult);
	assert(r==0);
	assert(vresult==NULL);
	//printf("n_read = %ld, N_SOURCES=%d N_RECORDS=%d\n", cthunk.n_read, N_SOURCES, N_RECORDS);
	if (result==0) {
	    assert(cthunk.n_read == N_RECORDS);
	}
    }
    //printf("%s:%d Destroying\n", __FILE__, __LINE__);
    {
	int r = queue_destroy(bl->primary_rowset_queue);
	assert(r==0);
    }
    {
	int r = queue_destroy(q);
	assert(r==0);
    }
    toku_brtloader_internal_destroy(bl, FALSE);
    {
	int r = toku_cachetable_close(&ct);
	assert(r==0);
    }
    for (int i=0; i<N_DEST_DBS; i++) {
	toku_free((void*)new_fnames_in_env[i]);
    }
    for (int i=0; i<N_SOURCES; i++) {
	toku_free(fnames[i]);
    }
    destroy_dbufio_fileset(bfs);
    toku_free(fnames);
    toku_free(fds);
    toku_free(brts);
    toku_free(new_fnames_in_env);
    toku_free(bt_compare_functions);
    toku_free(lsnp);
    toku_free(src_fidxs);
    toku_free(n_records_in_fd);
}


static int usage(const char *progname, int n) {
    fprintf(stderr, "Usage:\n %s [-v] [-q] [-r %d] [-s] [-m] [-tend NEVENTS] directory\n", progname, n);
    fprintf(stderr, "[-v] turn on verbose\n");
    fprintf(stderr, "[-q] turn off verbose\n");
    fprintf(stderr, "[-r %d] set the number of rows\n", n);
    fprintf(stderr, "[-s] set the small loader size factor\n");
    fprintf(stderr, "[-m] inject big malloc failures\n");
    fprintf(stderr, "[-tend NEVENTS] stop testing after N events\n");
    fprintf(stderr, "[-bad_read_errno ERRNO]\n");
    return 1;
}

int test_main (int argc, const char *argv[]) {
    int tstart = 0;
    int tend = -1;
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0],"-h")==0) {
            return usage(progname, N_RECORDS);
	} else if (strcmp(argv[0],"-v")==0) {
	    verbose=1;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose=0;
        } else if (strcmp(argv[0],"-r") == 0) {
            argc--; argv++;
            N_RECORDS = atoi(argv[0]);
        } else if (strcmp(argv[0],"-s") == 0) {
            toku_brtloader_set_size_factor(1);
        } else if (strcmp(argv[0],"-m") == 0) {
            my_malloc_event = 1;
	} else if (strcmp(argv[0],"-tend") == 0 && argc > 1) {
            argc--; argv++;
	    tend = atoi(argv[0]);
	} else if (strcmp(argv[0],"-tstart") == 0 && argc > 1) {
            argc--; argv++;
	    tstart = atoi(argv[0]);
        } else if (strcmp(argv[0], "-bad_read_errno") == 0 && argc > 1) {
            argc--; argv++;
            bad_read_errno = atoi(argv[0]);
	} else if (argc!=1) {
            return usage(progname, N_RECORDS);
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
    test(directory, FALSE);

    if (verbose) printf("my_malloc_count=%d big_count=%d\n", my_malloc_count, my_big_malloc_count);

    {
	int event_limit = event_count;
	if (tend>=0 && tend<event_limit) event_limit=tend;
	if (verbose) printf("event_limit=%d\n", event_limit);

	for (int i = tstart+1; i <= event_limit; i++) {
	    reset_event_counts();
	    reset_my_malloc_counts();
	    event_count_trigger = i;
	    r = system(unlink_all); CKERR(r);
	    r = toku_os_mkdir(directory, 0755); CKERR(r);
	    if (verbose) printf("event=%d\n", i);
	    test(directory, TRUE);
	}
	r = system(unlink_all); CKERR(r);
    }

    return 0;
}

C_END
