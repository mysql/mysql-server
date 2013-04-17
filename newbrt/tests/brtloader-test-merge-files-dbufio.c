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

static char *errorstr_static (int err) {
    static char errorstr[100];
    toku_brt_strerror_r(err, errorstr, sizeof(errorstr));
    return errorstr;
}
		       

static void err_cb(DB *db UU(), int dbn, int err, DBT *key UU(), DBT *val UU(), void *extra UU()) {
    fprintf(stderr, "error in test dbn=%d err=%d (%s)\n", dbn, err, errorstr_static(err));
    abort();
}

enum { N_SOURCES = 2, N_RECORDS=10, N_DEST_DBS=1 };

static char *make_fname(const char *directory, const char *fname, int idx) {
    int len = strlen(directory)+strlen(fname)+20;
    char *XMALLOC_N(len, result);
    int r = snprintf(result, len, "%s/%s%d", directory, fname, idx);
    assert(r<len);
    return result; // don't care that it's a little too long.
}



static void test (const char *directory) {

    int *XMALLOC_N(N_SOURCES, fds);

    char **XMALLOC_N(N_SOURCES, fnames);
    for (int i=0; i<N_SOURCES; i++) {
	fnames[i] = make_fname(directory, "temp", i);
	fds[i] = open(fnames[i], O_CREAT|O_RDWR, S_IRWXU);
	assert(fds[i]>=0);
    }
    for (int i=0; i<N_RECORDS; i++) {
	int size=4;
	int fdi = random()%N_SOURCES;
	int fd  = fds[fdi];
	{ int r = write(fd, &size, 4);  assert(r==4); }
	{ int r = write(fd, &i,    4);  assert(r==4); }
	{ int r = write(fd, &size, 4);  assert(r==4); }
	{ int r = write(fd, &i,    4);  assert(r==4); }
    }
    for (int i=0; i<N_SOURCES; i++) {
	off_t r = lseek(fds[i], 0, SEEK_SET);
	assert(r==0);
    }

    toku_set_func_malloc(my_malloc);
    brtloader_set_os_fwrite(bad_fwrite);
    toku_set_func_write(bad_write);
    toku_set_func_pwrite(bad_pwrite);

    BRTLOADER bl;
    DB **XMALLOC_N(N_DEST_DBS, dbs);
    const struct descriptor **XMALLOC_N(N_DEST_DBS, descriptors);
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
					       N_DEST_DBS, dbs,
					       descriptors,
					       new_fnames_in_env,
					       bt_compare_functions,
					       "tempxxxxxx",
					       *lsnp);
	assert(r==0);
    }

    brt_loader_init_error_callback(&bl->error_callback);
    brt_loader_set_error_function(&bl->error_callback, err_cb, NULL);
    brt_loader_init_poll_callback(&bl->poll_callback);

    brt_loader_set_poll_function(&bl->poll_callback, loader_poll_callback, NULL);

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
    bl->file_infos.n_files_extant = N_SOURCES;
    XREALLOC_N(bl->file_infos.n_files_limit, bl->file_infos.file_infos);
    const int BUFFER_SIZE = 100;
    for (int i=0; i<N_SOURCES; i++) {
	bl->file_infos.file_infos[i] = (struct file_info){ .is_open   = FALSE,
							   .is_extant = TRUE,
							   .fname     = toku_strdup(fnames[i]),
							   .file      = (FILE*)NULL,
							   .n_rows    = N_RECORDS,
							   .buffer_size = BUFFER_SIZE,
							   .buffer      = toku_xmalloc(BUFFER_SIZE)}; 
	src_fidxs[i].idx = i;
    }
    {
	int r = toku_merge_some_files_using_dbufio(TRUE, FIDX_NULL, q, N_SOURCES, bfs, src_fidxs, bl, 0, (DB*)NULL, compare_ints, 10000);
	if (r!=0) printf("%s:%d r=%d (%s)\n", __FILE__, __LINE__, r, errorstr_static(r));
	assert(r==0);
    }
    {
	int r = toku_brtloader_destroy(bl);
	assert(r==0);
    }
    {
	int r = toku_cachetable_close(&ct);
	assert(r==0);
    }
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
    test(directory);

    if (verbose) printf("my_malloc_count=%d big_count=%d\n", my_malloc_count, my_big_malloc_count);

    if (0) {
    int event_limit = event_count;
    if (verbose) printf("event_limit=%d\n", event_limit);

    for (int i = 1; i <= event_limit; i++) {
        reset_event_counts();
        reset_my_malloc_counts();
        event_count_trigger = i;
        r = system(unlink_all); CKERR(r);
        r = toku_os_mkdir(directory, 0755); CKERR(r);

	test(directory);
    }
    }

    return 0;
}

C_END
