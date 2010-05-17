/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef _TOKU_BRTLOADER_INTERNAL_H
#define _TOKU_BRTLOADER_INTERNAL_H
#ident "$Id: pqueue.c$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include <db.h>
#include "brttypes.h"
#include "brtloader.h"
#include "queue.h"
#include "toku_pthread.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* These functions are exported to allow the tests to compile. */

/* These structures maintain a collection of all the open temporary files used by the loader. */
struct file_info {
    BOOL is_open;
    BOOL is_extant; // if true, the file must be unlinked.
    char *fname;
    FILE *file;
    u_int64_t n_rows; // how many rows were written into that file
    size_t buffer_size;
    void *buffer;
};
struct file_infos {
    int n_files;
    int n_files_limit;
    struct file_info *file_infos;
    int n_files_open, n_files_extant;
    toku_pthread_mutex_t lock; // must protect this data structure because current activity performs a REALLOC(fi->file_infos).
};
typedef struct fidx { int idx; } FIDX;
static const FIDX FIDX_NULL __attribute__((__unused__)) = {-1};
static int fidx_is_null (const FIDX f) __attribute__((__unused__));
static int fidx_is_null (const FIDX f) { return f.idx==-1; }
FILE *toku_bl_fidx2file (BRTLOADER bl, FIDX i);

int brtloader_open_temp_file (BRTLOADER bl, FIDX*file_idx);

/* These data structures are used for manipulating a collection of rows in main memory. */
struct row {
    size_t off; // the offset in the data array.
    int   klen,vlen;
};
struct rowset {
    uint64_t memory_budget;
    size_t n_rows, n_rows_limit;
    struct row *rows;
    size_t n_bytes, n_bytes_limit;
    char *data;
};

int init_rowset (struct rowset *rows, uint64_t memory_budget);
void destroy_rowset (struct rowset *rows);
void add_row (struct rowset *rows, DBT *key, DBT *val);

int loader_write_row(DBT *key, DBT *val, FIDX data, FILE*, u_int64_t *dataoff, BRTLOADER bl);
int loader_read_row (FILE *f, DBT *key, DBT *val);

struct merge_fileset {
    int n_temp_files, n_temp_files_limit;
    FIDX *data_fidxs;
};

void init_merge_fileset (struct merge_fileset *fs);
void destroy_merge_fileset (struct merge_fileset *fs);

struct poll_callback_s {
    brt_loader_poll_func poll_function;
    void *poll_extra;
};
typedef struct poll_callback_s *brtloader_poll_callback;

int brt_loader_init_poll_callback(brtloader_poll_callback);

void brt_loader_destroy_poll_callback(brtloader_poll_callback);

void brt_loader_set_poll_function(brtloader_poll_callback, brt_loader_poll_func poll_function, void *poll_extra);

int brt_loader_call_poll_function(brtloader_poll_callback, float progress);

struct error_callback_s {
    brt_loader_error_func error_callback;
    void *extra;
    int did_callback;
    int error;
    DB *db;
    int which_db;
    DBT key;
    DBT val;
    toku_pthread_mutex_t mutex;
};
typedef struct error_callback_s *brtloader_error_callback;

int brt_loader_init_error_callback(brtloader_error_callback);

void brt_loader_destroy_error_callback(brtloader_error_callback);

int brt_loader_get_error(brtloader_error_callback);

void brt_loader_set_error_function(brtloader_error_callback, brt_loader_error_func error_function, void *extra);

int brt_loader_set_error(brtloader_error_callback, int error, DB *db, int which_db, DBT *key, DBT *val);

int brt_loader_call_error_function(brtloader_error_callback);

int brt_loader_set_error_and_callback(brtloader_error_callback, int error, DB *db, int which_db, DBT *key, DBT *val);

struct brtloader_s {
    BOOL panic;
    int panic_errno;
    
    generate_row_for_put_func generate_row_for_put;
    brt_compare_func *bt_compare_funs;

    DB *src_db;
    int N;
    DB **dbs;
    const struct descriptor **descriptors; // N of these
    const char **new_fnames_in_env; // the file names that the final data will be written to (relative to env).

    struct rowset primary_rowset; // the primary rows that have been put, but the secondary rows haven't been generated.
    struct rowset primary_rowset_temp; // the primary rows that are being worked on by the extractor_thread.

