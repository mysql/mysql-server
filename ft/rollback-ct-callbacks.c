/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "rollback-ct-callbacks.h"

#include <toku_portability.h>
#include <memory.h>
#include "ft-internal.h"
#include "fttypes.h"
#include "memarena.h"
#include "rollback.h"


// Cleanup the rollback memory
static void
rollback_log_destroy(ROLLBACK_LOG_NODE log) {
    memarena_close(&log->rollentry_arena);
    toku_free(log);
}

// Write something out.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.
void toku_rollback_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM logname,
                                          void *rollback_v,  void** UU(disk_data), void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size,
                                          BOOL write_me, BOOL keep_me, BOOL for_checkpoint, BOOL UU(is_clone)) {
    int r;
    ROLLBACK_LOG_NODE  log = cast_to_typeof(log) rollback_v;
    FT h = cast_to_typeof(h) extraargs;

    assert(log->blocknum.b==logname.b);
    if (write_me && !h->panic) {
        int n_workitems, n_threads; 
        assert(h->cf == cachefile);
        toku_cachefile_get_workqueue_load(cachefile, &n_workitems, &n_threads);

        r = toku_serialize_rollback_log_to(fd, log->blocknum, log, h, n_workitems, n_threads, for_checkpoint);
        if (r) {
            if (h->panic==0) {
                char *e = strerror(r);
                int   l = 200 + strlen(e);
                char s[l];
                h->panic=r;
                snprintf(s, l-1, "While writing data to disk, error %d (%s)", r, e);
                h->panic_string = toku_strdup(s);
            }
        }
    }
    *new_size = size;
    if (!keep_me) {
        rollback_log_destroy(log);
    }
}

int toku_rollback_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM logname, u_int32_t fullhash,
                                 void **rollback_pv,  void** UU(disk_data), PAIR_ATTR *sizep, int * UU(dirtyp), void *extraargs) {
    int r;
    FT h = cast_to_typeof(h) extraargs;
    assert(h->cf == cachefile);

    ROLLBACK_LOG_NODE *result = (ROLLBACK_LOG_NODE*)rollback_pv;
    r = toku_deserialize_rollback_log_from(fd, logname, fullhash, result, h);
    if (r==0) {
        *sizep = rollback_memory_size(*result);
    }
    return r;
}

void toku_rollback_pe_est_callback(
    void* rollback_v, 
    void* UU(disk_data),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    assert(rollback_v != NULL);
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

// callback for partially evicting a cachetable entry
int toku_rollback_pe_callback (
    void *rollback_v, 
    PAIR_ATTR UU(old_attr), 
    PAIR_ATTR* new_attr, 
    void* UU(extraargs)
    ) 
{
    assert(rollback_v != NULL);
    *new_attr = old_attr;
    return 0;
}

// partial fetch is never required for a rollback log node
BOOL toku_rollback_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    return FALSE;
}

// a rollback node should never be partial fetched, 
// because we always say it is not required.
// (pf req callback always returns false)
int toku_rollback_pf_callback(void* UU(ftnode_pv),  void* UU(disk_data), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
    assert(FALSE);
    return 0;
}

// the cleaner thread should never choose a rollback node for cleaning
int toku_rollback_cleaner_callback (
    void* UU(ftnode_pv),
    BLOCKNUM UU(blocknum),
    u_int32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(FALSE);
    return 0;
}

