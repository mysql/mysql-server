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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>

#include <string.h>
#include <time.h>
#include <stdarg.h>

#include <portability/memory.h>
#include <portability/toku_race_tools.h>
#include <portability/toku_atomic.h>
#include <portability/toku_pthread.h>
#include <portability/toku_portability.h>
#include <portability/toku_stdlib.h>
#include <portability/toku_time.h>

#include "ft/cachetable/cachetable.h"
#include "ft/cachetable/cachetable-internal.h"
#include "ft/cachetable/checkpoint.h"
#include "ft/logger/log-internal.h"
#include "util/rwlock.h"
#include "util/scoped_malloc.h"
#include "util/status.h"
#include "util/context.h"

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

// These should be in the cachetable object, but we make them file-wide so that gdb can get them easily.
// They were left here after engine status cleanup (#2949, rather than moved into the status struct)
// so they are still easily available to the debugger and to save lots of typing.
static uint64_t cachetable_miss;
static uint64_t cachetable_misstime;     // time spent waiting for disk read
static uint64_t cachetable_prefetches;    // how many times has a block been prefetched into the cachetable?
static uint64_t cachetable_evictions;
static uint64_t cleaner_executions; // number of times the cleaner thread's loop has executed

static CACHETABLE_STATUS_S ct_status;

// Note, toku_cachetable_get_status() is below, after declaration of cachetable.

#define STATUS_INIT(k,c,t,l,inc) TOKUFT_STATUS_INIT(ct_status, k, c, t, "cachetable: " l, inc)

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.

    STATUS_INIT(CT_MISS,                   CACHETABLE_MISS, UINT64, "miss", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_MISSTIME,               CACHETABLE_MISS_TIME, UINT64, "miss time", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_PREFETCHES,             CACHETABLE_PREFETCHES, UINT64, "prefetches", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_CURRENT,           CACHETABLE_SIZE_CURRENT, UINT64, "size current", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_LIMIT,             CACHETABLE_SIZE_LIMIT, UINT64, "size limit", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_WRITING,           CACHETABLE_SIZE_WRITING, UINT64, "size writing", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_NONLEAF,           CACHETABLE_SIZE_NONLEAF, UINT64, "size nonleaf", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_LEAF,              CACHETABLE_SIZE_LEAF, UINT64, "size leaf", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_ROLLBACK,          CACHETABLE_SIZE_ROLLBACK, UINT64, "size rollback", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_CACHEPRESSURE,     CACHETABLE_SIZE_CACHEPRESSURE, UINT64, "size cachepressure", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_SIZE_CLONED,            CACHETABLE_SIZE_CLONED, UINT64, "size currently cloned data for checkpoint", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_EVICTIONS,              CACHETABLE_EVICTIONS, UINT64, "evictions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_CLEANER_EXECUTIONS,     CACHETABLE_CLEANER_EXECUTIONS, UINT64, "cleaner executions", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_CLEANER_PERIOD,         CACHETABLE_CLEANER_PERIOD, UINT64, "cleaner period", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_CLEANER_ITERATIONS,     CACHETABLE_CLEANER_ITERATIONS, UINT64, "cleaner iterations", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    
    STATUS_INIT(CT_WAIT_PRESSURE_COUNT,    CACHETABLE_WAIT_PRESSURE_COUNT, UINT64, "number of waits on cache pressure", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_WAIT_PRESSURE_TIME,    CACHETABLE_WAIT_PRESSURE_TIME, UINT64, "time waiting on cache pressure", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_LONG_WAIT_PRESSURE_COUNT,    CACHETABLE_LONG_WAIT_PRESSURE_COUNT, UINT64, "number of long waits on cache pressure", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    STATUS_INIT(CT_LONG_WAIT_PRESSURE_TIME,    CACHETABLE_LONG_WAIT_PRESSURE_TIME, UINT64, "long time waiting on cache pressure", TOKU_ENGINE_STATUS|TOKU_GLOBAL_STATUS);
    ct_status.initialized = true;
}
#undef STATUS_INIT

#define STATUS_VALUE(x) ct_status.status[x].value.num

static void * const zero_value = nullptr;
static PAIR_ATTR const zero_attr = {
    .size = 0, 
    .nonleaf_size = 0, 
    .leaf_size = 0, 
    .rollback_size = 0, 
    .cache_pressure_size = 0,
    .is_valid = true
};


static inline void ctpair_destroy(PAIR p) {
    p->value_rwlock.deinit();
    paranoid_invariant(p->refcount == 0);
    nb_mutex_destroy(&p->disk_nb_mutex);
    toku_cond_destroy(&p->refcount_wait);
    toku_free(p);
}

static inline void pair_lock(PAIR p) {
    toku_mutex_lock(p->mutex);
}

static inline void pair_unlock(PAIR p) {
    toku_mutex_unlock(p->mutex);
}

// adds a reference to the PAIR
// on input and output, PAIR mutex is held
static void pair_add_ref_unlocked(PAIR p) {
    p->refcount++;
}

// releases a reference to the PAIR
// on input and output, PAIR mutex is held
static void pair_release_ref_unlocked(PAIR p) {
    paranoid_invariant(p->refcount > 0);
    p->refcount--;
    if (p->refcount == 0 && p->num_waiting_on_refs > 0) {
        toku_cond_broadcast(&p->refcount_wait);
    }
}

static void pair_wait_for_ref_release_unlocked(PAIR p) {
    p->num_waiting_on_refs++;
    while (p->refcount > 0) {
        toku_cond_wait(&p->refcount_wait, p->mutex);
    }
    p->num_waiting_on_refs--;
}

bool toku_ctpair_is_write_locked(PAIR pair) {
    return pair->value_rwlock.writers() == 1;
}

void
toku_cachetable_get_status(CACHETABLE ct, CACHETABLE_STATUS statp) {
    if (!ct_status.initialized) {
        status_init();
    }
    STATUS_VALUE(CT_MISS)                   = cachetable_miss;
    STATUS_VALUE(CT_MISSTIME)               = cachetable_misstime;
    STATUS_VALUE(CT_PREFETCHES)             = cachetable_prefetches;
    STATUS_VALUE(CT_EVICTIONS)              = cachetable_evictions;
    STATUS_VALUE(CT_CLEANER_EXECUTIONS)     = cleaner_executions;
    STATUS_VALUE(CT_CLEANER_PERIOD)         = toku_get_cleaner_period_unlocked(ct);
    STATUS_VALUE(CT_CLEANER_ITERATIONS)     = toku_get_cleaner_iterations_unlocked(ct);
    ct->ev.fill_engine_status();
    *statp = ct_status;
}

// FIXME global with no toku prefix
void remove_background_job_from_cf(CACHEFILE cf)
{
    bjm_remove_background_job(cf->bjm);
}

// FIXME global with no toku prefix
void cachefile_kibbutz_enq (CACHEFILE cf, void (*f)(void*), void *extra)
// The function f must call remove_background_job_from_cf when it completes
{
    int r = bjm_add_background_job(cf->bjm);
    // if client is adding a background job, then it must be done
    // at a time when the manager is accepting background jobs, otherwise
    // the client is screwing up
    assert_zero(r); 
    toku_kibbutz_enq(cf->cachetable->client_kibbutz, f, extra);
}

static int
checkpoint_thread (void *checkpointer_v)
// Effect:  If checkpoint_period>0 thn periodically run a checkpoint.
//  If someone changes the checkpoint_period (calling toku_set_checkpoint_period), then the checkpoint will run sooner or later.
//  If someone sets the checkpoint_shutdown boolean , then this thread exits. 
// This thread notices those changes by waiting on a condition variable.
{
    CHECKPOINTER CAST_FROM_VOIDP(cp, checkpointer_v);
    int r = toku_checkpoint(cp, cp->get_logger(), NULL, NULL, NULL, NULL, SCHEDULED_CHECKPOINT);
    invariant_zero(r);
    return r;
}

void toku_set_checkpoint_period (CACHETABLE ct, uint32_t new_period) {
    ct->cp.set_checkpoint_period(new_period);
}

uint32_t toku_get_checkpoint_period_unlocked (CACHETABLE ct) {
    return ct->cp.get_checkpoint_period();
}

void toku_set_cleaner_period (CACHETABLE ct, uint32_t new_period) {
    ct->cl.set_period(new_period);
}

uint32_t toku_get_cleaner_period_unlocked (CACHETABLE ct) {
    return ct->cl.get_period_unlocked();
}

void toku_set_cleaner_iterations (CACHETABLE ct, uint32_t new_iterations) {
    ct->cl.set_iterations(new_iterations);
}

uint32_t toku_get_cleaner_iterations (CACHETABLE ct) {
    return ct->cl.get_iterations();
}

uint32_t toku_get_cleaner_iterations_unlocked (CACHETABLE ct) {
    return ct->cl.get_iterations();
}

// reserve 25% as "unreservable".  The loader cannot have it.
#define unreservable_memory(size) ((size)/4)

int toku_cachetable_create(CACHETABLE *ct_result, long size_limit, LSN UU(initial_lsn), TOKULOGGER logger) {
    int result = 0;
    int r;

    if (size_limit == 0) {
        size_limit = 128*1024*1024;
    }

    CACHETABLE XCALLOC(ct);
    ct->list.init();
    ct->cf_list.init();

    int num_processors = toku_os_get_number_active_processors();
    int checkpointing_nworkers = (num_processors/4) ? num_processors/4 : 1;
    r = toku_kibbutz_create(num_processors, &ct->client_kibbutz);
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    r = toku_kibbutz_create(2*num_processors, &ct->ct_kibbutz);
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    r = toku_kibbutz_create(checkpointing_nworkers, &ct->checkpointing_kibbutz);
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    // must be done after creating ct_kibbutz
    r = ct->ev.init(size_limit, &ct->list, &ct->cf_list, ct->ct_kibbutz, EVICTION_PERIOD);
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    r = ct->cp.init(&ct->list, logger, &ct->ev, &ct->cf_list);
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    r = ct->cl.init(1, &ct->list, ct); // by default, start with one iteration
    if (r != 0) {
        result = r;
        goto cleanup;
    }
    ct->env_dir = toku_xstrdup(".");
cleanup:
    if (result == 0) {
        *ct_result = ct;
    } else {
        toku_cachetable_close(&ct);
    }
    return result;
}

// Returns a pointer to the checkpoint contained within
// the given cachetable.
CHECKPOINTER toku_cachetable_get_checkpointer(CACHETABLE ct) {
    return &ct->cp;
}

uint64_t toku_cachetable_reserve_memory(CACHETABLE ct, double fraction, uint64_t upper_bound) {
    uint64_t reserved_memory = ct->ev.reserve_memory(fraction, upper_bound);
    return reserved_memory;
}

void toku_cachetable_release_reserved_memory(CACHETABLE ct, uint64_t reserved_memory) {
    ct->ev.release_reserved_memory(reserved_memory);
}

void
toku_cachetable_set_env_dir(CACHETABLE ct, const char *env_dir) {
    toku_free(ct->env_dir);
    ct->env_dir = toku_xstrdup(env_dir);
}

// What cachefile goes with particular iname (iname relative to env)?
// The transaction that is adding the reference might not have a reference
// to the ft, therefore the cachefile might be closing.
// If closing, we want to return that it is not there, but must wait till after
// the close has finished.
// Once the close has finished, there must not be a cachefile with that name
// in the cachetable.
int toku_cachefile_of_iname_in_env (CACHETABLE ct, const char *iname_in_env, CACHEFILE *cf) {
    return ct->cf_list.cachefile_of_iname_in_env(iname_in_env, cf);
}

// What cachefile goes with particular fd?
// This function can only be called if the ft is still open, so file must 
// still be open
int toku_cachefile_of_filenum (CACHETABLE ct, FILENUM filenum, CACHEFILE *cf) {
    return ct->cf_list.cachefile_of_filenum(filenum, cf);
}

// TEST-ONLY function
// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cfptr, CACHETABLE ct, int fd, const char *fname_in_env) {
    FILENUM filenum = toku_cachetable_reserve_filenum(ct);
    bool was_open;
    return toku_cachetable_openfd_with_filenum(cfptr, ct, fd, fname_in_env, filenum, &was_open);
}

// Get a unique filenum from the cachetable
FILENUM
toku_cachetable_reserve_filenum(CACHETABLE ct) {
    return ct->cf_list.reserve_filenum();
}

static void create_new_cachefile(
    CACHETABLE ct,
    FILENUM filenum,
    uint32_t hash_id,
    int fd,
    const char *fname_in_env,
    struct fileid fileid,
    CACHEFILE *cfptr
    ) {
    // File is not open.  Make a new cachefile.
    CACHEFILE newcf = NULL;
    XCALLOC(newcf);
    newcf->cachetable = ct;
    newcf->hash_id = hash_id;
    newcf->fileid = fileid;

    newcf->filenum = filenum;
    newcf->fd = fd;
    newcf->fname_in_env = toku_xstrdup(fname_in_env);
    bjm_init(&newcf->bjm);
    *cfptr = newcf;
}

int toku_cachetable_openfd_with_filenum (CACHEFILE *cfptr, CACHETABLE ct, int fd, 
                                         const char *fname_in_env,
                                         FILENUM filenum, bool* was_open) {
    int r;
    CACHEFILE newcf;
    struct fileid fileid;
    
    assert(filenum.fileid != FILENUM_NONE.fileid);
    r = toku_os_get_unique_file_id(fd, &fileid);
    if (r != 0) { 
        r = get_error_errno();
        close(fd);
        return r;
    }
    ct->cf_list.write_lock();
    CACHEFILE existing_cf = ct->cf_list.find_cachefile_unlocked(&fileid);
    if (existing_cf) {
        *was_open = true;
        // Reuse an existing cachefile and close the caller's fd, whose
        // responsibility has been passed to us.
        r = close(fd);
        assert(r == 0);
        *cfptr = existing_cf;
        r = 0;
        goto exit;        
    }
    *was_open = false;
    ct->cf_list.verify_unused_filenum(filenum);
    // now let's try to find it in the stale cachefiles
    existing_cf = ct->cf_list.find_stale_cachefile_unlocked(&fileid);
    // found the stale file, 
    if (existing_cf) {
        // fix up the fields in the cachefile
        existing_cf->filenum = filenum;
        existing_cf->fd = fd;
        existing_cf->fname_in_env = toku_xstrdup(fname_in_env);
        bjm_init(&existing_cf->bjm);

        // now we need to move all the PAIRs in it back into the cachetable
        ct->list.write_list_lock();
        for (PAIR curr_pair = existing_cf->cf_head; curr_pair; curr_pair = curr_pair->cf_next) {
            pair_lock(curr_pair);
            ct->list.add_to_cachetable_only(curr_pair);
            pair_unlock(curr_pair);
        }
        ct->list.write_list_unlock();
        // move the cachefile back to the list of active cachefiles
        ct->cf_list.remove_stale_cf_unlocked(existing_cf);
        ct->cf_list.add_cf_unlocked(existing_cf);
        *cfptr = existing_cf;
        r = 0;
        goto exit;
    }

    create_new_cachefile(
        ct,
        filenum,
        ct->cf_list.get_new_hash_id_unlocked(),
        fd,
        fname_in_env,
        fileid,
        &newcf
        );

    ct->cf_list.add_cf_unlocked(newcf);

    *cfptr = newcf;
    r = 0;
 exit:
    ct->cf_list.write_unlock();
    return r;
}

static void cachetable_flush_cachefile (CACHETABLE, CACHEFILE cf, bool evict_completely);

//TEST_ONLY_FUNCTION
int toku_cachetable_openf (CACHEFILE *cfptr, CACHETABLE ct, const char *fname_in_env, int flags, mode_t mode) {
    char *fname_in_cwd = toku_construct_full_name(2, ct->env_dir, fname_in_env);
    int fd = open(fname_in_cwd, flags+O_BINARY, mode);
    int r;
    if (fd < 0) {
        r = get_error_errno();
    } else {
        r = toku_cachetable_openfd (cfptr, ct, fd, fname_in_env);
    }
    toku_free(fname_in_cwd);
    return r;
}

char *
toku_cachefile_fname_in_env (CACHEFILE cf) {
    return cf->fname_in_env;
}

int 
toku_cachefile_get_fd (CACHEFILE cf) {
    return cf->fd;
}

static void cachefile_destroy(CACHEFILE cf) {
    if (cf->free_userdata) {
        cf->free_userdata(cf, cf->userdata);
    }
    toku_free(cf);
}

void toku_cachefile_close(CACHEFILE *cfp, bool oplsn_valid, LSN oplsn) {
    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;

    bjm_wait_for_jobs_to_finish(cf->bjm);
    
    // Clients should never attempt to close a cachefile that is being
    // checkpointed. We notify clients this is happening in the
    // note_pin_by_checkpoint callback.
    assert(!cf->for_checkpoint);

    // Flush the cachefile and remove all of its pairs from the cachetable,
    // but keep the PAIRs linked in the cachefile. We will store the cachefile
    // away in case it gets opened immedietely
    //
    // if we are unlinking on close, then we want to evict completely,
    // otherwise, we will keep the PAIRs and cachefile around in case
    // a subsequent open comes soon
    cachetable_flush_cachefile(ct, cf, cf->unlink_on_close);

    // Call the close userdata callback to notify the client this cachefile
    // and its underlying file are going to be closed
    if (cf->close_userdata) {
        cf->close_userdata(cf, cf->fd, cf->userdata, oplsn_valid, oplsn);
    }
    // fsync and close the fd. 
    toku_file_fsync_without_accounting(cf->fd);
    int r = close(cf->fd);
    assert(r == 0);
    cf->fd = -1;

    // destroy the parts of the cachefile
    // that do not persist across opens/closes
    bjm_destroy(cf->bjm);
    cf->bjm = NULL;

    // remove the cf from the list of active cachefiles
    ct->cf_list.remove_cf(cf);
    cf->filenum = FILENUM_NONE;

    // Unlink the file if the bit was set
    if (cf->unlink_on_close) {
        char *fname_in_cwd = toku_cachetable_get_fname_in_cwd(cf->cachetable, cf->fname_in_env);
        r = unlink(fname_in_cwd);
        assert_zero(r);
        toku_free(fname_in_cwd);
    }
    toku_free(cf->fname_in_env);
    cf->fname_in_env = NULL;

    // we destroy the cf if the unlink bit was set or if no PAIRs exist
    // if no PAIRs exist, there is no sense in keeping the cachefile around
    bool destroy_cf = cf->unlink_on_close || (cf->cf_head == NULL);
    if (destroy_cf) {
        cachefile_destroy(cf);
    }
    else {
        ct->cf_list.add_stale_cf(cf);
    }
}

// This hash function comes from Jenkins:  http://burtleburtle.net/bob/c/lookup3.c
// The idea here is to mix the bits thoroughly so that we don't have to do modulo by a prime number.
// Instead we can use a bitmask on a table of size power of two.
// This hash function does yield improved performance on ./db-benchmark-test-tokudb and ./scanscan
static inline uint32_t rot(uint32_t x, uint32_t k) {
    return (x<<k) | (x>>(32-k));
}
static inline uint32_t final (uint32_t a, uint32_t b, uint32_t c) {
    c ^= b; c -= rot(b,14);
    a ^= c; a -= rot(c,11);
    b ^= a; b -= rot(a,25);
    c ^= b; c -= rot(b,16);
    a ^= c; a -= rot(c,4); 
    b ^= a; b -= rot(a,14);
    c ^= b; c -= rot(b,24);
    return c;
}

uint32_t toku_cachetable_hash (CACHEFILE cachefile, BLOCKNUM key)
// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.
{
    return final(cachefile->hash_id, (uint32_t)(key.b>>32), (uint32_t)key.b);
}

#define CLOCK_SATURATION 15
#define CLOCK_INITIAL_COUNT 3

// Requires pair's mutex to be held
static void pair_touch (PAIR p) {
    p->count = (p->count < CLOCK_SATURATION) ? p->count+1 : CLOCK_SATURATION;
}

// Remove a pair from the cachetable, requires write list lock to be held and p->mutex to be held
// Effects: the pair is removed from the LRU list and from the cachetable's hash table.
// The size of the objects in the cachetable is adjusted by the size of the pair being
// removed.
static void cachetable_remove_pair (pair_list* list, evictor* ev, PAIR p) {
    list->evict_completely(p);
    ev->remove_pair_attr(p->attr);
}

static void cachetable_free_pair(PAIR p) {
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEKEY key = p->key;
    void *value = p->value_data;
    void* disk_data = p->disk_data;
    void *write_extraargs = p->write_extraargs;
    PAIR_ATTR old_attr = p->attr;
    
    cachetable_evictions++;
    PAIR_ATTR new_attr = p->attr;
    // Note that flush_callback is called with write_me false, so the only purpose of this 
    // call is to tell the ft layer to evict the node (keep_me is false).
    // Also, because we have already removed the PAIR from the cachetable in 
    // cachetable_remove_pair, we cannot pass in p->cachefile and p->cachefile->fd
    // for the first two parameters, as these may be invalid (#5171), so, we
    // pass in NULL and -1, dummy values
    flush_callback(NULL, -1, key, value, &disk_data, write_extraargs, old_attr, &new_attr, false, false, true, false);
    
    ctpair_destroy(p);
}

