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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>

#if !TOKU_WINDOWS
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <toku_assert.h>
#include <string.h>
#include <fcntl.h>
#include "x1764.h"
#include "ftloader-internal.h"
#include "ft-internal.h"
#include "sub_block.h"
#include "sub_block_map.h"
#include "pqueue.h"
#include "dbufio.h"
#include "leafentry.h"
#include "log-internal.h"
#include "ft.h"

static size_t (*os_fwrite_fun)(const void *,size_t,size_t,FILE*)=NULL;
void ft_loader_set_os_fwrite (size_t (*fwrite_fun)(const void*,size_t,size_t,FILE*)) {
    os_fwrite_fun=fwrite_fun;
}

static size_t do_fwrite (const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (os_fwrite_fun) {
        return os_fwrite_fun(ptr, size, nmemb, stream);
    } else {
        return fwrite(ptr, size, nmemb, stream);
    }
}


// 1024 is the right size_factor for production.  
// Different values for these sizes may be used for testing.
static uint32_t size_factor = 1024;
static uint32_t default_loader_nodesize = FT_DEFAULT_NODE_SIZE;
static uint32_t default_loader_basementnodesize = FT_DEFAULT_BASEMENT_NODE_SIZE;

void
toku_ft_loader_set_size_factor(uint32_t factor) {
// For test purposes only
    size_factor = factor;
    default_loader_nodesize = (size_factor==1) ? (1<<15) : FT_DEFAULT_NODE_SIZE;
}

uint64_t
toku_ft_loader_get_rowset_budget_for_testing (void)
// For test purposes only.  In production, the rowset size is determined by negotation with the cachetable for some memory.  (See #2613).
{
    return 16ULL*size_factor*1024ULL;
}

void ft_loader_lock_init(FTLOADER bl) {
    invariant(!bl->mutex_init);
    toku_mutex_init(&bl->mutex, NULL); 
    bl->mutex_init = true;
}

void ft_loader_lock_destroy(FTLOADER bl) {
    if (bl->mutex_init) {
        toku_mutex_destroy(&bl->mutex);
        bl->mutex_init = false;
    }
}

static void ft_loader_lock(FTLOADER bl) {
    invariant(bl->mutex_init);
    toku_mutex_lock(&bl->mutex);
}

static void ft_loader_unlock(FTLOADER bl) {
    invariant(bl->mutex_init);
    toku_mutex_unlock(&bl->mutex);
}

static int add_big_buffer(struct file_info *file) {
    int result = 0;
    bool newbuffer = false;
    if (file->buffer == NULL) {
        file->buffer = toku_malloc(file->buffer_size);
        if (file->buffer == NULL)
            result = get_error_errno();
        else
            newbuffer = true;
    }
    if (result == 0) {
        int r = setvbuf(file->file, (char *) file->buffer, _IOFBF, file->buffer_size);
        if (r != 0) {
            result = get_error_errno();
            if (newbuffer) {
                toku_free(file->buffer);
                file->buffer = NULL;
            }
        }
    } 
    return result;
}

static void cleanup_big_buffer(struct file_info *file) {
    if (file->buffer) {
        toku_free(file->buffer);
        file->buffer = NULL;
    }
}

int ft_loader_init_file_infos (struct file_infos *fi) {
    int result = 0;
    toku_mutex_init(&fi->lock, NULL);
    fi->n_files = 0;
    fi->n_files_limit = 1;
    fi->n_files_open = 0;
    fi->n_files_extant = 0;
    MALLOC_N(fi->n_files_limit, fi->file_infos);
    if (fi->file_infos == NULL)
        result = get_error_errno();
    return result;
}

void ft_loader_fi_destroy (struct file_infos *fi, bool is_error)
// Effect: Free the resources in the fi.
// If is_error then we close and unlink all the temp files.
// If !is_error then requires that all the temp files have been closed and destroyed
// No error codes are returned.  If anything goes wrong with closing and unlinking then it's only in an is_error case, so we don't care.
{
    if (fi->file_infos == NULL) {
        // ft_loader_init_file_infos guarantees this isn't null, so if it is, we know it hasn't been inited yet and we don't need to destroy it.
        return;
    }
    toku_mutex_destroy(&fi->lock);
    if (!is_error) {
        invariant(fi->n_files_open==0);
        invariant(fi->n_files_extant==0);
    }
    for (int i=0; i<fi->n_files; i++) {
        if (fi->file_infos[i].is_open) {
            invariant(is_error);
            toku_os_fclose(fi->file_infos[i].file); // don't check for errors, since we are in an error case.
        }
        if (fi->file_infos[i].is_extant) {
            invariant(is_error);
            unlink(fi->file_infos[i].fname);
            toku_free(fi->file_infos[i].fname);
        }
        cleanup_big_buffer(&fi->file_infos[i]);
    }
    toku_free(fi->file_infos);
    fi->n_files=0;
    fi->n_files_limit=0;
    fi->file_infos = NULL;
}

static int open_file_add (struct file_infos *fi,
                          FILE *file,
                          char *fname,
                          /* out */ FIDX *idx)
{
    int result = 0;
    toku_mutex_lock(&fi->lock);
    if (fi->n_files >= fi->n_files_limit) {
        fi->n_files_limit *=2;
        XREALLOC_N(fi->n_files_limit, fi->file_infos);
    }
    invariant(fi->n_files < fi->n_files_limit);
    fi->file_infos[fi->n_files].is_open   = true;
    fi->file_infos[fi->n_files].is_extant = true;
    fi->file_infos[fi->n_files].fname     = fname;
    fi->file_infos[fi->n_files].file      = file;
    fi->file_infos[fi->n_files].n_rows    = 0;
    fi->file_infos[fi->n_files].buffer_size = FILE_BUFFER_SIZE;
    fi->file_infos[fi->n_files].buffer    = NULL;
    result = add_big_buffer(&fi->file_infos[fi->n_files]);
    if (result == 0) {
        idx->idx = fi->n_files;
        fi->n_files++;
        fi->n_files_extant++;
        fi->n_files_open++;
    }
   toku_mutex_unlock(&fi->lock);
    return result;
}

int ft_loader_fi_reopen (struct file_infos *fi, FIDX idx, const char *mode) {
    int result = 0;
    toku_mutex_lock(&fi->lock);
    int i = idx.idx;
    invariant(i>=0 && i<fi->n_files);
    invariant(!fi->file_infos[i].is_open);
    invariant(fi->file_infos[i].is_extant);
    fi->file_infos[i].file = toku_os_fopen(fi->file_infos[i].fname, mode);
    if (fi->file_infos[i].file == NULL) { 
        result = get_error_errno();
    } else {
        fi->file_infos[i].is_open = true;
        // No longer need the big buffer for reopened files.  Don't allocate the space, we need it elsewhere.
        //add_big_buffer(&fi->file_infos[i]);
        fi->n_files_open++;
    }
    toku_mutex_unlock(&fi->lock);
    return result;
}

int ft_loader_fi_close (struct file_infos *fi, FIDX idx, bool require_open)
{
    int result = 0;
    toku_mutex_lock(&fi->lock); 
    invariant(idx.idx >=0 && idx.idx < fi->n_files);
    if (fi->file_infos[idx.idx].is_open) {
        invariant(fi->n_files_open>0);   // loader-cleanup-test failure
        fi->n_files_open--;
        fi->file_infos[idx.idx].is_open = false;
        int r = toku_os_fclose(fi->file_infos[idx.idx].file);
        if (r)
            result = get_error_errno();
        cleanup_big_buffer(&fi->file_infos[idx.idx]);
    } else if (require_open)
        result = EINVAL;
    toku_mutex_unlock(&fi->lock); 
    return result;
}

int ft_loader_fi_unlink (struct file_infos *fi, FIDX idx) {
    int result = 0;
    toku_mutex_lock(&fi->lock);
    int id = idx.idx;
    invariant(id >=0 && id < fi->n_files);
    if (fi->file_infos[id].is_extant) { // must still exist
        invariant(fi->n_files_extant>0);
        fi->n_files_extant--;
        invariant(!fi->file_infos[id].is_open); // must be closed before we unlink
        fi->file_infos[id].is_extant = false;
        int r = unlink(fi->file_infos[id].fname);  
        if (r != 0) 
            result = get_error_errno();
        toku_free(fi->file_infos[id].fname);
        fi->file_infos[id].fname = NULL;
    } else
        result = EINVAL;
    toku_mutex_unlock(&fi->lock);
    return result;
}

int
ft_loader_fi_close_all(struct file_infos *fi) {
    int rval = 0;
    for (int i = 0; i < fi->n_files; i++) {
        int r;
        FIDX idx = { i };
        r = ft_loader_fi_close(fi, idx, false);  // ignore files that are already closed
        if (rval == 0 && r)
            rval = r;  // capture first error
    }
    return rval;
}

int ft_loader_open_temp_file (FTLOADER bl, FIDX *file_idx)
/* Effect: Open a temporary file in read-write mode.  Save enough information to close and delete the file later.
 * Return value: 0 on success, an error number otherwise.
 *  On error, *file_idx and *fnamep will be unmodified.
 *  The open file will be saved in bl->file_infos so that even if errors happen we can free them all.
 */
{
    int result = 0;
    FILE *f = NULL;
    int fd = -1;
    char *fname = toku_strdup(bl->temp_file_template);    
    if (fname == NULL)
        result = get_error_errno();
    else {
        fd = mkstemp(fname);
        if (fd < 0) { 
            result = get_error_errno();
        } else {
            f = toku_os_fdopen(fd, "r+");
            if (f == NULL)
                result = get_error_errno();
            else
                result = open_file_add(&bl->file_infos, f, fname, file_idx);
        }
    }
    if (result != 0) {
        if (fd >= 0) {
            toku_os_close(fd);
            unlink(fname);
        }
        if (f != NULL)
            toku_os_fclose(f);  // don't check for error because we're already in an error case
        if (fname != NULL)
            toku_free(fname);
    }
    return result;
}

void toku_ft_loader_internal_destroy (FTLOADER bl, bool is_error) {
    ft_loader_lock_destroy(bl);

    // These frees rely on the fact that if you free a NULL pointer then nothing bad happens.
    toku_free(bl->dbs);
    toku_free(bl->descriptors);
    toku_free(bl->root_xids_that_created);
    if (bl->new_fnames_in_env) {
        for (int i = 0; i < bl->N; i++)
            toku_free((char*)bl->new_fnames_in_env[i]);
        toku_free(bl->new_fnames_in_env);
    }
    toku_free(bl->extracted_datasizes);
    toku_free(bl->bt_compare_funs);
    toku_free((char*)bl->temp_file_template);
    ft_loader_fi_destroy(&bl->file_infos, is_error);

    for (int i = 0; i < bl->N; i++) 
        destroy_rowset(&bl->rows[i]);
    toku_free(bl->rows);

    for (int i = 0; i < bl->N; i++)
        destroy_merge_fileset(&bl->fs[i]);
    toku_free(bl->fs);

    if (bl->last_key) {
        for (int i=0; i < bl->N; i++) {
            toku_free(bl->last_key[i].data);
        }
        toku_free(bl->last_key);
        bl->last_key = NULL;
    }

    destroy_rowset(&bl->primary_rowset);

    for (int i=0; i<bl->N; i++) {
        if ( bl->fractal_queues ) {
            invariant(bl->fractal_queues[i]==NULL);
        }
    }
    toku_free(bl->fractal_threads);
    toku_free(bl->fractal_queues);
    toku_free(bl->fractal_threads_live);

    if (bl->did_reserve_memory) {
        invariant(bl->cachetable);
        toku_cachetable_release_reserved_memory(bl->cachetable, bl->reserved_memory);
    }

    ft_loader_destroy_error_callback(&bl->error_callback);
    ft_loader_destroy_poll_callback(&bl->poll_callback);

    //printf("Progress=%d/%d\n", bl->progress, PROGRESS_MAX);

    toku_free(bl);
}

static void *extractor_thread (void*);

#define MAX(a,b) (((a)<(b)) ? (b) : (a))

static uint64_t memory_per_rowset_during_extract (FTLOADER bl)
// Return how much memory can be allocated for each rowset.
{
    if (size_factor==1) {
        return 16*1024;
    } else {
        // There is a primary rowset being maintained by the foreground thread.
        // There could be two more in the queue.
        // There is one rowset for each index (bl->N) being filled in.
        // Later we may have sort_and_write operations spawning in parallel, and will need to account for that.
        int n_copies = (1 // primary rowset
                        +EXTRACTOR_QUEUE_DEPTH  // the number of primaries in the queue
                        +bl->N // the N rowsets being constructed by the extractor thread.
                        +bl->N // the N sort buffers
                        +1     // Give the extractor thread one more so that it can have temporary space for sorting.  This is overkill.
                        );
        int64_t extra_reserved_memory = bl->N * FILE_BUFFER_SIZE;  // for each index we are writing to a file at any given time.
        int64_t tentative_rowset_size = ((int64_t)(bl->reserved_memory - extra_reserved_memory))/(n_copies);
        return MAX(tentative_rowset_size, (int64_t)MIN_ROWSET_MEMORY);
    }
}

static unsigned ft_loader_get_fractal_workers_count(FTLOADER bl) {
    unsigned w = 0;
    while (1) {
        ft_loader_lock(bl);
        w = bl->fractal_workers;
        ft_loader_unlock(bl);
        if (w != 0)
            break;
        toku_pthread_yield();  // maybe use a cond var instead
    }
    return w;
}

static void ft_loader_set_fractal_workers_count(FTLOADER bl) {
    ft_loader_lock(bl);
    if (bl->fractal_workers == 0)
        bl->fractal_workers = 1;
    ft_loader_unlock(bl);
}

// To compute a merge, we have a certain amount of memory to work with.
// We perform only one fanin at a time.
// If the fanout is F then we are using
//   F merges.  Each merge uses
//   DBUFIO_DEPTH buffers for double buffering.  Each buffer is of size at least MERGE_BUF_SIZE
// so the memory is
//   F*MERGE_BUF_SIZE*DBUFIO_DEPTH storage.
// We use some additional space to buffer the outputs. 
//  That's FILE_BUFFER_SIZE for writing to a merge file if we are writing to a mergefile.
//  And we have FRACTAL_WRITER_ROWSETS*MERGE_BUF_SIZE per queue
//  And if we are doing a fractal, each worker could have have a fractal tree that it's working on.
//
// DBUFIO_DEPTH*F*MERGE_BUF_SIZE + FRACTAL_WRITER_ROWSETS*MERGE_BUF_SIZE + WORKERS*NODESIZE*2 <= RESERVED_MEMORY

static int64_t memory_avail_during_merge(FTLOADER bl, bool is_fractal_node) {
    // avail memory = reserved memory - WORKERS*NODESIZE*2 for the last merge stage only
    int64_t avail_memory = bl->reserved_memory;
    if (is_fractal_node) {
        // reserve space for the fractal writer thread buffers
        avail_memory -= (int64_t)ft_loader_get_fractal_workers_count(bl) * (int64_t)default_loader_nodesize * 2; // compressed and uncompressed buffers
    }
    return avail_memory;
}

static int merge_fanin (FTLOADER bl, bool is_fractal_node) {
    // return number of temp files to read in this pass
    int64_t memory_avail = memory_avail_during_merge(bl, is_fractal_node);
    int64_t nbuffers = memory_avail / (int64_t)TARGET_MERGE_BUF_SIZE;
    if (is_fractal_node)
        nbuffers -= FRACTAL_WRITER_ROWSETS;
    return MAX(nbuffers / (int64_t)DBUFIO_DEPTH, (int)MIN_MERGE_FANIN);
}