    QUEUE primary_rowset_queue; // main thread enqueues rowsets in this queue (in maybe 64MB chunks).  The extractor thread removes them, sorts them, adn writes to file.
    toku_pthread_t     extractor_thread;     // the thread that takes primary rowset and does extraction and the first level sort and write to file.
    BOOL extractor_live;

    struct rowset *rows; // secondary rows that have been put, but haven't been sorted and written to a file.
    u_int64_t n_rows; // how many rows have been put?
    struct merge_fileset *fs;

    const char *temp_file_template;

    CACHETABLE cachetable;
    uint64_t   reserved_memory; // how much memory are we allowed to use?

    /* To make it easier to recover from errors, we don't use FILE*, instead we use an index into the file_infos. */
    struct file_infos file_infos;

#define PROGRESS_MAX (1<<16)
    int progress;       // Progress runs from 0 to PROGRESS_MAX.  When we call the poll function we convert to a float from 0.0 to 1.0
    // We use an integer so that we can add to the progress using a fetch-and-add instruction.

    // These two are set in the close function, and used while running close
    struct error_callback_s error_callback;
    struct poll_callback_s poll_callback;

    int user_said_stop; // 0 if the poll_function always returned zero.  If it ever returns nonzero, then store that value here.
    LSN load_lsn; //LSN of the fsynced 'load' log entry.  Write this LSN (as checkpoint_lsn) in brt headers made by this loader.

    QUEUE *fractal_queues; // an array of work queues, one for each secondary index.
    toku_pthread_t *fractal_threads;
    BOOL *fractal_threads_live; // an array of bools indicating that fractal_threads[i] is a live thread.  (There is no NULL for a pthread_t, so we have to maintain this separately).

    toku_pthread_mutex_t mutex;
};

// Set the number of rows in the loader.  Used for test.
void toku_brt_loader_set_n_rows(BRTLOADER bl, u_int64_t n_rows);

// Get the number of rows in the loader.  Used for test.
u_int64_t toku_brt_loader_get_n_rows(BRTLOADER bl);

// The data passed into a fractal_thread via pthread_create.
struct fractal_thread_args {
    BRTLOADER bl;
    const struct descriptor *descriptor;
    int fd; // write the brt into tfd.
    int progress_allocation;
    QUEUE q;
    int errno_result; // the final result.
};

void toku_brt_loader_set_n_rows(BRTLOADER bl, u_int64_t n_rows);
u_int64_t toku_brt_loader_get_n_rows(BRTLOADER bl);

int merge_row_arrays_base (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
                           int which_db, DB *dest_db, brt_compare_func,
			   BRTLOADER,
                           struct rowset *);

int merge_files (struct merge_fileset *fs, BRTLOADER bl, int which_db, DB *dest_db, brt_compare_func, int progress_allocation, QUEUE);

#if defined(__cilkplusplus)
extern "Cilk++" {
#endif
int sort_and_write_rows (struct rowset rows, struct merge_fileset *fs, BRTLOADER bl, int which_db, DB *dest_db, brt_compare_func,
			 int progress_allocation);

int mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, brt_compare_func, BRTLOADER, struct rowset *);

//int write_file_to_dbfile (int outfile, FIDX infile, BRTLOADER bl, const struct descriptor *descriptor, int progress_allocation);
#if defined(__cilkplusplus)
};
#endif
int brt_loader_sort_and_write_rows (struct rowset *rows, struct merge_fileset *fs, BRTLOADER bl, int which_db, DB *dest_db, brt_compare_func,
				    int progress_allocation);

int toku_loader_write_brt_from_q_in_C (BRTLOADER bl,
				       const struct descriptor *descriptor,
				       int fd, // write to here
				       int progress_allocation,
				       QUEUE q);

int brt_loader_mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, brt_compare_func, BRTLOADER, struct rowset *);

int brt_loader_write_file_to_dbfile (int outfile, FIDX infile, BRTLOADER bl, const struct descriptor *descriptor, int progress_allocation);

int brtloader_init_file_infos (struct file_infos *fi);
void brtloader_fi_destroy (struct file_infos *fi, BOOL is_error);
int brtloader_fi_close (struct file_infos *fi, FIDX idx);
int brtloader_fi_reopen (struct file_infos *fi, FIDX idx, const char *mode);
int brtloader_fi_unlink (struct file_infos *fi, FIDX idx);

#if defined(__cplusplus)
};
#endif


#endif