// assumes value_rwlock and disk_nb_mutex held on entry
// responsibility of this function is to only write a locked PAIR to disk
// and NOTHING else. We do not manipulate the state of the PAIR
// of the cachetable here (with the exception of ct->size_current for clones)
//
// No pair_list lock should be held, and the PAIR mutex should not be held
//
static void cachetable_only_write_locked_data(
    evictor* ev,
    PAIR p, 
    bool for_checkpoint,
    PAIR_ATTR* new_attr,
    bool is_clone
    ) 
{    
    CACHETABLE_FLUSH_CALLBACK flush_callback = p->flush_callback;
    CACHEFILE cachefile = p->cachefile;
    CACHEKEY key = p->key;
    void *value = is_clone ? p->cloned_value_data : p->value_data;
    void *disk_data = p->disk_data;
    void *write_extraargs = p->write_extraargs;
    PAIR_ATTR old_attr;
    // we do this for drd. If we are a cloned pair and only 
    // have the disk_nb_mutex, it is a race to access p->attr.
    // Luckily, old_attr here is only used for some test applications,
    // so inaccurate non-size fields are ok.
    if (is_clone) {
        old_attr = make_pair_attr(p->cloned_value_size);
    }
    else {
        old_attr = p->attr;
    }
    bool dowrite = true;
        
    // write callback
    flush_callback(
        cachefile, 
        cachefile->fd, 
        key, 
        value, 
        &disk_data, 
        write_extraargs, 
        old_attr, 
        new_attr, 
        dowrite, 
        is_clone ? false : true, // keep_me (only keep if this is not cloned pointer)
        for_checkpoint, 
        is_clone //is_clone
        );
    p->disk_data = disk_data;
    if (is_clone) {
        p->cloned_value_data = NULL;
        ev->remove_cloned_data_size(p->cloned_value_size);
        p->cloned_value_size = 0;
    }    
}


//
// This function writes a PAIR's value out to disk. Currently, it is called
// by get_and_pin functions that write a PAIR out for checkpoint, by 
// evictor threads that evict dirty PAIRS, and by the checkpoint thread
// that needs to write out a dirty node for checkpoint.
//
// Requires on entry for p->mutex to NOT be held, otherwise 
// calling cachetable_only_write_locked_data will be very expensive
//
static void cachetable_write_locked_pair(
    evictor* ev,
    PAIR p, 
    bool for_checkpoint
    ) 
{
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = p->attr;
    // grabbing the disk_nb_mutex here ensures that
    // after this point, no one is writing out a cloned value
    // if we grab the disk_nb_mutex inside the if clause,
    // then we may try to evict a PAIR that is in the process
    // of having its clone be written out
    pair_lock(p);
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
    pair_unlock(p);
    // make sure that assumption about cloned_value_data is true
    // if we have grabbed the disk_nb_mutex, then that means that
    // there should be no cloned value data
    assert(p->cloned_value_data == NULL);
    if (p->dirty) {
        cachetable_only_write_locked_data(ev, p, for_checkpoint, &new_attr, false);
        //
        // now let's update variables
        //
        if (new_attr.is_valid) {
            p->attr = new_attr;
            ev->change_pair_attr(old_attr, new_attr);
        }
    }
    // the pair is no longer dirty once written
    p->dirty = CACHETABLE_CLEAN;
    pair_lock(p);
    nb_mutex_unlock(&p->disk_nb_mutex);
    pair_unlock(p);    
}

// Worker thread function to writes and evicts  a pair from memory to its cachefile
static void cachetable_evicter(void* extra) {
    PAIR p = (PAIR)extra;
    pair_list* pl = p->list;
    CACHEFILE cf = p->cachefile;
    pl->read_pending_exp_lock();
    bool for_checkpoint = p->checkpoint_pending;
    p->checkpoint_pending = false;
    // per the contract of evictor::evict_pair,
    // the pair's mutex, p->mutex, must be held on entry
    pair_lock(p);
    p->ev->evict_pair(p, for_checkpoint);
    pl->read_pending_exp_unlock();
    bjm_remove_background_job(cf->bjm);
}

static void cachetable_partial_eviction(void* extra) {
    PAIR p = (PAIR)extra;
    CACHEFILE cf = p->cachefile;
    p->ev->do_partial_eviction(p);
    bjm_remove_background_job(cf->bjm);
}

void toku_cachetable_swap_pair_values(PAIR old_pair, PAIR new_pair) {
    void* old_value = old_pair->value_data;
    void* new_value = new_pair->value_data;
    old_pair->value_data = new_value;
    new_pair->value_data = old_value;
}

void toku_cachetable_maybe_flush_some(CACHETABLE ct) {
    // TODO: <CER> Maybe move this...
    ct->ev.signal_eviction_thread();
}

// Initializes a pair's members.
//
void pair_init(PAIR p, 
    CACHEFILE cachefile, 
    CACHEKEY key, 
    void *value,
    PAIR_ATTR attr,
    enum cachetable_dirty dirty,
    uint32_t fullhash,
    CACHETABLE_WRITE_CALLBACK write_callback,
    evictor *ev,
    pair_list *list)
{
    p->cachefile = cachefile;
    p->key = key;
    p->value_data = value;
    p->cloned_value_data = NULL;
    p->cloned_value_size = 0;
    p->disk_data = NULL;
    p->attr = attr;
    p->dirty = dirty;
    p->fullhash = fullhash;

    p->flush_callback = write_callback.flush_callback;
    p->pe_callback = write_callback.pe_callback;
    p->pe_est_callback = write_callback.pe_est_callback;
    p->cleaner_callback = write_callback.cleaner_callback;
    p->clone_callback = write_callback.clone_callback;
    p->checkpoint_complete_callback = write_callback.checkpoint_complete_callback;
    p->write_extraargs = write_callback.write_extraargs;

    p->count = 0; // <CER> Is zero the correct init value?
    p->refcount = 0;
    p->num_waiting_on_refs = 0;
    toku_cond_init(&p->refcount_wait, NULL);
    p->checkpoint_pending = false;

    p->mutex = list->get_mutex_for_pair(fullhash);
    assert(p->mutex);
    p->value_rwlock.init(p->mutex);
    nb_mutex_init(&p->disk_nb_mutex);

    p->size_evicting_estimate = 0; // <CER> Is zero the correct init value?

    p->ev = ev;
    p->list = list;

    p->clock_next = p->clock_prev = NULL;
    p->pending_next = p->pending_prev = NULL;
    p->cf_next = p->cf_prev = NULL;
    p->hash_chain = NULL;
}

// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
//
// Requires pair list's write lock to be held on entry.
// the pair's mutex must be held as wel
// 
//
static PAIR cachetable_insert_at(CACHETABLE ct,
                                 CACHEFILE cachefile, CACHEKEY key, void *value,
                                 uint32_t fullhash,
                                 PAIR_ATTR attr,
                                 CACHETABLE_WRITE_CALLBACK write_callback,
                                 enum cachetable_dirty dirty) {
    PAIR MALLOC(p);
    assert(p);
    memset(p, 0, sizeof *p);
    pair_init(p,
        cachefile,
        key,
        value,
        attr,
        dirty,
        fullhash,
        write_callback,
        &ct->ev,
        &ct->list
        );

    ct->list.put(p);
    ct->ev.add_pair_attr(attr);
    return p;
}

// on input, the write list lock must be held AND 
// the pair's mutex must be held as wel
static void cachetable_insert_pair_at(CACHETABLE ct, PAIR p, PAIR_ATTR attr) {
    ct->list.put(p);
    ct->ev.add_pair_attr(attr);
}


// has ct locked on entry
// This function MUST NOT release and reacquire the cachetable lock
// Its callers (toku_cachetable_put_with_dep_pairs) depend on this behavior.
//
// Requires pair list's write lock to be held on entry
//
static void cachetable_put_internal(
    CACHEFILE cachefile, 
    PAIR p,
    void *value, 
    PAIR_ATTR attr,
    CACHETABLE_PUT_CALLBACK put_callback
    )
{
    CACHETABLE ct = cachefile->cachetable;
    //
    //
    // TODO: (Zardosht), make code run in debug only 
    //
    //
    //PAIR dummy_p = ct->list.find_pair(cachefile, key, fullhash);
    //invariant_null(dummy_p);
    cachetable_insert_pair_at(ct, p, attr);
    invariant_notnull(put_callback);
    put_callback(p->key, value, p);
}

// Pair mutex (p->mutex) is may or may not be held on entry,
// Holding the pair mutex on entry is not important
// for performance or corrrectness
// Pair is pinned on entry
static void
clone_pair(evictor* ev, PAIR p) {
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr;
    long clone_size = 0;

    // act of cloning should be fast,
    // not sure if we have to release
    // and regrab the cachetable lock,
    // but doing it for now
    p->clone_callback(
        p->value_data,
        &p->cloned_value_data,
        &clone_size,
        &new_attr,
        true,
        p->write_extraargs
        );
    
    // now we need to do the same actions we would do
    // if the PAIR had been written to disk
    //
    // because we hold the value_rwlock,
    // it doesn't matter whether we clear 
    // the pending bit before the clone
    // or after the clone
    p->dirty = CACHETABLE_CLEAN;
    if (new_attr.is_valid) {
        p->attr = new_attr;
        ev->change_pair_attr(old_attr, new_attr);
    }
    p->cloned_value_size = clone_size;
    ev->add_cloned_data_size(p->cloned_value_size);
}

static void checkpoint_cloned_pair(void* extra) {
    PAIR p = (PAIR)extra;
    CACHETABLE ct = p->cachefile->cachetable;
    PAIR_ATTR new_attr;
    // note that pending lock is not needed here because
    // we KNOW we are in the middle of a checkpoint
    // and that a begin_checkpoint cannot happen
    cachetable_only_write_locked_data(
        p->ev,
        p,
        true, //for_checkpoint
        &new_attr,
        true //is_clone
        );
    pair_lock(p);
    nb_mutex_unlock(&p->disk_nb_mutex);
    pair_unlock(p);
    ct->cp.remove_background_job();
}

static void
checkpoint_cloned_pair_on_writer_thread(CACHETABLE ct, PAIR p) {
    toku_kibbutz_enq(ct->checkpointing_kibbutz, checkpoint_cloned_pair, p);
}


//
// Given a PAIR p with the value_rwlock altready held, do the following:
//  - If the PAIR needs to be written out to disk for checkpoint:
//   - If the PAIR is cloneable, clone the PAIR and place the work
//      of writing the PAIR on a background thread.
//   - If the PAIR is not cloneable, write the PAIR to disk for checkpoint
//      on the current thread
//
// On entry, pair's mutex is NOT held
//
static void
write_locked_pair_for_checkpoint(CACHETABLE ct, PAIR p, bool checkpoint_pending)
{
    if (checkpoint_pending && p->checkpoint_complete_callback) {
        p->checkpoint_complete_callback(p->value_data);
    }
    if (p->dirty && checkpoint_pending) {
        if (p->clone_callback) {
            pair_lock(p);
            nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
            pair_unlock(p);
            assert(!p->cloned_value_data);
            clone_pair(&ct->ev, p);
            assert(p->cloned_value_data);
            // place it on the background thread and continue
            // responsibility of writer thread to release disk_nb_mutex
            ct->cp.add_background_job();
            checkpoint_cloned_pair_on_writer_thread(ct, p);
        }
        else {
            // The pair is not cloneable, just write the pair to disk
            // we already have p->value_rwlock and we just do the write in our own thread.            
            cachetable_write_locked_pair(&ct->ev, p, true); // keeps the PAIR's write lock
        }
    }
}

// On entry and exit: hold the pair's mutex (p->mutex)
// Method:   take write lock
//           maybe write out the node
//           Else release write lock
//
static void
write_pair_for_checkpoint_thread (evictor* ev, PAIR p)
{
    // Grab an exclusive lock on the pair.
    // If we grab an expensive lock, then other threads will return
    // TRY_AGAIN rather than waiting.  In production, the only time
    // another thread will check if grabbing a lock is expensive is when
    // we have a clone_callback (FTNODEs), so the act of checkpointing
    // will be cheap.  Also, much of the time we'll just be clearing
    // pending bits and that's definitely cheap. (see #5427)
    p->value_rwlock.write_lock(false);
    if (p->checkpoint_pending && p->checkpoint_complete_callback) {
        p->checkpoint_complete_callback(p->value_data);
    }
    if (p->dirty && p->checkpoint_pending) {
        if (p->clone_callback) {
            nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
            assert(!p->cloned_value_data);
            clone_pair(ev, p);
            assert(p->cloned_value_data);
        }
        else {
            // The pair is not cloneable, just write the pair to disk            
            // we already have p->value_rwlock and we just do the write in our own thread.
            // this will grab and release disk_nb_mutex
            pair_unlock(p);
            cachetable_write_locked_pair(ev, p, true); // keeps the PAIR's write lock
            pair_lock(p);
        }
        p->checkpoint_pending = false;
        
        // now release value_rwlock, before we write the PAIR out
        // so that the PAIR is available to client threads
        p->value_rwlock.write_unlock(); // didn't call cachetable_evict_pair so we have to unlock it ourselves.
        if (p->clone_callback) {
            // note that pending lock is not needed here because
            // we KNOW we are in the middle of a checkpoint
            // and that a begin_checkpoint cannot happen
            PAIR_ATTR attr;
            pair_unlock(p);
            cachetable_only_write_locked_data(
                ev,
                p,
                true, //for_checkpoint
                &attr,
                true //is_clone
                );
            pair_lock(p);
            nb_mutex_unlock(&p->disk_nb_mutex);
        }
    }
    else {
        //
        // we may clear the pending bit here because we have
        // both the cachetable lock and the PAIR lock.
        // The rule, as mentioned in  toku_cachetable_begin_checkpoint, 
        // is that to clear the bit, we must have both the PAIR lock
        // and the pending lock
        //
        p->checkpoint_pending = false;
        p->value_rwlock.write_unlock();
    }
}

//
// For each PAIR associated with these CACHEFILEs and CACHEKEYs
// if the checkpoint_pending bit is set and the PAIR is dirty, write the PAIR
// to disk.
// We assume the PAIRs passed in have been locked by the client that made calls
// into the cachetable that eventually make it here.
//
static void checkpoint_dependent_pairs(
    CACHETABLE ct,
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    bool* checkpoint_pending,
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
{
     for (uint32_t i =0; i < num_dependent_pairs; i++) {
         PAIR curr_dep_pair = dependent_pairs[i];
         // we need to update the dirtyness of the dependent pair,
         // because the client may have dirtied it while holding its lock,
         // and if the pair is pending a checkpoint, it needs to be written out
         if (dependent_dirty[i]) curr_dep_pair->dirty = CACHETABLE_DIRTY;
         if (checkpoint_pending[i]) {
             write_locked_pair_for_checkpoint(ct, curr_dep_pair, checkpoint_pending[i]);
         }
     }
}

void toku_cachetable_put_with_dep_pairs(
    CACHEFILE cachefile,
    CACHETABLE_GET_KEY_AND_FULLHASH get_key_and_fullhash,
    void *value,
    PAIR_ATTR attr,
    CACHETABLE_WRITE_CALLBACK write_callback,
    void *get_key_and_fullhash_extra,
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty, // array stating dirty/cleanness of dependent pairs
    CACHEKEY* key,
    uint32_t* fullhash,
    CACHETABLE_PUT_CALLBACK put_callback
    )
{
    //
    // need to get the key and filehash
    //
    CACHETABLE ct = cachefile->cachetable;
    if (ct->ev.should_client_thread_sleep()) {
        ct->ev.wait_for_cache_pressure_to_subside();
    }
    if (ct->ev.should_client_wake_eviction_thread()) {
        ct->ev.signal_eviction_thread();
    }

    PAIR p = NULL;
    XMALLOC(p);
    memset(p, 0, sizeof *p);

    ct->list.write_list_lock();
    get_key_and_fullhash(key, fullhash, get_key_and_fullhash_extra);
    pair_init(
        p, 
        cachefile, 
        *key, 
        value, 
        attr, 
        CACHETABLE_DIRTY, 
        *fullhash,
        write_callback,
        &ct->ev,
        &ct->list
        );
    pair_lock(p);
    p->value_rwlock.write_lock(true);
    cachetable_put_internal(
        cachefile,
        p,
        value,
        attr,
        put_callback
        );
    pair_unlock(p);
    bool checkpoint_pending[num_dependent_pairs];
    ct->list.write_pending_cheap_lock();
    for (uint32_t i = 0; i < num_dependent_pairs; i++) {
        checkpoint_pending[i] = dependent_pairs[i]->checkpoint_pending;
        dependent_pairs[i]->checkpoint_pending = false;
    }
    ct->list.write_pending_cheap_unlock();
    ct->list.write_list_unlock();

    //
    // now that we have inserted the row, let's checkpoint the 
    // dependent nodes, if they need checkpointing
    //
    checkpoint_dependent_pairs(
        ct,
        num_dependent_pairs,
        dependent_pairs,
        checkpoint_pending,
        dependent_dirty
        );
}

void toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, void*value, PAIR_ATTR attr,
                        CACHETABLE_WRITE_CALLBACK write_callback,
                        CACHETABLE_PUT_CALLBACK put_callback
                        ) {
    CACHETABLE ct = cachefile->cachetable;
    if (ct->ev.should_client_thread_sleep()) {
        ct->ev.wait_for_cache_pressure_to_subside();
    }
    if (ct->ev.should_client_wake_eviction_thread()) {
        ct->ev.signal_eviction_thread();
    }

    PAIR p = NULL;
    XMALLOC(p);
    memset(p, 0, sizeof *p);

    ct->list.write_list_lock();
    pair_init(
        p, 
        cachefile, 
        key, 
        value, 
        attr, 
        CACHETABLE_DIRTY, 
        fullhash,
        write_callback,
        &ct->ev,
        &ct->list
        );
    pair_lock(p);
    p->value_rwlock.write_lock(true);
    cachetable_put_internal(
        cachefile,
        p,
        value,
        attr,
        put_callback
        );
    pair_unlock(p);
    ct->list.write_list_unlock();
}

static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

//
// cachetable lock and PAIR lock are held on entry
// On exit, cachetable lock is still held, but PAIR lock
// is either released.
//
// No locks are held on entry (besides the rwlock write lock  of the PAIR)
//
static void
do_partial_fetch(
    CACHETABLE ct, 
    CACHEFILE cachefile, 
    PAIR p, 
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback, 
    void *read_extraargs,
    bool keep_pair_locked
    )
{
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = zero_attr;
    // As of Dr. No, only clean PAIRs may have pieces missing,
    // so we do a sanity check here.
    assert(!p->dirty);

    pair_lock(p);
    invariant(p->value_rwlock.writers());
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
    pair_unlock(p);
    int r = pf_callback(p->value_data, p->disk_data, read_extraargs, cachefile->fd, &new_attr);
    lazy_assert_zero(r);
    p->attr = new_attr;
    ct->ev.change_pair_attr(old_attr, new_attr);
    pair_lock(p);
    nb_mutex_unlock(&p->disk_nb_mutex);
    if (!keep_pair_locked) {
        p->value_rwlock.write_unlock();
    }
    pair_unlock(p);
}

void toku_cachetable_pf_pinned_pair(
    void* value,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    void* read_extraargs,
    CACHEFILE cf,
    CACHEKEY key,
    uint32_t fullhash
    ) 
{
    PAIR_ATTR attr;
    PAIR p = NULL;
    CACHETABLE ct = cf->cachetable;
    ct->list.pair_lock_by_fullhash(fullhash);
    p = ct->list.find_pair(cf, key, fullhash);
    assert(p != NULL);
    assert(p->value_data == value);
    assert(p->value_rwlock.writers());
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);    
    pair_unlock(p);
    
    int fd = cf->fd;
    pf_callback(value, p->disk_data, read_extraargs, fd, &attr);

    pair_lock(p);
    nb_mutex_unlock(&p->disk_nb_mutex);    
    pair_unlock(p);
}

int toku_cachetable_get_and_pin (
    CACHEFILE cachefile, 
    CACHEKEY key, 
    uint32_t fullhash, 
    void**value, 
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    bool may_modify_value,
    void* read_extraargs // parameter for fetch_callback, pf_req_callback, and pf_callback
    ) 
{
    pair_lock_type lock_type = may_modify_value ? PL_WRITE_EXPENSIVE : PL_READ;
    // We have separate parameters of read_extraargs and write_extraargs because
    // the lifetime of the two parameters are different. write_extraargs may be used
    // long after this function call (e.g. after a flush to disk), whereas read_extraargs
    // will not be used after this function returns. As a result, the caller may allocate
    // read_extraargs on the stack, whereas write_extraargs must be allocated
    // on the heap.
    return toku_cachetable_get_and_pin_with_dep_pairs (
        cachefile, 
        key, 
        fullhash, 
        value, 
        sizep,
        write_callback,
        fetch_callback, 
        pf_req_callback,
        pf_callback,
        lock_type,
        read_extraargs,
        0, // number of dependent pairs that we may need to checkpoint
        NULL, // array of dependent pairs
        NULL // array stating dirty/cleanness of dependent pairs
        );
}