static uint64_t memory_per_rowset_during_merge (FTLOADER bl, int merge_factor, bool is_fractal_node // if it is being sent to a q
                                                ) {
    int64_t memory_avail = memory_avail_during_merge(bl, is_fractal_node);
    int64_t nbuffers = DBUFIO_DEPTH * merge_factor;
    if (is_fractal_node)
        nbuffers += FRACTAL_WRITER_ROWSETS;
    return MAX(memory_avail / nbuffers, (int64_t)MIN_MERGE_BUF_SIZE);
}

int toku_ft_loader_internal_init (/* out */ FTLOADER *blp,
                                   CACHETABLE cachetable,
                                   generate_row_for_put_func g,
                                   DB *src_db,
                                   int N, FT_HANDLE brts[/*N*/], DB* dbs[/*N*/],
                                   const char *new_fnames_in_env[/*N*/],
                                   ft_compare_func bt_compare_functions[/*N*/],
                                   const char *temp_file_template,
                                   LSN load_lsn,
                                   TOKUTXN txn,
                                   bool reserve_memory,
                                   uint64_t reserve_memory_size,
                                   bool compress_intermediates)
// Effect: Allocate and initialize a FTLOADER, but do not create the extractor thread.
{
    FTLOADER CALLOC(bl); // initialized to all zeros (hence CALLOC)
    if (!bl) return get_error_errno();

    bl->generate_row_for_put = g;
    bl->cachetable = cachetable;
    if (reserve_memory && bl->cachetable) {
        bl->did_reserve_memory = true;
        bl->reserved_memory = toku_cachetable_reserve_memory(bl->cachetable, 2.0/3.0, reserve_memory_size); // allocate 2/3 of the unreserved part (which is 3/4 of the memory to start with).
    }
    else {
        bl->did_reserve_memory = false;
        bl->reserved_memory = 512*1024*1024; // if no cache table use 512MB.
    }
    bl->compress_intermediates = compress_intermediates;
    if (0) { // debug
        fprintf(stderr, "%s Reserved memory=%ld\n", __FUNCTION__, bl->reserved_memory);
    }

    bl->src_db = src_db;
    bl->N = N;
    bl->load_lsn = load_lsn;
    if (txn) {
        bl->load_root_xid = txn->txnid.parent_id64;
    }
    else {
        bl->load_root_xid = TXNID_NONE;
    }
    
    ft_loader_init_error_callback(&bl->error_callback);
    ft_loader_init_poll_callback(&bl->poll_callback);

#define MY_CALLOC_N(n,v) CALLOC_N(n,v); if (!v) { int r = get_error_errno(); toku_ft_loader_internal_destroy(bl, true); return r; }
#define SET_TO_MY_STRDUP(lval, s) do { char *v = toku_strdup(s); if (!v) { int r = get_error_errno(); toku_ft_loader_internal_destroy(bl, true); return r; } lval = v; } while (0)

    MY_CALLOC_N(N, bl->root_xids_that_created);
    for (int i=0; i<N; i++) if (brts[i]) bl->root_xids_that_created[i]=brts[i]->ft->h->root_xid_that_created;
    MY_CALLOC_N(N, bl->dbs);
    for (int i=0; i<N; i++) if (brts[i]) bl->dbs[i]=dbs[i];
    MY_CALLOC_N(N, bl->descriptors);
    for (int i=0; i<N; i++) if (brts[i]) bl->descriptors[i]=&brts[i]->ft->descriptor;
    MY_CALLOC_N(N, bl->new_fnames_in_env);
    for (int i=0; i<N; i++) SET_TO_MY_STRDUP(bl->new_fnames_in_env[i], new_fnames_in_env[i]);
    MY_CALLOC_N(N, bl->extracted_datasizes); // the calloc_n zeroed everything, which is what we want
    MY_CALLOC_N(N, bl->bt_compare_funs);
    for (int i=0; i<N; i++) bl->bt_compare_funs[i] = bt_compare_functions[i];

    MY_CALLOC_N(N, bl->fractal_queues);
    for (int i=0; i<N; i++) bl->fractal_queues[i]=NULL;
    MY_CALLOC_N(N, bl->fractal_threads);
    MY_CALLOC_N(N, bl->fractal_threads_live);
    for (int i=0; i<N; i++) bl->fractal_threads_live[i] = false;

    {
        int r = ft_loader_init_file_infos(&bl->file_infos); 
        if (r!=0) { toku_ft_loader_internal_destroy(bl, true); return r; }
    }

    SET_TO_MY_STRDUP(bl->temp_file_template, temp_file_template);

    bl->n_rows   = 0; 
    bl->progress = 0;
    bl->progress_callback_result = 0;

    MY_CALLOC_N(N, bl->rows);
    MY_CALLOC_N(N, bl->fs);
    MY_CALLOC_N(N, bl->last_key);
    for(int i=0;i<N;i++) {
        { 
            int r = init_rowset(&bl->rows[i], memory_per_rowset_during_extract(bl)); 
            if (r!=0) { toku_ft_loader_internal_destroy(bl, true); return r; } 
        }
        init_merge_fileset(&bl->fs[i]);
        bl->last_key[i].flags = DB_DBT_REALLOC; // don't really need this, but it's nice to maintain it.  We use ulen to keep track of the realloced space.
    }

    { 
        int r = init_rowset(&bl->primary_rowset, memory_per_rowset_during_extract(bl)); 
        if (r!=0) { toku_ft_loader_internal_destroy(bl, true); return r; }
    }
    {   int r = queue_create(&bl->primary_rowset_queue, EXTRACTOR_QUEUE_DEPTH); 
        if (r!=0) { toku_ft_loader_internal_destroy(bl, true); return r; }
    }
    //printf("%s:%d toku_pthread_create\n", __FILE__, __LINE__);
    {
        ft_loader_lock_init(bl);
    }

    *blp = bl;

    return 0;
}

int toku_ft_loader_open (/* out */ FTLOADER *blp,
                          CACHETABLE cachetable,
                          generate_row_for_put_func g,
                          DB *src_db,
                          int N, FT_HANDLE brts[/*N*/], DB* dbs[/*N*/],
                          const char *new_fnames_in_env[/*N*/],
                          ft_compare_func bt_compare_functions[/*N*/],
                          const char *temp_file_template,
                          LSN load_lsn,
                          TOKUTXN txn,
                          bool reserve_memory,
                          uint64_t reserve_memory_size,
                          bool compress_intermediates)
/* Effect: called by DB_ENV->create_loader to create a brt loader.
 * Arguments:
 *   blp                  Return the brt loader here.
 *   g                    The function for generating a row
 *   src_db               The source database.  Needed by g.  May be NULL if that's ok with g.
 *   N                    The number of dbs to create.
 *   dbs                  An array of open databases.  Used by g.  The data will be put in these database.
 *   new_fnames           The file names (these strings are owned by the caller: we make a copy for our own purposes).
 *   temp_file_template   A template suitable for mkstemp()
 * Return value: 0 on success, an error number otherwise.
 */
{
    int result = 0;
    {
        int r = toku_ft_loader_internal_init(blp, cachetable, g, src_db,
                                              N, brts, dbs,
                                              new_fnames_in_env,
                                              bt_compare_functions,
                                              temp_file_template,
                                              load_lsn,
                                              txn,
                                              reserve_memory,
                                              reserve_memory_size,
                                              compress_intermediates);
        if (r!=0) result = r;
    }
    if (result==0) {
        FTLOADER bl = *blp;
        int r = toku_pthread_create(&bl->extractor_thread, NULL, extractor_thread, (void*)bl); 
        if (r==0) {
            bl->extractor_live = true;
        } else  { 
            result = r;
            (void) toku_ft_loader_internal_destroy(bl, true);
        }
    }
    return result;
}

static void ft_loader_set_panic(FTLOADER bl, int error, bool callback, int which_db, DBT *key, DBT *val) {
    DB *db = nullptr;
    if (bl && bl->dbs && which_db >= 0 && which_db < bl->N) {
        db = bl->dbs[which_db];
    }
    int r = ft_loader_set_error(&bl->error_callback, error, db, which_db, key, val);
    if (r == 0 && callback)
        ft_loader_call_error_function(&bl->error_callback);
}

// One of the tests uses this.
FILE *toku_bl_fidx2file (FTLOADER bl, FIDX i) {
    toku_mutex_lock(&bl->file_infos.lock);
    invariant(i.idx >=0 && i.idx < bl->file_infos.n_files);
    invariant(bl->file_infos.file_infos[i.idx].is_open);
    FILE *result=bl->file_infos.file_infos[i.idx].file;
    toku_mutex_unlock(&bl->file_infos.lock);
    return result;
}

static int bl_finish_compressed_write(FILE *stream, struct wbuf *wb) {
    int r;
    char *compressed_buf = NULL;
    const size_t data_size = wb->ndone;
    invariant(data_size > 0);
    invariant(data_size <= MAX_UNCOMPRESSED_BUF);

    int n_sub_blocks = 0;
    int sub_block_size = 0;

    r = choose_sub_block_size(wb->ndone, max_sub_blocks, &sub_block_size, &n_sub_blocks);
    invariant(r==0);
    invariant(0 < n_sub_blocks && n_sub_blocks <= max_sub_blocks);
    invariant(sub_block_size > 0);

    struct sub_block sub_block[max_sub_blocks];
    // set the initial sub block size for all of the sub blocks
    for (int i = 0; i < n_sub_blocks; i++) {
        sub_block_init(&sub_block[i]);
    }
    set_all_sub_block_sizes(data_size, sub_block_size, n_sub_blocks, sub_block);

    size_t compressed_len = get_sum_compressed_size_bound(n_sub_blocks, sub_block, TOKU_DEFAULT_COMPRESSION_METHOD);
    const size_t sub_block_header_len = sub_block_header_size(n_sub_blocks);
    const size_t other_overhead = sizeof(uint32_t); //total_size
    const size_t header_len = sub_block_header_len + other_overhead;
    MALLOC_N(header_len + compressed_len, compressed_buf);
    if (compressed_buf == nullptr) {
        return ENOMEM;
    }

    // compress all of the sub blocks
    char *uncompressed_ptr = (char*)wb->buf;
    char *compressed_ptr = compressed_buf + header_len;
    compressed_len = compress_all_sub_blocks(n_sub_blocks, sub_block, uncompressed_ptr, compressed_ptr,
                                             get_num_cores(), get_ft_pool(), TOKU_DEFAULT_COMPRESSION_METHOD);

    //total_size does NOT include itself
    uint32_t total_size = compressed_len + sub_block_header_len;
    // serialize the sub block header
    uint32_t *ptr = (uint32_t *)(compressed_buf);
    *ptr++ = toku_htod32(total_size);
    *ptr++ = toku_htod32(n_sub_blocks);
    for (int i=0; i<n_sub_blocks; i++) {
        ptr[0] = toku_htod32(sub_block[i].compressed_size);
        ptr[1] = toku_htod32(sub_block[i].uncompressed_size);
        ptr[2] = toku_htod32(sub_block[i].xsum);
        ptr += 3;
    }
    // Mark as written
    wb->ndone = 0;

    size_t size_to_write = total_size + 4; // Includes writing total_size

    {
        size_t written = do_fwrite(compressed_buf, 1, size_to_write, stream);
        if (written!=size_to_write) {
            if (os_fwrite_fun)    // if using hook to induce artificial errors (for testing) ...
                r = get_maybe_error_errno();        // ... then there is no error in the stream, but there is one in errno
            else
                r = ferror(stream);
            invariant(r!=0);
            goto exit;
        }
    }
    r = 0;
exit:
    if (compressed_buf) {
        toku_free(compressed_buf);
    }
    return r;
}

static int bl_compressed_write(void *ptr, size_t nbytes, FILE *stream, struct wbuf *wb) {
    invariant(wb->size <= MAX_UNCOMPRESSED_BUF);
    size_t bytes_left = nbytes;
    char *buf = (char*)ptr;

    while (bytes_left > 0) {
        size_t bytes_to_copy = bytes_left;
        if (wb->ndone + bytes_to_copy > wb->size) {
            bytes_to_copy = wb->size - wb->ndone;
        }
        wbuf_nocrc_literal_bytes(wb, buf, bytes_to_copy);
        if (wb->ndone == wb->size) {
            //Compress, write to disk, and empty out wb
            int r = bl_finish_compressed_write(stream, wb);
            if (r != 0) {
                errno = r;
                return -1;
            }
            wb->ndone = 0;
        }
        bytes_left -= bytes_to_copy;
        buf += bytes_to_copy;
    }
    return 0;
}

