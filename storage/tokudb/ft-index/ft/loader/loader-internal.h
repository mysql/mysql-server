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

#pragma once

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."

#include <db.h>

#include "portability/toku_pthread.h"

#include "loader/dbufio.h"
#include "loader/loader.h"
#include "util/queue.h"

enum {
    EXTRACTOR_QUEUE_DEPTH = 2,
    FILE_BUFFER_SIZE  = 1<<24,
    MIN_ROWSET_MEMORY = 1<<23,
    MIN_MERGE_FANIN   = 2,
    FRACTAL_WRITER_QUEUE_DEPTH = 3,
    FRACTAL_WRITER_ROWSETS = FRACTAL_WRITER_QUEUE_DEPTH + 2,
    DBUFIO_DEPTH = 2,
    TARGET_MERGE_BUF_SIZE = 1<<24, // we'd like the merge buffer to be this big.
    MIN_MERGE_BUF_SIZE = 1<<20, // always use at least this much
    MAX_UNCOMPRESSED_BUF = MIN_MERGE_BUF_SIZE
};

/* These functions are exported to allow the tests to compile. */

/* These structures maintain a collection of all the open temporary files used by the loader. */
struct file_info {
    bool is_open;
    bool is_extant; // if true, the file must be unlinked.
    char *fname;
    FILE *file;
    uint64_t n_rows; // how many rows were written into that file
    size_t buffer_size;
    void *buffer;
};
struct file_infos {
    int n_files;
    int n_files_limit;
    struct file_info *file_infos;
    int n_files_open, n_files_extant;
    toku_mutex_t lock; // must protect this data structure because current activity performs a REALLOC(fi->file_infos).
};
typedef struct fidx { int idx; } FIDX;
static const FIDX FIDX_NULL __attribute__((__unused__)) = {-1};
static int fidx_is_null (const FIDX f) __attribute__((__unused__));
static int fidx_is_null (const FIDX f) { return f.idx==-1; }
FILE *toku_bl_fidx2file (FTLOADER bl, FIDX i);

int ft_loader_open_temp_file (FTLOADER bl, FIDX*file_idx);

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
int add_row (struct rowset *rows, DBT *key, DBT *val);

int loader_write_row(DBT *key, DBT *val, FIDX data, FILE*, uint64_t *dataoff, struct wbuf *wb, FTLOADER bl);
int loader_read_row (FILE *f, DBT *key, DBT *val);

struct merge_fileset {
    bool have_sorted_output;  // Is there an previous key?
    FIDX sorted_output;       // this points to one of the data_fidxs.  If output_is_sorted then this is the file containing sorted data.  It's still open
    DBT  prev_key;            // What is it?  If it's here, its the last output in the merge fileset

    int n_temp_files, n_temp_files_limit;
    FIDX *data_fidxs;
};

void init_merge_fileset (struct merge_fileset *fs);
void destroy_merge_fileset (struct merge_fileset *fs);

struct poll_callback_s {
    ft_loader_poll_func poll_function;
    void *poll_extra;
};
typedef struct poll_callback_s *ft_loader_poll_callback;

int ft_loader_init_poll_callback(ft_loader_poll_callback);

void ft_loader_destroy_poll_callback(ft_loader_poll_callback);

void ft_loader_set_poll_function(ft_loader_poll_callback, ft_loader_poll_func poll_function, void *poll_extra);

int ft_loader_call_poll_function(ft_loader_poll_callback, float progress);

struct error_callback_s {
    int error;
    ft_loader_error_func error_callback;
    void *extra;
    DB *db;
    int which_db;
    DBT key;
    DBT val;
    bool did_callback;
    toku_mutex_t mutex;
};
typedef struct error_callback_s *ft_loader_error_callback;

void ft_loader_init_error_callback(ft_loader_error_callback);

void ft_loader_destroy_error_callback(ft_loader_error_callback);

int ft_loader_get_error(ft_loader_error_callback);