// Read a pair from a cachefile into memory using the pair's fetch callback
// on entry, pair mutex (p->mutex) is NOT held, but pair is pinned
static void cachetable_fetch_pair(
    CACHETABLE ct, 
    CACHEFILE cf, 
    PAIR p, 
    CACHETABLE_FETCH_CALLBACK fetch_callback, 
    void* read_extraargs,
    bool keep_pair_locked
    ) 
{
    // helgrind
    CACHEKEY key = p->key;
    uint32_t fullhash = p->fullhash;

    void *toku_value = NULL;
    void *disk_data = NULL;
    PAIR_ATTR attr;
    
    // FIXME this should be enum cachetable_dirty, right?
    int dirty = 0;

    pair_lock(p);
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
    pair_unlock(p);

    int r;
    r = fetch_callback(cf, p, cf->fd, key, fullhash, &toku_value, &disk_data, &attr, &dirty, read_extraargs);
    if (dirty) {
        p->dirty = CACHETABLE_DIRTY;
    }
    assert(r == 0);

    p->value_data = toku_value;
    p->disk_data = disk_data;
    p->attr = attr;
    ct->ev.add_pair_attr(attr);
    pair_lock(p);
    nb_mutex_unlock(&p->disk_nb_mutex);
    if (!keep_pair_locked) {
        p->value_rwlock.write_unlock();
    }
    pair_unlock(p);
}

static bool get_checkpoint_pending(PAIR p, pair_list* pl) {
    bool checkpoint_pending = false;
    pl->read_pending_cheap_lock();
    checkpoint_pending = p->checkpoint_pending;
    p->checkpoint_pending = false;
    pl->read_pending_cheap_unlock();
    return checkpoint_pending;
}

static void checkpoint_pair_and_dependent_pairs(
    CACHETABLE ct,
    PAIR p,
    bool p_is_pending_checkpoint,
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    bool* dependent_pairs_pending_checkpoint,
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
{
    
    //
    // A checkpoint must not begin while we are checking dependent pairs or pending bits. 
    // Here is why.
    //
    // Now that we have all of the locks on the pairs we 
    // care about, we can take care of the necessary checkpointing.
    // For each pair, we simply need to write the pair if it is 
    // pending a checkpoint. If no pair is pending a checkpoint,
    // then all of this work will be done with the cachetable lock held,
    // so we don't need to worry about a checkpoint beginning 
    // in the middle of any operation below. If some pair
    // is pending a checkpoint, then the checkpoint thread
    // will not complete its current checkpoint until it can
    // successfully grab a lock on the pending pair and 
    // remove it from its list of pairs pending a checkpoint.
    // This cannot be done until we release the lock
    // that we have, which is not done in this function.
    // So, the point is, it is impossible for a checkpoint
    // to begin while we write any of these locked pairs
    // for checkpoint, even though writing a pair releases
    // the cachetable lock.
    //
    write_locked_pair_for_checkpoint(ct, p, p_is_pending_checkpoint);
    
    checkpoint_dependent_pairs(
        ct,
        num_dependent_pairs,
        dependent_pairs,
        dependent_pairs_pending_checkpoint,
        dependent_dirty
        );
}

static void unpin_pair(PAIR p, bool read_lock_grabbed) {
    if (read_lock_grabbed) {
        p->value_rwlock.read_unlock();
    }
    else {
        p->value_rwlock.write_unlock();
    }
}


// on input, the pair's mutex is held,
// on output, the pair's mutex is not held.
// if true, we must try again, and pair is not pinned
// if false, we succeeded, the pair is pinned
static bool try_pin_pair(
    PAIR p,
    CACHETABLE ct,
    CACHEFILE cachefile,
    pair_lock_type lock_type,
    uint32_t num_dependent_pairs,
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    void* read_extraargs,
    bool already_slept
    )
{
    bool dep_checkpoint_pending[num_dependent_pairs];
    bool try_again = true;
    bool expensive = (lock_type == PL_WRITE_EXPENSIVE);
    if (lock_type != PL_READ) {
        p->value_rwlock.write_lock(expensive);
    }
    else {
        p->value_rwlock.read_lock();
    }
    pair_touch(p);
    pair_unlock(p);

    bool partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
    
    if (partial_fetch_required) {    
        toku::context pf_ctx(CTX_PARTIAL_FETCH);

        if (ct->ev.should_client_thread_sleep() && !already_slept) {
            pair_lock(p);
            unpin_pair(p, (lock_type == PL_READ));
            pair_unlock(p);
            try_again = true;
            goto exit;
        }
        if (ct->ev.should_client_wake_eviction_thread()) {
            ct->ev.signal_eviction_thread();
        }
        //
        // Just because the PAIR exists does necessarily mean the all the data the caller requires
        // is in memory. A partial fetch may be required, which is evaluated above
        // if the variable is true, a partial fetch is required so we must grab the PAIR's write lock
        // and then call a callback to retrieve what we need
        //
        assert(partial_fetch_required);
        // As of Dr. No, only clean PAIRs may have pieces missing,
        // so we do a sanity check here.
        assert(!p->dirty);

        if (lock_type == PL_READ) {
            pair_lock(p);
            p->value_rwlock.read_unlock();
            p->value_rwlock.write_lock(true);
            pair_unlock(p);
        }
        else if (lock_type == PL_WRITE_CHEAP) {
            pair_lock(p);
            p->value_rwlock.write_unlock();
            p->value_rwlock.write_lock(true);
            pair_unlock(p);
        }
        
        partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
        if (partial_fetch_required) {
            do_partial_fetch(ct, cachefile, p, pf_callback, read_extraargs, true);
        }
        if (lock_type == PL_READ) {
            //
            // TODO: Zardosht, somehow ensure that a partial eviction cannot happen
            // between these two calls
            //
            pair_lock(p);
            p->value_rwlock.write_unlock();
            p->value_rwlock.read_lock();
            pair_unlock(p);
        }
        else if (lock_type == PL_WRITE_CHEAP) {
            pair_lock(p);
            p->value_rwlock.write_unlock();
            p->value_rwlock.write_lock(false);
            pair_unlock(p);
        }
        // small hack here for #5439,
        // for queries, pf_req_callback does some work for the caller,
        // that information may be out of date after a write_unlock
        // followed by a relock, so we do it again.
        bool pf_required = pf_req_callback(p->value_data,read_extraargs);
        assert(!pf_required);
    }

    if (lock_type != PL_READ) {
        ct->list.read_pending_cheap_lock();
        bool p_checkpoint_pending = p->checkpoint_pending;
        p->checkpoint_pending = false;
        for (uint32_t i = 0; i < num_dependent_pairs; i++) {
            dep_checkpoint_pending[i] = dependent_pairs[i]->checkpoint_pending;
            dependent_pairs[i]->checkpoint_pending = false;
        }
        ct->list.read_pending_cheap_unlock();
        checkpoint_pair_and_dependent_pairs(
            ct,
            p,
            p_checkpoint_pending,
            num_dependent_pairs,
            dependent_pairs,
            dep_checkpoint_pending,
            dependent_dirty
            );
    }

    try_again = false;
exit:
    return try_again;
}

int toku_cachetable_get_and_pin_with_dep_pairs (
    CACHEFILE cachefile,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long *sizep,
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    pair_lock_type lock_type,
    void* read_extraargs, // parameter for fetch_callback, pf_req_callback, and pf_callback
    uint32_t num_dependent_pairs, // number of dependent pairs that we may need to checkpoint
    PAIR* dependent_pairs,
    enum cachetable_dirty* dependent_dirty // array stating dirty/cleanness of dependent pairs
    )
// See cachetable/cachetable.h
{
    CACHETABLE ct = cachefile->cachetable;
    bool wait = false;
    bool already_slept = false;
    bool dep_checkpoint_pending[num_dependent_pairs];

    // 
    // If in the process of pinning the node we add data to the cachetable via a partial fetch
    // or a full fetch, we may need to first sleep because there is too much data in the 
    // cachetable. In those cases, we set the bool wait to true and goto try_again, so that
    // we can do our sleep and then restart the function.
    //
beginning:
    if (wait) {
        // We shouldn't be holding the read list lock while
        // waiting for the evictor to remove pairs.
        already_slept = true;
        ct->ev.wait_for_cache_pressure_to_subside();
    }

    ct->list.pair_lock_by_fullhash(fullhash);
    PAIR p = ct->list.find_pair(cachefile, key, fullhash);
    if (p) {
        // on entry, holds p->mutex (which is locked via pair_lock_by_fullhash)
        // on exit, does not hold p->mutex
        bool try_again = try_pin_pair(
            p,
            ct,
            cachefile,
            lock_type,
            num_dependent_pairs,
            dependent_pairs,
            dependent_dirty,
            pf_req_callback,
            pf_callback,
            read_extraargs,
            already_slept
            );
        if (try_again) {
            wait = true;
            goto beginning;
        }
        else {
            goto got_value;
        }
    }
    else {
        toku::context fetch_ctx(CTX_FULL_FETCH);

        ct->list.pair_unlock_by_fullhash(fullhash);
        // we only want to sleep once per call to get_and_pin. If we have already
        // slept and there is still cache pressure, then we might as 
        // well just complete the call, because the sleep did not help
        // By sleeping only once per get_and_pin, we prevent starvation and ensure
        // that we make progress (however slow) on each thread, which allows
        // assumptions of the form 'x will eventually happen'.
        // This happens in extreme scenarios.
        if (ct->ev.should_client_thread_sleep() && !already_slept) {
            wait = true;
            goto beginning;
        }
        if (ct->ev.should_client_wake_eviction_thread()) {
            ct->ev.signal_eviction_thread();
        }
        // Since the pair was not found, we need the write list
        // lock to add it.  So, we have to release the read list lock
        // first.
        ct->list.write_list_lock();
        ct->list.pair_lock_by_fullhash(fullhash);
        p = ct->list.find_pair(cachefile, key, fullhash);
        if (p != NULL) {
            ct->list.write_list_unlock();
            // on entry, holds p->mutex,
            // on exit, does not hold p->mutex
            bool try_again = try_pin_pair(
                p,
                ct,
                cachefile,
                lock_type,
                num_dependent_pairs,
                dependent_pairs,
                dependent_dirty,
                pf_req_callback,
                pf_callback,
                read_extraargs,
                already_slept
                );
            if (try_again) {
                wait = true;
                goto beginning;
            }
            else {
                goto got_value;
            }
        }
        assert(p == NULL);

        // Insert a PAIR into the cachetable
        // NOTE: At this point we still have the write list lock held.
        p = cachetable_insert_at(
            ct,
            cachefile,
            key,
            zero_value,
            fullhash,
            zero_attr,
            write_callback,
            CACHETABLE_CLEAN
            );
        invariant_notnull(p);

        // Pin the pair.
        p->value_rwlock.write_lock(true);
        pair_unlock(p);


        if (lock_type != PL_READ) {
            ct->list.read_pending_cheap_lock();
            invariant(!p->checkpoint_pending);
            for (uint32_t i = 0; i < num_dependent_pairs; i++) {
                dep_checkpoint_pending[i] = dependent_pairs[i]->checkpoint_pending;
                dependent_pairs[i]->checkpoint_pending = false;
            }
            ct->list.read_pending_cheap_unlock();
        }
        // We should release the lock before we perform
        // these expensive operations.
        ct->list.write_list_unlock();

        if (lock_type != PL_READ) {
            checkpoint_dependent_pairs(
                ct,
                num_dependent_pairs,
                dependent_pairs,
                dep_checkpoint_pending,
                dependent_dirty
                );
        }
        uint64_t t0 = get_tnow();

        // Retrieve the value of the PAIR from disk.
        // The pair being fetched will be marked as pending if a checkpoint happens during the
        // fetch because begin_checkpoint will mark as pending any pair that is locked even if it is clean.        
        cachetable_fetch_pair(ct, cachefile, p, fetch_callback, read_extraargs, true);
        cachetable_miss++;
        cachetable_misstime += get_tnow() - t0;

        // If the lock_type requested was a PL_READ, we downgrade to PL_READ,
        // but if the request was for a PL_WRITE_CHEAP, we don't bother 
        // downgrading, because we would have to possibly resolve the 
        // checkpointing again, and that would just make this function even 
        // messier.
        //
        // TODO(yoni): in case of PL_WRITE_CHEAP, write and use
        // p->value_rwlock.write_change_status_to_not_expensive(); (Also name it better)
        // to downgrade from an expensive write lock to a cheap one
        if (lock_type == PL_READ) {
            pair_lock(p);
            p->value_rwlock.write_unlock();
            p->value_rwlock.read_lock();
            pair_unlock(p);
            // small hack here for #5439,
            // for queries, pf_req_callback does some work for the caller,
            // that information may be out of date after a write_unlock
            // followed by a read_lock, so we do it again.
            bool pf_required = pf_req_callback(p->value_data,read_extraargs);
            assert(!pf_required);
        }
        goto got_value;
    }
got_value:
    *value = p->value_data;
    if (sizep) *sizep = p->attr.size;
    return 0;
}

// Lookup a key in the cachetable.  If it is found and it is not being written, then
// acquire a read lock on the pair, update the LRU list, and return sucess.
//
// However, if the page is clean or has checkpoint pending, don't return success.
// This will minimize the number of dirty nodes.
// Rationale:  maybe_get_and_pin is used when the system has an alternative to modifying a node.
//  In the context of checkpointing, we don't want to gratuituously dirty a page, because it causes an I/O.
//  For example, imagine that we can modify a bit in a dirty parent, or modify a bit in a clean child, then we should modify
//  the dirty parent (which will have to do I/O eventually anyway) rather than incur a full block write to modify one bit.
//  Similarly, if the checkpoint is actually pending, we don't want to block on it.
int toku_cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, pair_lock_type lock_type, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    int r = -1;
    ct->list.pair_lock_by_fullhash(fullhash);
    PAIR p = ct->list.find_pair(cachefile, key, fullhash);
    if (p) {
        const bool lock_is_expensive = (lock_type == PL_WRITE_EXPENSIVE);
        bool got_lock = false;
        switch (lock_type) {
        case PL_READ:
            if (p->value_rwlock.try_read_lock()) {
                got_lock = p->dirty;

                if (!got_lock) {
                    p->value_rwlock.read_unlock();
                }
            }
            break;
        case PL_WRITE_CHEAP:
        case PL_WRITE_EXPENSIVE:
            if (p->value_rwlock.try_write_lock(lock_is_expensive)) {
                // we got the lock fast, so continue
                ct->list.read_pending_cheap_lock();

                // if pending a checkpoint, then we don't want to return
                // the value to the user, because we are responsible for
                // handling the checkpointing, which we do not want to do,
                // because it is expensive
                got_lock = p->dirty && !p->checkpoint_pending;

                ct->list.read_pending_cheap_unlock();
                if (!got_lock) {
                    p->value_rwlock.write_unlock();
                }
            }
            break;
        }
        if (got_lock) {
            pair_touch(p);
            *value = p->value_data;
            r = 0;
        }
    }
    ct->list.pair_unlock_by_fullhash(fullhash);
    return r;
}

//Used by flusher threads to possibly pin child on client thread if pinning is cheap
//Same as toku_cachetable_maybe_get_and_pin except that we don't care if the node is clean or dirty (return the node regardless).
//All other conditions remain the same.
int toku_cachetable_maybe_get_and_pin_clean (CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, pair_lock_type lock_type, void**value) {
    CACHETABLE ct = cachefile->cachetable;
    int r = -1;
    ct->list.pair_lock_by_fullhash(fullhash);
    PAIR p = ct->list.find_pair(cachefile, key, fullhash);
    if (p) {
        const bool lock_is_expensive = (lock_type == PL_WRITE_EXPENSIVE);
        bool got_lock = false;
        switch (lock_type) {
        case PL_READ:
            if (p->value_rwlock.try_read_lock()) {
                got_lock = true;
            } else if (!p->value_rwlock.read_lock_is_expensive()) {
                p->value_rwlock.write_lock(lock_is_expensive);
                got_lock = true;
            }
            if (got_lock) {
                pair_touch(p);
            }
            pair_unlock(p);
            break;
        case PL_WRITE_CHEAP:
        case PL_WRITE_EXPENSIVE:
            if (p->value_rwlock.try_write_lock(lock_is_expensive)) {
                got_lock = true;
            } else if (!p->value_rwlock.write_lock_is_expensive()) {
                p->value_rwlock.write_lock(lock_is_expensive);
                got_lock = true;
            }
            if (got_lock) {
                pair_touch(p);
            }
            pair_unlock(p);
            if (got_lock) {
                bool checkpoint_pending = get_checkpoint_pending(p, &ct->list);
                write_locked_pair_for_checkpoint(ct, p, checkpoint_pending);
            }
            break;
        }
        if (got_lock) {
            *value = p->value_data;
            r = 0;
        }
    } else {
        ct->list.pair_unlock_by_fullhash(fullhash);
    }
    return r;
}

//
// internal function to unpin a PAIR.
// As of Clayface, this is may be called in two ways:
//  - with flush false
//  - with flush true
// The first is for when this is run during run_unlockers in 
// toku_cachetable_get_and_pin_nonblocking, the second is during
// normal operations. Only during normal operations do we want to possibly
// induce evictions or sleep.
//
static int
cachetable_unpin_internal(
    CACHEFILE cachefile, 
    PAIR p,
    enum cachetable_dirty dirty, 
    PAIR_ATTR attr,
    bool flush
    )
{
    invariant_notnull(p);

    CACHETABLE ct = cachefile->cachetable;
    bool added_data_to_cachetable = false;

    // hack for #3969, only exists in case where we run unlockers
    pair_lock(p);
    PAIR_ATTR old_attr = p->attr;
    PAIR_ATTR new_attr = attr;
    if (dirty) {
        p->dirty = CACHETABLE_DIRTY;
    }
    if (attr.is_valid) {
        p->attr = attr;
    }
    bool read_lock_grabbed = p->value_rwlock.readers() != 0;
    unpin_pair(p, read_lock_grabbed);
    pair_unlock(p);
    
    if (attr.is_valid) {
        if (new_attr.size > old_attr.size) {
            added_data_to_cachetable = true;
        }
        ct->ev.change_pair_attr(old_attr, new_attr);
    }

    // see comments above this function to understand this code
    if (flush && added_data_to_cachetable) {
        if (ct->ev.should_client_thread_sleep()) {
            ct->ev.wait_for_cache_pressure_to_subside();
        }
        if (ct->ev.should_client_wake_eviction_thread()) {
            ct->ev.signal_eviction_thread();
        }
    }
    return 0;
}

int toku_cachetable_unpin(CACHEFILE cachefile, PAIR p, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    return cachetable_unpin_internal(cachefile, p, dirty, attr, true);
}
int toku_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE cachefile, PAIR p, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    return cachetable_unpin_internal(cachefile, p, dirty, attr, false);
}

static void
run_unlockers (UNLOCKERS unlockers) {
    while (unlockers) {
        assert(unlockers->locked);
        unlockers->locked = false;
        unlockers->f(unlockers->extra);
        unlockers=unlockers->next;
    }
}

//
// This function tries to pin the pair without running the unlockers.
// If it can pin the pair cheaply, it does so, and returns 0. 
// If the pin will be expensive, it runs unlockers, 
// pins the pair, then releases the pin,
// and then returns TOKUDB_TRY_AGAIN
//
// on entry, pair mutex is held,
// on exit, pair mutex is NOT held
static int
maybe_pin_pair(
    PAIR p, 
    pair_lock_type lock_type,
    UNLOCKERS unlockers
    )
{
    int retval = 0;
    bool expensive = (lock_type == PL_WRITE_EXPENSIVE);

    // we can pin the PAIR. In each case, we check to see
    // if acquiring the pin is expensive. If so, we run the unlockers, set the
    // retval to TOKUDB_TRY_AGAIN, pin AND release the PAIR.
    // If not, then we pin the PAIR, keep retval at 0, and do not
    // run the unlockers, as we intend to return the value to the user
    if (lock_type == PL_READ) {
        if (p->value_rwlock.read_lock_is_expensive()) {
            pair_add_ref_unlocked(p);
            pair_unlock(p);
            run_unlockers(unlockers);
            retval = TOKUDB_TRY_AGAIN;
            pair_lock(p);
            pair_release_ref_unlocked(p);
        }
        p->value_rwlock.read_lock();
    }
    else if (lock_type == PL_WRITE_EXPENSIVE || lock_type == PL_WRITE_CHEAP){
        if (p->value_rwlock.write_lock_is_expensive()) {
            pair_add_ref_unlocked(p);
            pair_unlock(p);
            run_unlockers(unlockers);
            // change expensive to false because 
            // we will unpin the pair immedietely
            // after pinning it
            expensive = false;
            retval = TOKUDB_TRY_AGAIN;
            pair_lock(p);
            pair_release_ref_unlocked(p);
        }
        p->value_rwlock.write_lock(expensive);
    }
    else {
        abort();
    }

    if (retval == TOKUDB_TRY_AGAIN) {
        unpin_pair(p, (lock_type == PL_READ));
    }    
    pair_touch(p);
    pair_unlock(p);
    return retval;
}