static int bl_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream, struct wbuf *wb, FTLOADER bl)
/* Effect: this is a wrapper for fwrite that returns 0 on success, otherwise returns an error number.
 * Arguments:
 *   ptr    the data to be writen.
 *   size   the amount of data to be written.
 *   nmemb  the number of units of size to be written.
 *   stream write the data here.
 *   wb     where to write uncompressed data (if we're compressing) or ignore if NULL
 *   bl     passed so we can panic the ft_loader if something goes wrong (recording the error number).
 * Return value: 0 on success, an error number otherwise.
 */
{
    if (!bl->compress_intermediates || !wb) {
        size_t r = do_fwrite(ptr, size, nmemb, stream);
        if (r!=nmemb) {
            int e;
            if (os_fwrite_fun)    // if using hook to induce artificial errors (for testing) ...
                e = get_maybe_error_errno();        // ... then there is no error in the stream, but there is one in errno
            else
                e = ferror(stream);
            invariant(e!=0);
            return e;
        }
    } else {
        size_t num_bytes = size * nmemb;
        int r = bl_compressed_write(ptr, num_bytes, stream, wb);
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

static int bl_fread (void *ptr, size_t size, size_t nmemb, FILE *stream)
/* Effect: this is a wrapper for fread that returns 0 on success, otherwise returns an error number.
 * Arguments:
 *  ptr      read data into here.
 *  size     size of data element to be read.
 *  nmemb    number of data elements to be read.
 *  stream   where to read the data from.
 * Return value: 0 on success, an error number otherwise.
 */
{
    size_t r = fread(ptr, size, nmemb, stream);
    if (r==0) {
        if (feof(stream)) return EOF;
        else {
        do_error: ;
            int e = ferror(stream);
            // r == 0 && !feof && e == 0, how does this happen? invariant(e!=0);
            return e;
        }
    } else if (r<nmemb) {
        goto do_error;
    } else {
        return 0;
    }
}

static int bl_write_dbt (DBT *dbt, FILE* datafile, uint64_t *dataoff, struct wbuf *wb, FTLOADER bl)
{
    int r;
    int dlen = dbt->size;
    if ((r=bl_fwrite(&dlen,     sizeof(dlen), 1,    datafile, wb, bl))) return r;
    if ((r=bl_fwrite(dbt->data, 1,            dlen, datafile, wb, bl))) return r;
    if (dataoff)
        *dataoff += dlen + sizeof(dlen);
    return 0;
}

static int bl_read_dbt (/*in*/DBT *dbt, FILE *stream)
{
    int len;
    {
        int r;
        if ((r = bl_fread(&len, sizeof(len), 1, stream))) return r;
        invariant(len>=0);
    }
    if ((int)dbt->ulen<len) { dbt->ulen=len; dbt->data=toku_xrealloc(dbt->data, len); }
    {
        int r;
        if ((r = bl_fread(dbt->data, 1, len, stream)))     return r;
    }
    dbt->size = len;
    return 0;
}

static int bl_read_dbt_from_dbufio (/*in*/DBT *dbt, DBUFIO_FILESET bfs, int filenum)
{
    int result = 0;
    uint32_t len;
    {
        size_t n_read;
        int r = dbufio_fileset_read(bfs, filenum, &len, sizeof(len), &n_read);
        if (r!=0) {
            result = r;
        } else if (n_read<sizeof(len)) {
            result = TOKUDB_NO_DATA; // must have run out of data prematurely.  This is not EOF, it's a real error.
        }
    }
    if (result==0) {
        if (dbt->ulen<len) {
            void * data = toku_realloc(dbt->data, len);
            if (data==NULL) {
                result = get_error_errno();
            } else {
                dbt->ulen=len;
                dbt->data=data;
            }
        }
    }
    if (result==0) {
        size_t n_read;
        int r = dbufio_fileset_read(bfs, filenum, dbt->data, len, &n_read);
        if (r!=0) {
            result = r;
        } else if (n_read<len) {
            result = TOKUDB_NO_DATA; // must have run out of data prematurely.  This is not EOF, it's a real error.
        } else {
            dbt->size = len;
        }
    }
    return result;
}


int loader_write_row(DBT *key, DBT *val, FIDX data, FILE *dataf, uint64_t *dataoff, struct wbuf *wb, FTLOADER bl)
/* Effect: Given a key and a val (both DBTs), write them to a file.  Increment *dataoff so that it's up to date.
 * Arguments:
 *   key, val   write these.
 *   data       the file to write them to
 *   dataoff    a pointer to a counter that keeps track of the amount of data written so far.
 *   wb         a pointer (possibly NULL) to buffer uncompressed output
 *   bl         the ft_loader (passed so we can panic if needed).
 * Return value: 0 on success, an error number otherwise.
 */
{
    //int klen = key->size;
    //int vlen = val->size;
    int r;
    // we have a chance to handle the errors because when we close we can delete all the files.
    if ((r=bl_write_dbt(key, dataf, dataoff, wb, bl))) return r;
    if ((r=bl_write_dbt(val, dataf, dataoff, wb, bl))) return r;
    toku_mutex_lock(&bl->file_infos.lock);
    bl->file_infos.file_infos[data.idx].n_rows++;
    toku_mutex_unlock(&bl->file_infos.lock);
    return 0;
}

int loader_read_row (FILE *f, DBT *key, DBT *val)
/* Effect: Read a key value pair from a file.  The DBTs must have DB_DBT_REALLOC set.
 * Arguments:
 *    f         where to read it from.
 *    key, val  read it into these.
 *    bl        passed so we can panic if needed.
 * Return value: 0 on success, an error number otherwise.
 * Requires:   The DBTs must have DB_DBT_REALLOC
 */
{
    {
        int r = bl_read_dbt(key, f);
        if (r!=0) return r;
    }
    {
        int r = bl_read_dbt(val, f);
        if (r!=0) return r;
    }
    return 0;
}

static int loader_read_row_from_dbufio (DBUFIO_FILESET bfs, int filenum, DBT *key, DBT *val)
/* Effect: Read a key value pair from a file.  The DBTs must have DB_DBT_REALLOC set.
 * Arguments:
 *    f         where to read it from.
 *    key, val  read it into these.
 *    bl        passed so we can panic if needed.
 * Return value: 0 on success, an error number otherwise.
 * Requires:   The DBTs must have DB_DBT_REALLOC
 */
{
    {
        int r = bl_read_dbt_from_dbufio(key, bfs, filenum);
        if (r!=0) return r;
    }
    {
        int r = bl_read_dbt_from_dbufio(val, bfs, filenum);
        if (r!=0) return r;
    }
    return 0;
}


int init_rowset (struct rowset *rows, uint64_t memory_budget) 
/* Effect: Initialize a collection of rows to be empty. */
{
    int result = 0;

    rows->memory_budget = memory_budget;

    rows->rows = NULL;
    rows->data = NULL;

    rows->n_rows = 0;
    rows->n_rows_limit = 100;
    MALLOC_N(rows->n_rows_limit, rows->rows);
    if (rows->rows == NULL)
        result = get_error_errno();
    rows->n_bytes = 0;
    rows->n_bytes_limit = (size_factor==1) ? 1024*size_factor*16 : memory_budget;
    //printf("%s:%d n_bytes_limit=%ld (size_factor based limit=%d)\n", __FILE__, __LINE__, rows->n_bytes_limit, 1024*size_factor*16);
    rows->data = (char *) toku_malloc(rows->n_bytes_limit);
    if (rows->rows==NULL || rows->data==NULL) {
        if (result == 0)
            result = get_error_errno();
        toku_free(rows->rows);
        toku_free(rows->data);
        rows->rows = NULL;
        rows->data = NULL;
    }
    return result;
}

static void zero_rowset (struct rowset *rows) {
    memset(rows, 0, sizeof(*rows));
}

void destroy_rowset (struct rowset *rows) {
    if ( rows ) {
        toku_free(rows->data);
        toku_free(rows->rows);
        zero_rowset(rows);
    }
}

static int row_wont_fit (struct rowset *rows, size_t size)
/* Effect: Return nonzero if adding a row of size SIZE would be too big (bigger than the buffer limit) */ 
{
    // Account for the memory used by the data and also the row structures.
    size_t memory_in_use = (rows->n_rows*sizeof(struct row)
                            + rows->n_bytes);
    return (rows->memory_budget <  memory_in_use + size);
}

int add_row (struct rowset *rows, DBT *key, DBT *val)
/* Effect: add a row to a collection. */
{
    int result = 0;
    if (rows->n_rows >= rows->n_rows_limit) {
        struct row *old_rows = rows->rows;
        size_t old_n_rows_limit = rows->n_rows_limit;
        rows->n_rows_limit *= 2;
        REALLOC_N(rows->n_rows_limit, rows->rows);
        if (rows->rows == NULL) {
            result = get_error_errno();
            rows->rows = old_rows;
            rows->n_rows_limit = old_n_rows_limit;
            return result;
        }
    }
    size_t off      = rows->n_bytes;
    size_t next_off = off + key->size + val->size;

    struct row newrow; 
    memset(&newrow, 0, sizeof newrow); newrow.off = off; newrow.klen = key->size; newrow.vlen = val->size;

    rows->rows[rows->n_rows++] = newrow;
    if (next_off > rows->n_bytes_limit) {
        size_t old_n_bytes_limit = rows->n_bytes_limit;
        while (next_off > rows->n_bytes_limit) {
            rows->n_bytes_limit = rows->n_bytes_limit*2; 
        }
        invariant(next_off <= rows->n_bytes_limit);
        char *old_data = rows->data;
        REALLOC_N(rows->n_bytes_limit, rows->data);
        if (rows->data == NULL) {
            result = get_error_errno();
            rows->data = old_data;
            rows->n_bytes_limit = old_n_bytes_limit;
            return result;
        }
    }
    memcpy(rows->data+off,           key->data, key->size);
    memcpy(rows->data+off+key->size, val->data, val->size);
    rows->n_bytes = next_off;
    return result;
}

static int process_primary_rows (FTLOADER bl, struct rowset *primary_rowset);

static int finish_primary_rows_internal (FTLOADER bl)
// now we have been asked to finish up.
// Be sure to destroy the rowsets.
{
    int *MALLOC_N(bl->N, ra);
    if (ra==NULL) return get_error_errno();

    for (int i = 0; i < bl->N; i++) {
        //printf("%s:%d extractor finishing index %d with %ld rows\n", __FILE__, __LINE__, i, rows->n_rows);
        ra[i] = sort_and_write_rows(bl->rows[i], &(bl->fs[i]), bl, i, bl->dbs[i], bl->bt_compare_funs[i]);
        zero_rowset(&bl->rows[i]);
    }

    // accept any of the error codes (in this case, the last one).
    int r = 0;
    for (int i = 0; i < bl->N; i++)
        if (ra[i] != 0)
            r = ra[i];

    toku_free(ra);
    return r;
}

static int finish_primary_rows (FTLOADER bl) {
    return           finish_primary_rows_internal (bl);
}

static void* extractor_thread (void *blv) {
    FTLOADER bl = (FTLOADER)blv;
    int r = 0;
    while (1) {
        void *item;
        {
            int rq = queue_deq(bl->primary_rowset_queue, &item, NULL, NULL);
            if (rq==EOF) break;
            invariant(rq==0); // other errors are arbitrarily bad.
        }
        struct rowset *primary_rowset = (struct rowset *)item;

        //printf("%s:%d extractor got %ld rows\n", __FILE__, __LINE__, primary_rowset.n_rows);

        // Now we have some rows to output
        {
            r = process_primary_rows(bl, primary_rowset);
            if (r)
                ft_loader_set_panic(bl, r, false, 0, nullptr, nullptr);
        }
    }

    //printf("%s:%d extractor finishing\n", __FILE__, __LINE__);
    if (r == 0) {
        r = finish_primary_rows(bl); 
        if (r) 
            ft_loader_set_panic(bl, r, false, 0, nullptr, nullptr);
        
    }
    return NULL;
}

static void enqueue_for_extraction (FTLOADER bl) {
    //printf("%s:%d enqueing %ld items\n", __FILE__, __LINE__, bl->primary_rowset.n_rows);
    struct rowset *XMALLOC(enqueue_me);
    *enqueue_me = bl->primary_rowset;
    zero_rowset(&bl->primary_rowset);
    int r = queue_enq(bl->primary_rowset_queue, (void*)enqueue_me, 1, NULL);
    resource_assert_zero(r); 
}

static int loader_do_put(FTLOADER bl,
                         DBT *pkey,
                         DBT *pval)
{
    int result;
    result = add_row(&bl->primary_rowset, pkey, pval);
    if (result == 0 && row_wont_fit(&bl->primary_rowset, 0)) {
        // queue the rows for further processing by the extractor thread.
        //printf("%s:%d please extract %ld\n", __FILE__, __LINE__, bl->primary_rowset.n_rows);
        enqueue_for_extraction(bl);
        {
            int r = init_rowset(&bl->primary_rowset, memory_per_rowset_during_extract(bl)); 
            // bl->primary_rowset will get destroyed by toku_ft_loader_abort
            if (r != 0) 
                result = r;
        }
    }
    return result;
}

static int 
finish_extractor (FTLOADER bl) {
    //printf("%s:%d now finishing extraction\n", __FILE__, __LINE__);

    int rval;

    if (bl->primary_rowset.n_rows>0) {
        enqueue_for_extraction(bl);
    } else {
        destroy_rowset(&bl->primary_rowset);
    }
    //printf("%s:%d please finish extraction\n", __FILE__, __LINE__);
    {
        int r = queue_eof(bl->primary_rowset_queue);
        invariant(r==0);
    }
    //printf("%s:%d joining\n", __FILE__, __LINE__);
    {
        void *toku_pthread_retval;
        int r = toku_pthread_join(bl->extractor_thread, &toku_pthread_retval);
        resource_assert_zero(r); 
        invariant(toku_pthread_retval == NULL);
        bl->extractor_live = false;
    }
    {
        int r = queue_destroy(bl->primary_rowset_queue);
        invariant(r==0);
    }

    rval = ft_loader_fi_close_all(&bl->file_infos);

   //printf("%s:%d joined\n", __FILE__, __LINE__);
    return rval;
}

static const DBT zero_dbt = {0,0,0,0};

static DBT make_dbt (void *data, uint32_t size) {
    DBT result = zero_dbt;
    result.data = data;
    result.size = size;
    return result;
}

#define inc_error_count() error_count++

static TXNID leafentry_xid(FTLOADER bl, int which_db) {
    TXNID le_xid = TXNID_NONE;
    if (bl->root_xids_that_created && bl->load_root_xid != bl->root_xids_that_created[which_db])
        le_xid = bl->load_root_xid;
    return le_xid;
}

size_t ft_loader_leafentry_size(size_t key_size, size_t val_size, TXNID xid) {
    size_t s = 0;
    if (xid == TXNID_NONE)
        s = LE_CLEAN_MEMSIZE(val_size) + key_size + sizeof(uint32_t);
    else
        s = LE_MVCC_COMMITTED_MEMSIZE(val_size) + key_size + sizeof(uint32_t);
    return s;
}

static int process_primary_rows_internal (FTLOADER bl, struct rowset *primary_rowset)
// process the rows in primary_rowset, and then destroy the rowset.
// if FLUSH is true then write all the buffered rows out.
// if primary_rowset is NULL then treat it as empty.
{
    int error_count = 0;
    int *XMALLOC_N(bl->N, error_codes);

    // If we parallelize the first for loop, dest_keys/dest_vals init&cleanup need to move inside
    DBT_ARRAY dest_keys;
    DBT_ARRAY dest_vals;
    toku_dbt_array_init(&dest_keys, 1);
    toku_dbt_array_init(&dest_vals, 1);

    for (int i = 0; i < bl->N; i++) {
        unsigned int klimit,vlimit; // maximum row sizes.
        toku_ft_get_maximum_advised_key_value_lengths(&klimit, &vlimit);

        error_codes[i] = 0;
        struct rowset *rows = &(bl->rows[i]);
        struct merge_fileset *fs = &(bl->fs[i]);
        ft_compare_func compare = bl->bt_compare_funs[i];

        // Don't parallelize this loop, or we have to lock access to add_row() which would be a lot of overehad.
        // Also this way we can reuse the DB_DBT_REALLOC'd values inside dest_keys/dest_vals without a race.
        for (size_t prownum=0; prownum<primary_rowset->n_rows; prownum++) {
            if (error_count) break;

            struct row *prow = &primary_rowset->rows[prownum];
            DBT pkey = zero_dbt;
            DBT pval = zero_dbt;
            pkey.data = primary_rowset->data + prow->off;
            pkey.size = prow->klen;
            pval.data = primary_rowset->data + prow->off + prow->klen;
            pval.size = prow->vlen;


            DBT_ARRAY key_array;
            DBT_ARRAY val_array;
            if (bl->dbs[i] != bl->src_db) {
                int r = bl->generate_row_for_put(bl->dbs[i], bl->src_db, &dest_keys, &dest_vals, &pkey, &pval);
                if (r != 0) {
                    error_codes[i] = r;
                    inc_error_count();
                    break;
                }
                paranoid_invariant(dest_keys.size <= dest_keys.capacity);
                paranoid_invariant(dest_vals.size <= dest_vals.capacity);
                paranoid_invariant(dest_keys.size == dest_vals.size);

                key_array = dest_keys;
                val_array = dest_vals;
            } else {
                key_array.size = key_array.capacity = 1;
                key_array.dbts = &pkey;

                val_array.size = val_array.capacity = 1;
                val_array.dbts = &pval;
            }
            for (uint32_t row = 0; row < key_array.size; row++) {
                DBT *dest_key = &key_array.dbts[row];
                DBT *dest_val = &val_array.dbts[row];
                if (dest_key->size > klimit) {
                    error_codes[i] = EINVAL;
                    fprintf(stderr, "Key too big (keysize=%d bytes, limit=%d bytes)\n", dest_key->size, klimit);
                    inc_error_count();
                    break;
                }
                if (dest_val->size > vlimit) {
                    error_codes[i] = EINVAL;
                    fprintf(stderr, "Row too big (rowsize=%d bytes, limit=%d bytes)\n", dest_val->size, vlimit);
                    inc_error_count();
                    break;
                }

                bl->extracted_datasizes[i] += ft_loader_leafentry_size(dest_key->size, dest_val->size, leafentry_xid(bl, i));

                if (row_wont_fit(rows, dest_key->size + dest_val->size)) {
                    //printf("%s:%d rows.n_rows=%ld rows.n_bytes=%ld\n", __FILE__, __LINE__, rows->n_rows, rows->n_bytes);
                    int r = sort_and_write_rows(*rows, fs, bl, i, bl->dbs[i], compare); // cannot spawn this because of the race on rows.  If we were to create a new rows, and if sort_and_write_rows were to destroy the rows it is passed, we could spawn it, however.
                    // If we do spawn this, then we must account for the additional storage in the memory_per_rowset() function.
                    init_rowset(rows, memory_per_rowset_during_extract(bl)); // we passed the contents of rows to sort_and_write_rows.
                    if (r != 0) {
                        error_codes[i] = r;
                        inc_error_count();
                        break;
                    }
                }
                int r = add_row(rows, dest_key, dest_val);
                if (r != 0) {
                    error_codes[i] = r;
                    inc_error_count();
                    break;
                }
            }
        }
    }
    toku_dbt_array_destroy(&dest_keys);
    toku_dbt_array_destroy(&dest_vals);
    
    destroy_rowset(primary_rowset);
    toku_free(primary_rowset);
    int r = 0;
    if (error_count > 0) {
        for (int i=0; i<bl->N; i++) {
            if (error_codes[i]) {
                r = error_codes[i];
                ft_loader_set_panic(bl, r, false, i, nullptr, nullptr);
            }
        }
        invariant(r); // found the error 
    }
    toku_free(error_codes);
    return r;
}

static int process_primary_rows (FTLOADER bl, struct rowset *primary_rowset) {
    int r = process_primary_rows_internal (bl, primary_rowset);
    return r;
}
 
int toku_ft_loader_put (FTLOADER bl, DBT *key, DBT *val)
/* Effect: Put a key-value pair into the brt loader.  Called by DB_LOADER->put().
 * Return value: 0 on success, an error number otherwise.
 */
{
    if (ft_loader_get_error(&bl->error_callback)) 
        return EINVAL; // previous panic
    bl->n_rows++;
//    return loader_write_row(key, val, bl->fprimary_rows, &bl->fprimary_offset, bl);
    return loader_do_put(bl, key, val);
}

void toku_ft_loader_set_n_rows(FTLOADER bl, uint64_t n_rows) {
    bl->n_rows = n_rows;
}

uint64_t toku_ft_loader_get_n_rows(FTLOADER bl) {
    return bl->n_rows;
}

int merge_row_arrays_base (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
                           int which_db, DB *dest_db, ft_compare_func compare,
                           
                           FTLOADER bl,
                           struct rowset *rowset)
/* Effect: Given two arrays of rows, a and b, merge them using the comparison function, and write them into dest.
 *   This function is suitable for use in a mergesort.
 *   If a pair of duplicate keys is ever noticed, then call the error_callback function (if it exists), and return DB_KEYEXIST.
 * Arguments:
 *   dest    write the rows here
 *   a,b     the rows being merged
 *   an,bn   the lenth of a and b respectively.
 *   dest_db We need the dest_db to run the comparison function.
 *   compare We need the compare function for the dest_db.
 */
{
    while (an>0 && bn>0) {
        DBT akey; memset(&akey, 0, sizeof akey); akey.data=rowset->data+a->off; akey.size=a->klen;
        DBT bkey; memset(&bkey, 0, sizeof bkey); bkey.data=rowset->data+b->off; bkey.size=b->klen;

        int compare_result = compare(dest_db, &akey, &bkey);
        if (compare_result==0) {
            if (bl->error_callback.error_callback) {
                DBT aval; memset(&aval, 0, sizeof aval); aval.data=rowset->data + a->off + a->klen; aval.size = a->vlen;
                ft_loader_set_error(&bl->error_callback, DB_KEYEXIST, dest_db, which_db, &akey, &aval);
            }
            return DB_KEYEXIST;
        } else if (compare_result<0) {
            // a is smaller
            *dest = *a;
            dest++; a++; an--;
        } else {
            *dest = *b;
            dest++; b++; bn--;
        }
    }
    while (an>0) {
        *dest = *a;
        dest++; a++; an--;
    }
    while (bn>0) {
        *dest = *b;
        dest++; b++; bn--;
    }
    return 0;
}

static int binary_search (int *location,
                          const DBT *key,
                          struct row a[/*an*/], int an,
                          int abefore,
                          int which_db, DB *dest_db, ft_compare_func compare,
                          FTLOADER bl,
                          struct rowset *rowset)
// Given a sorted array of rows a, and a dbt key, find the first row in a that is > key.
// If no such row exists, then consider the result to be equal to an.
// On success store abefore+the index into *location
// Return 0 on success.
// Return DB_KEYEXIST if we find a row that is equal to key.
{
    if (an==0) {
        *location = abefore;
        return 0;
    } else {
        int a2 = an/2;
        DBT akey = make_dbt(rowset->data+a[a2].off,  a[a2].klen);
        int compare_result = compare(dest_db, key, &akey);
        if (compare_result==0) {
            if (bl->error_callback.error_callback) {
                DBT aval = make_dbt(rowset->data + a[a2].off + a[a2].klen,  a[a2].vlen);
                ft_loader_set_error(&bl->error_callback, DB_KEYEXIST, dest_db, which_db, &akey, &aval);
            }
            return DB_KEYEXIST;
        } else if (compare_result<0) {
            // key is before a2
            if (an==1) {
                *location = abefore;
                return 0;
            } else {
                return binary_search(location, key,
                                     a,    a2,
                                     abefore,
                                     which_db, dest_db, compare, bl, rowset);
            }
        } else {
            // key is after a2
            if (an==1) {
                *location = abefore + 1;
                return 0;
            } else {
                return binary_search(location, key,
                                     a+a2, an-a2,
                                     abefore+a2,
                                     which_db, dest_db, compare, bl, rowset);
            }
        }
    }
}
                   

#define SWAP(typ,x,y) { typ tmp = x; x=y; y=tmp; }

static int merge_row_arrays (struct row dest[/*an+bn*/], struct row a[/*an*/], int an, struct row b[/*bn*/], int bn,
                             int which_db, DB *dest_db, ft_compare_func compare,
                             FTLOADER bl,
                             struct rowset *rowset)
/* Effect: Given two sorted arrays of rows, a and b, merge them using the comparison function, and write them into dest.
 * Arguments:
 *   dest    write the rows here
 *   a,b     the rows being merged
 *   an,bn   the lenth of a and b respectively.
 *   dest_db We need the dest_db to run the comparison function.
 *   compare We need the compare function for the dest_db.
 */
{
    if (an + bn < 10000) {
        return merge_row_arrays_base(dest, a, an, b, bn, which_db, dest_db, compare, bl, rowset);
    }
    if (an < bn) {
        SWAP(struct row *,a, b)
        SWAP(int         ,an,bn)
    }
    // an >= bn
    int a2 = an/2;
    DBT akey = make_dbt(rowset->data+a[a2].off, a[a2].klen);
    int b2 = 0; // initialize to zero so we can add the answer in.
    {
        int r = binary_search(&b2, &akey, b, bn, 0, which_db, dest_db, compare, bl, rowset);
        if (r!=0) return r; // for example if we found a duplicate, called the error_callback, and now we return an error code.
    }
    int ra, rb;
    ra = merge_row_arrays(dest,       a,    a2,    b,    b2,    which_db, dest_db, compare, bl, rowset);
    rb = merge_row_arrays(dest+a2+b2, a+a2, an-a2, b+b2, bn-b2, which_db, dest_db, compare, bl, rowset);
    if (ra!=0) return ra;
    else       return rb;
}

int mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, ft_compare_func compare, FTLOADER bl, struct rowset *rowset)
/* Sort an array of rows (using mergesort).
 * Arguments:
 *   rows   sort this array of rows.
 *   n      the length of the array.
 *   dest_db  used by the comparison function.
 *   compare  the compare function
 */
{
    if (n<=1) return 0; // base case is sorted
    int mid = n/2;
    int r1, r2;
    r1 = mergesort_row_array (rows,     mid,   which_db, dest_db, compare, bl, rowset);

    // Don't spawn this one explicitly
    r2 =            mergesort_row_array (rows+mid, n-mid, which_db, dest_db, compare, bl, rowset);

    if (r1!=0) return r1;
    if (r2!=0) return r2;

    struct row *MALLOC_N(n, tmp); 
    if (tmp == NULL) return get_error_errno();
    {
        int r = merge_row_arrays(tmp, rows, mid, rows+mid, n-mid, which_db, dest_db, compare, bl, rowset);
        if (r!=0) {
            toku_free(tmp);
            return r;
        }
    }
    memcpy(rows, tmp, sizeof(*tmp)*n);
    toku_free(tmp);
    return 0;
}

// C function for testing mergesort_row_array 
int ft_loader_mergesort_row_array (struct row rows[/*n*/], int n, int which_db, DB *dest_db, ft_compare_func compare, FTLOADER bl, struct rowset *rowset) {
    return mergesort_row_array (rows, n, which_db, dest_db, compare, bl, rowset);
}

static int sort_rows (struct rowset *rows, int which_db, DB *dest_db, ft_compare_func compare,
                      FTLOADER bl)
/* Effect: Sort a collection of rows.
 * If any duplicates are found, then call the error_callback function and return non zero.
 * Otherwise return 0.
 * Arguments:
 *   rowset    the */
{
    return mergesort_row_array(rows->rows, rows->n_rows, which_db, dest_db, compare, bl, rows);
}

/* filesets Maintain a collection of files.  Typically these files are each individually sorted, and we will merge them.
 * These files have two parts, one is for the data rows, and the other is a collection of offsets so we an more easily parallelize the manipulation (e.g., by allowing us to find the offset of the ith row quickly). */

void init_merge_fileset (struct merge_fileset *fs)
/* Effect: Initialize a fileset */ 
{
    fs->have_sorted_output = false;
    fs->sorted_output      = FIDX_NULL;
    fs->prev_key           = zero_dbt;
    fs->prev_key.flags     = DB_DBT_REALLOC;

    fs->n_temp_files = 0;
    fs->n_temp_files_limit = 0;
    fs->data_fidxs = NULL;
}

void destroy_merge_fileset (struct merge_fileset *fs)
/* Effect: Destroy a fileset. */
{
    if ( fs ) {
        toku_destroy_dbt(&fs->prev_key);
        fs->n_temp_files = 0;
        fs->n_temp_files_limit = 0;
        toku_free(fs->data_fidxs);
        fs->data_fidxs = NULL;
    }
}


static int extend_fileset (FTLOADER bl, struct merge_fileset *fs, FIDX*ffile)
/* Effect: Add two files (one for data and one for idx) to the fileset.
 * Arguments:
 *   bl   the ft_loader (needed to panic if anything goes wrong, and also to get the temp_file_template.
 *   fs   the fileset
 *   ffile  the data file (which will be open)
 *   fidx   the index file (which will be open)
 */
{
    FIDX sfile;
    int r;
    r = ft_loader_open_temp_file(bl, &sfile); if (r!=0) return r;

    if (fs->n_temp_files+1 > fs->n_temp_files_limit) {
        fs->n_temp_files_limit = (fs->n_temp_files+1)*2;
        XREALLOC_N(fs->n_temp_files_limit, fs->data_fidxs);
    }
    fs->data_fidxs[fs->n_temp_files] = sfile;
    fs->n_temp_files++;

    *ffile = sfile;
    return 0;
}

// RFP maybe this should be buried in the ft_loader struct
static toku_mutex_t update_progress_lock = TOKU_MUTEX_INITIALIZER;

static int update_progress (int N,
                            FTLOADER bl,
                            const char *UU(message))
{
    // Must protect the increment and the call to the poll_function.
    toku_mutex_lock(&update_progress_lock);
    bl->progress+=N;

    int result;
    if (bl->progress_callback_result == 0) {
        //printf(" %20s: %d ", message, bl->progress);
        result = ft_loader_call_poll_function(&bl->poll_callback, (float)bl->progress/(float)PROGRESS_MAX);
        if (result!=0) {
            bl->progress_callback_result = result;
        }
    } else {
        result = bl->progress_callback_result;
    }
    toku_mutex_unlock(&update_progress_lock);
    return result;
}


static int write_rowset_to_file (FTLOADER bl, FIDX sfile, const struct rowset rows) {
    int r = 0;
    // Allocate a buffer if we're compressing intermediates.
    char *uncompressed_buffer = nullptr;
    if (bl->compress_intermediates) {
        MALLOC_N(MAX_UNCOMPRESSED_BUF, uncompressed_buffer);
        if (uncompressed_buffer == nullptr) {
            return ENOMEM;
        }
    }
    struct wbuf wb;
    wbuf_init(&wb, uncompressed_buffer, MAX_UNCOMPRESSED_BUF);

    FILE *sstream = toku_bl_fidx2file(bl, sfile);
    for (size_t i=0; i<rows.n_rows; i++) {
        DBT skey = make_dbt(rows.data + rows.rows[i].off,                     rows.rows[i].klen);
        DBT sval = make_dbt(rows.data + rows.rows[i].off + rows.rows[i].klen, rows.rows[i].vlen);
        
        uint64_t soffset=0; // don't really need this.
        r = loader_write_row(&skey, &sval, sfile, sstream, &soffset, &wb, bl);
        if (r != 0) {
            goto exit;
        }
    }

    if (bl->compress_intermediates && wb.ndone > 0) {
        r = bl_finish_compressed_write(sstream, &wb);
        if (r != 0) {
            goto exit;
        }
    }
    r = 0;
exit:
    if (uncompressed_buffer) {
        toku_free(uncompressed_buffer);
    }
    return r;
}


int sort_and_write_rows (struct rowset rows, struct merge_fileset *fs, FTLOADER bl, int which_db, DB *dest_db, ft_compare_func compare)
/* Effect: Given a rowset, sort it and write it to a temporary file.
 * Note:  The loader maintains for each index the most recently written-to file, as well as the DBT for the last key written into that file.
 *   If this rowset is sorted and all greater than that dbt, then we append to the file (skipping the sort, and reducing the number of temporary files).
 * Arguments:
 *   rows    the rowset
 *   fs      the fileset into which the sorted data will be added
 *   bl      the ft_loader
 *   dest_db the DB, needed for the comparison function.
 *   compare The comparison function.
 * Returns 0 on success, otherwise an error number.
 * Destroy the rowset after finishing it.
 * Note: There is no sense in trying to calculate progress by this function since it's done concurrently with the loader->put operation.
 * Note first time called: invariant: fs->have_sorted_output == false
 */
{
    //printf(" sort_and_write use %d progress=%d fin at %d\n", progress_allocation, bl->progress, bl->progress+progress_allocation);

    // TODO: erase the files, and deal with all the cleanup on error paths
    //printf("%s:%d sort_rows n_rows=%ld\n", __FILE__, __LINE__, rows->n_rows);
    //bl_time_t before_sort = bl_time_now();

    int result;
    if (rows.n_rows == 0) {
        result = 0;
    } else {
        result = sort_rows(&rows, which_db, dest_db, compare, bl);

        //bl_time_t after_sort = bl_time_now();

        if (result == 0) {
            DBT min_rowset_key = make_dbt(rows.data+rows.rows[0].off, rows.rows[0].klen);
            if (fs->have_sorted_output && compare(dest_db, &fs->prev_key, &min_rowset_key) < 0) {
                // write everything to the same output if the max key in the temp file (prev_key) is < min of the sorted rowset
                result = write_rowset_to_file(bl, fs->sorted_output, rows);
                if (result == 0) {
                    // set the max key in the temp file to the max key in the sorted rowset
                    result = toku_dbt_set(rows.rows[rows.n_rows-1].klen, rows.data + rows.rows[rows.n_rows-1].off, &fs->prev_key, NULL);
                }
            } else {
                // write the sorted rowset into a new temp file
                if (fs->have_sorted_output) {
                    fs->have_sorted_output = false;
                    result = ft_loader_fi_close(&bl->file_infos, fs->sorted_output, true);
                }
                if (result == 0) {
                    FIDX sfile = FIDX_NULL;
                    result = extend_fileset(bl, fs, &sfile);
                    if (result == 0) {
                        result = write_rowset_to_file(bl, sfile, rows);
                        if (result == 0) {
                            fs->have_sorted_output = true; fs->sorted_output = sfile;
                            // set the max key in the temp file to the max key in the sorted rowset
                            result = toku_dbt_set(rows.rows[rows.n_rows-1].klen, rows.data + rows.rows[rows.n_rows-1].off, &fs->prev_key, NULL);
                        }
                    }
                }
                // Note: if result == 0 then invariant fs->have_sorted_output == true
            }
        }
    }

    destroy_rowset(&rows);

    //bl_time_t after_write = bl_time_now();
    
    return result;
}

// C function for testing sort_and_write_rows
int ft_loader_sort_and_write_rows (struct rowset *rows, struct merge_fileset *fs, FTLOADER bl, int which_db, DB *dest_db, ft_compare_func compare) {
    return sort_and_write_rows (*rows, fs, bl, which_db, dest_db, compare);
}

int toku_merge_some_files_using_dbufio (const bool to_q, FIDX dest_data, QUEUE q, int n_sources, DBUFIO_FILESET bfs, FIDX srcs_fidxs[/*n_sources*/], FTLOADER bl, int which_db, DB *dest_db, ft_compare_func compare, int progress_allocation)
/* Effect: Given an array of FILE*'s each containing sorted, merge the data and write it to an output.  All the files remain open after the merge.
 *   This merge is performed in one pass, so don't pass too many files in.  If you need a tree of merges do it elsewhere.
 *   If TO_Q is true then we write rowsets into queue Q.  Otherwise we write into dest_data.
 * Modifies:  May modify the arrays of files (but if modified, it must be a permutation so the caller can use that array to close everything.)
 * Requires: The number of sources is at least one, and each of the input files must have at least one row in it.
 * Arguments:
 *   to_q         boolean indicating that output is queue (true) or a file (false)
 *   dest_data    where to write the sorted data
 *   q            where to write the sorted data
 *   n_sources    how many source files.
 *   srcs_data    the array of source data files.
 *   bl           the ft_loader.
 *   dest_db      the destination DB (used in the comparison function).
 * Return value: 0 on success, otherwise an error number.
 * The fidxs are not closed by this function.
 */
{
    int result = 0;

    FILE *dest_stream = to_q ? NULL : toku_bl_fidx2file(bl, dest_data);

    //printf(" merge_some_files progress=%d fin at %d\n", bl->progress, bl->progress+progress_allocation);
    DBT keys[n_sources];
    DBT vals[n_sources];
    uint64_t dataoff[n_sources];
    DBT zero = zero_dbt;  zero.flags=DB_DBT_REALLOC;

    for (int i=0; i<n_sources; i++) {
        keys[i] = vals[i] = zero; // fill these all in with zero so we can delete stuff more reliably.
    }

    pqueue_t      *pq = NULL;
    pqueue_node_t *MALLOC_N(n_sources, pq_nodes); // freed in cleanup
    if (pq_nodes == NULL) { result = get_error_errno(); }

    if (result==0) {
        int r = pqueue_init(&pq, n_sources, which_db, dest_db, compare, &bl->error_callback);
        if (r!=0) result = r; 
    }

    uint64_t n_rows = 0;
    if (result==0) {
        // load pqueue with first value from each source
        for (int i=0; i<n_sources; i++) {
            int r = loader_read_row_from_dbufio(bfs, i, &keys[i], &vals[i]);
            if (r==EOF) continue; // if the file is empty, don't initialize the pqueue.
            if (r!=0) {
                result = r;
                break;
            }

            pq_nodes[i].key = &keys[i];
            pq_nodes[i].val = &vals[i];
            pq_nodes[i].i   = i;
            r = pqueue_insert(pq, &pq_nodes[i]);
            if (r!=0) {
                result = r;
                // path tested by loader-dup-test5.tdbrun
                // printf("%s:%d returning\n", __FILE__, __LINE__);
                break;
            }

            dataoff[i] = 0;
            toku_mutex_lock(&bl->file_infos.lock);
            n_rows += bl->file_infos.file_infos[srcs_fidxs[i].idx].n_rows;
            toku_mutex_unlock(&bl->file_infos.lock);
        }
    }
    uint64_t n_rows_done = 0;

    struct rowset *output_rowset = NULL;
    if (result==0 && to_q) {
        XMALLOC(output_rowset); // freed in cleanup
        int r = init_rowset(output_rowset, memory_per_rowset_during_merge(bl, n_sources, to_q));
        if (r!=0) result = r;
    }

    // Allocate a buffer if we're compressing intermediates.
    char *uncompressed_buffer = nullptr;
    struct wbuf wb;
    if (bl->compress_intermediates && !to_q) {
        MALLOC_N(MAX_UNCOMPRESSED_BUF, uncompressed_buffer);
        if (uncompressed_buffer == nullptr) {
            result = ENOMEM;
        }
    }
    wbuf_init(&wb, uncompressed_buffer, MAX_UNCOMPRESSED_BUF);
    
    //printf(" n_rows=%ld\n", n_rows);
    while (result==0 && pqueue_size(pq)>0) {
        int mini;
        {
            // get the minimum 
            pqueue_node_t *node;
            int r = pqueue_pop(pq, &node);
            if (r!=0) {
                result = r;
                invariant(0);
                break;
            }
            mini = node->i;
        }
        if (to_q) {
            if (row_wont_fit(output_rowset, keys[mini].size + vals[mini].size)) {
                {
                    int r = queue_enq(q, (void*)output_rowset, 1, NULL);
                    if (r!=0) {
                        result = r;
                        break;
                    }
                }
                XMALLOC(output_rowset); // freed in cleanup
                {
                    int r = init_rowset(output_rowset, memory_per_rowset_during_merge(bl, n_sources, to_q));
                    if (r!=0) {        
                        result = r;
                        break;
                    }
                }
            }
            {
                int r = add_row(output_rowset, &keys[mini], &vals[mini]);
                if (r!=0) {
                    result = r;
                    break;
                }
            }
        } else {
            // write it to the dest file
            int r = loader_write_row(&keys[mini], &vals[mini], dest_data, dest_stream, &dataoff[mini], &wb, bl);
            if (r!=0) {
                result = r;
                break;
            }
        }
        
        {
            // read next row from file that just sourced min value 
            int r = loader_read_row_from_dbufio(bfs, mini, &keys[mini], &vals[mini]);
            if (r!=0) {
                if (r==EOF) {
                    // on feof, queue size permanently smaller
                    toku_free(keys[mini].data);  keys[mini].data = NULL;
                    toku_free(vals[mini].data);  vals[mini].data = NULL;
                } else {
                    fprintf(stderr, "%s:%d r=%d errno=%d bfs=%p mini=%d\n", __FILE__, __LINE__, r, get_maybe_error_errno(), bfs, mini);
                    dbufio_print(bfs);
                    result = r;
                    break;
                }
            } else {
                // insert value into queue (re-populate queue)
                pq_nodes[mini].key = &keys[mini];
                r = pqueue_insert(pq, &pq_nodes[mini]);
                if (r!=0) {
                    // Note: This error path tested by loader-dup-test1.tdbrun (and by loader-dup-test4)
                    result = r;
                    // printf("%s:%d returning\n", __FILE__, __LINE__);
                    break;
                }
            }
        }
 
        n_rows_done++;
        const uint64_t rows_per_report = size_factor*1024;
        if (n_rows_done%rows_per_report==0) {
            // need to update the progress.
            double fraction_of_remaining_we_just_did = (double)rows_per_report / (double)(n_rows - n_rows_done + rows_per_report);
            invariant(0<= fraction_of_remaining_we_just_did && fraction_of_remaining_we_just_did<=1);
            int progress_just_done = fraction_of_remaining_we_just_did * progress_allocation;
            progress_allocation -= progress_just_done;
            // ignore the result from update_progress here, we'll call update_progress again below, which will give us the nonzero result.
            int r = update_progress(progress_just_done, bl, "in file merge");
            if (0) printf("%s:%d Progress=%d\n", __FILE__, __LINE__, r);
        }
    }
    if (result == 0 && uncompressed_buffer != nullptr && wb.ndone > 0) {
        result = bl_finish_compressed_write(dest_stream, &wb);
    }

    if (result==0 && to_q) {
        int r = queue_enq(q, (void*)output_rowset, 1, NULL);
        if (r!=0) 
            result = r;
        else 
            output_rowset = NULL;
    }

    // cleanup
    if (uncompressed_buffer) {
        toku_free(uncompressed_buffer);
    }
    for (int i=0; i<n_sources; i++) {
        toku_free(keys[i].data);  keys[i].data = NULL;
        toku_free(vals[i].data);  vals[i].data = NULL;
    }
    if (output_rowset) {
        destroy_rowset(output_rowset);
        toku_free(output_rowset);
    }
    if (pq) { pqueue_free(pq); pq=NULL; }
    toku_free(pq_nodes);
    {
        int r = update_progress(progress_allocation, bl, "end of merge_some_files");
        //printf("%s:%d Progress=%d\n", __FILE__, __LINE__, r);
        if (r!=0 && result==0) result = r;
    }
    return result;
}

static int merge_some_files (const bool to_q, FIDX dest_data, QUEUE q, int n_sources, FIDX srcs_fidxs[/*n_sources*/], FTLOADER bl, int which_db, DB *dest_db, ft_compare_func compare, int progress_allocation)
{
    int result = 0;
    DBUFIO_FILESET bfs = NULL;
    int *MALLOC_N(n_sources, fds);
    if (fds==NULL) result=get_error_errno();
    if (result==0) {
        for (int i=0; i<n_sources; i++) {
            int r = fileno(toku_bl_fidx2file(bl, srcs_fidxs[i])); // we rely on the fact that when the files are closed, the fd is also closed.
            if (r==-1) {
                result=get_error_errno();
                break;
            }
            fds[i] = r;
        }
    }
    if (result==0) {
        int r = create_dbufio_fileset(&bfs, n_sources, fds,
                memory_per_rowset_during_merge(bl, n_sources, to_q), bl->compress_intermediates);
        if (r!=0) { result = r; }
    }
        
    if (result==0) {
        int r = toku_merge_some_files_using_dbufio (to_q, dest_data, q, n_sources, bfs, srcs_fidxs, bl, which_db, dest_db, compare, progress_allocation);
        if (r!=0) { result = r; }
    }

    if (bfs!=NULL) {
        if (result != 0)
            (void) panic_dbufio_fileset(bfs, result);
        int r = destroy_dbufio_fileset(bfs);
        if (r!=0 && result==0) result=r;
        bfs = NULL;
    }
    if (fds!=NULL) {
        toku_free(fds);
        fds = NULL;
    }
    return result;
}

static int int_min (int a, int b)
{
    if (a<b) return a;
    else return b;
}

static int n_passes (int N, int B) {
    int result = 0;
    while (N>1) {
        N = (N+B-1)/B;
        result++;
    }
    return result;
}

int merge_files (struct merge_fileset *fs,
                 FTLOADER bl,
                 // These are needed for the comparison function and error callback.
                 int which_db, DB *dest_db, ft_compare_func compare,
                 int progress_allocation,
                 // Write rowsets into this queue.
                 QUEUE output_q
                 )
/* Effect:  Given a fileset, merge all the files writing all the answers into a queue.
 *   All the files in fs, and any temporary files will be closed and unlinked (and the fileset will be empty)
 * Return value: 0 on success, otherwise an error number.
 *   On error *fs will contain no open files.  All the files (including any temporary files) will be closed and unlinked.
 *    (however the fs will still need to be deallocated.)
 */
{
    //printf(" merge_files %d files\n", fs->n_temp_files);
    //printf(" merge_files use %d progress=%d fin at %d\n", progress_allocation, bl->progress, bl->progress+progress_allocation);
    const int final_mergelimit   = (size_factor == 1) ? 4 : merge_fanin(bl, true); // try for a merge to the leaf level
    const int earlier_mergelimit = (size_factor == 1) ? 4 : merge_fanin(bl, false); // try for a merge at nonleaf.
    int n_passes_left  = (fs->n_temp_files<=final_mergelimit)
        ? 1
        : 1+n_passes((fs->n_temp_files+final_mergelimit-1)/final_mergelimit, earlier_mergelimit);
    // printf("%d files, %d on last pass, %d on earlier passes, %d passes\n", fs->n_temp_files, final_mergelimit, earlier_mergelimit, n_passes_left);
    int result = 0;
    while (fs->n_temp_files > 0) {
        int progress_allocation_for_this_pass = progress_allocation/n_passes_left;
        progress_allocation -= progress_allocation_for_this_pass;
        //printf("%s:%d n_passes_left=%d progress_allocation_for_this_pass=%d\n", __FILE__, __LINE__, n_passes_left, progress_allocation_for_this_pass);

        invariant(fs->n_temp_files>0);
        struct merge_fileset next_file_set;
        bool to_queue = (bool)(fs->n_temp_files <= final_mergelimit);
        init_merge_fileset(&next_file_set);
        while (fs->n_temp_files>0) {
            // grab some files and merge them.
            int n_to_merge = int_min(to_queue?final_mergelimit:earlier_mergelimit, fs->n_temp_files);

            // We are about to do n_to_merge/n_temp_files of the remaining for this pass.
            int progress_allocation_for_this_subpass = progress_allocation_for_this_pass * (double)n_to_merge / (double)fs->n_temp_files;
            // printf("%s:%d progress_allocation_for_this_subpass=%d n_temp_files=%d b=%llu\n", __FILE__, __LINE__, progress_allocation_for_this_subpass, fs->n_temp_files, (long long unsigned) memory_per_rowset_during_merge(bl, n_to_merge, to_queue));
            progress_allocation_for_this_pass -= progress_allocation_for_this_subpass;

            //printf("%s:%d merging\n", __FILE__, __LINE__);
            FIDX merged_data = FIDX_NULL;

            FIDX *XMALLOC_N(n_to_merge, data_fidxs);
            for (int i=0; i<n_to_merge; i++) {
                data_fidxs[i] = FIDX_NULL;
            }
            for (int i=0; i<n_to_merge; i++) {
                int idx = fs->n_temp_files -1 -i;
                FIDX fidx = fs->data_fidxs[idx];
                result = ft_loader_fi_reopen(&bl->file_infos, fidx, "r");
                if (result) break;
                data_fidxs[i] = fidx;
            }
            if (result==0 && !to_queue) {
                result = extend_fileset(bl, &next_file_set,  &merged_data);
            }

            if (result==0) {
                result = merge_some_files(to_queue, merged_data, output_q, n_to_merge, data_fidxs, bl, which_db, dest_db, compare, progress_allocation_for_this_subpass);
                // if result!=0, fall through
                if (result==0) {
                    /*nothing*/;// this is gratuitous, but we need something to give code coverage tools to help us know that it's important to distinguish between result==0 and result!=0
                }
            }

            //printf("%s:%d merged\n", __FILE__, __LINE__);
            for (int i=0; i<n_to_merge; i++) {
                if (!fidx_is_null(data_fidxs[i])) {
                    {
                        int r = ft_loader_fi_close(&bl->file_infos, data_fidxs[i], true);
                        if (r!=0 && result==0) result = r;
                    }
                    {
                        int r = ft_loader_fi_unlink(&bl->file_infos, data_fidxs[i]);
                        if (r!=0 && result==0) result = r;
                    }
                    data_fidxs[i] = FIDX_NULL;
                }
            }

            fs->n_temp_files -= n_to_merge;
            if (!to_queue && !fidx_is_null(merged_data)) {
                int r = ft_loader_fi_close(&bl->file_infos, merged_data, true);
                if (r!=0 && result==0) result = r;
            }
            toku_free(data_fidxs);

            if (result!=0) break;
        }

        destroy_merge_fileset(fs);
        *fs = next_file_set;

        // Update the progress
        n_passes_left--;

        if (result==0) { invariant(progress_allocation_for_this_pass==0); }

        if (result!=0) break;
    }
    if (result) ft_loader_set_panic(bl, result, true, which_db, nullptr, nullptr);

    {
        int r = queue_eof(output_q);
        if (r!=0 && result==0) result = r;
    }
    // It's conceivable that the progress_allocation could be nonzero (for example if bl->N==0)
    {
        int r = update_progress(progress_allocation, bl, "did merge_files");
        if (r!=0 && result==0) result = r;
    }
    return result;
}

struct subtree_info {
    int64_t block;
};

struct subtrees_info {
    int64_t next_free_block;
    int64_t n_subtrees;       // was n_blocks
    int64_t n_subtrees_limit;
    struct subtree_info *subtrees;
};

static void subtrees_info_init(struct subtrees_info *p) {
    p->next_free_block = p->n_subtrees = p->n_subtrees_limit = 0;
    p->subtrees = NULL;
}

static void subtrees_info_destroy(struct subtrees_info *p) {
    toku_free(p->subtrees);
    p->subtrees = NULL;
}

static void allocate_node (struct subtrees_info *sts, int64_t b) {
    if (sts->n_subtrees >= sts->n_subtrees_limit) {
        sts->n_subtrees_limit *= 2;
        XREALLOC_N(sts->n_subtrees_limit, sts->subtrees);
    }
    sts->subtrees[sts->n_subtrees].block = b;
    sts->n_subtrees++;
}

// dbuf will always contained 512-byte aligned buffer, but the length might not be a multiple of 512 bytes.  If that's what you want, then pad it.
struct dbuf {
    unsigned char *buf;
    int buflen;
    int off;
    int error;
};

struct leaf_buf {
    BLOCKNUM blocknum;
    TXNID xid;
    uint64_t nkeys, ndata, dsize;
    FTNODE node;
    XIDS xids;
    uint64_t off;
};

struct translation {
    int64_t off, size;
};

struct dbout {
    int fd;
    toku_off_t current_off;

    int64_t n_translations;
    int64_t n_translations_limit;
    struct translation *translation;
    toku_mutex_t mutex;
    FT h;
};

static inline void dbout_init(struct dbout *out, FT h) {
    out->fd = -1;
    out->current_off = 0;
    out->n_translations = out->n_translations_limit = 0;
    out->translation = NULL;
    toku_mutex_init(&out->mutex, NULL);
    out->h = h;
}

static inline void dbout_destroy(struct dbout *out) {
    if (out->fd >= 0) {
        toku_os_close(out->fd);
        out->fd = -1;
    }
    toku_free(out->translation);
    out->translation = NULL;
    toku_mutex_destroy(&out->mutex);
}

static inline void dbout_lock(struct dbout *out) {
    toku_mutex_lock(&out->mutex);
}

static inline void dbout_unlock(struct dbout *out) {
    toku_mutex_unlock(&out->mutex);
}

static void seek_align_locked(struct dbout *out) {
    toku_off_t old_current_off = out->current_off;
    int alignment = 4096;
    out->current_off += alignment-1;
    out->current_off &= ~(alignment-1);
    toku_off_t r = lseek(out->fd, out->current_off, SEEK_SET);
    invariant(r==out->current_off);
    invariant(out->current_off >= old_current_off);
    invariant(out->current_off < old_current_off+alignment);
    invariant(out->current_off % alignment == 0);
}

static void seek_align(struct dbout *out) {
    dbout_lock(out);
    seek_align_locked(out);
    dbout_unlock(out);
}

static void dbuf_init (struct dbuf *dbuf) {
    dbuf->buf = 0;
    dbuf->buflen = 0;
    dbuf->off = 0;
    dbuf->error = 0;
}

static void dbuf_destroy (struct dbuf *dbuf) {
    toku_free(dbuf->buf); dbuf->buf = NULL;
}

static int allocate_block (struct dbout *out, int64_t *ret_block_number)
// Return the new block number
{
    int result = 0;
    dbout_lock(out);
    int64_t block_number = out->n_translations;
    if (block_number >= out->n_translations_limit) {
        int64_t old_n_translations_limit = out->n_translations_limit;
        struct translation *old_translation = out->translation;
        if (out->n_translations_limit==0) {
            out->n_translations_limit = 1;
        } else {
            out->n_translations_limit *= 2;
        }
        REALLOC_N(out->n_translations_limit, out->translation);
        if (out->translation == NULL) {
            result = get_error_errno();
            invariant(result);
            out->n_translations_limit = old_n_translations_limit;
            out->translation = old_translation;
            goto cleanup;
        }
    }
    out->n_translations++;
    *ret_block_number = block_number;
cleanup:
    dbout_unlock(out);
    return result;
}

static void putbuf_bytes (struct dbuf *dbuf, const void *bytes, int nbytes) {
    if (!dbuf->error && dbuf->off + nbytes > dbuf->buflen) {
        unsigned char *oldbuf = dbuf->buf;
        int oldbuflen = dbuf->buflen;
        dbuf->buflen += dbuf->off + nbytes;
        dbuf->buflen *= 2;
        REALLOC_N_ALIGNED(512, dbuf->buflen, dbuf->buf);
        if (dbuf->buf == NULL) {
            dbuf->error = get_error_errno();
            dbuf->buf = oldbuf;
            dbuf->buflen = oldbuflen;
        }
    }
    if (!dbuf->error) {
        memcpy(dbuf->buf + dbuf->off, bytes, nbytes);
        dbuf->off += nbytes;
    }
}

static void putbuf_int32 (struct dbuf *dbuf, int v) {
    putbuf_bytes(dbuf, &v, 4);
}

static void putbuf_int64 (struct dbuf *dbuf, long long v) {
    putbuf_int32(dbuf, v>>32);
    putbuf_int32(dbuf, v&0xFFFFFFFF);
}

static struct leaf_buf *start_leaf (struct dbout *out, const DESCRIPTOR UU(desc), int64_t lblocknum, TXNID xid, uint32_t UU(target_nodesize)) {
    invariant(lblocknum < out->n_translations_limit);

    struct leaf_buf *XMALLOC(lbuf);
    lbuf->blocknum.b = lblocknum;
    lbuf->xid = xid;
    lbuf->nkeys = lbuf->ndata = lbuf->dsize = 0;
    lbuf->off = 0;

    lbuf->xids = xids_get_root_xids();
    if (xid != TXNID_NONE) {
        XIDS new_xids = NULL;
        int r = xids_create_child(lbuf->xids, &new_xids, xid); 
        assert(r == 0 && new_xids);
        xids_destroy(&lbuf->xids);
        lbuf->xids = new_xids;
    }

    FTNODE XMALLOC(node);
    toku_initialize_empty_ftnode(node, lbuf->blocknum, 0 /*height*/, 1 /*basement nodes*/, FT_LAYOUT_VERSION, 0);
    BP_STATE(node, 0) = PT_AVAIL;
    lbuf->node = node;

    return lbuf;
}

static void finish_leafnode (struct dbout *out, struct leaf_buf *lbuf, int progress_allocation, FTLOADER bl, uint32_t target_basementnodesize, enum toku_compression_method target_compression_method);
static int write_nonleaves (FTLOADER bl, FIDX pivots_fidx, struct dbout *out, struct subtrees_info *sts, const DESCRIPTOR descriptor, uint32_t target_nodesize, uint32_t target_basementnodesize, enum toku_compression_method target_compression_method);
static void add_pair_to_leafnode (struct leaf_buf *lbuf, unsigned char *key, int keylen, unsigned char *val, int vallen, int this_leafentry_size, STAT64INFO stats_to_update);
static int write_translation_table (struct dbout *out, long long *off_of_translation_p);
static int write_header (struct dbout *out, long long translation_location_on_disk, long long translation_size_on_disk);

static void drain_writer_q(QUEUE q) {
    void *item;
    while (1) {
        int r = queue_deq(q, &item, NULL, NULL);
        if (r == EOF)
            break;
        invariant(r == 0);
        struct rowset *rowset = (struct rowset *) item;
        destroy_rowset(rowset);
        toku_free(rowset);
    }
}

static void cleanup_maxkey(DBT *maxkey) {
    if (maxkey->flags == DB_DBT_REALLOC) {
        toku_free(maxkey->data);
        maxkey->data = NULL;
        maxkey->flags = 0;
    }
}

static void update_maxkey(DBT *maxkey, DBT *key) {
    cleanup_maxkey(maxkey);
    *maxkey = *key;
}

static int copy_maxkey(DBT *maxkey) {
    DBT newkey;
    toku_init_dbt_flags(&newkey, DB_DBT_REALLOC);
    int r = toku_dbt_set(maxkey->size, maxkey->data, &newkey, NULL);
    if (r == 0)
        update_maxkey(maxkey, &newkey);
    return r;
}

static int toku_loader_write_ft_from_q (FTLOADER bl,
                                         const DESCRIPTOR descriptor,
                                         int fd, // write to here
                                         int progress_allocation,
                                         QUEUE q,
                                         uint64_t total_disksize_estimate,
                                         int which_db,
                                         uint32_t target_nodesize,
                                         uint32_t target_basementnodesize,
                                         enum toku_compression_method target_compression_method)
// Effect: Consume a sequence of rowsets work from a queue, creating a fractal tree.  Closes fd.
{
    // set the number of fractal tree writer threads so that we can partition memory in the merger
    ft_loader_set_fractal_workers_count(bl);

    int result = 0;
    int r;

    // The pivots file will contain all the pivot strings (in the form <size(32bits)> <data>)
    // The pivots_fname is the name of the pivots file.
    // Note that the pivots file will have one extra pivot in it (the last key in the dictionary) which will not appear in the tree.
    int64_t n_pivots=0; // number of pivots in pivots_file
    FIDX pivots_file;  // the file

    r = ft_loader_open_temp_file (bl, &pivots_file);
    if (r) {
        result = r; 
        drain_writer_q(q); 
        return result;
    }
    FILE *pivots_stream = toku_bl_fidx2file(bl, pivots_file);

    TXNID root_xid_that_created = TXNID_NONE;
    if (bl->root_xids_that_created)
        root_xid_that_created = bl->root_xids_that_created[which_db];

    // TODO: (Zardosht/Yoni/Leif), do this code properly
    struct ft ft;
    toku_ft_init(&ft, (BLOCKNUM){0}, bl->load_lsn, root_xid_that_created, target_nodesize, target_basementnodesize, target_compression_method);

    struct dbout out;
    ZERO_STRUCT(out);
    dbout_init(&out, &ft);
    out.fd = fd;
    out.current_off = 8192; // leave 8K reserved at beginning
    out.n_translations = 3; // 3 translations reserved at the beginning
    out.n_translations_limit = 4;
    MALLOC_N(out.n_translations_limit, out.translation);
    if (out.translation == NULL) {
        result = get_error_errno();
        dbout_destroy(&out);
        drain_writer_q(q);
        toku_free(ft.h);
        return result;
    }

    // The blocks_array will contain all the block numbers that correspond to the pivots.  Generally there should be one more block than pivot.
    struct subtrees_info sts; 
    subtrees_info_init(&sts);
    sts.next_free_block  = 3;
    sts.n_subtrees       = 0;
    sts.n_subtrees_limit = 1;
    MALLOC_N(sts.n_subtrees_limit, sts.subtrees);
    if (sts.subtrees == NULL) {
        result = get_error_errno();
        subtrees_info_destroy(&sts);
        dbout_destroy(&out);
        drain_writer_q(q);
        toku_free(ft.h);
        return result;
    }

    out.translation[0].off = -2LL; out.translation[0].size = 0; // block 0 is NULL
    invariant(1==RESERVED_BLOCKNUM_TRANSLATION);
    invariant(2==RESERVED_BLOCKNUM_DESCRIPTOR);
    out.translation[1].off = -1;                                // block 1 is the block translation, filled in later
    out.translation[2].off = -1;                                // block 2 is the descriptor
    seek_align(&out);
    int64_t lblock = 0;  // make gcc --happy
    result = allocate_block(&out, &lblock);
    invariant(result == 0); // can not fail since translations reserved above

    TXNID le_xid = leafentry_xid(bl, which_db);
    struct leaf_buf *lbuf = start_leaf(&out, descriptor, lblock, le_xid, target_nodesize);
    uint64_t n_rows_remaining = bl->n_rows;
    uint64_t old_n_rows_remaining = bl->n_rows;

    uint64_t  used_estimate = 0;  // how much diskspace have we used up?

    DBT maxkey = make_dbt(0, 0); // keep track of the max key of the current node

    STAT64INFO_S deltas = ZEROSTATS;
    while (result == 0) {
        void *item;
        {
            int rr = queue_deq(q, &item, NULL, NULL);
            if (rr == EOF) break;
            if (rr != 0) {
                ft_loader_set_panic(bl, rr, true, which_db, nullptr, nullptr);
                break;
            }
        }
        struct rowset *output_rowset = (struct rowset *)item;

        for (unsigned int i = 0; i < output_rowset->n_rows; i++) {
            DBT key = make_dbt(output_rowset->data+output_rowset->rows[i].off,                               output_rowset->rows[i].klen);
            DBT val = make_dbt(output_rowset->data+output_rowset->rows[i].off + output_rowset->rows[i].klen, output_rowset->rows[i].vlen);

            size_t this_leafentry_size = ft_loader_leafentry_size(key.size, val.size, le_xid);

            used_estimate += this_leafentry_size;

            // Spawn off a node if
            //   a) there is at least one row in it, and
            //   b) this item would make the nodesize too big, or
            //   c) the remaining amount won't fit in the current node and the current node's data is more than the remaining amount
            uint64_t remaining_amount = total_disksize_estimate - used_estimate;
            uint64_t used_here = lbuf->off + 1000;             // leave 1000 for various overheads.
            uint64_t target_size = (target_nodesize*7L)/8;     // use only 7/8 of the node.
            uint64_t used_here_with_next_key = used_here + this_leafentry_size;
            if (lbuf->nkeys > 0 &&
                ((used_here_with_next_key >= target_size) || (used_here + remaining_amount >= target_size && lbuf->off > remaining_amount))) {

                int progress_this_node = progress_allocation * (double)(old_n_rows_remaining - n_rows_remaining)/(double)old_n_rows_remaining;
                progress_allocation -= progress_this_node;
                old_n_rows_remaining = n_rows_remaining;

                allocate_node(&sts, lblock);

                n_pivots++;

                invariant(maxkey.data != NULL);
                if ((r = bl_write_dbt(&maxkey, pivots_stream, NULL, nullptr, bl))) {
                    ft_loader_set_panic(bl, r, true, which_db, nullptr, nullptr);
                    if (result == 0) result = r;
                    break;
                }

                finish_leafnode(&out, lbuf, progress_this_node, bl, target_basementnodesize, target_compression_method);
                lbuf = NULL;

                r = allocate_block(&out, &lblock);
                if (r != 0) {
                    ft_loader_set_panic(bl, r, true, which_db, nullptr, nullptr);
                    if (result == 0) result = r;
                    break;
                }
                lbuf = start_leaf(&out, descriptor, lblock, le_xid, target_nodesize);
            }

            add_pair_to_leafnode(lbuf, (unsigned char *) key.data, key.size, (unsigned char *) val.data, val.size, this_leafentry_size, &deltas);
            n_rows_remaining--;

            update_maxkey(&maxkey, &key); // set the new maxkey to the current key
        }

        r = copy_maxkey(&maxkey); // make a copy of maxkey before the rowset is destroyed
        if (result == 0)
            result = r;
        destroy_rowset(output_rowset);
        toku_free(output_rowset);

        if (result == 0)
            result = ft_loader_get_error(&bl->error_callback); // check if an error was posted and terminate this quickly
    }

    if (deltas.numrows || deltas.numbytes) {
        toku_ft_update_stats(&ft.in_memory_stats, deltas);
    }

    cleanup_maxkey(&maxkey);

    if (lbuf) {
        allocate_node(&sts, lblock);
        {
            int p = progress_allocation/2;
            finish_leafnode(&out, lbuf, p, bl, target_basementnodesize, target_compression_method);
            progress_allocation -= p;
        }
    }


    if (result == 0) {
        result = ft_loader_get_error(&bl->error_callback); // if there were any prior errors then exit
    }

    if (result != 0) goto error;

    // We haven't paniced, so the sum should add up.
    invariant(used_estimate == total_disksize_estimate);

    n_pivots++;

    {
        DBT key = make_dbt(0,0); // must write an extra DBT into the pivots file.
        r = bl_write_dbt(&key, pivots_stream, NULL, nullptr, bl);
        if (r) { 
            result = r; goto error;
        }
    }

    r = write_nonleaves(bl, pivots_file, &out, &sts, descriptor, target_nodesize, target_basementnodesize, target_compression_method);
    if (r) {
        result = r; goto error;
    }

    {
        invariant(sts.n_subtrees==1);
        out.h->h->root_blocknum = make_blocknum(sts.subtrees[0].block);
        toku_free(sts.subtrees); sts.subtrees = NULL;

        // write the descriptor
        {
            seek_align(&out);
            invariant(out.n_translations >= RESERVED_BLOCKNUM_DESCRIPTOR);
            invariant(out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].off == -1);
            out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].off = out.current_off;
            size_t desc_size = 4+toku_serialize_descriptor_size(descriptor);
            invariant(desc_size>0);
            out.translation[RESERVED_BLOCKNUM_DESCRIPTOR].size = desc_size;
            struct wbuf wbuf;
            char *XMALLOC_N(desc_size, buf);
            wbuf_init(&wbuf, buf, desc_size);
            toku_serialize_descriptor_contents_to_wbuf(&wbuf, descriptor);
            uint32_t checksum = x1764_finish(&wbuf.checksum);
            wbuf_int(&wbuf, checksum);
            invariant(wbuf.ndone==desc_size);
            r = toku_os_write(out.fd, wbuf.buf, wbuf.ndone);
            out.current_off += desc_size;
            toku_free(buf);    // wbuf_destroy
            if (r) {
                result = r; goto error;
            }
        }

        long long off_of_translation;
        r = write_translation_table(&out, &off_of_translation);
        if (r) {
            result = r; goto error;
        }

        r = write_header(&out, off_of_translation, (out.n_translations+1)*16+4);
        if (r) {
            result = r; goto error;
        }

        r = update_progress(progress_allocation, bl, "wrote tdb file");
        if (r) {
            result = r; goto error;
        }
    }

    r = fsync(out.fd);
    if (r) {
        result = get_error_errno(); goto error;
    }

    // Do we need to pay attention to user_said_stop?  Or should the guy at the other end of the queue pay attention and send in an EOF.

 error:
    {
        int rr = toku_os_close(fd);
        if (rr)
            result = get_error_errno();
    }
    out.fd = -1;

    subtrees_info_destroy(&sts);
    dbout_destroy(&out);
    drain_writer_q(q);
    toku_free(ft.h);

    return result;
}