void ft_loader_set_error_function(ft_loader_error_callback, ft_loader_error_func error_function, void *extra);

int ft_loader_set_error(ft_loader_error_callback, int error, DB *db, int which_db, DBT *key, DBT *val);

int ft_loader_call_error_function(ft_loader_error_callback);

int ft_loader_set_error_and_callback(ft_loader_error_callback, int error, DB *db, int which_db, DBT *key, DBT *val);

struct ft_loader_s {
    // These two are set in the close function, and used while running close
    struct error_callback_s error_callback;
    struct poll_callback_s poll_callback;

    generate_row_for_put_func generate_row_for_put;
    ft_compare_func *bt_compare_funs;

    DB *src_db;
    int N;
    DB **dbs; // N of these
    DESCRIPTOR *descriptors; // N of these.
    TXNID      *root_xids_that_created; // N of these.
    const char **new_fnames_in_env; // N of these.  The file names that the final data will be written to (relative to env).

    uint64_t *extracted_datasizes; // N of these.

    struct rowset primary_rowset; // the primary rows that have been put, but the secondary rows haven't been generated.
    struct rowset primary_rowset_temp; // the primary rows that are being worked on by the extractor_thread.

    QUEUE primary_rowset_queue; // main thread enqueues rowsets in this queue (in maybe 64MB chunks).  The extractor thread removes them, sorts them, adn writes to file.
    toku_pthread_t     extractor_thread;     // the thread that takes primary rowset and does extraction and the first level sort and write to file.
    bool extractor_live;

    DBT  *last_key;         // for each rowset, remember the most recently output key.  The system may choose not to keep this up-to-date when a rowset is unsorted.  These keys are malloced and ulen maintains the size of the malloced block.
    
    struct rowset *rows; // secondary rows that have been put, but haven't been sorted and written to a file.
    uint64_t n_rows; // how many rows have been put?
    struct merge_fileset *fs;

    const char *temp_file_template;

    CACHETABLE cachetable;
    bool did_reserve_memory;
    bool compress_intermediates;
    bool allow_puts;
    uint64_t   reserved_memory; // how much memory are we allowed to use?

    /* To make it easier to recover from errors, we don't use FILE*, instead we use an index into the file_infos. */
    struct file_infos file_infos;

#define PROGRESS_MAX (1<<16)
    int progress;       // Progress runs from 0 to PROGRESS_MAX.  When we call the poll function we convert to a float from 0.0 to 1.0
    // We use an integer so that we can add to the progress using a fetch-and-add instruction.

    int progress_callback_result; // initially zero, if any call to the poll function callback returns nonzero, we save the result here (and don't call the poll callback function again).

    LSN load_lsn; //LSN of the fsynced 'load' log entry.  Write this LSN (as checkpoint_lsn) in ft headers made by this loader.
    TXNID load_root_xid; //(Root) transaction that performed the load.

    QUEUE *fractal_queues; // an array of work queues, one for each secondary index.
    toku_pthread_t *fractal_threads;
    bool *fractal_threads_live; // an array of bools indicating that fractal_threads[i] is a live thread.  (There is no NULL for a pthread_t, so we have to maintain this separately).

    unsigned fractal_workers; // number of fractal tree writer threads

    toku_mutex_t mutex;
    bool mutex_init;
};

// Set the number of rows in the loader.  Used for test.
void toku_ft_loader_set_n_rows(FTLOADER bl, uint64_t n_rows);

// Get the number of rows in the loader.  Used for test.
uint64_t toku_ft_loader_get_n_rows(FTLOADER bl);

// The data passed into a fractal_thread via pthread_create.
struct fractal_thread_args {
    FTLOADER                bl;
    const DESCRIPTOR descriptor;
    int                      fd; // write the ft into fd.
    int                      progress_allocation;
    QUEUE                    q;
    uint64_t                 total_disksize_estimate;
    int                      errno_result; // the final result.
    int                      which_db;
    uint32_t                 target_nodesize;
    uint32_t                 target_basementnodesize;
    enum toku_compression_method target_compression_method;
    uint32_t                 target_fanout;
};