int toku_cachetable_get_and_pin_nonblocking(
    CACHEFILE cf,
    CACHEKEY key,
    uint32_t fullhash,
    void**value,
    long* UU(sizep),
    CACHETABLE_WRITE_CALLBACK write_callback,
    CACHETABLE_FETCH_CALLBACK fetch_callback,
    CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
    pair_lock_type lock_type,
    void *read_extraargs,
    UNLOCKERS unlockers
    )
// See cachetable/cachetable.h.
{
    CACHETABLE ct = cf->cachetable;
    assert(lock_type == PL_READ ||
        lock_type == PL_WRITE_CHEAP ||
        lock_type == PL_WRITE_EXPENSIVE
        );
try_again:
    ct->list.pair_lock_by_fullhash(fullhash);
    PAIR p = ct->list.find_pair(cf, key, fullhash);
    if (p == NULL) {
        toku::context fetch_ctx(CTX_FULL_FETCH);

        // Not found
        ct->list.pair_unlock_by_fullhash(fullhash);
        ct->list.write_list_lock();
        ct->list.pair_lock_by_fullhash(fullhash);
        p = ct->list.find_pair(cf, key, fullhash);
        if (p != NULL) {
            // we just did another search with the write list lock and 
            // found the pair this means that in between our 
            // releasing the read list lock and grabbing the write list lock,
            // another thread snuck in and inserted the PAIR into
            // the cachetable. For simplicity, we just return
            // to the top and restart the function
            ct->list.write_list_unlock();
            ct->list.pair_unlock_by_fullhash(fullhash);
            goto try_again;
        }

        p = cachetable_insert_at(
            ct,
            cf,
            key,
            zero_value,
            fullhash,
            zero_attr,
            write_callback,
            CACHETABLE_CLEAN
            );
        assert(p);
        // grab expensive write lock, because we are about to do a fetch
        // off disk
        // No one can access this pair because
        // we hold the write list lock and we just injected
        // the pair into the cachetable. Therefore, this lock acquisition
        // will not block.
        p->value_rwlock.write_lock(true);
        pair_unlock(p);
        run_unlockers(unlockers); // we hold the write list_lock.
        ct->list.write_list_unlock();

        // at this point, only the pair is pinned,
        // and no pair mutex held, and 
        // no list lock is held
        uint64_t t0 = get_tnow();
        cachetable_fetch_pair(ct, cf, p, fetch_callback, read_extraargs, false);
        cachetable_miss++;
        cachetable_misstime += get_tnow() - t0;

        if (ct->ev.should_client_thread_sleep()) {
            ct->ev.wait_for_cache_pressure_to_subside();
        }
        if (ct->ev.should_client_wake_eviction_thread()) {
            ct->ev.signal_eviction_thread();
        }

        return TOKUDB_TRY_AGAIN;
    }
    else {
        int r = maybe_pin_pair(p, lock_type, unlockers);
        if (r == TOKUDB_TRY_AGAIN) {
            return TOKUDB_TRY_AGAIN;
        }
        assert_zero(r);

        if (lock_type != PL_READ) {
            bool checkpoint_pending = get_checkpoint_pending(p, &ct->list);
            write_locked_pair_for_checkpoint(ct, p, checkpoint_pending);
        }

        // At this point, we have pinned the PAIR
        // and resolved its checkpointing. The pair's
        // mutex is not held. The read list lock IS held. Before
        // returning the PAIR to the user, we must
        // still check for partial fetch
        bool partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
        if (partial_fetch_required) {
            toku::context fetch_ctx(CTX_PARTIAL_FETCH);

            run_unlockers(unlockers);

            // we are now getting an expensive write lock, because we
            // are doing a partial fetch. So, if we previously have 
            // either a read lock or a cheap write lock, we need to 
            // release and reacquire the correct lock type
            if (lock_type == PL_READ) {
                pair_lock(p);
                p->value_rwlock.read_unlock();
                p->value_rwlock.write_lock(true);
                pair_unlock(p);
            }
            else if (lock_type == PL_WRITE_CHEAP) {
                pair_lock(p);
                p->value_rwlock.write_unlock();
                p->value_rwlock.write_lock(true);
                pair_unlock(p);
            }

            // Now wait for the I/O to occur.
            partial_fetch_required = pf_req_callback(p->value_data,read_extraargs);
            if (partial_fetch_required) {
                do_partial_fetch(ct, cf, p, pf_callback, read_extraargs, false);
            }
            else {
                pair_lock(p);
                p->value_rwlock.write_unlock();
                pair_unlock(p);
            }

            if (ct->ev.should_client_thread_sleep()) {
                ct->ev.wait_for_cache_pressure_to_subside();
            }
            if (ct->ev.should_client_wake_eviction_thread()) {
                ct->ev.signal_eviction_thread();
            }

            return TOKUDB_TRY_AGAIN;
        }
        else {
            *value = p->value_data;
            return 0;    
        }
    }
    // We should not get here. Above code should hit a return in all cases.
    abort();
}

struct cachefile_prefetch_args {
    PAIR p;
    CACHETABLE_FETCH_CALLBACK fetch_callback;
    void* read_extraargs;
};

struct cachefile_partial_prefetch_args {
    PAIR p;
    CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback;
    void *read_extraargs;
};

// Worker thread function to read a pair from a cachefile to memory
static void cachetable_reader(void* extra) {
    struct cachefile_prefetch_args* cpargs = (struct cachefile_prefetch_args*)extra;
    CACHEFILE cf = cpargs->p->cachefile;
    CACHETABLE ct = cf->cachetable;
    cachetable_fetch_pair(
        ct,
        cpargs->p->cachefile,
        cpargs->p,
        cpargs->fetch_callback,
        cpargs->read_extraargs,
        false
        );
    bjm_remove_background_job(cf->bjm);
    toku_free(cpargs);
}

static void cachetable_partial_reader(void* extra) {
    struct cachefile_partial_prefetch_args *cpargs = (struct cachefile_partial_prefetch_args*)extra;
    CACHEFILE cf = cpargs->p->cachefile;
    CACHETABLE ct = cf->cachetable;
    do_partial_fetch(ct, cpargs->p->cachefile, cpargs->p, cpargs->pf_callback, cpargs->read_extraargs, false);
    bjm_remove_background_job(cf->bjm);
    toku_free(cpargs);
}

int toku_cachefile_prefetch(CACHEFILE cf, CACHEKEY key, uint32_t fullhash,
                            CACHETABLE_WRITE_CALLBACK write_callback,
                            CACHETABLE_FETCH_CALLBACK fetch_callback,
                            CACHETABLE_PARTIAL_FETCH_REQUIRED_CALLBACK pf_req_callback,
                            CACHETABLE_PARTIAL_FETCH_CALLBACK pf_callback,
                            void *read_extraargs,
                            bool *doing_prefetch)
// Effect: See the documentation for this function in cachetable/cachetable.h
{
    int r = 0;
    PAIR p = NULL;
    if (doing_prefetch) {
        *doing_prefetch = false;
    }
    CACHETABLE ct = cf->cachetable;
    // if cachetable has too much data, don't bother prefetching
    if (ct->ev.should_client_thread_sleep()) {
        goto exit;
    }
    ct->list.pair_lock_by_fullhash(fullhash);
    // lookup
    p = ct->list.find_pair(cf, key, fullhash);
    // if not found then create a pair and fetch it
    if (p == NULL) {
        cachetable_prefetches++;
        ct->list.pair_unlock_by_fullhash(fullhash);
        ct->list.write_list_lock();
        ct->list.pair_lock_by_fullhash(fullhash);
        p = ct->list.find_pair(cf, key, fullhash);
        if (p != NULL) {
            ct->list.write_list_unlock();
            goto found_pair;
        }
        
        r = bjm_add_background_job(cf->bjm);
        assert_zero(r);
        p = cachetable_insert_at(
            ct, 
            cf, 
            key, 
            zero_value, 
            fullhash, 
            zero_attr, 
            write_callback,
            CACHETABLE_CLEAN
            );
        assert(p);
        p->value_rwlock.write_lock(true);
        pair_unlock(p);
        ct->list.write_list_unlock();
        
        struct cachefile_prefetch_args *MALLOC(cpargs);
        cpargs->p = p;
        cpargs->fetch_callback = fetch_callback;
        cpargs->read_extraargs = read_extraargs;
        toku_kibbutz_enq(ct->ct_kibbutz, cachetable_reader, cpargs);
        if (doing_prefetch) {
            *doing_prefetch = true;
        }
        goto exit;
    }

found_pair:
    // at this point, p is found, pair's mutex is grabbed, and
    // no list lock is held
    // TODO(leif): should this also just go ahead and wait if all there
    // are to wait for are readers?
    if (p->value_rwlock.try_write_lock(true)) {
        // nobody else is using the node, so we should go ahead and prefetch
        pair_touch(p);
        pair_unlock(p);
        bool partial_fetch_required = pf_req_callback(p->value_data, read_extraargs);

        if (partial_fetch_required) {
            r = bjm_add_background_job(cf->bjm);
            assert_zero(r);
            struct cachefile_partial_prefetch_args *MALLOC(cpargs);
            cpargs->p = p;
            cpargs->pf_callback = pf_callback;
            cpargs->read_extraargs = read_extraargs;
            toku_kibbutz_enq(ct->ct_kibbutz, cachetable_partial_reader, cpargs);
            if (doing_prefetch) {
                *doing_prefetch = true;
            }
        }
        else {
            pair_lock(p);
            p->value_rwlock.write_unlock();
            pair_unlock(p);
        }
    }
    else {
        // Couldn't get the write lock cheaply
        pair_unlock(p);
    }
exit:
    return 0;
}

void toku_cachefile_verify (CACHEFILE cf) {
    toku_cachetable_verify(cf->cachetable);
}

void toku_cachetable_verify (CACHETABLE ct) {
    ct->list.verify();
}



struct pair_flush_for_close{
    PAIR p;
    BACKGROUND_JOB_MANAGER bjm;
};

static void cachetable_flush_pair_for_close(void* extra) {
    struct pair_flush_for_close *CAST_FROM_VOIDP(args, extra);
    PAIR p = args->p;
    CACHEFILE cf = p->cachefile;
    CACHETABLE ct = cf->cachetable;
    PAIR_ATTR attr;
    cachetable_only_write_locked_data(
        &ct->ev,
        p,
        false, // not for a checkpoint, as we assert above
        &attr,
        false // not a clone
        );            
    p->dirty = CACHETABLE_CLEAN;
    bjm_remove_background_job(args->bjm);
    toku_free(args);
}


static void flush_pair_for_close_on_background_thread(
    PAIR p, 
    BACKGROUND_JOB_MANAGER bjm, 
    CACHETABLE ct
    ) 
{
    pair_lock(p);
    assert(p->value_rwlock.users() == 0);
    assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
    assert(!p->cloned_value_data);
    if (p->dirty == CACHETABLE_DIRTY) {
        int r = bjm_add_background_job(bjm);
        assert_zero(r);
        struct pair_flush_for_close *XMALLOC(args);
        args->p = p;
        args->bjm = bjm;
        toku_kibbutz_enq(ct->ct_kibbutz, cachetable_flush_pair_for_close, args);
    }
    pair_unlock(p);
}

static void remove_pair_for_close(PAIR p, CACHETABLE ct, bool completely) {
    pair_lock(p);
    assert(p->value_rwlock.users() == 0);
    assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
    assert(!p->cloned_value_data);
    assert(p->dirty == CACHETABLE_CLEAN);
    assert(p->refcount == 0);
    if (completely) {
        cachetable_remove_pair(&ct->list, &ct->ev, p);
        pair_unlock(p);
        // TODO: Eventually, we should not hold the write list lock during free
        cachetable_free_pair(p);
    }
    else {
        // if we are not evicting completely,
        // we only want to remove the PAIR from the cachetable,
        // that is, remove from the hashtable and various linked
        // list, but we will keep the PAIRS and the linked list
        // in the cachefile intact, as they will be cached away
        // in case an open comes soon.
        ct->list.evict_from_cachetable(p);
        pair_unlock(p);
    }
}

// helper function for cachetable_flush_cachefile, which happens on a close
// writes out the dirty pairs on background threads and returns when
// the writing is done
static void write_dirty_pairs_for_close(CACHETABLE ct, CACHEFILE cf) {
    BACKGROUND_JOB_MANAGER bjm = NULL;
    bjm_init(&bjm);
    ct->list.write_list_lock(); // TODO: (Zardosht), verify that this lock is unnecessary to take here
    PAIR p = NULL;
    // write out dirty PAIRs
    uint32_t i;
    if (cf) {
        for (i = 0, p = cf->cf_head;
            i < cf->num_pairs;
            i++, p = p->cf_next)
        {
            flush_pair_for_close_on_background_thread(p, bjm, ct);
        }
    }
    else {
        for (i = 0, p = ct->list.m_checkpoint_head;
            i < ct->list.m_n_in_table;
            i++, p = p->clock_next)
        {
            flush_pair_for_close_on_background_thread(p, bjm, ct);
        }
    }
    ct->list.write_list_unlock();
    bjm_wait_for_jobs_to_finish(bjm);
    bjm_destroy(bjm);
}

static void remove_all_pairs_for_close(CACHETABLE ct, CACHEFILE cf, bool evict_completely) {
    ct->list.write_list_lock();
    if (cf) {
        if (evict_completely) {
            // if we are evicting completely, then the PAIRs will
            // be removed from the linked list managed by the
            // cachefile, so this while loop works
            while (cf->num_pairs > 0) {
                PAIR p = cf->cf_head;
                remove_pair_for_close(p, ct, evict_completely);
            }
        }
        else {
            // on the other hand, if we are not evicting completely,
            // then the cachefile's linked list stays intact, and we must
            // iterate like this.
            for (PAIR p = cf->cf_head; p; p = p->cf_next) {
                remove_pair_for_close(p, ct, evict_completely);
            }
        }
    }
    else {
        while (ct->list.m_n_in_table > 0) {
            PAIR p = ct->list.m_checkpoint_head;
            // if there is no cachefile, then we better
            // be evicting completely because we have no
            // cachefile to save the PAIRs to. At least,
            // we have no guarantees that the cachefile
            // will remain good
            invariant(evict_completely);
            remove_pair_for_close(p, ct, true);
        } 
    }
    ct->list.write_list_unlock();
}

static void verify_cachefile_flushed(CACHETABLE ct UU(), CACHEFILE cf UU()) {
#ifdef TOKU_DEBUG_PARANOID
    // assert here that cachefile is flushed by checking
    // pair_list and finding no pairs belonging to this cachefile
    // Make a list of pairs that belong to this cachefile.
    if (cf) {
        ct->list.write_list_lock();
        // assert here that cachefile is flushed by checking
        // pair_list and finding no pairs belonging to this cachefile
        // Make a list of pairs that belong to this cachefile.
        uint32_t i;
        PAIR p = NULL;
        for (i = 0, p = ct->list.m_checkpoint_head; 
             i < ct->list.m_n_in_table; 
             i++, p = p->clock_next) 
         {
             assert(p->cachefile != cf);
         }
         ct->list.write_list_unlock();
    }
#endif
}

// Flush (write to disk) all of the pairs that belong to a cachefile (or all pairs if 
// the cachefile is NULL.
// Must be holding cachetable lock on entry.
// 
// This function assumes that no client thread is accessing or 
// trying to access the cachefile while this function is executing.
// This implies no client thread will be trying to lock any nodes
// belonging to the cachefile.
//
// This function also assumes that the cachefile is not in the process
// of being used by a checkpoint. If a checkpoint is currently happening,
// it does NOT include this cachefile.
//
static void cachetable_flush_cachefile(CACHETABLE ct, CACHEFILE cf, bool evict_completely) {
    //
    // Because work on a kibbutz is always done by the client thread,
    // and this function assumes that no client thread is doing any work
    // on the cachefile, we assume that no client thread will be adding jobs
    // to this cachefile's kibbutz.
    //
    // The caller of this function must ensure that there are 
    // no jobs added to the kibbutz. This implies that the only work other 
    // threads may be doing is work by the writer threads.
    //
    // first write out dirty PAIRs
    write_dirty_pairs_for_close(ct, cf);

    // now that everything is clean, get rid of everything
    remove_all_pairs_for_close(ct, cf, evict_completely);

    verify_cachefile_flushed(ct, cf);
}

/* Requires that no locks be held that are used by the checkpoint logic */
void
toku_cachetable_minicron_shutdown(CACHETABLE ct) {
    int  r = ct->cp.shutdown();
    assert(r==0);
    ct->cl.destroy();
}

void toku_cachetable_prepare_close(CACHETABLE ct UU()) {
    extern bool toku_serialize_in_parallel;
    toku_drd_unsafe_set(&toku_serialize_in_parallel, true);
}

/* Requires that it all be flushed. */
void toku_cachetable_close (CACHETABLE *ctp) {
    CACHETABLE ct = *ctp;
    ct->cp.destroy();
    ct->cl.destroy();
    ct->cf_list.free_stale_data(&ct->ev);
    cachetable_flush_cachefile(ct, NULL, true);
    ct->ev.destroy();
    ct->list.destroy();
    ct->cf_list.destroy();
    
    if (ct->client_kibbutz)
        toku_kibbutz_destroy(ct->client_kibbutz);
    if (ct->ct_kibbutz)
        toku_kibbutz_destroy(ct->ct_kibbutz);
    if (ct->checkpointing_kibbutz)
        toku_kibbutz_destroy(ct->checkpointing_kibbutz);
    toku_free(ct->env_dir);
    toku_free(ct);
    *ctp = 0;
}

static PAIR test_get_pair(CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, bool have_ct_lock) {
    CACHETABLE ct = cachefile->cachetable;

    if (!have_ct_lock) {
        ct->list.read_list_lock();
    }
    
    PAIR p = ct->list.find_pair(cachefile, key, fullhash);
    assert(p != NULL);
    if (!have_ct_lock) {
        ct->list.read_list_unlock();
    }
    return p;
}

//test-only wrapper
int toku_test_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    // By default we don't have the lock
    PAIR p = test_get_pair(cachefile, key, fullhash, false);
    return toku_cachetable_unpin(cachefile, p, dirty, attr); // assume read lock is not grabbed, and that it is a write lock
}

//test-only wrapper
int toku_test_cachetable_unpin_ct_prelocked_no_flush(CACHEFILE cachefile, CACHEKEY key, uint32_t fullhash, enum cachetable_dirty dirty, PAIR_ATTR attr) {
    // We hold the cachetable mutex.
    PAIR p = test_get_pair(cachefile, key, fullhash, true);
    return toku_cachetable_unpin_ct_prelocked_no_flush(cachefile, p, dirty, attr);
}

//test-only wrapper
int toku_test_cachetable_unpin_and_remove (
    CACHEFILE cachefile, 
    CACHEKEY key,
    CACHETABLE_REMOVE_KEY remove_key,
    void* remove_key_extra) 
{
    uint32_t fullhash = toku_cachetable_hash(cachefile, key);
    PAIR p = test_get_pair(cachefile, key, fullhash, false);
    return toku_cachetable_unpin_and_remove(cachefile, p, remove_key, remove_key_extra);
}