int toku_loader_write_brt_from_q_in_C (FTLOADER                bl,
                                       const DESCRIPTOR descriptor,
                                       int                      fd, // write to here
                                       int                      progress_allocation,
                                       QUEUE                    q,
                                       uint64_t                 total_disksize_estimate,
                                       int                      which_db,
                                       uint32_t                 target_nodesize,
                                       uint32_t                 target_basementnodesize,
                                       enum toku_compression_method target_compression_method)
// This is probably only for testing.
{
    target_nodesize = target_nodesize == 0 ? default_loader_nodesize : target_nodesize;
    target_basementnodesize = target_basementnodesize == 0 ? default_loader_basementnodesize : target_basementnodesize;
    return toku_loader_write_ft_from_q (bl, descriptor, fd, progress_allocation, q, total_disksize_estimate, which_db, target_nodesize, target_basementnodesize, target_compression_method);
}


static void* fractal_thread (void *ftav) {
    struct fractal_thread_args *fta = (struct fractal_thread_args *)ftav;
    int r = toku_loader_write_ft_from_q (fta->bl, fta->descriptor, fta->fd, fta->progress_allocation, fta->q, fta->total_disksize_estimate, fta->which_db, fta->target_nodesize, fta->target_basementnodesize, fta->target_compression_method);
    fta->errno_result = r;
    return NULL;
}