void toku_ft_loader_set_n_rows(FTLOADER bl, uint64_t n_rows);
uint64_t toku_ft_loader_get_n_rows(FTLOADER bl);

int merge_row_arrays_base (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
                           int which_db, DB *dest_db, ft_compare_func,
			   FTLOADER,
                           struct rowset *);

int merge_files (struct merge_fileset *fs, FTLOADER bl, int which_db, DB *dest_db, ft_compare_func, int progress_allocation, QUEUE);

int sort_and_write_rows (struct rowset rows, struct merge_fileset *fs, FTLOADER bl, int which_db, DB *dest_db, ft_compare_func);

int mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, ft_compare_func, FTLOADER, struct rowset *);

//int write_file_to_dbfile (int outfile, FIDX infile, FTLOADER bl, const DESCRIPTOR descriptor, int progress_allocation);
int toku_merge_some_files_using_dbufio (const bool to_q, FIDX dest_data, QUEUE q, int n_sources, DBUFIO_FILESET bfs, FIDX srcs_fidxs[/*n_sources*/], FTLOADER bl, int which_db, DB *dest_db, ft_compare_func compare, int progress_allocation);

int ft_loader_sort_and_write_rows (struct rowset *rows, struct merge_fileset *fs, FTLOADER bl, int which_db, DB *dest_db, ft_compare_func);

// This is probably only for testing.
int toku_loader_write_ft_from_q_in_C (FTLOADER                 bl,
				      const DESCRIPTOR         descriptor,
				      int                      fd, // write to here
				      int                      progress_allocation,
				      QUEUE                    q,
				      uint64_t                 total_disksize_estimate,
                                      int                      which_db,
                                      uint32_t                 target_nodesize,
                                      uint32_t                 target_basementnodesize,
                                      enum toku_compression_method target_compression_method,
                                      uint32_t                 fanout);

int ft_loader_mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, ft_compare_func, FTLOADER, struct rowset *);

int ft_loader_write_file_to_dbfile (int outfile, FIDX infile, FTLOADER bl, const DESCRIPTOR descriptor, int progress_allocation);

int ft_loader_init_file_infos (struct file_infos *fi);
void ft_loader_fi_destroy (struct file_infos *fi, bool is_error);
int ft_loader_fi_close (struct file_infos *fi, FIDX idx, bool require_open);
int ft_loader_fi_close_all (struct file_infos *fi);
int ft_loader_fi_reopen (struct file_infos *fi, FIDX idx, const char *mode);
int ft_loader_fi_unlink (struct file_infos *fi, FIDX idx);

int toku_ft_loader_internal_init (/* out */ FTLOADER *blp,
				   CACHETABLE cachetable,
				   generate_row_for_put_func g,
				   DB *src_db,
				   int N, FT_HANDLE ft_hs[/*N*/], DB* dbs[/*N*/],
				   const char *new_fnames_in_env[/*N*/],
				   ft_compare_func bt_compare_functions[/*N*/],
				   const char *temp_file_template,
				   LSN load_lsn,
                                   TOKUTXN txn,
                                   bool reserve_memory,
                                   uint64_t reserve_memory_size,
                                   bool compress_intermediates,
                                   bool allow_puts);

void toku_ft_loader_internal_destroy (FTLOADER bl, bool is_error);

// For test purposes only.  (In production, the rowset size is determined by negotation with the cachetable for some memory.  See #2613.)
uint64_t toku_ft_loader_get_rowset_budget_for_testing (void);

int toku_ft_loader_finish_extractor(FTLOADER bl);

int toku_ft_loader_get_error(FTLOADER bl, int *loader_errno);

void ft_loader_lock_init(FTLOADER bl);
void ft_loader_lock_destroy(FTLOADER bl);
void ft_loader_set_fractal_workers_count_from_c(FTLOADER bl);