int toku_cachetable_unpin_and_remove (
    CACHEFILE cachefile, 
    PAIR p,
    CACHETABLE_REMOVE_KEY remove_key,
    void* remove_key_extra
    ) 
{
    invariant_notnull(p);
    int r = ENOENT;
    CACHETABLE ct = cachefile->cachetable;

    p->dirty = CACHETABLE_CLEAN; // clear the dirty bit.  We're just supposed to remove it.
    // grab disk_nb_mutex to ensure any background thread writing
    // out a cloned value completes
    pair_lock(p);
    assert(p->value_rwlock.writers());
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
    pair_unlock(p);
    assert(p->cloned_value_data == NULL);
    
    //
    // take care of key removal
    //
    ct->list.write_list_lock();
    ct->list.read_pending_cheap_lock();
    bool for_checkpoint = p->checkpoint_pending;
    // now let's wipe out the pending bit, because we are
    // removing the PAIR
    p->checkpoint_pending = false;
    
    // For the PAIR to not be picked by the
    // cleaner thread, we mark the cachepressure_size to be 0
    // (This is redundant since we have the write_list_lock)
    // This should not be an issue because we call
    // cachetable_remove_pair before
    // releasing the cachetable lock.
    //
    CACHEKEY key_to_remove = p->key;
    p->attr.cache_pressure_size = 0;
    //
    // callback for removing the key
    // for FTNODEs, this leads to calling
    // toku_free_blocknum
    //
    if (remove_key) {
        remove_key(
            &key_to_remove, 
            for_checkpoint, 
            remove_key_extra
            );
    }    
    ct->list.read_pending_cheap_unlock();

    pair_lock(p);
    p->value_rwlock.write_unlock();
    nb_mutex_unlock(&p->disk_nb_mutex);
    //
    // As of Clayface (6.5), only these threads may be
    // blocked waiting to lock this PAIR:
    //  - the checkpoint thread (because a checkpoint is in progress
    //     and the PAIR was in the list of pending pairs)
    //  - a client thread running get_and_pin_nonblocking, who
    //     ran unlockers, then waited on the PAIR lock.
    //     While waiting on a PAIR lock, another thread comes in,
    //     locks the PAIR, and ends up calling unpin_and_remove,
    //     all while get_and_pin_nonblocking is waiting on the PAIR lock.
    //     We did not realize this at first, which caused bug #4357
    // The following threads CANNOT be blocked waiting on 
    // the PAIR lock:
    //  - a thread trying to run eviction via run_eviction. 
    //     That cannot happen because run_eviction only
    //     attempts to lock PAIRS that are not locked, and this PAIR
    //     is locked.
    //  - cleaner thread, for the same reason as a thread running 
    //     eviction
    //  - client thread doing a normal get_and_pin. The client is smart
    //     enough to not try to lock a PAIR that another client thread
    //     is trying to unpin and remove. Note that this includes work
    //     done on kibbutzes.
    //  - writer thread. Writer threads do not grab PAIR locks. They
    //     get PAIR locks transferred to them by client threads.
    //

    // first thing we do is remove the PAIR from the various
    // cachetable data structures, so no other thread can possibly
    // access it. We do not want to risk some other thread
    // trying to lock this PAIR if we release the write list lock
    // below. If some thread is already waiting on the lock,
    // then we let that thread grab the lock and finish, but
    // we don't want any NEW threads to try to grab the PAIR
    // lock.
    //
    // Because we call cachetable_remove_pair and wait,
    // the threads that may be waiting
    // on this PAIR lock must be careful to do NOTHING with the PAIR 
    // As per our analysis above, we only need
    // to make sure the checkpoint thread and get_and_pin_nonblocking do
    // nothing, and looking at those functions, it is clear they do nothing.
    // 
    cachetable_remove_pair(&ct->list, &ct->ev, p);
    ct->list.write_list_unlock();
    if (p->refcount > 0) {
        pair_wait_for_ref_release_unlocked(p);
    }
    if (p->value_rwlock.users() > 0) {
        // Need to wait for everyone else to leave
        // This write lock will be granted only after all waiting
        // threads are done.
        p->value_rwlock.write_lock(true);
        assert(p->refcount == 0);
        assert(p->value_rwlock.users() == 1);  // us
        assert(!p->checkpoint_pending);
        assert(p->attr.cache_pressure_size == 0);
        p->value_rwlock.write_unlock();
    }
    // just a sanity check
    assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
    assert(p->cloned_value_data == NULL);
    //Remove pair. 
    pair_unlock(p);
    cachetable_free_pair(p);
    r = 0;
    return r;
}

int set_filenum_in_array(const FT &ft, const uint32_t index, FILENUM *const array);
int set_filenum_in_array(const FT &ft, const uint32_t index, FILENUM *const array) {
    array[index] = toku_cachefile_filenum(ft->cf);
    return 0;
}

static int log_open_txn (TOKUTXN txn, void* extra) {
    int r;
    checkpointer* cp = (checkpointer *)extra;
    TOKULOGGER logger = txn->logger;
    FILENUMS open_filenums;
    uint32_t num_filenums = txn->open_fts.size();
    FILENUM array[num_filenums];
    if (toku_txn_is_read_only(txn)) {
        goto cleanup;
    }
    else {
        cp->increment_num_txns();
    }

    open_filenums.num      = num_filenums;
    open_filenums.filenums = array;
    //Fill in open_filenums
    r = txn->open_fts.iterate<FILENUM, set_filenum_in_array>(array);
    invariant(r==0);
    switch (toku_txn_get_state(txn)) {
    case TOKUTXN_LIVE:{
        toku_log_xstillopen(logger, NULL, 0, txn,
                            toku_txn_get_txnid(txn),
                            toku_txn_get_txnid(toku_logger_txn_parent(txn)),
                            txn->roll_info.rollentry_raw_count,
                            open_filenums,
                            txn->force_fsync_on_commit,
                            txn->roll_info.num_rollback_nodes,
                            txn->roll_info.num_rollentries,
                            txn->roll_info.spilled_rollback_head,
                            txn->roll_info.spilled_rollback_tail,
                            txn->roll_info.current_rollback);
        goto cleanup;
    }
    case TOKUTXN_PREPARING: {
        TOKU_XA_XID xa_xid;
        toku_txn_get_prepared_xa_xid(txn, &xa_xid);
        toku_log_xstillopenprepared(logger, NULL, 0, txn,
                                    toku_txn_get_txnid(txn),
                                    &xa_xid,
                                    txn->roll_info.rollentry_raw_count,
                                    open_filenums,
                                    txn->force_fsync_on_commit,
                                    txn->roll_info.num_rollback_nodes,
                                    txn->roll_info.num_rollentries,
                                    txn->roll_info.spilled_rollback_head,
                                    txn->roll_info.spilled_rollback_tail,
                                    txn->roll_info.current_rollback);
        goto cleanup;
    }
    case TOKUTXN_RETIRED:
    case TOKUTXN_COMMITTING:
    case TOKUTXN_ABORTING: {
        assert(0);
    }
    }
    // default is an error
    assert(0);
cleanup:
    return 0;
}

// Requires:   All three checkpoint-relevant locks must be held (see checkpoint.c).
// Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
//             Use the begin_checkpoint callback to take necessary snapshots (header, btt)
//             Mark every dirty node as "pending."  ("Pending" means that the node must be
//                                                    written to disk before it can be modified.)
void toku_cachetable_begin_checkpoint (CHECKPOINTER cp, TOKULOGGER UU(logger)) {    
    cp->begin_checkpoint();
}


// This is used by the cachetable_race test.  
static volatile int toku_checkpointing_user_data_status = 0;
static void toku_cachetable_set_checkpointing_user_data_status (int v) {
    toku_checkpointing_user_data_status = v;
}
int toku_cachetable_get_checkpointing_user_data_status (void) {
    return toku_checkpointing_user_data_status;
}

// Requires:   The big checkpoint lock must be held (see checkpoint.c).
// Algorithm:  Write all pending nodes to disk
//             Use checkpoint callback to write snapshot information to disk (header, btt)
//             Use end_checkpoint callback to fsync dictionary and log, and to free unused blocks
// Note:       If testcallback is null (for testing purposes only), call it after writing dictionary but before writing log
void toku_cachetable_end_checkpoint(CHECKPOINTER cp, TOKULOGGER UU(logger),
                               void (*testcallback_f)(void*),  void* testextra) {
    cp->end_checkpoint(testcallback_f, testextra);
}

TOKULOGGER toku_cachefile_logger (CACHEFILE cf) {
    return cf->cachetable->cp.get_logger();
}

FILENUM toku_cachefile_filenum (CACHEFILE cf) {
    return cf->filenum;
}

// debug functions

int toku_cachetable_assert_all_unpinned (CACHETABLE ct) {
    uint32_t i;
    int some_pinned=0;
    ct->list.read_list_lock();
    for (i=0; i<ct->list.m_table_size; i++) {
        PAIR p;
        for (p=ct->list.m_table[i]; p; p=p->hash_chain) {
            pair_lock(p);
            if (p->value_rwlock.users()) {
                //printf("%s:%d pinned: %" PRId64 " (%p)\n", __FILE__, __LINE__, p->key.b, p->value_data);
                some_pinned=1;
            }
            pair_unlock(p);
        }
    }
    ct->list.read_list_unlock();
    return some_pinned;
}

int toku_cachefile_count_pinned (CACHEFILE cf, int print_them) {
    assert(cf != NULL);
    int n_pinned=0;
    CACHETABLE ct = cf->cachetable;
    ct->list.read_list_lock();

    // Iterate over all the pairs to find pairs specific to the
    // given cachefile.
    for (uint32_t i = 0; i < ct->list.m_table_size; i++) {
        for (PAIR p = ct->list.m_table[i]; p; p = p->hash_chain) {
            if (p->cachefile == cf) {
                pair_lock(p);
                if (p->value_rwlock.users()) {
                    if (print_them) {
                        printf("%s:%d pinned: %" PRId64 " (%p)\n", 
                                __FILE__,
                                __LINE__,
                                p->key.b,
                                p->value_data);
                    }
                    n_pinned++;
                }                
                pair_unlock(p);
            }
        }
    }

    ct->list.read_list_unlock();
    return n_pinned;
}

void toku_cachetable_print_state (CACHETABLE ct) {
    uint32_t i;
    ct->list.read_list_lock();
    for (i=0; i<ct->list.m_table_size; i++) {
        PAIR p = ct->list.m_table[i];
        if (p != 0) {
            pair_lock(p);
            printf("t[%u]=", i);
            for (p=ct->list.m_table[i]; p; p=p->hash_chain) {
                printf(" {%" PRId64 ", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, (int) p->dirty, p->value_rwlock.users(), p->attr.size);
            }
            printf("\n");
            pair_unlock(p);
        }
    }
    ct->list.read_list_unlock();
}

void toku_cachetable_get_state (CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr) {
    ct->list.get_state(num_entries_ptr, hash_size_ptr);
    ct->ev.get_state(size_current_ptr, size_limit_ptr);
}

int toku_cachetable_get_key_state (CACHETABLE ct, CACHEKEY key, CACHEFILE cf, void **value_ptr,
                                   int *dirty_ptr, long long *pin_ptr, long *size_ptr) {
    int r = -1;
    uint32_t fullhash = toku_cachetable_hash(cf, key);
    ct->list.read_list_lock();
    PAIR p = ct->list.find_pair(cf, key, fullhash);
    if (p) {
        pair_lock(p);
        if (value_ptr)
            *value_ptr = p->value_data;
        if (dirty_ptr)
            *dirty_ptr = p->dirty;
        if (pin_ptr)
            *pin_ptr = p->value_rwlock.users();
        if (size_ptr)
            *size_ptr = p->attr.size;
        r = 0;
        pair_unlock(p);
    }
    ct->list.read_list_unlock();
    return r;
}

void
toku_cachefile_set_userdata (CACHEFILE cf,
                             void *userdata,
                             void (*log_fassociate_during_checkpoint)(CACHEFILE, void*),
                             void (*close_userdata)(CACHEFILE, int, void*, bool, LSN),
                             void (*free_userdata)(CACHEFILE, void*),
                             void (*checkpoint_userdata)(CACHEFILE, int, void*),
                             void (*begin_checkpoint_userdata)(LSN, void*),
                             void (*end_checkpoint_userdata)(CACHEFILE, int, void*),
                             void (*note_pin_by_checkpoint)(CACHEFILE, void*),
                             void (*note_unpin_by_checkpoint)(CACHEFILE, void*)) {
    cf->userdata = userdata;
    cf->log_fassociate_during_checkpoint = log_fassociate_during_checkpoint;
    cf->close_userdata = close_userdata;
    cf->free_userdata = free_userdata;
    cf->checkpoint_userdata = checkpoint_userdata;
    cf->begin_checkpoint_userdata = begin_checkpoint_userdata;
    cf->end_checkpoint_userdata = end_checkpoint_userdata;
    cf->note_pin_by_checkpoint = note_pin_by_checkpoint;
    cf->note_unpin_by_checkpoint = note_unpin_by_checkpoint;
}

void *toku_cachefile_get_userdata(CACHEFILE cf) {
    return cf->userdata;
}

CACHETABLE
toku_cachefile_get_cachetable(CACHEFILE cf) {
    return cf->cachetable;
}

//Only called by ft_end_checkpoint
//Must have access to cf->fd (must be protected)
void toku_cachefile_fsync(CACHEFILE cf) {
    toku_file_fsync(cf->fd);
}

// Make it so when the cachefile closes, the underlying file is unlinked
void toku_cachefile_unlink_on_close(CACHEFILE cf) {
    assert(!cf->unlink_on_close);
    cf->unlink_on_close = true;
}

// is this cachefile marked as unlink on close?
bool toku_cachefile_is_unlink_on_close(CACHEFILE cf) {
    return cf->unlink_on_close;
}

uint64_t toku_cachefile_size(CACHEFILE cf) {
    int64_t file_size;
    int fd = toku_cachefile_get_fd(cf);
    int r = toku_os_get_file_size(fd, &file_size);
    assert_zero(r);
    return file_size;
}

char *
toku_construct_full_name(int count, ...) {
    va_list ap;
    char *name = NULL;
    size_t n = 0;
    int i;
    va_start(ap, count);
    for (i=0; i<count; i++) {
        char *arg = va_arg(ap, char *);
        if (arg) {
            n += 1 + strlen(arg) + 1;
            char *XMALLOC_N(n, newname);
            if (name && !toku_os_is_absolute_name(arg))
                snprintf(newname, n, "%s/%s", name, arg);
            else
                snprintf(newname, n, "%s", arg);
            toku_free(name);
            name = newname;
        }
    }
    va_end(ap);

    return name;
}

char *
toku_cachetable_get_fname_in_cwd(CACHETABLE ct, const char * fname_in_env) {
    return toku_construct_full_name(2, ct->env_dir, fname_in_env);
}

static long
cleaner_thread_rate_pair(PAIR p)
{
    return p->attr.cache_pressure_size;
}

static int const CLEANER_N_TO_CHECK = 8;

int toku_cleaner_thread_for_test (CACHETABLE ct) {
    return ct->cl.run_cleaner();
}

int toku_cleaner_thread (void *cleaner_v) {
    cleaner* cl = (cleaner *) cleaner_v;
    assert(cl);
    return cl->run_cleaner();
}

/////////////////////////////////////////////////////////////////////////
//
// cleaner methods
//
ENSURE_POD(cleaner);

int cleaner::init(uint32_t _cleaner_iterations, pair_list* _pl, CACHETABLE _ct) {
    // default is no cleaner, for now
    m_cleaner_cron_init = false;
    int r = toku_minicron_setup(&m_cleaner_cron, 0, toku_cleaner_thread, this);
    if (r == 0) {
        m_cleaner_cron_init = true;
    }
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&m_cleaner_iterations, sizeof m_cleaner_iterations);
    m_cleaner_iterations = _cleaner_iterations;
    m_pl = _pl;
    m_ct = _ct;
    m_cleaner_init = true;
    return r;
}

// this function is allowed to be called multiple times
void cleaner::destroy(void) {
    if (!m_cleaner_init) {
        return;
    }
    if (m_cleaner_cron_init && !toku_minicron_has_been_shutdown(&m_cleaner_cron)) {
        // for test code only, production code uses toku_cachetable_minicron_shutdown()
        int r = toku_minicron_shutdown(&m_cleaner_cron);
        assert(r==0);
    }
}

uint32_t cleaner::get_iterations(void) {
    return m_cleaner_iterations;
}

void cleaner::set_iterations(uint32_t new_iterations) {
    m_cleaner_iterations = new_iterations;
}

uint32_t cleaner::get_period_unlocked(void) {
    return toku_minicron_get_period_in_seconds_unlocked(&m_cleaner_cron);
}

//
// Sets how often the cleaner thread will run, in seconds
//
void cleaner::set_period(uint32_t new_period) {
    toku_minicron_change_period(&m_cleaner_cron, new_period*1000);
}

// Effect:  runs a cleaner.
//
// We look through some number of nodes, the first N that we see which are
// unlocked and are not involved in a cachefile flush, pick one, and call
// the cleaner callback.  While we're picking a node, we have the
// cachetable lock the whole time, so we don't need any extra
// synchronization.  Once we have one we want, we lock it and notify the
// cachefile that we're doing some background work (so a flush won't
// start).  At this point, we can safely unlock the cachetable, do the
// work (callback), and unlock/release our claim to the cachefile.
int cleaner::run_cleaner(void) {
    toku::context cleaner_ctx(CTX_CLEANER);

    int r;
    uint32_t num_iterations = this->get_iterations();
    for (uint32_t i = 0; i < num_iterations; ++i) {
        cleaner_executions++;
        m_pl->read_list_lock();
        PAIR best_pair = NULL;
        int n_seen = 0;
        long best_score = 0;
        const PAIR first_pair = m_pl->m_cleaner_head;
        if (first_pair == NULL) {
            // nothing in the cachetable, just get out now
            m_pl->read_list_unlock();
            break;
        }
        // here we select a PAIR for cleaning
        // look at some number of PAIRS, and
        // pick what we think is the best one for cleaning
        //***** IMPORTANT ******
        // we MUST not pick a PAIR whose rating is 0. We have
        // numerous assumptions in other parts of the code that
        // this is the case:
        //  - this is how rollback nodes and leaf nodes are not selected for cleaning
        //  - this is how a thread that is calling unpin_and_remove will prevent
        //     the cleaner thread from picking its PAIR (see comments in that function)
        do {
            //
            // We are already holding onto best_pair, if we run across a pair that
            // has the same mutex due to a collision in the hashtable, we need
            // to be careful.
            //
            if (best_pair && m_pl->m_cleaner_head->mutex == best_pair->mutex) {
                // Advance the cleaner head.
                long score = 0;
                // only bother with this pair if it has no current users
                if (m_pl->m_cleaner_head->value_rwlock.users() == 0) {
                    score = cleaner_thread_rate_pair(m_pl->m_cleaner_head);
                    if (score > best_score) {
                        best_score = score;
                        best_pair = m_pl->m_cleaner_head;
                    }
                }
                m_pl->m_cleaner_head = m_pl->m_cleaner_head->clock_next;
                continue;
            }
            pair_lock(m_pl->m_cleaner_head);
            if (m_pl->m_cleaner_head->value_rwlock.users() > 0) {
                pair_unlock(m_pl->m_cleaner_head);
            }
            else {
                n_seen++;
                long score = 0;
                score = cleaner_thread_rate_pair(m_pl->m_cleaner_head);
                if (score > best_score) {
                    best_score = score;
                    // Since we found a new best pair, we need to
                    // free the old best pair.
                    if (best_pair) {
                        pair_unlock(best_pair);
                    }
                    best_pair = m_pl->m_cleaner_head;
                }
                else {
                    pair_unlock(m_pl->m_cleaner_head);
                }
            }
            // Advance the cleaner head.
            m_pl->m_cleaner_head = m_pl->m_cleaner_head->clock_next;
        } while (m_pl->m_cleaner_head != first_pair && n_seen < CLEANER_N_TO_CHECK);
        m_pl->read_list_unlock();

        //
        // at this point, if we have found a PAIR for cleaning,
        // that is, best_pair != NULL, we do the clean
        //
        // if best_pair !=NULL, then best_pair->mutex is held
        // no list lock is held
        //
        if (best_pair) {
            CACHEFILE cf = best_pair->cachefile;
            // try to add a background job to the manager
            // if we can't, that means the cachefile is flushing, so
            // we simply continue the for loop and this iteration
            // becomes a no-op
            r = bjm_add_background_job(cf->bjm);
            if (r) {
                pair_unlock(best_pair);
                continue;
            }
            best_pair->value_rwlock.write_lock(true);
            pair_unlock(best_pair);
            // verify a key assumption.
            assert(cleaner_thread_rate_pair(best_pair) > 0);
            // check the checkpoint_pending bit
            m_pl->read_pending_cheap_lock();
            bool checkpoint_pending = best_pair->checkpoint_pending;
            best_pair->checkpoint_pending = false;
            m_pl->read_pending_cheap_unlock();
            if (checkpoint_pending) {
                write_locked_pair_for_checkpoint(m_ct, best_pair, true);
            }

            bool cleaner_callback_called = false;

            // it's theoretically possible that after writing a PAIR for checkpoint, the
            // PAIR's heuristic tells us nothing needs to be done. It is not possible
            // in Dr. Noga, but unit tests verify this behavior works properly.
            if (cleaner_thread_rate_pair(best_pair) > 0) {
                r = best_pair->cleaner_callback(best_pair->value_data,
                                                    best_pair->key,
                                                    best_pair->fullhash,
                                                    best_pair->write_extraargs);
                assert_zero(r);
                cleaner_callback_called = true;
            }

            // The cleaner callback must have unlocked the pair, so we
            // don't need to unlock it if the cleaner callback is called.
            if (!cleaner_callback_called) {
                pair_lock(best_pair);
                best_pair->value_rwlock.write_unlock();
                pair_unlock(best_pair);
            }
            // We need to make sure the cachefile sticks around so a close
            // can't come destroy it.  That's the purpose of this
            // "add/remove_background_job" business, which means the
            // cachefile is still valid here, even though the cleaner
            // callback unlocks the pair. 
            bjm_remove_background_job(cf->bjm);
        }
        else {
            // If we didn't find anything this time around the cachetable,
            // we probably won't find anything if we run around again, so
            // just break out from the for-loop now and 
            // we'll try again when the cleaner thread runs again.
            break;
        }
    }
    return 0;
}