static int loader_do_i (FTLOADER bl,
                        int which_db,
                        DB *dest_db,
                        ft_compare_func compare,
                        const DESCRIPTOR descriptor,
                        const char *new_fname,
                        int progress_allocation // how much progress do I need to add into bl->progress by the end..
                        )
/* Effect: Handle the file creating for one particular DB in the bulk loader. */
/* Requires: The data is fully extracted, so we can do merges out of files and write the ft file. */
{
    //printf("doing i use %d progress=%d fin at %d\n", progress_allocation, bl->progress, bl->progress+progress_allocation);
    struct merge_fileset *fs = &(bl->fs[which_db]);
    struct rowset *rows = &(bl->rows[which_db]);
    invariant(rows->data==NULL); // the rows should be all cleaned up already

    // a better allocation would be to figure out roughly how many merge passes we'll need.
    int allocation_for_merge = (2*progress_allocation)/3;
    progress_allocation -= allocation_for_merge;

    int r;
    r = queue_create(&bl->fractal_queues[which_db], FRACTAL_WRITER_QUEUE_DEPTH);
    if (r) goto error;

    {
        mode_t mode = S_IRWXU|S_IRWXG|S_IRWXO;
        int fd = toku_os_open(new_fname, O_RDWR| O_CREAT | O_BINARY, mode); // #2621
        if (fd < 0) {
            r = get_error_errno(); goto error;
        }

        uint32_t target_nodesize, target_basementnodesize;
        enum toku_compression_method target_compression_method;
        r = dest_db->get_pagesize(dest_db, &target_nodesize);
        invariant_zero(r);
        r = dest_db->get_readpagesize(dest_db, &target_basementnodesize);
        invariant_zero(r);
        r = dest_db->get_compression_method(dest_db, &target_compression_method);
        invariant_zero(r);

        // This structure must stay live until the join below.
        struct fractal_thread_args fta = { bl,
                                           descriptor,
                                           fd,
                                           progress_allocation,
                                           bl->fractal_queues[which_db],
                                           bl->extracted_datasizes[which_db],
                                           0,
                                           which_db,
                                           target_nodesize,
                                           target_basementnodesize,
                                           target_compression_method,
        };

        r = toku_pthread_create(bl->fractal_threads+which_db, NULL, fractal_thread, (void*)&fta);
        if (r) {
            int r2 __attribute__((__unused__)) = queue_destroy(bl->fractal_queues[which_db]);
            // ignore r2, since we already have an error
            goto error;
        }
        invariant(bl->fractal_threads_live[which_db]==false);
        bl->fractal_threads_live[which_db] = true;

        r = merge_files(fs, bl, which_db, dest_db, compare, allocation_for_merge, bl->fractal_queues[which_db]);

        {
            void *toku_pthread_retval;
            int r2 = toku_pthread_join(bl->fractal_threads[which_db], &toku_pthread_retval);
            invariant(fta.bl==bl); // this is a gratuitous assertion to make sure that the fta struct is still live here.  A previous bug but that struct into a C block statement.
            resource_assert_zero(r2);
            invariant(toku_pthread_retval==NULL);
            invariant(bl->fractal_threads_live[which_db]);
            bl->fractal_threads_live[which_db] = false;
            if (r == 0) r = fta.errno_result;
        }
    }

 error: // this is the cleanup code.  Even if r==0 (no error) we fall through to here.
    {
        int r2 = queue_destroy(bl->fractal_queues[which_db]);
        invariant(r2==0);
        bl->fractal_queues[which_db]=NULL;
    }

    // if we get here we need to free up the merge_fileset and the rowset, as well as the keys
    toku_free(rows->data); rows->data = NULL;
    toku_free(rows->rows); rows->rows = NULL;
    toku_free(fs->data_fidxs); fs->data_fidxs = NULL;
    return r;
}

static int toku_ft_loader_close_internal (FTLOADER bl)
/* Effect: Close the bulk loader.
 * Return all the file descriptors in the array fds. */
{
    int result = 0;
    if (bl->N == 0)
        result = update_progress(PROGRESS_MAX, bl, "done");
    else {
        int remaining_progress = PROGRESS_MAX;
        for (int i = 0; i < bl->N; i++) {
            // Take the unallocated progress and divide it among the unfinished jobs.
            // This calculation allocates all of the PROGRESS_MAX bits of progress to some job.
            int allocate_here = remaining_progress/(bl->N - i);
            remaining_progress -= allocate_here;
            char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(bl->cachetable, bl->new_fnames_in_env[i]);
            result = loader_do_i(bl, i, bl->dbs[i], bl->bt_compare_funs[i], bl->descriptors[i], fname_in_cwd, allocate_here);
            toku_free(fname_in_cwd);
            if (result != 0) 
                goto error;
            invariant(0 <= bl->progress && bl->progress <= PROGRESS_MAX);
        }
        if (result==0) invariant(remaining_progress==0);

        // fsync the directory containing the new tokudb files.
        char *fname0 = toku_cachetable_get_fname_in_cwd(bl->cachetable, bl->new_fnames_in_env[0]);
        int r = toku_fsync_directory(fname0);
        toku_free(fname0);
        if (r != 0) {
            result = r; goto error;
        }
    }
    invariant(bl->file_infos.n_files_open   == 0);
    invariant(bl->file_infos.n_files_extant == 0);
    invariant(bl->progress == PROGRESS_MAX);
 error:
    toku_ft_loader_internal_destroy(bl, (bool)(result!=0));
    return result;
}