static_assert(std::is_pod<pair_list>::value, "pair_list isn't POD");

const uint32_t INITIAL_PAIR_LIST_SIZE = 1<<20;
uint32_t PAIR_LOCK_SIZE = 1<<20;

void toku_pair_list_set_lock_size(uint32_t num_locks) {
    PAIR_LOCK_SIZE = num_locks;
}

static void evict_pair_from_cachefile(PAIR p) {
    CACHEFILE cf = p->cachefile;
    if (p->cf_next) {
        p->cf_next->cf_prev = p->cf_prev;
    }
    if (p->cf_prev) {
        p->cf_prev->cf_next = p->cf_next;
    }
    else if (p->cachefile->cf_head == p) {
        cf->cf_head = p->cf_next;
    }
    p->cf_prev = p->cf_next = NULL;
    cf->num_pairs--;
}

// Allocates the hash table of pairs inside this pair list.
//
void pair_list::init() {
    m_table_size = INITIAL_PAIR_LIST_SIZE;
    m_num_locks = PAIR_LOCK_SIZE;
    m_n_in_table = 0;
    m_clock_head = NULL;
    m_cleaner_head = NULL;
    m_checkpoint_head = NULL;
    m_pending_head = NULL;
    m_table = NULL;
    

    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
#if defined(HAVE_PTHREAD_RWLOCKATTR_SETKIND_NP)
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#else
    // TODO: need to figure out how to make writer-preferential rwlocks
    // happen on osx
#endif
    toku_pthread_rwlock_init(&m_list_lock, &attr);
    toku_pthread_rwlock_init(&m_pending_lock_expensive, &attr);    
    toku_pthread_rwlock_init(&m_pending_lock_cheap, &attr);    
    XCALLOC_N(m_table_size, m_table);
    XCALLOC_N(m_num_locks, m_mutexes);
    for (uint64_t i = 0; i < m_num_locks; i++) {
        toku_mutex_init(&m_mutexes[i].aligned_mutex, NULL);
    }
}

// Frees the pair_list hash table.  It is expected to be empty by
// the time this is called.  Returns an error if there are any
// pairs in any of the hash table slots.
void pair_list::destroy() {
    // Check if any entries exist in the hash table.
    for (uint32_t i = 0; i < m_table_size; ++i) {
        invariant_null(m_table[i]);
    }
    for (uint64_t i = 0; i < m_num_locks; i++) {
        toku_mutex_destroy(&m_mutexes[i].aligned_mutex);
    }
    toku_pthread_rwlock_destroy(&m_list_lock);    
    toku_pthread_rwlock_destroy(&m_pending_lock_expensive);    
    toku_pthread_rwlock_destroy(&m_pending_lock_cheap);    
    toku_free(m_table);
    toku_free(m_mutexes);
}

// adds a PAIR to the cachetable's structures,
// but does NOT add it to the list maintained by
// the cachefile
void pair_list::add_to_cachetable_only(PAIR p) {
    // sanity check to make sure that the PAIR does not already exist
    PAIR pp = this->find_pair(p->cachefile, p->key, p->fullhash);
    assert(pp == NULL);

    this->add_to_clock(p);
    this->add_to_hash_chain(p);
    m_n_in_table++;
}

// This places the given pair inside of the pair list.
//
// requires caller to have grabbed write lock on list.
// requires caller to have p->mutex held as well
//
void pair_list::put(PAIR p) {
    this->add_to_cachetable_only(p);
    this->add_to_cf_list(p);
}

// This removes the given pair from completely from the pair list.
//
// requires caller to have grabbed write lock on list, and p->mutex held
//
void pair_list::evict_completely(PAIR p) {
    this->evict_from_cachetable(p);
    this->evict_from_cachefile(p);
}

// Removes the PAIR from the cachetable's lists,
// but does NOT impact the list maintained by the cachefile
void pair_list::evict_from_cachetable(PAIR p) {
    this->pair_remove(p);
    this->pending_pairs_remove(p);
    this->remove_from_hash_chain(p);
    
    assert(m_n_in_table > 0);
    m_n_in_table--;    
}

// Removes the PAIR from the cachefile's list of PAIRs
void pair_list::evict_from_cachefile(PAIR p) {
    evict_pair_from_cachefile(p);
}

// 
// Remove pair from linked list for cleaner/clock
//
//
// requires caller to have grabbed write lock on list.
//
void pair_list::pair_remove (PAIR p) {
    if (p->clock_prev == p) {
        invariant(m_clock_head == p);
        invariant(p->clock_next == p);
        invariant(m_cleaner_head == p);
        invariant(m_checkpoint_head == p);
        m_clock_head = NULL;
        m_cleaner_head = NULL;
        m_checkpoint_head = NULL;
    }
    else {
        if (p == m_clock_head) {
            m_clock_head = m_clock_head->clock_next;
        }
        if (p == m_cleaner_head) {
            m_cleaner_head = m_cleaner_head->clock_next;
        }
        if (p == m_checkpoint_head) {
            m_checkpoint_head = m_checkpoint_head->clock_next;
        }
        p->clock_prev->clock_next = p->clock_next;
        p->clock_next->clock_prev = p->clock_prev;
    }
    p->clock_prev = p->clock_next = NULL;
}

//Remove a pair from the list of pairs that were marked with the
//pending bit for the in-progress checkpoint.
//
// requires that if the caller is the checkpoint thread, then a read lock
// is grabbed on the list. Otherwise, must have write lock on list.
//
void pair_list::pending_pairs_remove (PAIR p) {
    if (p->pending_next) {
        p->pending_next->pending_prev = p->pending_prev;
    }
    if (p->pending_prev) {
        p->pending_prev->pending_next = p->pending_next;
    }
    else if (m_pending_head==p) {
        m_pending_head = p->pending_next;
    }
    p->pending_prev = p->pending_next = NULL;
}

void pair_list::remove_from_hash_chain(PAIR p) {
    // Remove it from the hash chain.
    unsigned int h = p->fullhash&(m_table_size - 1);
    paranoid_invariant(m_table[h] != NULL);
    if (m_table[h] == p) {
        m_table[h] = p->hash_chain;
    }
    else {
        PAIR curr = m_table[h];
        while (curr->hash_chain != p) {
            curr = curr->hash_chain;
        }
        // remove p from the singular linked list
        curr->hash_chain = p->hash_chain;
    }
    p->hash_chain = NULL;
}

// Returns a pair from the pair list, using the given 
// pair.  If the pair cannot be found, null is returned.
//
// requires caller to have grabbed either a read lock on the list or
// bucket's mutex.
//
PAIR pair_list::find_pair(CACHEFILE file, CACHEKEY key, uint32_t fullhash) {
    PAIR found_pair = nullptr;
    for (PAIR p = m_table[fullhash&(m_table_size - 1)]; p; p = p->hash_chain) {
        if (p->key.b == key.b && p->cachefile == file) {
            found_pair = p;
            break;
        }
    }
    return found_pair;
}

// Add PAIR to linked list shared by cleaner thread and clock
//
// requires caller to have grabbed write lock on list.
//
void pair_list::add_to_clock (PAIR p) {
    // requires that p is not currently in the table.
    // inserts p into the clock list at the tail.

    p->count = CLOCK_INITIAL_COUNT;
    //assert either both head and tail are set or they are both NULL
    // tail and head exist
    if (m_clock_head) {
        assert(m_cleaner_head);
        assert(m_checkpoint_head);
        // insert right before the head
        p->clock_next = m_clock_head;
        p->clock_prev = m_clock_head->clock_prev;

        p->clock_prev->clock_next = p;
        p->clock_next->clock_prev = p;

    }
    // this is the first element in the list
    else {
        m_clock_head = p;
        p->clock_next = p->clock_prev = m_clock_head;
        m_cleaner_head = p;
        m_checkpoint_head = p;
    }
}

// add the pair to the linked list that of PAIRs belonging 
// to the same cachefile. This linked list is used
// in cachetable_flush_cachefile.
void pair_list::add_to_cf_list(PAIR p) {
    CACHEFILE cf = p->cachefile;
    if (cf->cf_head) {
        cf->cf_head->cf_prev = p;
    }
    p->cf_next = cf->cf_head;
    p->cf_prev = NULL;
    cf->cf_head = p;
    cf->num_pairs++;
}

// Add PAIR to the hashtable
//
// requires caller to have grabbed write lock on list
// and to have grabbed the p->mutex.
void pair_list::add_to_hash_chain(PAIR p) {
    uint32_t h = p->fullhash & (m_table_size - 1);
    p->hash_chain = m_table[h];
    m_table[h] = p;
}

// test function
//
// grabs and releases write list lock
//
void pair_list::verify() {
    this->write_list_lock();
    uint32_t num_found = 0;

    // First clear all the verify flags by going through the hash chains
    {
        uint32_t i;
        for (i = 0; i < m_table_size; i++) {
            PAIR p;
            for (p = m_table[i]; p; p = p->hash_chain) {
                num_found++;
            }
        }
    }
    assert(num_found == m_n_in_table);
    num_found = 0;
    // Now go through the clock chain, make sure everything in the LRU chain is hashed.
    {
        PAIR p;
        bool is_first = true;
        for (p = m_clock_head; m_clock_head != NULL && (p != m_clock_head || is_first); p=p->clock_next) {
            is_first=false;
            PAIR p2;
            uint32_t fullhash = p->fullhash;
            //assert(fullhash==toku_cachetable_hash(p->cachefile, p->key));
            for (p2 = m_table[fullhash&(m_table_size-1)]; p2; p2=p2->hash_chain) {
                if (p2==p) {
                    /* found it */
                    num_found++;
                    goto next;
                }
            }
            fprintf(stderr, "Something in the clock chain is not hashed\n");
            assert(0);
        next:;
        }
        assert (num_found == m_n_in_table);
    }
    this->write_list_unlock();
}

// If given pointers are not null, assign the hash table size of 
// this pair list and the number of pairs in this pair list.
//
//
// grabs and releases read list lock
//
void pair_list::get_state(int *num_entries, int *hash_size) {
    this->read_list_lock();
    if (num_entries) {
        *num_entries = m_n_in_table;
    }
    if (hash_size) {
        *hash_size = m_table_size;
    }
    this->read_list_unlock();
}

void pair_list::read_list_lock() {
    toku_pthread_rwlock_rdlock(&m_list_lock);
}

void pair_list::read_list_unlock() {
    toku_pthread_rwlock_rdunlock(&m_list_lock);
}

void pair_list::write_list_lock() {
    toku_pthread_rwlock_wrlock(&m_list_lock);
}

void pair_list::write_list_unlock() {
    toku_pthread_rwlock_wrunlock(&m_list_lock);
}

void pair_list::read_pending_exp_lock() {
    toku_pthread_rwlock_rdlock(&m_pending_lock_expensive);
}

void pair_list::read_pending_exp_unlock() {
    toku_pthread_rwlock_rdunlock(&m_pending_lock_expensive);
}

void pair_list::write_pending_exp_lock() {
    toku_pthread_rwlock_wrlock(&m_pending_lock_expensive);
}

void pair_list::write_pending_exp_unlock() {
    toku_pthread_rwlock_wrunlock(&m_pending_lock_expensive);
}

void pair_list::read_pending_cheap_lock() {
    toku_pthread_rwlock_rdlock(&m_pending_lock_cheap);
}

void pair_list::read_pending_cheap_unlock() {
    toku_pthread_rwlock_rdunlock(&m_pending_lock_cheap);
}

void pair_list::write_pending_cheap_lock() {
    toku_pthread_rwlock_wrlock(&m_pending_lock_cheap);
}

void pair_list::write_pending_cheap_unlock() {
    toku_pthread_rwlock_wrunlock(&m_pending_lock_cheap);
}

toku_mutex_t* pair_list::get_mutex_for_pair(uint32_t fullhash) {
    return &m_mutexes[fullhash&(m_num_locks - 1)].aligned_mutex;
}

void pair_list::pair_lock_by_fullhash(uint32_t fullhash) {
    toku_mutex_lock(&m_mutexes[fullhash&(m_num_locks - 1)].aligned_mutex);
}

void pair_list::pair_unlock_by_fullhash(uint32_t fullhash) {
    toku_mutex_unlock(&m_mutexes[fullhash&(m_num_locks - 1)].aligned_mutex);
}


ENSURE_POD(evictor);

//
// This is the function that runs eviction on its own thread.
//
static void *eviction_thread(void *evictor_v) {
    evictor* CAST_FROM_VOIDP(evictor, evictor_v);
    evictor->run_eviction_thread();
    return evictor_v;
}

//
// Starts the eviction thread, assigns external object references,
// and initializes all counters and condition variables.
//
int evictor::init(long _size_limit, pair_list* _pl, cachefile_list* _cf_list, KIBBUTZ _kibbutz, uint32_t eviction_period) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&m_ev_thread_is_running, sizeof m_ev_thread_is_running);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&m_size_evicting, sizeof m_size_evicting);

    // set max difference to around 500MB
    int64_t max_diff = (1 << 29);
    
    m_low_size_watermark = _size_limit;
    // these values are selected kind of arbitrarily right now as 
    // being a percentage more than low_size_watermark, which is provided
    // by the caller.
    m_low_size_hysteresis = (11 * _size_limit)/10; //10% more
    if ((m_low_size_hysteresis - m_low_size_watermark) > max_diff) {
        m_low_size_hysteresis = m_low_size_watermark + max_diff;
    }
    m_high_size_hysteresis = (5 * _size_limit)/4; // 20% more
    if ((m_high_size_hysteresis - m_low_size_hysteresis) > max_diff) {
        m_high_size_hysteresis = m_low_size_hysteresis + max_diff;
    }
    m_high_size_watermark = (3 * _size_limit)/2; // 50% more
    if ((m_high_size_watermark - m_high_size_hysteresis) > max_diff) {
        m_high_size_watermark = m_high_size_hysteresis + max_diff;
    }
    
    m_size_reserved = unreservable_memory(_size_limit);
    m_size_current = 0;
    m_size_cloned_data = 0;
    m_size_evicting = 0;

    m_size_nonleaf = create_partitioned_counter(); 
    m_size_leaf = create_partitioned_counter();
    m_size_rollback = create_partitioned_counter();
    m_size_cachepressure = create_partitioned_counter();
    m_wait_pressure_count = create_partitioned_counter();
    m_wait_pressure_time = create_partitioned_counter();
    m_long_wait_pressure_count = create_partitioned_counter();
    m_long_wait_pressure_time = create_partitioned_counter();

    m_pl = _pl;
    m_cf_list = _cf_list;
    m_kibbutz = _kibbutz;    
    toku_mutex_init(&m_ev_thread_lock, NULL);
    toku_cond_init(&m_flow_control_cond, NULL);
    toku_cond_init(&m_ev_thread_cond, NULL);
    m_num_sleepers = 0;    
    m_ev_thread_is_running = false;
    m_period_in_seconds = eviction_period;

    unsigned int seed = (unsigned int) time(NULL);
    int r = myinitstate_r(seed, m_random_statebuf, sizeof m_random_statebuf, &m_random_data);
    assert_zero(r);

    // start the background thread    
    m_run_thread = true;
    m_num_eviction_thread_runs = 0;
    m_ev_thread_init = false;
    r = toku_pthread_create(&m_ev_thread, NULL, eviction_thread, this); 
    if (r == 0) {
        m_ev_thread_init = true;
    }
    m_evictor_init = true;
    return r;
}

//
// This stops the eviction thread and clears the condition variable.
//
// NOTE: This should only be called if there are no evictions in progress.
//
void evictor::destroy() { 
    if (!m_evictor_init) {
        return;
    }
    assert(m_size_evicting == 0);
    //
    // commented out of Ming, because we could not finish
    // #5672. Once #5672 is solved, we should restore this
    //
    //assert(m_size_current == 0);

    // Stop the eviction thread.
    if (m_ev_thread_init) {
        toku_mutex_lock(&m_ev_thread_lock);
        m_run_thread = false;
        this->signal_eviction_thread();
        toku_mutex_unlock(&m_ev_thread_lock);
        void *ret;
        int r = toku_pthread_join(m_ev_thread, &ret); 
        assert_zero(r);
        assert(!m_ev_thread_is_running);
    }
    destroy_partitioned_counter(m_size_nonleaf);
    m_size_nonleaf = NULL;
    destroy_partitioned_counter(m_size_leaf);
    m_size_leaf = NULL;
    destroy_partitioned_counter(m_size_rollback);
    m_size_rollback = NULL;
    destroy_partitioned_counter(m_size_cachepressure);    
    m_size_cachepressure = NULL;

    destroy_partitioned_counter(m_wait_pressure_count); m_wait_pressure_count = NULL;
    destroy_partitioned_counter(m_wait_pressure_time); m_wait_pressure_time = NULL;
    destroy_partitioned_counter(m_long_wait_pressure_count); m_long_wait_pressure_count = NULL;
    destroy_partitioned_counter(m_long_wait_pressure_time); m_long_wait_pressure_time = NULL;

    toku_cond_destroy(&m_flow_control_cond);
    toku_cond_destroy(&m_ev_thread_cond);
    toku_mutex_destroy(&m_ev_thread_lock);
}

//
// Increases status variables and the current size variable
// of the evictor based on the given pair attribute.
//
void evictor::add_pair_attr(PAIR_ATTR attr) {
    assert(attr.is_valid);
    add_to_size_current(attr.size);
    increment_partitioned_counter(m_size_nonleaf, attr.nonleaf_size);
    increment_partitioned_counter(m_size_leaf, attr.leaf_size);
    increment_partitioned_counter(m_size_rollback, attr.rollback_size);
    increment_partitioned_counter(m_size_cachepressure, attr.cache_pressure_size);
}

//
// Decreases status variables and the current size variable
// of the evictor based on the given pair attribute.
//
void evictor::remove_pair_attr(PAIR_ATTR attr) {
    assert(attr.is_valid);
    remove_from_size_current(attr.size);
    increment_partitioned_counter(m_size_nonleaf, 0 - attr.nonleaf_size);
    increment_partitioned_counter(m_size_leaf, 0 - attr.leaf_size);
    increment_partitioned_counter(m_size_rollback, 0 - attr.rollback_size);
    increment_partitioned_counter(m_size_cachepressure, 0 - attr.cache_pressure_size);
}

//
// Updates this evictor's stats to match the "new" pair attribute given
// while also removing the given "old" pair attribute. 
//
void evictor::change_pair_attr(PAIR_ATTR old_attr, PAIR_ATTR new_attr) {
    this->add_pair_attr(new_attr);
    this->remove_pair_attr(old_attr);
}

//
// Adds the given size to the evictor's estimation of 
// the size of the cachetable.
//
void evictor::add_to_size_current(long size) {
    (void) toku_sync_fetch_and_add(&m_size_current, size);
}

//
// Subtracts the given size from the evictor's current
// approximation of the cachetable size.
//
void evictor::remove_from_size_current(long size) {
    (void) toku_sync_fetch_and_sub(&m_size_current, size);
}

//
// Adds the size of cloned data to necessary variables in the evictor
//
void evictor::add_cloned_data_size(long size) {
    (void) toku_sync_fetch_and_add(&m_size_cloned_data, size);
    add_to_size_current(size);
}

//
// Removes  the size of cloned data to necessary variables in the evictor
//
void evictor::remove_cloned_data_size(long size) {
    (void) toku_sync_fetch_and_sub(&m_size_cloned_data, size);
    remove_from_size_current(size);
}

//
// TODO: (Zardosht) comment this function
//
uint64_t evictor::reserve_memory(double fraction, uint64_t upper_bound) {
    toku_mutex_lock(&m_ev_thread_lock);
    uint64_t reserved_memory = fraction * (m_low_size_watermark - m_size_reserved);
    if (0) { // debug
        fprintf(stderr, "%s %" PRIu64 " %" PRIu64 "\n", __PRETTY_FUNCTION__, reserved_memory, upper_bound);
    }
    if (upper_bound > 0 && reserved_memory > upper_bound) {
        reserved_memory = upper_bound;
    }
    m_size_reserved += reserved_memory;
    (void) toku_sync_fetch_and_add(&m_size_current, reserved_memory);
    this->signal_eviction_thread();  
    toku_mutex_unlock(&m_ev_thread_lock);

    if (this->should_client_thread_sleep()) {
        this->wait_for_cache_pressure_to_subside();
    }
    return reserved_memory;
}

//
// TODO: (Zardosht) comment this function
//
void evictor::release_reserved_memory(uint64_t reserved_memory){
    (void) toku_sync_fetch_and_sub(&m_size_current, reserved_memory);
    toku_mutex_lock(&m_ev_thread_lock);    
    m_size_reserved -= reserved_memory;
    // signal the eviction thread in order to possibly wake up sleeping clients
    if (m_num_sleepers  > 0) {
        this->signal_eviction_thread();
    }
    toku_mutex_unlock(&m_ev_thread_lock);
}