int toku_ft_loader_close (FTLOADER bl,
                           ft_loader_error_func error_function, void *error_extra,
                           ft_loader_poll_func  poll_function,  void *poll_extra
                           )
{
    int result = 0;

    int r;

    //printf("Closing\n");

    ft_loader_set_error_function(&bl->error_callback, error_function, error_extra);

    ft_loader_set_poll_function(&bl->poll_callback, poll_function, poll_extra);

    if (bl->extractor_live) {
        r = finish_extractor(bl);
        if (r)
            result = r;
        invariant(!bl->extractor_live);
    }

    // check for an error during extraction
    if (result == 0) {
        r = ft_loader_call_error_function(&bl->error_callback);
        if (r)
            result = r;
    }

    if (result == 0) {
        r = toku_ft_loader_close_internal(bl);
        if (r && result == 0)
            result = r;
    } else
        toku_ft_loader_internal_destroy(bl, true);

    return result;
}

int toku_ft_loader_finish_extractor(FTLOADER bl) {
    int result = 0;
    if (bl->extractor_live) {
        int r = finish_extractor(bl);
        if (r)
            result = r;
        invariant(!bl->extractor_live);
    } else
        result = EINVAL;
    return result;
}

int toku_ft_loader_abort(FTLOADER bl, bool is_error) 
/* Effect : Abort the bulk loader, free ft_loader resources */
{
    int result = 0;

    // cleanup the extractor thread
    if (bl->extractor_live) {
        int r = finish_extractor(bl);
        if (r)
            result = r;
        invariant(!bl->extractor_live);
    }

    for (int i = 0; i < bl->N; i++)
        invariant(!bl->fractal_threads_live[i]);

    toku_ft_loader_internal_destroy(bl, is_error);
    return result;
}

int toku_ft_loader_get_error(FTLOADER bl, int *error) {
    *error = ft_loader_get_error(&bl->error_callback);
    return 0;
}

static void add_pair_to_leafnode (struct leaf_buf *lbuf, unsigned char *key, int keylen, unsigned char *val, int vallen, int this_leafentry_size, STAT64INFO stats_to_update) {
    lbuf->nkeys++;
    lbuf->ndata++;
    lbuf->dsize += keylen + vallen;
    lbuf->off += this_leafentry_size;

    // append this key val pair to the leafnode 
    // #3588 TODO just make a clean ule and append it to the omt
    // #3588 TODO can do the rebalancing here and avoid a lot of work later
    FTNODE leafnode = lbuf->node;
    uint32_t idx = BLB_DATA(leafnode, 0)->omt_size();
    DBT thekey = { .data = key, .size = (uint32_t) keylen }; 
    DBT theval = { .data = val, .size = (uint32_t) vallen };
    FT_MSG_S cmd = { .type = FT_INSERT,
                     .msn = ZERO_MSN,
                     .xids = lbuf->xids,
                     .u = { .id = { &thekey, &theval } } };
    uint64_t workdone=0;
    toku_ft_bn_apply_cmd_once(BLB(leafnode,0), &cmd, idx, NULL, TXNID_NONE, make_gc_info(true), &workdone, stats_to_update);
}

static int write_literal(struct dbout *out, void*data,  size_t len) {
    invariant(out->current_off%4096==0);
    int result = toku_os_write(out->fd, data, len);
    if (result == 0)
        out->current_off+=len;
    return result;
}