//
// This function is the eviction thread. It runs for the lifetime of 
// the evictor. Goes to sleep for period_in_seconds 
// by waiting on m_ev_thread_cond.
//
void evictor::run_eviction_thread(){
    toku_mutex_lock(&m_ev_thread_lock);
    while (m_run_thread) {
        m_num_eviction_thread_runs++; // for test purposes only
        m_ev_thread_is_running = true;
        // responsibility of run_eviction to release and 
        // regrab ev_thread_lock as it sees fit
        this->run_eviction();
        m_ev_thread_is_running = false;

        if (m_run_thread) {
            //
            // sleep until either we are signaled
            // via signal_eviction_thread or 
            // m_period_in_seconds amount of time has passed
            //
            if (m_period_in_seconds) {
                toku_timespec_t wakeup_time;
                struct timeval tv;
                gettimeofday(&tv, 0);
                wakeup_time.tv_sec  = tv.tv_sec;
                wakeup_time.tv_nsec = tv.tv_usec * 1000LL;
                wakeup_time.tv_sec += m_period_in_seconds;
                toku_cond_timedwait(
                    &m_ev_thread_cond, 
                    &m_ev_thread_lock, 
                    &wakeup_time
                    );
            }
            // for test purposes, we have an option of 
            // not waiting on a period, but rather sleeping indefinitely
            else {
                toku_cond_wait(&m_ev_thread_cond, &m_ev_thread_lock);
            }
        }
    }
    toku_mutex_unlock(&m_ev_thread_lock);
}

//
// runs eviction.
// on entry, ev_thread_lock is grabbed, on exit, ev_thread_lock must still be grabbed
// it is the responsibility of this function to release and reacquire ev_thread_lock as it sees fit.
//
void evictor::run_eviction(){
    //
    // These variables will help us detect if everything in the clock is currently being accessed.
    // We must detect this case otherwise we will end up in an infinite loop below.
    //
    bool exited_early = false;
    uint32_t num_pairs_examined_without_evicting = 0;
    
    while (this->eviction_needed()) {
        if (m_num_sleepers > 0 && this->should_sleeping_clients_wakeup()) {
            toku_cond_broadcast(&m_flow_control_cond);
        }
        // release ev_thread_lock so that eviction may run without holding mutex
        toku_mutex_unlock(&m_ev_thread_lock);

        // first try to do an eviction from stale cachefiles
        bool some_eviction_ran = m_cf_list->evict_some_stale_pair(this);
        if (!some_eviction_ran) {
            m_pl->read_list_lock();
            PAIR curr_in_clock = m_pl->m_clock_head;
            // if nothing to evict, we need to exit
            if (!curr_in_clock) {
                m_pl->read_list_unlock();
                toku_mutex_lock(&m_ev_thread_lock);
                exited_early = true;
                goto exit;
            }
            if (num_pairs_examined_without_evicting > m_pl->m_n_in_table) {
                // we have a cycle where everything in the clock is in use
                // do not return an error
                // just let memory be overfull
                m_pl->read_list_unlock();
                toku_mutex_lock(&m_ev_thread_lock);
                exited_early = true;
                goto exit;
            }
            bool eviction_run = run_eviction_on_pair(curr_in_clock);
            if (eviction_run) {
                // reset the count
                num_pairs_examined_without_evicting = 0;
            }
            else {
                num_pairs_examined_without_evicting++;
            }
            // at this point, either curr_in_clock is still in the list because it has not been fully evicted,
            // and we need to move ct->m_clock_head over. Otherwise, curr_in_clock has been fully evicted
            // and we do NOT need to move ct->m_clock_head, as the removal of curr_in_clock
            // modified ct->m_clock_head
            if (m_pl->m_clock_head && (m_pl->m_clock_head == curr_in_clock)) {
                m_pl->m_clock_head = m_pl->m_clock_head->clock_next;
            }
            m_pl->read_list_unlock();
        }
        toku_mutex_lock(&m_ev_thread_lock);
    }

exit:
    if (m_num_sleepers > 0 && (exited_early || this->should_sleeping_clients_wakeup())) {
        toku_cond_broadcast(&m_flow_control_cond);
    }
    return;
}

//
// NOTE: Cachetable lock held on entry.
// Runs eviction on the given PAIR.  This may be a 
// partial eviction or full eviction.
//
// on entry, pair mutex is NOT held, but pair list's read list lock 
// IS held
// on exit, the same conditions must apply
//
bool evictor::run_eviction_on_pair(PAIR curr_in_clock) {
    uint32_t n_in_table;
    int64_t size_current;
    bool ret_val = false;
    // function meant to be called on PAIR that is not being accessed right now
    CACHEFILE cf = curr_in_clock->cachefile;
    int r = bjm_add_background_job(cf->bjm);
    if (r) {
        goto exit;
    }
    pair_lock(curr_in_clock);
    // these are the circumstances under which we don't run eviction on a pair: 
    //  - if other users are waiting on the lock 
    //  - if the PAIR is referenced by users 
    //  - if the PAIR's disk_nb_mutex is in use, implying that it is 
    //    undergoing a checkpoint
    if (curr_in_clock->value_rwlock.users() || 
        curr_in_clock->refcount > 0 || 
        nb_mutex_users(&curr_in_clock->disk_nb_mutex)) 
    {
        pair_unlock(curr_in_clock);
        bjm_remove_background_job(cf->bjm);
        goto exit;
    }

    // extract and use these values so that we don't risk them changing
    // out from underneath us in calculations below.
    n_in_table = m_pl->m_n_in_table;
    size_current = m_size_current; 

    // now that we have the pair mutex we care about, we can
    // release the read list lock and reacquire it at the end of the function
    m_pl->read_list_unlock();
    ret_val = true;
    if (curr_in_clock->count > 0) {
        toku::context pe_ctx(CTX_PARTIAL_EVICTION);

        uint32_t curr_size = curr_in_clock->attr.size;
        // if the size of this PAIR is greater than the average size of PAIRs
        // in the cachetable, then decrement it, otherwise, decrement
        // probabilistically
        if (curr_size*n_in_table >= size_current) {
            curr_in_clock->count--;
        } else {
            // generate a random number between 0 and 2^16
            assert(size_current <= (INT64_MAX / ((1<<16)-1))); // to protect against possible overflows
            int32_t rnd = myrandom_r(&m_random_data) % (1<<16);
            // The if-statement below will be true with probability of
            // curr_size/(average size of PAIR in cachetable)
            // Here is how the math is done:
            //   average_size = size_current/n_in_table
            //   curr_size/average_size = curr_size*n_in_table/size_current
            //   we evaluate if a random number from 0 to 2^16 is less than
            //   than curr_size/average_size * 2^16. So, our if-clause should be
            //    if (2^16*curr_size/average_size > rnd)
            //    this evaluates to:
            //    if (2^16*curr_size*n_in_table/size_current > rnd)
            //    by multiplying each side of the equation by size_current, we get
            //    if (2^16*curr_size*n_in_table > rnd*size_current)
            //    and dividing each side by 2^16,
            //    we get the if-clause below
            //
            if ((((int64_t)curr_size) * n_in_table) >= (((int64_t)rnd) * size_current)>>16) {
                curr_in_clock->count--;
            }
        }
        // call the partial eviction callback
        curr_in_clock->value_rwlock.write_lock(true);

        void *value = curr_in_clock->value_data;
        void* disk_data = curr_in_clock->disk_data;
        void *write_extraargs = curr_in_clock->write_extraargs;
        enum partial_eviction_cost cost;
        long bytes_freed_estimate = 0;
        curr_in_clock->pe_est_callback(
            value, 
            disk_data,
            &bytes_freed_estimate, 
            &cost, 
            write_extraargs
            );
        if (cost == PE_CHEAP) {
            pair_unlock(curr_in_clock);
            curr_in_clock->size_evicting_estimate = 0;
            this->do_partial_eviction(curr_in_clock);
            bjm_remove_background_job(cf->bjm);
        }
        else if (cost == PE_EXPENSIVE) {
            // only bother running an expensive partial eviction
            // if it is expected to free space
            if (bytes_freed_estimate > 0) {
                pair_unlock(curr_in_clock);
                curr_in_clock->size_evicting_estimate = bytes_freed_estimate;
                toku_mutex_lock(&m_ev_thread_lock);
                m_size_evicting += bytes_freed_estimate;
                toku_mutex_unlock(&m_ev_thread_lock);
                toku_kibbutz_enq(
                    m_kibbutz, 
                    cachetable_partial_eviction, 
                    curr_in_clock
                    );
            }
            else {
                curr_in_clock->value_rwlock.write_unlock();
                pair_unlock(curr_in_clock);
                bjm_remove_background_job(cf->bjm);
            }
        }
        else {
            assert(false);
        }        
    }
    else {
        toku::context pe_ctx(CTX_FULL_EVICTION);

        // responsibility of try_evict_pair to eventually remove background job
        // pair's mutex is still grabbed here
        this->try_evict_pair(curr_in_clock);
    }
    // regrab the read list lock, because the caller assumes
    // that it is held. The contract requires this.
    m_pl->read_list_lock();
exit:
    return ret_val;
}

struct pair_unpin_with_new_attr_extra {
    pair_unpin_with_new_attr_extra(evictor *e, PAIR p) :
        ev(e), pair(p) {
    }
    evictor *ev;
    PAIR pair;
};

static void pair_unpin_with_new_attr(PAIR_ATTR new_attr, void *extra) {
    struct pair_unpin_with_new_attr_extra *info =
        reinterpret_cast<struct pair_unpin_with_new_attr_extra *>(extra);
    PAIR p = info->pair;
    evictor *ev = info->ev;

    // change the attr in the evictor, then update the value in the pair
    ev->change_pair_attr(p->attr, new_attr);
    p->attr = new_attr;

    // unpin
    pair_lock(p);
    p->value_rwlock.write_unlock();
    pair_unlock(p);
}

//
// on entry and exit, pair's mutex is not held
// on exit, PAIR is unpinned
//
void evictor::do_partial_eviction(PAIR p) {
    // Copy the old attr
    PAIR_ATTR old_attr = p->attr;
    long long size_evicting_estimate = p->size_evicting_estimate;

    struct pair_unpin_with_new_attr_extra extra(this, p);
    p->pe_callback(p->value_data, old_attr, p->write_extraargs,
                   // passed as the finalize continuation, which allows the
                   // pe_callback to unpin the node before doing expensive cleanup
                   pair_unpin_with_new_attr, &extra);

    // now that the pe_callback (and its pair_unpin_with_new_attr continuation)
    // have finished, we can safely decrease size_evicting
    this->decrease_size_evicting(size_evicting_estimate);
}

//
// CT lock held on entry
// background job has been added for p->cachefile on entry
// responsibility of this function to make sure that background job is removed
//
// on entry, pair's mutex is held, on exit, the pair's mutex is NOT held
//
void evictor::try_evict_pair(PAIR p) {
    CACHEFILE cf = p->cachefile;
    // evictions without a write or unpinned pair's that are clean
    // can be run in the current thread

    // the only caller, run_eviction_on_pair, should call this function
    // only if no one else is trying to use it
    assert(!p->value_rwlock.users());
    p->value_rwlock.write_lock(true);
    // if the PAIR is dirty, the running eviction requires writing the 
    // PAIR out. if the disk_nb_mutex is grabbed, then running 
    // eviction requires waiting for the disk_nb_mutex to become available,
    // which may be expensive. Hence, if either is true, we 
    // do the eviction on a writer thread
    if (!p->dirty && (nb_mutex_writers(&p->disk_nb_mutex) == 0)) {
        p->size_evicting_estimate = 0;
        //
        // This method will unpin PAIR and release PAIR mutex
        //
        // because the PAIR is not dirty, we can safely pass
        // false for the for_checkpoint parameter
        this->evict_pair(p, false);
        bjm_remove_background_job(cf->bjm);
    }
    else {
        pair_unlock(p);
        toku_mutex_lock(&m_ev_thread_lock);
        assert(m_size_evicting >= 0);
        p->size_evicting_estimate = p->attr.size;
        m_size_evicting += p->size_evicting_estimate;
        assert(m_size_evicting >= 0);
        toku_mutex_unlock(&m_ev_thread_lock);
        toku_kibbutz_enq(m_kibbutz, cachetable_evicter, p);
    }
}

//
// Requires: This thread must hold the write lock (nb_mutex) for the pair.
//                The pair's mutex (p->mutex) is also held.
//                on exit, neither is held
//
void evictor::evict_pair(PAIR p, bool for_checkpoint) {
    if (p->dirty) {
        pair_unlock(p);
        cachetable_write_locked_pair(this, p, for_checkpoint);
        pair_lock(p);
    }
    // one thing we can do here is extract the size_evicting estimate,
    // have decrease_size_evicting take the estimate and not the pair,
    // and do this work after we have called 
    // cachetable_maybe_remove_and_free_pair
    this->decrease_size_evicting(p->size_evicting_estimate);
    // if we are to remove this pair, we need the write list lock,
    // to get it in a way that avoids deadlocks, we must first release
    // the pair's mutex, then grab the write list lock, then regrab the 
    // pair's mutex. The pair cannot go anywhere because
    // the pair is still pinned
    nb_mutex_lock(&p->disk_nb_mutex, p->mutex);
    pair_unlock(p);
    m_pl->write_list_lock();
    pair_lock(p);
    p->value_rwlock.write_unlock();
    nb_mutex_unlock(&p->disk_nb_mutex);
    // at this point, we have the pair list's write list lock
    // and we have the pair's mutex (p->mutex) held
    
    // this ensures that a clone running in the background first completes
    bool removed = false;
    if (p->value_rwlock.users() == 0 && p->refcount == 0) {
        // assumption is that if we are about to remove the pair
        // that no one has grabbed the disk_nb_mutex,
        // and that there is no cloned_value_data, because
        // no one is writing a cloned value out.
        assert(nb_mutex_users(&p->disk_nb_mutex) == 0);
        assert(p->cloned_value_data == NULL);
        cachetable_remove_pair(m_pl, this, p);
        removed = true;
    }
    pair_unlock(p);
    m_pl->write_list_unlock();
    // do not want to hold the write list lock while freeing a pair
    if (removed) {
        cachetable_free_pair(p);
    }
}

//
// this function handles the responsibilities for writer threads when they 
// decrease size_evicting. The responsibilities are:
//  - decrease m_size_evicting in a thread safe manner
//  - in some circumstances, signal the eviction thread
//
void evictor::decrease_size_evicting(long size_evicting_estimate) {
    if (size_evicting_estimate > 0) {
        toku_mutex_lock(&m_ev_thread_lock);
        int64_t buffer = m_high_size_hysteresis - m_low_size_watermark;
        // if size_evicting is transitioning from greater than buffer to below buffer, and
        // some client threads are sleeping, we need to wake up the eviction thread.
        // Here is why. In this scenario, we are in one of two cases:
        //  - size_current - size_evicting < low_size_watermark
        //     If this is true, then size_current < high_size_hysteresis, which
        //     means we need to wake up sleeping clients
        //  - size_current - size_evicting > low_size_watermark, 
        //       which means more evictions must be run.
        //  The consequences of both cases are the responsibility 
        //  of the eviction thread. 
        //
        bool need_to_signal_ev_thread = 
            (m_num_sleepers > 0) &&
            !m_ev_thread_is_running &&
            (m_size_evicting > buffer) &&
            ((m_size_evicting - size_evicting_estimate) <= buffer);
        m_size_evicting -= size_evicting_estimate;
        assert(m_size_evicting >= 0);
        if (need_to_signal_ev_thread) {
            this->signal_eviction_thread();
        }
        toku_mutex_unlock(&m_ev_thread_lock);
    }
}

//
// Wait for cache table space to become available 
// size_current is number of bytes currently occupied by data (referred to by pairs)
// size_evicting is number of bytes queued up to be evicted
//
void evictor::wait_for_cache_pressure_to_subside() {
    uint64_t t0 = toku_current_time_microsec();
    toku_mutex_lock(&m_ev_thread_lock);
    m_num_sleepers++;
    this->signal_eviction_thread();
    toku_cond_wait(&m_flow_control_cond, &m_ev_thread_lock);    
    m_num_sleepers--;
    toku_mutex_unlock(&m_ev_thread_lock);
    uint64_t t1 = toku_current_time_microsec();
    increment_partitioned_counter(m_wait_pressure_count, 1);
    uint64_t tdelta = t1 - t0;
    increment_partitioned_counter(m_wait_pressure_time, tdelta);
    if (tdelta > 1000000) {
        increment_partitioned_counter(m_long_wait_pressure_count, 1);
        increment_partitioned_counter(m_long_wait_pressure_time, tdelta);
    }
}

//
// Get the status of the current estimated size of the cachetable,
// and the evictor's set limit. 
//
void evictor::get_state(long *size_current_ptr, long *size_limit_ptr) {
    if (size_current_ptr) {
        *size_current_ptr = m_size_current;
    }
    if (size_limit_ptr) {
        *size_limit_ptr = m_low_size_watermark;
    }
}

//
// Force the eviction thread to do some work.
//
// This function does not require any mutex to be held. 
// As a result, scheduling is not guaranteed, but that is tolerable.
//
void evictor::signal_eviction_thread() {
    toku_cond_signal(&m_ev_thread_cond);
}

//
// Returns true if the cachetable is so over subscribed, that a client thread should sleep
//
// This function may be called in a thread-unsafe manner. Locks are not
// required to read size_current. The result is that 
// the values may be a little off, but we think that is tolerable.
//
bool evictor::should_client_thread_sleep(){
    return unsafe_read_size_current() > m_high_size_watermark;
}

//
// Returns true if a sleeping client should be woken up because
// the cachetable is not overly subscribed
//
// This function may be called in a thread-unsafe manner. Locks are not
// required to read size_current. The result is that 
// the values may be a little off, but we think that is tolerable.
//
bool evictor::should_sleeping_clients_wakeup() {
    return unsafe_read_size_current() <= m_high_size_hysteresis;
}

// 
// Returns true if a client thread should try to wake up the eviction
// thread because the client thread has noticed too much data taken
// up in the cachetable.
//
// This function may be called in a thread-unsafe manner. Locks are not
// required to read size_current or size_evicting. The result is that 
// the values may be a little off, but we think that is tolerable.
// If the caller wants to ensure that ev_thread_is_running and size_evicting
// are accurate, then the caller must hold ev_thread_lock before
// calling this function.
//
bool evictor::should_client_wake_eviction_thread() {
    return
        !m_ev_thread_is_running &&
        ((unsafe_read_size_current() - m_size_evicting) > m_low_size_hysteresis);
}

//
// Determines if eviction is needed. If the current size of
// the cachetable exceeds the sum of our fixed size limit and
// the amount of data currently being evicted, then eviction is needed
//
bool evictor::eviction_needed() {
    return (m_size_current - m_size_evicting) > m_low_size_watermark;
}

inline int64_t evictor::unsafe_read_size_current(void) const {
    return m_size_current;
}

void evictor::fill_engine_status() {
    STATUS_VALUE(CT_SIZE_CURRENT)           = m_size_current;
    STATUS_VALUE(CT_SIZE_LIMIT)             = m_low_size_hysteresis;
    STATUS_VALUE(CT_SIZE_WRITING)           = m_size_evicting;
    STATUS_VALUE(CT_SIZE_NONLEAF) = read_partitioned_counter(m_size_nonleaf);
    STATUS_VALUE(CT_SIZE_LEAF) = read_partitioned_counter(m_size_leaf);
    STATUS_VALUE(CT_SIZE_ROLLBACK) = read_partitioned_counter(m_size_rollback);
    STATUS_VALUE(CT_SIZE_CACHEPRESSURE) = read_partitioned_counter(m_size_cachepressure);
    STATUS_VALUE(CT_SIZE_CLONED) = m_size_cloned_data;
    STATUS_VALUE(CT_WAIT_PRESSURE_COUNT) = read_partitioned_counter(m_wait_pressure_count);
    STATUS_VALUE(CT_WAIT_PRESSURE_TIME) = read_partitioned_counter(m_wait_pressure_time);
    STATUS_VALUE(CT_LONG_WAIT_PRESSURE_COUNT) = read_partitioned_counter(m_long_wait_pressure_count);
    STATUS_VALUE(CT_LONG_WAIT_PRESSURE_TIME) = read_partitioned_counter(m_long_wait_pressure_time);
}

////////////////////////////////////////////////////////////////////////////////

ENSURE_POD(checkpointer);

//
// Sets the cachetable reference in this checkpointer class, this is temporary.
//
int checkpointer::init(pair_list *_pl, 
                        TOKULOGGER _logger,
                        evictor *_ev,
                        cachefile_list *files) {
    m_list = _pl;
    m_logger = _logger;
    m_ev = _ev;
    m_cf_list = files;
    bjm_init(&m_checkpoint_clones_bjm);
    
    // Default is no checkpointing.
    m_checkpointer_cron_init = false;
    int r = toku_minicron_setup(&m_checkpointer_cron, 0, checkpoint_thread, this);
    if (r == 0) {
        m_checkpointer_cron_init = true;
    }
    m_checkpointer_init = true;
    return r;
}

void checkpointer::destroy() {
    if (!m_checkpointer_init) {
        return;
    }
    if (m_checkpointer_cron_init && !this->has_been_shutdown()) {
        // for test code only, production code uses toku_cachetable_minicron_shutdown()
        int r = this->shutdown();
        assert(r == 0);
    }
    bjm_destroy(m_checkpoint_clones_bjm);
}

//
// Sets how often the checkpoint thread will run, in seconds
//
void checkpointer::set_checkpoint_period(uint32_t new_period) {
    toku_minicron_change_period(&m_checkpointer_cron, new_period*1000);
}

//
// Sets how often the checkpoint thread will run.
//
uint32_t checkpointer::get_checkpoint_period() {
    return toku_minicron_get_period_in_seconds_unlocked(&m_checkpointer_cron);
}

//
// Stops the checkpoint thread.
//
int checkpointer::shutdown() {
    return toku_minicron_shutdown(&m_checkpointer_cron);
}

//
// If checkpointing is running, this returns false.
//
bool checkpointer::has_been_shutdown() {
    return toku_minicron_has_been_shutdown(&m_checkpointer_cron);
}

TOKULOGGER checkpointer::get_logger() {
    return m_logger;
}

void checkpointer::increment_num_txns() {
    m_checkpoint_num_txns++;
}

struct iterate_begin_checkpoint {
    LSN lsn_of_checkpoint_in_progress;
    iterate_begin_checkpoint(LSN lsn) : lsn_of_checkpoint_in_progress(lsn) { }
    static int fn(const CACHEFILE &cf, const uint32_t UU(idx), struct iterate_begin_checkpoint *info) {
        assert(cf->begin_checkpoint_userdata);
        if (cf->for_checkpoint) {
            cf->begin_checkpoint_userdata(info->lsn_of_checkpoint_in_progress, cf->userdata);
        }
        return 0;
    }
};

//
// Update the user data in any cachefiles in our checkpoint list.
//
void checkpointer::update_cachefiles() {
    struct iterate_begin_checkpoint iterate(m_lsn_of_checkpoint_in_progress);
    int r = m_cf_list->m_active_fileid.iterate<struct iterate_begin_checkpoint,
                                               iterate_begin_checkpoint::fn>(&iterate);
    assert_zero(r);
}

struct iterate_note_pin {
    static int fn(const CACHEFILE &cf, uint32_t UU(idx), void **UU(extra)) {
        assert(cf->note_pin_by_checkpoint);
        cf->note_pin_by_checkpoint(cf, cf->userdata);
        cf->for_checkpoint = true;
        return 0;
    }
};

//
// Sets up and kicks off a checkpoint.
//
void checkpointer::begin_checkpoint() {
    // 1. Initialize the accountability counters.
    m_checkpoint_num_txns = 0;
    
    // 2. Make list of cachefiles to be included in the checkpoint.
    m_cf_list->read_lock();
    m_cf_list->m_active_fileid.iterate<void *, iterate_note_pin::fn>(nullptr);
    m_checkpoint_num_files = m_cf_list->m_active_fileid.size();
    m_cf_list->read_unlock();
    
    // 3. Create log entries for this checkpoint.
    if (m_logger) {
        this->log_begin_checkpoint();
    }

    bjm_reset(m_checkpoint_clones_bjm);

    m_list->write_pending_exp_lock();
    m_list->read_list_lock();
    m_cf_list->read_lock(); // needed for update_cachefiles
    m_list->write_pending_cheap_lock();
    // 4. Turn on all the relevant checkpoint pending bits.
    this->turn_on_pending_bits();
    
    // 5.
    this->update_cachefiles();
    m_list->write_pending_cheap_unlock();
    m_cf_list->read_unlock();
    m_list->read_list_unlock();
    m_list->write_pending_exp_unlock();
}

struct iterate_log_fassociate {
    static int fn(const CACHEFILE &cf, uint32_t UU(idx), void **UU(extra)) {
        assert(cf->log_fassociate_during_checkpoint);
        cf->log_fassociate_during_checkpoint(cf, cf->userdata);
        return 0;
    }
};

//
// Assuming the logger exists, this will write out the folloing 
// information to the log.
//
// 1. Writes the BEGIN_CHECKPOINT to the log.
// 2. Writes the list of open dictionaries to the log.
// 3. Writes the list of open transactions to the log.
// 4. Writes the list of dicionaries that have had rollback logs suppresed.
//
// NOTE: This also has the side effecto of setting the LSN
// of checkpoint in progress.
//
void checkpointer::log_begin_checkpoint() {
    int r = 0;
    
    // Write the BEGIN_CHECKPOINT to the log.
    LSN begin_lsn={ .lsn = (uint64_t) -1 }; // we'll need to store the lsn of the checkpoint begin in all the trees that are checkpointed.
    TXN_MANAGER mgr = toku_logger_get_txn_manager(m_logger);
    TXNID last_xid = toku_txn_manager_get_last_xid(mgr);
    toku_log_begin_checkpoint(m_logger, &begin_lsn, 0, 0, last_xid);
    m_lsn_of_checkpoint_in_progress = begin_lsn;

    // Log the list of open dictionaries.
    m_cf_list->m_active_fileid.iterate<void *, iterate_log_fassociate::fn>(nullptr);

    // Write open transactions to the log.
    r = toku_txn_manager_iter_over_live_txns(
        m_logger->txn_manager,
        log_open_txn,
        this
        );
    assert(r == 0);
}

//
// Sets the pending bits of EVERY PAIR in the cachetable, regardless of
// whether the PAIR is clean or not. It will be the responsibility of
// end_checkpoint or client threads to simply clear the pending bit
// if the PAIR is clean.
//
// On entry and exit , the pair list's read list lock is grabbed, and
// both pending locks are grabbed
//
void checkpointer::turn_on_pending_bits() {
    PAIR p = NULL;
    uint32_t i;
    for (i = 0, p = m_list->m_checkpoint_head; i < m_list->m_n_in_table; i++, p = p->clock_next) {
        assert(!p->checkpoint_pending);
        //Only include pairs belonging to cachefiles in the checkpoint
        if (!p->cachefile->for_checkpoint) {
            continue;
        }
        // Mark everything as pending a checkpoint
        //
        // The rule for the checkpoint_pending bit is as follows:
        //  - begin_checkpoint may set checkpoint_pending to true
        //    even though the pair lock on the node is not held.
        //  - any thread that wants to clear the pending bit must own
        //     the PAIR lock. Otherwise,
        //     we may end up clearing the pending bit before the
        //     current lock is ever released.
        p->checkpoint_pending = true;
        if (m_list->m_pending_head) {
            m_list->m_pending_head->pending_prev = p;
        }
        p->pending_next = m_list->m_pending_head;
        p->pending_prev = NULL;
        m_list->m_pending_head = p;
    }
    invariant(p == m_list->m_checkpoint_head);
}

void checkpointer::add_background_job() {
    int r = bjm_add_background_job(m_checkpoint_clones_bjm);
    assert_zero(r);
}
void checkpointer::remove_background_job() {
    bjm_remove_background_job(m_checkpoint_clones_bjm);
}

void checkpointer::end_checkpoint(void (*testcallback_f)(void*),  void* testextra) {
    toku::scoped_malloc checkpoint_cfs_buf(m_checkpoint_num_files * sizeof(CACHEFILE));
    CACHEFILE *checkpoint_cfs = reinterpret_cast<CACHEFILE *>(checkpoint_cfs_buf.get());

    this->fill_checkpoint_cfs(checkpoint_cfs);    
    this->checkpoint_pending_pairs();
    this->checkpoint_userdata(checkpoint_cfs);
    // For testing purposes only.  Dictionary has been fsync-ed to disk but log has not yet been written.
    if (testcallback_f) {
        testcallback_f(testextra);      
    }
    this->log_end_checkpoint();
    this->end_checkpoint_userdata(checkpoint_cfs);

    // Delete list of cachefiles in the checkpoint,
    this->remove_cachefiles(checkpoint_cfs);
}

struct iterate_checkpoint_cfs {
    CACHEFILE *checkpoint_cfs;
    uint32_t checkpoint_num_files;
    uint32_t curr_index;
    iterate_checkpoint_cfs(CACHEFILE *cfs, uint32_t num_files) :
        checkpoint_cfs(cfs), checkpoint_num_files(num_files), curr_index(0) {
    }
    static int fn(const CACHEFILE &cf, uint32_t UU(idx), struct iterate_checkpoint_cfs *info) {
        if (cf->for_checkpoint) {
            assert(info->curr_index < info->checkpoint_num_files);
            info->checkpoint_cfs[info->curr_index] = cf;
            info->curr_index++;
        }
        return 0;
    }
};

void checkpointer::fill_checkpoint_cfs(CACHEFILE* checkpoint_cfs) {
    struct iterate_checkpoint_cfs iterate(checkpoint_cfs, m_checkpoint_num_files);

    m_cf_list->read_lock();
    m_cf_list->m_active_fileid.iterate<struct iterate_checkpoint_cfs, iterate_checkpoint_cfs::fn>(&iterate);
    assert(iterate.curr_index == m_checkpoint_num_files);
    m_cf_list->read_unlock();
}

void checkpointer::checkpoint_pending_pairs() {
    PAIR p;
    m_list->read_list_lock();
    while ((p = m_list->m_pending_head)!=0) {
        // <CER> TODO: Investigate why we move pending head outisde of the pending_pairs_remove() call.
        m_list->m_pending_head = m_list->m_pending_head->pending_next;
        m_list->pending_pairs_remove(p);
        // if still pending, clear the pending bit and write out the node
        pair_lock(p);
        m_list->read_list_unlock();
        write_pair_for_checkpoint_thread(m_ev, p);
        pair_unlock(p);
        m_list->read_list_lock();
    }
    assert(!m_list->m_pending_head);
    m_list->read_list_unlock();
    bjm_wait_for_jobs_to_finish(m_checkpoint_clones_bjm);
}

void checkpointer::checkpoint_userdata(CACHEFILE* checkpoint_cfs) {
    // have just written data blocks, so next write the translation and header for each open dictionary
    for (uint32_t i = 0; i < m_checkpoint_num_files; i++) {
        CACHEFILE cf = checkpoint_cfs[i];
        assert(cf->for_checkpoint);
        assert(cf->checkpoint_userdata);
        toku_cachetable_set_checkpointing_user_data_status(1);
        cf->checkpoint_userdata(cf, cf->fd, cf->userdata);
        toku_cachetable_set_checkpointing_user_data_status(0);
    }
}

void checkpointer::log_end_checkpoint() {
    if (m_logger) {
        toku_log_end_checkpoint(m_logger, NULL,
                                1, // want the end_checkpoint to be fsync'd
                                m_lsn_of_checkpoint_in_progress, 
                                0,
                                m_checkpoint_num_files,
                                m_checkpoint_num_txns);
        toku_logger_note_checkpoint(m_logger, m_lsn_of_checkpoint_in_progress);
    }
}

void checkpointer::end_checkpoint_userdata(CACHEFILE* checkpoint_cfs) {
    // everything has been written to file and fsynced
    // ... call checkpoint-end function in block translator
    //     to free obsolete blocks on disk used by previous checkpoint
    //cachefiles_in_checkpoint is protected by the checkpoint_safe_lock
    for (uint32_t i = 0; i < m_checkpoint_num_files; i++) {
        CACHEFILE cf = checkpoint_cfs[i];
        assert(cf->for_checkpoint);
        assert(cf->end_checkpoint_userdata);
        cf->end_checkpoint_userdata(cf, cf->fd, cf->userdata);
    }
}

//
// Deletes all the cachefiles in this checkpointers cachefile list. 
//
void checkpointer::remove_cachefiles(CACHEFILE* checkpoint_cfs) {
    // making this a while loop because note_unpin_by_checkpoint may destroy the cachefile
    for (uint32_t i = 0; i < m_checkpoint_num_files; i++) {
        CACHEFILE cf = checkpoint_cfs[i];
        // Checking for function existing so that this function
        // can be called from cachetable tests.
        assert(cf->for_checkpoint);
        cf->for_checkpoint = false;
        assert(cf->note_unpin_by_checkpoint);
        // Clear the bit saying theis file is in the checkpoint.
        cf->note_unpin_by_checkpoint(cf, cf->userdata);
    }
}


////////////////////////////////////////////////////////
//
// cachefiles list
//
static_assert(std::is_pod<cachefile_list>::value, "cachefile_list isn't POD");

void cachefile_list::init() {
    m_next_filenum_to_use.fileid = 0;
    m_next_hash_id_to_use = 0;
    toku_pthread_rwlock_init(&m_lock, NULL);
    m_active_filenum.create();
    m_active_fileid.create();
    m_stale_fileid.create();
}

void cachefile_list::destroy() {
    m_active_filenum.destroy();
    m_active_fileid.destroy();
    m_stale_fileid.destroy();
    toku_pthread_rwlock_destroy(&m_lock);
}

void cachefile_list::read_lock() {
    toku_pthread_rwlock_rdlock(&m_lock);
}

void cachefile_list::read_unlock() {
    toku_pthread_rwlock_rdunlock(&m_lock);
}

void cachefile_list::write_lock() {
    toku_pthread_rwlock_wrlock(&m_lock);
}

void cachefile_list::write_unlock() {
    toku_pthread_rwlock_wrunlock(&m_lock);
}

struct iterate_find_iname {
    const char *iname_in_env;
    CACHEFILE found_cf;
    iterate_find_iname(const char *iname) : iname_in_env(iname), found_cf(nullptr) { }
    static int fn(const CACHEFILE &cf, uint32_t UU(idx), struct iterate_find_iname *info) {
        if (cf->fname_in_env && strcmp(cf->fname_in_env, info->iname_in_env) == 0) {
            info->found_cf = cf;
            return -1;
        }
        return 0;
    }
};

int cachefile_list::cachefile_of_iname_in_env(const char *iname_in_env, CACHEFILE *cf) {
    struct iterate_find_iname iterate(iname_in_env);

    read_lock();
    int r = m_active_fileid.iterate<iterate_find_iname, iterate_find_iname::fn>(&iterate);
    if (iterate.found_cf != nullptr) {
        assert(strcmp(iterate.found_cf->fname_in_env, iname_in_env) == 0);
        *cf = iterate.found_cf;
        r = 0;
    } else {
        r = ENOENT;
    }
    read_unlock();
    return r;
}

static int cachefile_find_by_filenum(const CACHEFILE &a_cf, const FILENUM &b) {
    const FILENUM a = a_cf->filenum;
    if (a.fileid < b.fileid) {
        return -1;
    } else if (a.fileid == b.fileid) {
        return 0;
    } else {
        return 1;
    }
}

int cachefile_list::cachefile_of_filenum(FILENUM filenum, CACHEFILE *cf) {
    read_lock();
    int r = m_active_filenum.find_zero<FILENUM, cachefile_find_by_filenum>(filenum, cf, nullptr);
    if (r == DB_NOTFOUND) {
        r = ENOENT;
    } else {
        invariant_zero(r);
    }
    read_unlock();
    return r;
}

static int cachefile_find_by_fileid(const CACHEFILE &a_cf, const struct fileid &b) {
    return toku_fileid_cmp(a_cf->fileid, b);
}

void cachefile_list::add_cf_unlocked(CACHEFILE cf) {
    int r;
    r = m_active_filenum.insert<FILENUM, cachefile_find_by_filenum>(cf, cf->filenum, nullptr);
    assert_zero(r);
    r = m_active_fileid.insert<struct fileid, cachefile_find_by_fileid>(cf, cf->fileid, nullptr);
    assert_zero(r);
}

void cachefile_list::add_stale_cf(CACHEFILE cf) {
    write_lock();
    int r = m_stale_fileid.insert<struct fileid, cachefile_find_by_fileid>(cf, cf->fileid, nullptr);
    assert_zero(r);
    write_unlock();
}

void cachefile_list::remove_cf(CACHEFILE cf) {
    write_lock();

    uint32_t idx;
    int r;
    r = m_active_filenum.find_zero<FILENUM, cachefile_find_by_filenum>(cf->filenum, nullptr, &idx);
    assert_zero(r);
    r = m_active_filenum.delete_at(idx);
    assert_zero(r);

    r = m_active_fileid.find_zero<struct fileid, cachefile_find_by_fileid>(cf->fileid, nullptr, &idx);
    assert_zero(r);
    r = m_active_fileid.delete_at(idx);
    assert_zero(r);

    write_unlock();
}

void cachefile_list::remove_stale_cf_unlocked(CACHEFILE cf) {
    uint32_t idx;
    int r;
    r = m_stale_fileid.find_zero<struct fileid, cachefile_find_by_fileid>(cf->fileid, nullptr, &idx);
    assert_zero(r);
    r = m_stale_fileid.delete_at(idx);
    assert_zero(r);
}

FILENUM cachefile_list::reserve_filenum() {
    // taking a write lock because we are modifying next_filenum_to_use
    write_lock();
    while (1) {
        int r = m_active_filenum.find_zero<FILENUM, cachefile_find_by_filenum>(m_next_filenum_to_use, nullptr, nullptr);
        if (r == 0) {
            m_next_filenum_to_use.fileid++;
            continue;
        }
        assert(r == DB_NOTFOUND);
        break;
    }
    FILENUM filenum = m_next_filenum_to_use;
    m_next_filenum_to_use.fileid++;
    write_unlock();
    return filenum;
}

uint32_t cachefile_list::get_new_hash_id_unlocked() {
    uint32_t retval = m_next_hash_id_to_use;
    m_next_hash_id_to_use++;
    return retval;
}

CACHEFILE cachefile_list::find_cachefile_unlocked(struct fileid* fileid) {
    CACHEFILE cf = nullptr;
    int r = m_active_fileid.find_zero<struct fileid, cachefile_find_by_fileid>(*fileid, &cf, nullptr);
    if (r == 0) {
        assert(!cf->unlink_on_close);
    }
    return cf;
}

CACHEFILE cachefile_list::find_stale_cachefile_unlocked(struct fileid* fileid) {
    CACHEFILE cf = nullptr;
    int r = m_stale_fileid.find_zero<struct fileid, cachefile_find_by_fileid>(*fileid, &cf, nullptr);
    if (r == 0) {
        assert(!cf->unlink_on_close);
    }
    return cf;
}

void cachefile_list::verify_unused_filenum(FILENUM filenum) {
    int r = m_active_filenum.find_zero<FILENUM, cachefile_find_by_filenum>(filenum, nullptr, nullptr);
    assert(r == DB_NOTFOUND);
}

// returns true if some eviction ran, false otherwise
bool cachefile_list::evict_some_stale_pair(evictor* ev) {
    write_lock();
    if (m_stale_fileid.size() == 0) {
        write_unlock();
        return false;
    }

    CACHEFILE stale_cf = nullptr;
    int r = m_stale_fileid.fetch(0, &stale_cf);
    assert_zero(r);

    // we should not have a cf in the stale list
    // that does not have any pairs
    PAIR p = stale_cf->cf_head;
    paranoid_invariant(p != NULL);
    evict_pair_from_cachefile(p);

    // now that we have evicted something,
    // let's check if the cachefile is needed anymore
    //
    // it is not needed if the latest eviction caused
    // the cf_head for that cf to become null
    bool destroy_cf = stale_cf->cf_head == nullptr;
    if (destroy_cf) {
        remove_stale_cf_unlocked(stale_cf);
    }

    write_unlock();
    
    ev->remove_pair_attr(p->attr);
    cachetable_free_pair(p);
    if (destroy_cf) {
        cachefile_destroy(stale_cf);
    }
    return true;
}

void cachefile_list::free_stale_data(evictor* ev) {
    write_lock();
    while (m_stale_fileid.size() != 0) {
        CACHEFILE stale_cf = nullptr;
        int r = m_stale_fileid.fetch(0, &stale_cf); 
        assert_zero(r);

        // we should not have a cf in the stale list
        // that does not have any pairs
        PAIR p = stale_cf->cf_head;
        paranoid_invariant(p != NULL);
        
        evict_pair_from_cachefile(p);
        ev->remove_pair_attr(p->attr);
        cachetable_free_pair(p);
        
        // now that we have evicted something,
        // let's check if the cachefile is needed anymore
        if (stale_cf->cf_head == NULL) {
            remove_stale_cf_unlocked(stale_cf);
            cachefile_destroy(stale_cf);
        }
    }
    write_unlock();
}

void __attribute__((__constructor__)) toku_cachetable_helgrind_ignore(void);
void
toku_cachetable_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cachetable_miss, sizeof cachetable_miss);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cachetable_misstime, sizeof cachetable_misstime);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cachetable_prefetches, sizeof cachetable_prefetches);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cachetable_evictions, sizeof cachetable_evictions);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&cleaner_executions, sizeof cleaner_executions);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&ct_status, sizeof ct_status);
}

#undef STATUS_VALUE