static void finish_leafnode (struct dbout *out, struct leaf_buf *lbuf, int progress_allocation, FTLOADER bl, uint32_t target_basementnodesize, enum toku_compression_method target_compression_method) {
    int result = 0;

    // serialize leaf to buffer
    size_t serialized_leaf_size = 0;
    size_t uncompressed_serialized_leaf_size = 0;
    char *serialized_leaf = NULL;
    FTNODE_DISK_DATA ndd = NULL;
    result = toku_serialize_ftnode_to_memory(lbuf->node, &ndd, target_basementnodesize, target_compression_method, true, true, &serialized_leaf_size, &uncompressed_serialized_leaf_size, &serialized_leaf);

    // write it out
    if (result == 0) {
        dbout_lock(out);
        long long off_of_leaf = out->current_off;
        result = write_literal(out, serialized_leaf, serialized_leaf_size);
        if (result == 0) {
            out->translation[lbuf->blocknum.b].off  = off_of_leaf;
            out->translation[lbuf->blocknum.b].size = serialized_leaf_size;
            seek_align_locked(out);
        }
        dbout_unlock(out);
    }

    // free the node
    if (serialized_leaf) {
        toku_free(ndd);
        toku_free(serialized_leaf);
    }
    toku_ftnode_free(&lbuf->node);
    xids_destroy(&lbuf->xids);
    toku_free(lbuf);

    //printf("Nodewrite %d (%.1f%%):", progress_allocation, 100.0*progress_allocation/PROGRESS_MAX);
    if (result == 0)
        result = update_progress(progress_allocation, bl, "wrote node");

    if (result)
        ft_loader_set_panic(bl, result, true, 0, nullptr, nullptr);
}

static int write_translation_table (struct dbout *out, long long *off_of_translation_p) {
    seek_align(out);
    struct dbuf ttable;
    dbuf_init(&ttable);
    long long off_of_translation = out->current_off;
    long long bt_size_on_disk = out->n_translations * 16 + 20;
    putbuf_int64(&ttable, out->n_translations);    // number of records
    putbuf_int64(&ttable, -1LL); // the linked list
    out->translation[1].off = off_of_translation;
    out->translation[1].size = bt_size_on_disk;
    for (int i=0; i<out->n_translations; i++) {
        putbuf_int64(&ttable, out->translation[i].off);
        putbuf_int64(&ttable, out->translation[i].size);
    }
    unsigned int checksum = x1764_memory(ttable.buf, ttable.off);
    putbuf_int32(&ttable, checksum);
    // pad it to 512 zeros
    long long encoded_length = ttable.off;
    {
        int nbytes_to_add = roundup_to_multiple(512, ttable.off) - encoded_length;
        char zeros[nbytes_to_add];
        for (int i=0; i<nbytes_to_add; i++) zeros[i]=0;
        putbuf_bytes(&ttable, zeros, nbytes_to_add);
    }
    int result = ttable.error;
    if (result == 0) {
        invariant(bt_size_on_disk==encoded_length);
        result = toku_os_pwrite(out->fd, ttable.buf, ttable.off, off_of_translation);
    }
    dbuf_destroy(&ttable);
    *off_of_translation_p = off_of_translation;
    return result;
}

static int
write_header (struct dbout *out, long long translation_location_on_disk, long long translation_size_on_disk) {
    int result = 0;
    size_t size = toku_serialize_ft_size(out->h->h);
    size_t alloced_size = roundup_to_multiple(512, size);
    struct wbuf wbuf;
    char *MALLOC_N_ALIGNED(512, alloced_size, buf);
    if (buf == NULL) {
        result = get_error_errno();
    } else {
        wbuf_init(&wbuf, buf, size);
        out->h->h->on_disk_stats = out->h->in_memory_stats;
        toku_serialize_ft_to_wbuf(&wbuf, out->h->h, translation_location_on_disk, translation_size_on_disk);
        for (size_t i=size; i<alloced_size; i++) buf[i]=0; // initialize all those unused spots to zero
        if (wbuf.ndone != size)
            result = EINVAL;
        else {
            assert(wbuf.ndone <= alloced_size);
            result = toku_os_pwrite(out->fd, wbuf.buf, alloced_size, 0);
        }
        toku_free(buf);
    }
    return result;
}

static int read_some_pivots (FIDX pivots_file, int n_to_read, FTLOADER bl,
                      /*out*/ DBT pivots[/*n_to_read*/])
// pivots is an array to be filled in.  The pivots array is uninitialized.
{
    for (int i = 0; i < n_to_read; i++)
        pivots[i] = zero_dbt;

    FILE *pivots_stream = toku_bl_fidx2file(bl, pivots_file);

    int result = 0;
    for (int i = 0; i < n_to_read; i++) {
        int r = bl_read_dbt(&pivots[i], pivots_stream);
        if (r != 0) {
            result = r;
            break;
        }
    }
    return result;
}

static void delete_pivots(DBT pivots[], int n) {
    for (int i = 0; i < n; i++) 
        toku_free(pivots[i].data);
    toku_free(pivots);
}

static int setup_nonleaf_block (int n_children,
                                struct subtrees_info *subtrees,         FIDX pivots_file,        int64_t first_child_offset_in_subtrees,
                                struct subtrees_info *next_subtrees,    FIDX next_pivots_file,
                                struct dbout *out, FTLOADER bl,
                                /*out*/int64_t *blocknum,
                                /*out*/struct subtree_info **subtrees_info_p,
                                /*out*/DBT **pivots_p)
// Do the serial part of setting up a non leaf block.
//   Read the pivots out of the file, and store them in a newly allocated array of DBTs (returned in *pivots_p)  There are (n_blocks_to_use-1) of these.
//   Copy the final pivot into the next_pivots file instead of returning it.
//   Copy the subtree_info from the subtrees structure, and store them in a newly allocated array of subtree_infos (return in *subtrees_info_p).  There are n_blocks_to_use of these.
//   Allocate a block number and return it in *blocknum.
//   Store the blocknum in the next_blocks structure, so it can be combined with the pivots at the next level of the tree.
//   Update n_blocks_used and n_translations.
// This code cannot be called in parallel because of all the race conditions.
// The actual creation of the node can be called in parallel after this work is done.
{
    //printf("Nonleaf has children :"); for(int i=0; i<n_children; i++) printf(" %ld", subtrees->subtrees[i].block); printf("\n");

    int result = 0;
    
    DBT *MALLOC_N(n_children, pivots);
    if (pivots == NULL) {
        result = get_error_errno();
    }

    if (result == 0) {
        int r = read_some_pivots(pivots_file, n_children, bl, pivots);
        if (r)
            result = r;
    }

    if (result == 0) {
        FILE *next_pivots_stream = toku_bl_fidx2file(bl, next_pivots_file);
        int r = bl_write_dbt(&pivots[n_children-1], next_pivots_stream, NULL, nullptr, bl);
        if (r)
            result = r;
    }

    if (result == 0) {
        // The last pivot was written to the next_pivots file, so we free it now instead of returning it.
        toku_free(pivots[n_children-1].data);
        pivots[n_children-1] = zero_dbt;

        struct subtree_info *XMALLOC_N(n_children, subtrees_array);
        for (int i = 0; i < n_children; i++) {
            int64_t from_blocknum = first_child_offset_in_subtrees + i;
            subtrees_array[i] = subtrees->subtrees[from_blocknum];
        }

        int r = allocate_block(out, blocknum);
        if (r) {
            toku_free(subtrees_array);
            result = r;
        } else {
            allocate_node(next_subtrees, *blocknum);
            
            *pivots_p = pivots;
            *subtrees_info_p = subtrees_array;
        }
    }

    if (result != 0) {
        if (pivots) {
            delete_pivots(pivots, n_children); pivots = NULL;
        }
    }

    return result;
}

static void write_nonleaf_node (FTLOADER bl, struct dbout *out, int64_t blocknum_of_new_node, int n_children,
                                DBT *pivots, /* must free this array, as well as the things it points t */
                                struct subtree_info *subtree_info, int height, const DESCRIPTOR UU(desc), uint32_t UU(target_nodesize), uint32_t target_basementnodesize, enum toku_compression_method target_compression_method)
{
    //Nodes do not currently touch descriptors
    invariant(height > 0);

    int result = 0;

    FTNODE XMALLOC(node);
    toku_initialize_empty_ftnode(node, make_blocknum(blocknum_of_new_node), height, n_children,
                                  FT_LAYOUT_VERSION, 0);
    node->totalchildkeylens = 0;
    for (int i=0; i<n_children-1; i++) {
        toku_clone_dbt(&node->childkeys[i], pivots[i]);
        node->totalchildkeylens += pivots[i].size;
    }
    assert(node->bp);
    for (int i=0; i<n_children; i++) {
        BP_BLOCKNUM(node,i)  = make_blocknum(subtree_info[i].block); 
        BP_STATE(node,i) = PT_AVAIL;
    }

    FTNODE_DISK_DATA ndd = NULL;
    if (result == 0) {
        size_t n_bytes;
        size_t n_uncompressed_bytes;
        char *bytes;
        int r;
        r = toku_serialize_ftnode_to_memory(node, &ndd, target_basementnodesize, target_compression_method, true, true, &n_bytes, &n_uncompressed_bytes, &bytes);
        if (r) {
            result = r;
        } else {
            dbout_lock(out);
            out->translation[blocknum_of_new_node].off = out->current_off;
            out->translation[blocknum_of_new_node].size = n_bytes;
            //fprintf(stderr, "Wrote internal node at %ld (%ld bytes)\n", out->current_off, n_bytes);
            //for (uint32_t i=0; i<n_bytes; i++) { unsigned char b = bytes[i]; printf("%d:%02x (%d) ('%c')\n", i, b, b, (b>=' ' && b<128) ? b : '*'); }
            r = write_literal(out, bytes, n_bytes); 
            if (r)
                result = r;
            else
                seek_align_locked(out);
            dbout_unlock(out);
            toku_free(bytes);
        }
    }

    for (int i=0; i<n_children-1; i++) {
        toku_free(pivots[i].data);
        toku_free(node->childkeys[i].data);
    }
    for (int i=0; i<n_children; i++) {
        destroy_nonleaf_childinfo(BNC(node,i));
    }
    toku_free(pivots);
    toku_free(node->bp);
    toku_free(node->childkeys);
    toku_free(node);
    toku_free(ndd);
    toku_free(subtree_info);

    if (result != 0)
        ft_loader_set_panic(bl, result, true, 0, nullptr, nullptr);
}

static int write_nonleaves (FTLOADER bl, FIDX pivots_fidx, struct dbout *out, struct subtrees_info *sts, const DESCRIPTOR descriptor, uint32_t target_nodesize, uint32_t target_basementnodesize, enum toku_compression_method target_compression_method) {
    int result = 0;
    int height = 1;

    // Watch out for the case where we saved the last pivot but didn't write any more nodes out.
    // The trick is not to look at n_pivots, but to look at blocks.n_blocks
    while (sts->n_subtrees > 1) {
        // If there is more than one block in blocks, then we must build another level of the tree.

        // we need to create a pivots file for the pivots of the next level.
        // and a blocks_array
        // So for example.
        //  1) we grab 16 pivots and 16 blocks.
        //  2) We put the 15 pivots and 16 blocks into an non-leaf node.
        //  3) We put the 16th pivot into the next pivots file.
        {
            int r = fseek(toku_bl_fidx2file(bl, pivots_fidx), 0, SEEK_SET);
            if (r!=0) { return get_error_errno(); }
        }

        FIDX next_pivots_file;
        { 
            int r = ft_loader_open_temp_file (bl, &next_pivots_file); 
            if (r != 0) { result = r; break; } 
        }

        struct subtrees_info next_sts; 
        subtrees_info_init(&next_sts);
        next_sts.n_subtrees = 0;
        next_sts.n_subtrees_limit = 1;
        XMALLOC_N(next_sts.n_subtrees_limit, next_sts.subtrees);

        const int n_per_block = 15;
        int64_t n_subtrees_used = 0;
        while (sts->n_subtrees - n_subtrees_used >= n_per_block*2) {
            // grab the first N_PER_BLOCK and build a node.
            DBT *pivots;
            int64_t blocknum_of_new_node;
            struct subtree_info *subtree_info;
            int r = setup_nonleaf_block (n_per_block,
                                         sts, pivots_fidx, n_subtrees_used,
                                         &next_sts, next_pivots_file,
                                         out, bl,
                                         &blocknum_of_new_node, &subtree_info, &pivots);
            if (r) {
                result = r;
                break;
            } else {
                write_nonleaf_node(bl, out, blocknum_of_new_node, n_per_block, pivots, subtree_info, height, descriptor, target_nodesize, target_basementnodesize, target_compression_method); // frees all the data structures that go into making the node.
                n_subtrees_used += n_per_block;
            }
        }

        int64_t n_blocks_left = sts->n_subtrees - n_subtrees_used;
        if (result == 0) {
            // Now we have a one or two blocks at the end to handle.
            invariant(n_blocks_left>=2);
            if (n_blocks_left > n_per_block) {
                // Write half the remaining blocks
                int64_t n_first = n_blocks_left/2;
                DBT *pivots;
                int64_t blocknum_of_new_node;
                struct subtree_info *subtree_info;
                int r = setup_nonleaf_block(n_first,
                                            sts, pivots_fidx, n_subtrees_used,
                                            &next_sts, next_pivots_file,
                                            out, bl,
                                            &blocknum_of_new_node, &subtree_info, &pivots);
                if (r) {
                    result = r;
                } else {
                    write_nonleaf_node(bl, out, blocknum_of_new_node, n_first, pivots, subtree_info, height, descriptor, target_nodesize, target_basementnodesize, target_compression_method);
                    n_blocks_left -= n_first;
                    n_subtrees_used += n_first;
                }
            }
        }
        if (result == 0) {
            // Write the last block. 
            DBT *pivots;
            int64_t blocknum_of_new_node;
            struct subtree_info *subtree_info;
            int r = setup_nonleaf_block(n_blocks_left,
                                        sts, pivots_fidx, n_subtrees_used,
                                        &next_sts, next_pivots_file,
                                        out, bl,
                                        &blocknum_of_new_node, &subtree_info, &pivots);
            if (r) {
                result = r;
            } else {
                write_nonleaf_node(bl, out, blocknum_of_new_node, n_blocks_left, pivots, subtree_info, height, descriptor, target_nodesize, target_basementnodesize, target_compression_method);
                n_subtrees_used += n_blocks_left;
            }
        }
        if (result == 0)
            invariant(n_subtrees_used == sts->n_subtrees);


        if (result == 0) // pick up write_nonleaf_node errors
            result = ft_loader_get_error(&bl->error_callback);

        // Now set things up for the next iteration.
        int r = ft_loader_fi_close(&bl->file_infos, pivots_fidx, true); if (r != 0 && result == 0) result = r;
        r = ft_loader_fi_unlink(&bl->file_infos, pivots_fidx);    if (r != 0 && result == 0) result = r;
        pivots_fidx = next_pivots_file;
        toku_free(sts->subtrees); sts->subtrees = NULL;
        *sts = next_sts;
        height++;

        if (result)
            break;
    }
    { int r = ft_loader_fi_close (&bl->file_infos, pivots_fidx, true); if (r != 0 && result == 0) result = r; }
    { int r = ft_loader_fi_unlink(&bl->file_infos, pivots_fidx); if (r != 0 && result == 0) result = r; }
    return result;
}

void ft_loader_set_fractal_workers_count_from_c(FTLOADER bl) {
    ft_loader_set_fractal_workers_count (bl);
}


